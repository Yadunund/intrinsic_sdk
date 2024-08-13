// Copyright 2023 Intrinsic Innovation LLC

#include "intrinsic/icon/interprocess/binary_futex.h"

#include <linux/futex.h>
#include <sys/syscall.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "intrinsic/icon/testing/realtime_annotations.h"
#include "intrinsic/icon/utils/realtime_status.h"

namespace intrinsic::icon {
namespace {

inline int64_t futex(std::atomic<uint32_t> *uaddr, int futex_op, uint32_t val,
                     bool private_futex,
                     const struct timespec *timeout = nullptr,
                     uint32_t *uaddr2 = nullptr,
                     uint32_t val3 = 0) INTRINSIC_SUPPRESS_REALTIME_CHECK {
  // For the static analysis, we allow futex operations even though they can
  // be blocking, because the blocking behavior is usually intended.

  if (private_futex) {
    // This option bit can be employed with all futex operations. It tells the
    // kernel that the futex is process-private and not shared with another
    // process (i.e., it is being used for synchronization only between threads
    // of the same process). This allows the kernel to make some
    // additional performance optimizations.
    futex_op |= FUTEX_PRIVATE_FLAG;
  }
  return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2,
                 FUTEX_BITSET_MATCH_ANY);
}

// Iff the value is one, we return true and set the value to zero.
bool TryWait(std::atomic<uint32_t> &val) {
  uint32_t one = 1;
  return val.compare_exchange_strong(one, 0);
}

RealtimeStatus Wait(std::atomic<uint32_t> &val, const timespec *ts,
                    bool private_futex) {
  const absl::Time start_time = absl::Now();
  while (true) {
    if (TryWait(val)) {
      return OkStatus();
    }

    // The value is not yet what we expect, let's wait for it.
    auto ret = futex(&val, FUTEX_WAIT_BITSET | FUTEX_CLOCK_REALTIME, 0,
                     private_futex, ts);
    if (ret == -1 && errno == ETIMEDOUT) {
      return DeadlineExceededError(RealtimeStatus::StrCat(
          "Timeout after ",
          absl::ToDoubleMilliseconds(absl::Now() - start_time), " ms"));
    }
    // If we have an EAGAIN, we woke up because another thread changed the
    // underlying value and we can retry decrementing the atomic value.
    // If we have EINTR, we woke up from an external signal. We've encountered
    // various SIGPROF when running on forge and decided to ignore these and
    // retry sleeping. If we have any other error message, that means we have an
    // actual error.
    if (ret == -1 && errno != EAGAIN && errno != EINTR) {
      return InternalError(strerror(errno));
    }
  }
}

}  // namespace

BinaryFutex::BinaryFutex(bool posted, bool private_futex)
    : val_(posted == true ? 1 : 0), private_futex_(private_futex) {}
BinaryFutex::BinaryFutex(BinaryFutex &&other) : val_(other.val_.load()) {}
BinaryFutex &BinaryFutex::operator=(BinaryFutex &&other) {
  if (this != &other) {
    val_.store(other.val_.load());
  }
  return *this;
}

RealtimeStatus BinaryFutex::Post() {
  uint32_t zero = 0;
  // We need to make a copy of the private_futex_ member variable. Otherwise,
  // we might end up with a data race if the thread destructing the futex
  // reads `val_` between the `compare_exchange_strong` and the
  // `futex` call.
  const bool private_futex = private_futex_;
  // Take the address before, since the class instance could be destroyed before
  // `futex()` is called.
  std::atomic<uint32_t> *val_addr = &val_;
  if (val_.compare_exchange_strong(zero, 1)) {
    // `futex` could fail with EFAULT, if `val_addr` is not a valid
    // user-space address anymore. This can happen if another thread destroyed
    // the BinaryFutex between the previous line and this one. Therefore, we
    // ignore the return value here. Another error value should only be EINVAL,
    // which should never happen here
    // ("EINVAL: The kernel detected an inconsistency between the user-space
    // state at uaddr and the kernel state—that is, it detected a waiter which
    // waits in FUTEX_LOCK_PI or FUTEX_LOCK_PI2 on uaddr.").
    //
    // One indicating that we wake up at most 1 other client.
    futex(val_addr, FUTEX_WAKE, 1, private_futex);
  }
  return OkStatus();
}

RealtimeStatus BinaryFutex::WaitUntil(absl::Time deadline) const {
  if (deadline == absl::InfiniteFuture()) {
    return Wait(val_, nullptr, private_futex_);
  }
  auto ts = absl::ToTimespec(deadline);
  return Wait(val_, &ts, private_futex_);
}

RealtimeStatus BinaryFutex::WaitFor(absl::Duration timeout) const {
  return WaitUntil(absl::Now() + timeout);
}

bool BinaryFutex::TryWait() const { return icon::TryWait(val_); }

uint32_t BinaryFutex::Value() const { return val_; }

}  // namespace intrinsic::icon
