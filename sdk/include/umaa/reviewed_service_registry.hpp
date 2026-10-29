// GENERATED from catalog/umaa6-six-icd-reviewed.json; do not edit.
// This is an ICD literal topic allow-list, NOT Component Definition/QoS proof.
#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace umaa::reviewed {
enum class Role { Report, Ack, Status, Command, ExecutionStatus };
struct Topic {
  std::string_view service, name, type;
  Role role;
  bool qos_verified;
};
inline constexpr std::array<Topic, 113> topics{{
  Topic{"UMAA::EO::AnchorControl", "UMAA::EO::AnchorControl::AnchorCommandAckReportType", "UMAA::EO::AnchorControl::AnchorCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::EO::AnchorControl", "UMAA::EO::AnchorControl::AnchorCommandStatusType", "UMAA::EO::AnchorControl::AnchorCommandStatusType", Role::Status, false},
  Topic{"UMAA::EO::AnchorControl", "UMAA::EO::AnchorControl::AnchorCommandType", "UMAA::EO::AnchorControl::AnchorCommandType", Role::Command, false},
  Topic{"UMAA::EO::AnchorSpecs", "UMAA::EO::AnchorSpecs::AnchorSpecsReportType", "UMAA::EO::AnchorSpecs::AnchorSpecsReportType", Role::Report, false},
  Topic{"UMAA::EO::AnchorStatus", "UMAA::EO::AnchorStatus::AnchorReportType", "UMAA::EO::AnchorStatus::AnchorReportType", Role::Report, false},
  Topic{"UMAA::EO::BatterySpecs", "UMAA::EO::BatterySpecs::BatterySpecsReportType", "UMAA::EO::BatterySpecs::BatterySpecsReportType", Role::Report, false},
  Topic{"UMAA::EO::BatteryStatus", "UMAA::EO::BatteryStatus::BatteryReportType", "UMAA::EO::BatteryStatus::BatteryReportType", Role::Report, false},
  Topic{"UMAA::EO::EngineControl", "UMAA::EO::EngineControl::EngineCommandStatusType", "UMAA::EO::EngineControl::EngineCommandStatusType", Role::Status, false},
  Topic{"UMAA::EO::EngineControl", "UMAA::EO::EngineControl::EngineCommandAckReportType", "UMAA::EO::EngineControl::EngineCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::EO::EngineControl", "UMAA::EO::EngineControl::EngineCommandType", "UMAA::EO::EngineControl::EngineCommandType", Role::Command, false},
  Topic{"UMAA::EO::EngineStatus", "UMAA::EO::EngineStatus::EngineReportType", "UMAA::EO::EngineStatus::EngineReportType", Role::Report, false},
  Topic{"UMAA::EO::FuelTankSpecs", "UMAA::EO::FuelTankSpecs::FuelTankSpecsReportType", "UMAA::EO::FuelTankSpecs::FuelTankSpecsReportType", Role::Report, false},
  Topic{"UMAA::EO::FuelTankStatus", "UMAA::EO::FuelTankStatus::FuelTankReportType", "UMAA::EO::FuelTankStatus::FuelTankReportType", Role::Report, false},
  Topic{"UMAA::EO::GeneratorSpecs", "UMAA::EO::GeneratorSpecs::GeneratorSpecsReportType", "UMAA::EO::GeneratorSpecs::GeneratorSpecsReportType", Role::Report, false},
  Topic{"UMAA::EO::GeneratorStatus", "UMAA::EO::GeneratorStatus::GeneratorReportType", "UMAA::EO::GeneratorStatus::GeneratorReportType", Role::Report, false},
  Topic{"UMAA::EO::MastControl", "UMAA::EO::MastControl::MastCommandAckReportType", "UMAA::EO::MastControl::MastCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::EO::MastControl", "UMAA::EO::MastControl::MastCommandStatusType", "UMAA::EO::MastControl::MastCommandStatusType", Role::Status, false},
  Topic{"UMAA::EO::MastControl", "UMAA::EO::MastControl::MastCommandType", "UMAA::EO::MastControl::MastCommandType", Role::Command, false},
  Topic{"UMAA::EO::MastStatus", "UMAA::EO::MastStatus::MastReportType", "UMAA::EO::MastStatus::MastReportType", Role::Report, false},
  Topic{"UMAA::EO::UVPlatformSpecs", "UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType", "UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType", Role::Report, false},
  Topic{"UMAA::EO::UVPlatformSpecs", "UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType", "UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType", Role::Report, false},
  Topic{"UMAA::MM::ActiveConstraintsControl", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandAckReportType", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::ActiveConstraintsControl", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandStatusType", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::ActiveConstraintsControl", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandType", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandType", Role::Command, false},
  Topic{"UMAA::MM::ConditionalControl", "UMAA::MM::ConditionalControl::ConditionalAddCommandAckReportType", "UMAA::MM::ConditionalControl::ConditionalAddCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::ConditionalControl", "UMAA::MM::ConditionalControl::ConditionalAddCommandStatusType", "UMAA::MM::ConditionalControl::ConditionalAddCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::ConditionalControl", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandAckReportType", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::ConditionalControl", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandStatusType", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::ConditionalControl", "UMAA::MM::ConditionalControl::ConditionalAddCommandType", "UMAA::MM::ConditionalControl::ConditionalAddCommandType", Role::Command, false},
  Topic{"UMAA::MM::ConditionalControl", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandType", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandType", Role::Command, false},
  Topic{"UMAA::MM::ConditionalReport", "UMAA::MM::ConditionalReport::ConditionalReportType", "UMAA::MM::ConditionalReport::ConditionalReportType", Role::Report, false},
  Topic{"UMAA::MM::MissionPlanConstraintControl", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandAckReportType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanConstraintControl", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandStatusType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanConstraintControl", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandAckReportType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanConstraintControl", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandStatusType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanConstraintControl", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanConstraintControl", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanExecutionControl", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandAckReportType", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanExecutionControl", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandStatusType", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanExecutionControl", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandType", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanExecutionStatus", "UMAA::MM::MissionPlanExecutionStatus::MissionPlanExecutionReportType", "UMAA::MM::MissionPlanExecutionStatus::MissionPlanExecutionReportType", Role::Report, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandAckReportType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandStatusType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandAckReportType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandStatusType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandAckReportType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandStatusType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanMissionControl", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanObjectiveControl", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandAckReportType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanObjectiveControl", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandStatusType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanObjectiveControl", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandAckReportType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanObjectiveControl", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandStatusType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanObjectiveControl", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanObjectiveControl", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanReport", "UMAA::MM::MissionPlanReport::MissionPlanReportType", "UMAA::MM::MissionPlanReport::MissionPlanReportType", Role::Report, false},
  Topic{"UMAA::MM::MissionPlanTaskControl", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandAckReportType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanTaskControl", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandStatusType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanTaskControl", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandAckReportType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::MissionPlanTaskControl", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandStatusType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::MissionPlanTaskControl", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandType", Role::Command, false},
  Topic{"UMAA::MM::MissionPlanTaskControl", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandType", Role::Command, false},
  Topic{"UMAA::MM::ObjectiveExecutionControl", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandAckReportType", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::ObjectiveExecutionControl", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandStatusType", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::ObjectiveExecutionControl", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandType", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandType", Role::Command, false},
  Topic{"UMAA::MM::ObjectiveExecutionStatus", "UMAA::MM::ObjectiveExecutionStatus::ObjectiveExecutionReportType", "UMAA::MM::ObjectiveExecutionStatus::ObjectiveExecutionReportType", Role::Report, false},
  Topic{"UMAA::MM::TaskPlanExecutionControl", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandAckReportType", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MM::TaskPlanExecutionControl", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandStatusType", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandStatusType", Role::Status, false},
  Topic{"UMAA::MM::TaskPlanExecutionControl", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandType", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandType", Role::Command, false},
  Topic{"UMAA::MO::ContactManeuverInfluenceStatus", "UMAA::MO::ContactManeuverInfluenceStatus::ContactManeuverInfluenceReportType", "UMAA::MO::ContactManeuverInfluenceStatus::ContactManeuverInfluenceReportType", Role::Report, false},
  Topic{"UMAA::MO::CoordinationSituationalSignalStatus", "UMAA::MO::CoordinationSituationalSignalStatus::CoordinationSituationalSignalReportType", "UMAA::MO::CoordinationSituationalSignalStatus::CoordinationSituationalSignalReportType", Role::Report, false},
  Topic{"UMAA::MO::GlobalDriftControl", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandAckReportType", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MO::GlobalDriftControl", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandStatusType", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandStatusType", Role::Status, false},
  Topic{"UMAA::MO::GlobalDriftControl", "UMAA::MO::GlobalDriftControl::GlobalDriftExecutionStatusReportType", "UMAA::MO::GlobalDriftControl::GlobalDriftExecutionStatusReportType", Role::ExecutionStatus, false},
  Topic{"UMAA::MO::GlobalDriftControl", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandType", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandType", Role::Command, false},
  Topic{"UMAA::MO::GlobalHoverControl", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandAckReportType", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MO::GlobalHoverControl", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandStatusType", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandStatusType", Role::Status, false},
  Topic{"UMAA::MO::GlobalHoverControl", "UMAA::MO::GlobalHoverControl::GlobalHoverExecutionStatusReportType", "UMAA::MO::GlobalHoverControl::GlobalHoverExecutionStatusReportType", Role::ExecutionStatus, false},
  Topic{"UMAA::MO::GlobalHoverControl", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandType", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandType", Role::Command, false},
  Topic{"UMAA::MO::GlobalVectorControl", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandAckReportType", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MO::GlobalVectorControl", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandStatusType", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandStatusType", Role::Status, false},
  Topic{"UMAA::MO::GlobalVectorControl", "UMAA::MO::GlobalVectorControl::GlobalVectorExecutionStatusReportType", "UMAA::MO::GlobalVectorControl::GlobalVectorExecutionStatusReportType", Role::ExecutionStatus, false},
  Topic{"UMAA::MO::GlobalVectorControl", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandType", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandType", Role::Command, false},
  Topic{"UMAA::MO::GlobalWaypointControl", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandAckReportType", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MO::GlobalWaypointControl", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandStatusType", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandStatusType", Role::Status, false},
  Topic{"UMAA::MO::GlobalWaypointControl", "UMAA::MO::GlobalWaypointControl::GlobalWaypointExecutionStatusReportType", "UMAA::MO::GlobalWaypointControl::GlobalWaypointExecutionStatusReportType", Role::ExecutionStatus, false},
  Topic{"UMAA::MO::GlobalWaypointControl", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandType", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandType", Role::Command, false},
  Topic{"UMAA::MO::PrimitiveDriverControl", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandAckReportType", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::MO::PrimitiveDriverControl", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandStatusType", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandStatusType", Role::Status, false},
  Topic{"UMAA::MO::PrimitiveDriverControl", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverExecutionStatusReportType", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverExecutionStatusReportType", Role::ExecutionStatus, false},
  Topic{"UMAA::MO::PrimitiveDriverControl", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandType", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandType", Role::Command, false},
  Topic{"UMAA::SA::ContactCOLREGSClassificationStatus", "UMAA::SA::ContactCOLREGSClassificationStatus::ContactCOLREGSClassificationReportType", "UMAA::SA::ContactCOLREGSClassificationStatus::ContactCOLREGSClassificationReportType", Role::Report, false},
  Topic{"UMAA::SA::ContactReport", "UMAA::SA::ContactReport::ContactReportType", "UMAA::SA::ContactReport::ContactReportType", Role::Report, false},
  Topic{"UMAA::SA::ContactVisualClassificationStatus", "UMAA::SA::ContactVisualClassificationStatus::ContactVisualClassificationReportType", "UMAA::SA::ContactVisualClassificationStatus::ContactVisualClassificationReportType", Role::Report, false},
  Topic{"UMAA::SA::DateTimeStatus", "UMAA::SA::DateTimeStatus::DateTimeReportType", "UMAA::SA::DateTimeStatus::DateTimeReportType", Role::Report, false},
  Topic{"UMAA::SA::ECEFPoseStatus", "UMAA::SA::ECEFPoseStatus::ECEFPoseReportType", "UMAA::SA::ECEFPoseStatus::ECEFPoseReportType", Role::Report, false},
  Topic{"UMAA::SA::GlobalPoseStatus", "UMAA::SA::GlobalPoseStatus::GlobalPoseReportType", "UMAA::SA::GlobalPoseStatus::GlobalPoseReportType", Role::Report, false},
  Topic{"UMAA::SA::MagneticVariationSpecs", "UMAA::SA::MagneticVariationSpecs::MagneticVariationSpecsReportType", "UMAA::SA::MagneticVariationSpecs::MagneticVariationSpecsReportType", Role::Report, false},
  Topic{"UMAA::SA::MagneticVariationStatus", "UMAA::SA::MagneticVariationStatus::MagneticVariationReportType", "UMAA::SA::MagneticVariationStatus::MagneticVariationReportType", Role::Report, false},
  Topic{"UMAA::SA::SeaStateReport", "UMAA::SA::SeaStateReport::SeaStateReportType", "UMAA::SA::SeaStateReport::SeaStateReportType", Role::Report, false},
  Topic{"UMAA::SA::SpeedStatus", "UMAA::SA::SpeedStatus::SpeedReportType", "UMAA::SA::SpeedStatus::SpeedReportType", Role::Report, false},
  Topic{"UMAA::SA::TranslationalShipMotionStatus", "UMAA::SA::TranslationalShipMotionStatus::TranslationalShipMotionReportType", "UMAA::SA::TranslationalShipMotionStatus::TranslationalShipMotionReportType", Role::Report, false},
  Topic{"UMAA::SA::VelocityStatus", "UMAA::SA::VelocityStatus::VelocityReportType", "UMAA::SA::VelocityStatus::VelocityReportType", Role::Report, false},
  Topic{"UMAA::SA::WaterCurrentStatus", "UMAA::SA::WaterCurrentStatus::WaterCurrentReportType", "UMAA::SA::WaterCurrentStatus::WaterCurrentReportType", Role::Report, false},
  Topic{"UMAA::SA::WindStatus", "UMAA::SA::WindStatus::WindReportType", "UMAA::SA::WindStatus::WindReportType", Role::Report, false},
  Topic{"UMAA::SEM::GPSStatus", "UMAA::SEM::GPSStatus::GPSReportType", "UMAA::SEM::GPSStatus::GPSReportType", Role::Report, false},
  Topic{"UMAA::SEM::InertialSensorControl", "UMAA::SEM::InertialSensorControl::InertialSensorCommandAckReportType", "UMAA::SEM::InertialSensorControl::InertialSensorCommandAckReportType", Role::Ack, false},
  Topic{"UMAA::SEM::InertialSensorControl", "UMAA::SEM::InertialSensorControl::InertialSensorCommandStatusType", "UMAA::SEM::InertialSensorControl::InertialSensorCommandStatusType", Role::Status, false},
  Topic{"UMAA::SEM::InertialSensorControl", "UMAA::SEM::InertialSensorControl::InertialSensorCommandType", "UMAA::SEM::InertialSensorControl::InertialSensorCommandType", Role::Command, false},
  Topic{"UMAA::SEM::InertialSensorStatus", "UMAA::SEM::InertialSensorStatus::InertialSensorReportType", "UMAA::SEM::InertialSensorStatus::InertialSensorReportType", Role::Report, false},
  Topic{"UMAA::SO::HealthReport", "UMAA::SO::HealthReport::HealthReportType", "UMAA::SO::HealthReport::HealthReportType", Role::Report, false},
  Topic{"UMAA::SO::LogReport", "UMAA::SO::LogReport::LogReportType", "UMAA::SO::LogReport::LogReportType", Role::Report, false},
}};
inline constexpr std::size_t service_count = 53;

struct CommandFamily {
  std::string_view service, family, command_topic, ack_topic, status_topic;
  bool qos_verified;
};
inline constexpr std::array<CommandFamily, 24> command_families{{
  CommandFamily{"UMAA::EO::AnchorControl", "AnchorControl", "UMAA::EO::AnchorControl::AnchorCommandType", "UMAA::EO::AnchorControl::AnchorCommandAckReportType", "UMAA::EO::AnchorControl::AnchorCommandStatusType", false},
  CommandFamily{"UMAA::EO::EngineControl", "EngineControl", "UMAA::EO::EngineControl::EngineCommandType", "UMAA::EO::EngineControl::EngineCommandAckReportType", "UMAA::EO::EngineControl::EngineCommandStatusType", false},
  CommandFamily{"UMAA::EO::MastControl", "MastControl", "UMAA::EO::MastControl::MastCommandType", "UMAA::EO::MastControl::MastCommandAckReportType", "UMAA::EO::MastControl::MastCommandStatusType", false},
  CommandFamily{"UMAA::MM::ActiveConstraintsControl", "ActiveConstraints", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandType", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandAckReportType", "UMAA::MM::ActiveConstraintsControl::ActiveConstraintsCommandStatusType", false},
  CommandFamily{"UMAA::MM::ConditionalControl", "ConditionalAdd", "UMAA::MM::ConditionalControl::ConditionalAddCommandType", "UMAA::MM::ConditionalControl::ConditionalAddCommandAckReportType", "UMAA::MM::ConditionalControl::ConditionalAddCommandStatusType", false},
  CommandFamily{"UMAA::MM::ConditionalControl", "ConditionalDelete", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandType", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandAckReportType", "UMAA::MM::ConditionalControl::ConditionalDeleteCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanConstraintControl", "MissionPlanConstraintAdd", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandAckReportType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintAddCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanConstraintControl", "MissionPlanConstraintDelete", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandAckReportType", "UMAA::MM::MissionPlanConstraintControl::MissionPlanConstraintDeleteCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanExecutionControl", "MissionPlanExecution", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandType", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandAckReportType", "UMAA::MM::MissionPlanExecutionControl::MissionPlanExecutionCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanMissionControl", "MissionPlanMissionAdd", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandAckReportType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionAddCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanMissionControl", "MissionPlanMissionClear", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandAckReportType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionClearCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanMissionControl", "MissionPlanMissionDelete", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandAckReportType", "UMAA::MM::MissionPlanMissionControl::MissionPlanMissionDeleteCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanObjectiveControl", "MissionPlanObjectiveAdd", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandAckReportType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveAddCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanObjectiveControl", "MissionPlanObjectiveDelete", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandAckReportType", "UMAA::MM::MissionPlanObjectiveControl::MissionPlanObjectiveDeleteCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanTaskControl", "MissionPlanTaskAdd", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandAckReportType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskAddCommandStatusType", false},
  CommandFamily{"UMAA::MM::MissionPlanTaskControl", "MissionPlanTaskDelete", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandAckReportType", "UMAA::MM::MissionPlanTaskControl::MissionPlanTaskDeleteCommandStatusType", false},
  CommandFamily{"UMAA::MM::ObjectiveExecutionControl", "ObjectiveExecution", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandType", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandAckReportType", "UMAA::MM::ObjectiveExecutionControl::ObjectiveExecutionCommandStatusType", false},
  CommandFamily{"UMAA::MM::TaskPlanExecutionControl", "TaskPlanExecution", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandType", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandAckReportType", "UMAA::MM::TaskPlanExecutionControl::TaskPlanExecutionCommandStatusType", false},
  CommandFamily{"UMAA::MO::GlobalDriftControl", "GlobalDrift", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandType", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandAckReportType", "UMAA::MO::GlobalDriftControl::GlobalDriftCommandStatusType", false},
  CommandFamily{"UMAA::MO::GlobalHoverControl", "GlobalHover", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandType", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandAckReportType", "UMAA::MO::GlobalHoverControl::GlobalHoverCommandStatusType", false},
  CommandFamily{"UMAA::MO::GlobalVectorControl", "GlobalVector", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandType", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandAckReportType", "UMAA::MO::GlobalVectorControl::GlobalVectorCommandStatusType", false},
  CommandFamily{"UMAA::MO::GlobalWaypointControl", "GlobalWaypoint", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandType", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandAckReportType", "UMAA::MO::GlobalWaypointControl::GlobalWaypointCommandStatusType", false},
  CommandFamily{"UMAA::MO::PrimitiveDriverControl", "PrimitiveDriver", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandType", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandAckReportType", "UMAA::MO::PrimitiveDriverControl::PrimitiveDriverCommandStatusType", false},
  CommandFamily{"UMAA::SEM::InertialSensorControl", "InertialSensor", "UMAA::SEM::InertialSensorControl::InertialSensorCommandType", "UMAA::SEM::InertialSensorControl::InertialSensorCommandAckReportType", "UMAA::SEM::InertialSensorControl::InertialSensorCommandStatusType", false},
}};
// Only catalog-reviewed identities: never a substitute for a DDS Topic/Key
// binding or Component Definition QoS evidence.
inline const CommandFamily& require_command_family(
    std::string_view service, std::string_view command_topic,
    bool require_normative_qos = true) {
  for (const auto& f : command_families) {
    if (f.service == service && f.command_topic == command_topic) {
      if (require_normative_qos && !f.qos_verified)
        throw std::logic_error("UMAA Command family QoS not yet reviewed");
      return f;
    }
  }
  throw std::invalid_argument("Command family missing from reviewed ICD catalog");
}

inline const Topic& require_topic(std::string_view service,
     std::string_view topic, std::string_view type, Role role,
     bool require_normative_qos = true) {
  for (const auto& candidate : topics) {
    if (candidate.service == service && candidate.name == topic) {
      if (candidate.type != type || candidate.role != role)
        throw std::invalid_argument("reviewed Topic type/role mismatch");
      if (require_normative_qos && !candidate.qos_verified)
        throw std::logic_error("UMAA Component Definition QoS not yet reviewed");
      return candidate;
    }
  }
  throw std::invalid_argument("Topic missing from reviewed ICD catalog");
}
} // namespace umaa::reviewed
