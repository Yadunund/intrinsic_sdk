// Copyright 2023 Intrinsic Innovation LLC

#include "intrinsic/util/log_lines.h"

#include <string_view>

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"

namespace intrinsic {

void LogLines(int severity, std::string_view text, const char* file_name,
              int line_number) {
  // Since there are multiple lines, using FATAL will cause only
  // the first line to be printed, so downgrade to ERROR, but
  // remember that it is FATAL so that we abort after logging everything.
  const int original_severity = severity;
  if (severity == static_cast<int>(absl::LogSeverity::kFatal)) {
    severity = static_cast<int>(absl::LogSeverity::kError);
  }

  // Split the string by newline, log every individual line
  std::string_view::size_type current_position = 0;
  std::string_view::size_type newline_at = text.find_first_of('\n');
  while (current_position < text.size() &&
         newline_at != std::string_view::npos) {
    std::string_view::size_type str_length = newline_at - current_position;
    LOG(LEVEL(severity)).AtLocation(file_name, line_number)
        << absl::ClippedSubstr(text, current_position, str_length);
    current_position = newline_at + 1;
    newline_at = text.find_first_of('\n', current_position);
  }

  // Residual text (no more newline)
  if (current_position < text.size()) {
    LOG(LEVEL(severity)).AtLocation(file_name, line_number)
        << absl::ClippedSubstr(text, current_position);
  }

  // If this was a fatal error abort
  if (original_severity == static_cast<int>(absl::LogSeverity::kFatal)) {
    LOG(FATAL) << "Aborting due to previous errors.";
  }
}

}  //  end namespace intrinsic
