#include "umaa/reviewed_service_registry.hpp"
#include <cassert>
#include <exception>
#include <string_view>

using umaa::reviewed::require_topic;
using umaa::reviewed::Role;

template<class F> void rejects(F&& f) {
  bool caught = false;
  try { f(); } catch (const std::exception&) { caught = true; }
  assert(caught);
}
int main() {
  using namespace std::literals;
  static_assert(umaa::reviewed::topics.size() == 113);
  static_assert(umaa::reviewed::service_count == 53);
  auto& b = require_topic("UMAA::EO::BatteryStatus", "UMAA::EO::BatteryStatus::BatteryReportType",
                          "UMAA::EO::BatteryStatus::BatteryReportType", Role::Report, false);
  assert(!b.qos_verified);
  auto& c = require_topic("UMAA::EO::EngineControl", "UMAA::EO::EngineControl::EngineCommandType",
                          "UMAA::EO::EngineControl::EngineCommandType", Role::Command, false);
  assert(c.role == Role::Command);
  auto& s = require_topic("UMAA::SO::LogReport", "UMAA::SO::LogReport::LogReportType",
                          "UMAA::SO::LogReport::LogReportType", Role::Report, false);
  assert(s.type == "UMAA::SO::LogReport::LogReportType"sv);
  rejects([&]{ require_topic(b.service,b.name,b.type,Role::Command,false); });
  rejects([&]{ require_topic(b.service,b.name,"SomeOther::Type",Role::Report,false); });
  rejects([&]{ require_topic(b.service,b.name,b.type,Role::Report,true); });
  rejects([&]{ require_topic("UMAA::Unknown","UMAA::Unknown::Report","UMAA::Unknown::Report",Role::Report,false); });
  static_assert(umaa::reviewed::command_families.size() == 24);
  const auto& family = umaa::reviewed::require_command_family(
      "UMAA::EO::EngineControl",
      "UMAA::EO::EngineControl::EngineCommandType", false);
  assert(family.ack_topic ==
         "UMAA::EO::EngineControl::EngineCommandAckReportType"sv);
  const auto& conditional = umaa::reviewed::require_command_family(
      "UMAA::MM::ConditionalControl",
      "UMAA::MM::ConditionalControl::ConditionalAddCommandType", false);
  assert(conditional.status_topic ==
         "UMAA::MM::ConditionalControl::ConditionalAddCommandStatusType"sv);
  rejects([&]{(void)umaa::reviewed::require_command_family(
      family.service,family.command_topic,true);});
  rejects([&]{(void)umaa::reviewed::require_command_family(
      family.service,"UMAA::EO::EngineControl::InventedCommandType",false);});
  for (const auto& f:umaa::reviewed::command_families) {
    assert(!f.qos_verified);
    assert(!f.command_topic.empty() && !f.ack_topic.empty() && !f.status_topic.empty());
    (void)require_topic(f.service,f.command_topic,f.command_topic,Role::Command,false);
    (void)require_topic(f.service,f.ack_topic,f.ack_topic,Role::Ack,false);
    (void)require_topic(f.service,f.status_topic,f.status_topic,Role::Status,false);
  }
  for (const auto& t:umaa::reviewed::topics) {
    assert(!t.service.empty() && !t.name.empty() && !t.type.empty());
    assert(!t.qos_verified); // never automatically mark per-service Component QoS as approved
  }
}
