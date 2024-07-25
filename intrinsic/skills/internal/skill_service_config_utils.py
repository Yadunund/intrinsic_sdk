# Copyright 2023 Intrinsic Innovation LLC

"""Utils for working with the SkillServiceConfig proto."""

from absl import logging
from google.protobuf import descriptor_pb2
from intrinsic.skills.internal import proto_utils
from intrinsic.skills.proto import skill_manifest_pb2
from intrinsic.skills.proto import skill_service_config_pb2


def get_skill_service_config(
    filename: str,
) -> skill_service_config_pb2.SkillServiceConfig:
  """Returns the SkillServiceConfig at `skill_service_config_filename`.

  Reads the proto binary file at `skill_service_config_filename` and returns
  the contents. This file must contain a proto binary
  intrinsic_proto.skills.SkillServiceConfig message.

  Args:
    filename: the filename of the binary proto file

  Returns:
    A skill_service_config_pb2.SkillServiceConfig message
  """
  logging.info(
      "Reading skill configuration proto from: %s",
      filename,
  )
  service_config = skill_service_config_pb2.SkillServiceConfig()
  logging.info("\nImporting service_config from file.")
  with open(filename, "rb") as f:
    service_config.ParseFromString(f.read())
  logging.info("\nUsing skill configuration proto:\n%s", service_config)
  return service_config


def get_skill_service_config_from_manifest(
    manifest_pbbin_filename: str,
    proto_descriptor_pbbin_filename: str,
    version: str,
) -> skill_service_config_pb2.SkillServiceConfig:
  """Returns the SkillServiceConfig for the skill with the given manifest."""
  service_config = skill_service_config_pb2.SkillServiceConfig()

  logging.info("Loading manifest from: %s", manifest_pbbin_filename)
  with open(manifest_pbbin_filename, "rb") as f:
    manifest = skill_manifest_pb2.Manifest.FromString(f.read())

  service_config.skill_name = manifest.id.name
  if manifest.options.HasField("cancellation_ready_timeout"):
    service_config.execution_service_options.cancellation_ready_timeout.CopyFrom(
        manifest.options.cancellation_ready_timeout
    )
  if manifest.options.HasField("execution_timeout"):
    service_config.execution_service_options.execution_timeout.CopyFrom(
        manifest.options.execution_timeout
    )

  service_config.status_info.extend(manifest.status_info)

  logging.info(
      "Reading proto descriptor proto from: %s",
      proto_descriptor_pbbin_filename,
  )
  with open(proto_descriptor_pbbin_filename, "rb") as f:
    file_descriptor_set = descriptor_pb2.FileDescriptorSet.FromString(f.read())

  service_config.skill_description.CopyFrom(
      proto_utils.proto_from_skill_manifest(
          manifest, file_descriptor_set, version=version
      )
  )

  return service_config
