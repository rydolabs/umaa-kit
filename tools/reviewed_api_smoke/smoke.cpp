// This executable never starts DDS or actuates devices. It verifies that
// audited UMAA 6.0 service Handler APIs compile against real Cyclone TypeSupport.
#include "umaa/generated/official_eo_bindings.hpp"
#include <type_traits>

namespace api = umaa::generated;
static_assert(std::is_class_v<api::AnchorControlApi::Command>);
static_assert(std::is_class_v<api::AnchorSpecsApi::Report>);
static_assert(std::is_class_v<api::AnchorStatusApi::Report>);
static_assert(std::is_class_v<api::BatterySpecsApi::Report>);
static_assert(std::is_class_v<api::BatteryStatusApi::Report>);
static_assert(std::is_class_v<api::GlobalDriftControlApi::ExecutionStatus>);
static_assert(std::is_class_v<api::ConditionalControlApi::ConditionalAddCommand>);
static_assert(std::is_class_v<api::MissionPlanMissionControlApi::MissionPlanMissionDeleteCommand>);
static_assert(std::is_class_v<api::WindStatusApi::Report>);
static_assert(std::is_class_v<api::GPSStatusApi::Report>);
static_assert(std::is_class_v<api::LogReportApi::Report>);
static_assert(std::is_class_v<api::EngineStatusApi::Report>);
static_assert(std::is_class_v<api::FuelTankSpecsApi::Report>);
static_assert(std::is_class_v<api::FuelTankStatusApi::Report>);
static_assert(std::is_class_v<api::GeneratorSpecsApi::Report>);
static_assert(std::is_class_v<api::GeneratorStatusApi::Report>);
static_assert(std::is_class_v<api::MastStatusApi::Report>);
static_assert(std::is_class_v<api::EngineControlApi::Command>);
static_assert(std::is_class_v<api::MastControlApi::Command>);
static_assert(std::is_class_v<api::UVPlatformSpecsApi::UVPlatformCapabilitiesReportType>);
static_assert(std::is_class_v<api::UVPlatformSpecsApi::UVPlatformSpecsReportType>);
static_assert(std::is_abstract_v<api::UVPlatformSpecsApi::Handler>);
static_assert(std::is_abstract_v<api::EngineControlApi::Handler>);
int main() { return 0; }
