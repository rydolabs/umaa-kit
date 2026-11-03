// Actual Cyclone DDS generated C TypeSupport <-> SDK BatteryStatus values.
//
// INTEGRATION PROOF ONLY: the native compiler owns wire encoding. These
// conversion functions do not implement CDR, select normative service QoS,
// reconcile two-topic List transactions, or declare UMAA conformance.
#pragma once

#include "umaa/official_battery_host.hpp"
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace umaa::cyclone::official_battery {
namespace host = ::umaa::official_battery;
using NativeReport = UMAA_EO_BatteryStatus_BatteryReportType;
using NativeElement = UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement;
using NativeTime = UMAA_Common_Measurement_DateTime;

inline void verify_time(const host::DateTime& t) {
  if (t.seconds == INT64_MIN || t.nanoseconds < 0 ||
      t.nanoseconds > 999999999)
    throw std::invalid_argument("official Battery DateTime outside IDL bounds");
}

inline void verify_number(double v, double minimum, double maximum,
                          const char* name) {
  if (!std::isfinite(v) || v < minimum || v > maximum)
    throw std::invalid_argument(std::string("official Battery scalar bound: ") + name);
}

inline void verify_report(const host::Report& value) {
  verify_time(value.timeStamp);
  if (value.cellsListMetadata.updateElementTimestamp)
    verify_time(*value.cellsListMetadata.updateElementTimestamp);
  if (value.cellsListMetadata.size < 0)
    throw std::invalid_argument("official Battery LargeList size must be nonnegative");
  const int state = static_cast<int>(value.state);
  if (state < 0 || state > 2)
    throw std::invalid_argument("official Battery state unknown");
  if (value.chargeRemaining)
    verify_number(*value.chargeRemaining, 0, 1000, "chargeRemaining");
  if (value.current)
    verify_number(*value.current, -100000, 100000, "current");
  if (value.energyUsageRate)
    verify_number(*value.energyUsageRate, 0, 100000000, "energyUsageRate");
  if (value.hours)
    verify_number(*value.hours, 0, 10505, "hours");
  if (value.temp)
    verify_number(*value.temp, -273, 1000, "temp");
  if (value.voltage)
    verify_number(*value.voltage, -100000, 100000, "voltage");
}

inline void verify_cell(const host::ListElement& e) {
  verify_time(e.elementTimestamp);
  verify_number(e.element.current, -100000, 100000, "cell.current");
  verify_number(e.element.temperature, -273, 1000, "cell.temperature");
  verify_number(e.element.voltage, -100000, 100000, "cell.voltage");
}

template<class NativeGuid>
inline void set_guid(NativeGuid& target, const host::Guid& value) {
  static_assert(sizeof(NativeGuid) == 16, "NumericGUID must be exactly 16 octets");
  std::memcpy(&target, value.data(), 16);
}

template<class NativeGuid>
inline host::Guid get_guid(const NativeGuid& source) {
  static_assert(sizeof(NativeGuid) == 16, "NumericGUID must be exactly 16 octets");
  host::Guid result{};
  std::memcpy(result.data(), &source, 16);
  return result;
}

inline void set_time(NativeTime& dest, const host::DateTime& src) {
  verify_time(src);
  dest.seconds = src.seconds;
  dest.nanoseconds = src.nanoseconds;
}

inline host::DateTime get_time(const NativeTime& src) {
  host::DateTime dest{src.seconds, src.nanoseconds};
  verify_time(dest);
  return dest;
}

class ReportWire {
 public:
  explicit ReportWire(const host::Report& src) {
    verify_report(src);
    auto set_optional = [](const std::optional<double>& value,
                           double& storage, double*& ptr) {
      if (value) {
        storage = *value;
        ptr = &storage;
      }
    };
    set_optional(src.chargeRemaining, charge_, data_.chargeRemaining);
    set_optional(src.current, current_, data_.current);
    set_optional(src.energyUsageRate, rate_, data_.energyUsageRate);
    set_optional(src.hours, hours_, data_.hours);
    set_optional(src.temp, temp_, data_.temp);
    set_optional(src.voltage, voltage_, data_.voltage);
    data_.state = static_cast<decltype(data_.state)>(static_cast<int>(src.state));
    set_time(data_.timeStamp, src.timeStamp);
    set_guid(data_.source.id, src.source.id);
    set_guid(data_.source.parentID, src.source.parentID);
    set_guid(data_.cellsListMetadata.listID, src.cellsListMetadata.listID);
    set_guid(data_.cellsListMetadata.updateElementID, src.cellsListMetadata.updateElementID);
    set_guid(data_.cellsListMetadata.startingElementID, src.cellsListMetadata.startingElementID);
    data_.cellsListMetadata.size = src.cellsListMetadata.size;
    if (src.cellsListMetadata.updateElementTimestamp) {
      set_time(checkpoint_, *src.cellsListMetadata.updateElementTimestamp);
      data_.cellsListMetadata.updateElementTimestamp = &checkpoint_;
    }
  }
  ReportWire(const ReportWire&) = delete;
  ReportWire& operator=(const ReportWire&) = delete;
  ReportWire(ReportWire&&) = delete;
  ReportWire& operator=(ReportWire&&) = delete;
  const NativeReport& get() const noexcept { return data_; }
 private:
  NativeReport data_{};
  double charge_{}, current_{}, rate_{}, hours_{}, temp_{}, voltage_{};
  NativeTime checkpoint_{};
};

inline host::Report decode_report(const NativeReport& data) {
  host::Report result{};
  if (data.chargeRemaining) result.chargeRemaining = *data.chargeRemaining;
  if (data.current) result.current = *data.current;
  if (data.energyUsageRate) result.energyUsageRate = *data.energyUsageRate;
  if (data.hours) result.hours = *data.hours;
  if (data.temp) result.temp = *data.temp;
  if (data.voltage) result.voltage = *data.voltage;
  result.state = static_cast<host::State>(data.state);
  result.timeStamp = get_time(data.timeStamp);
  result.source.id = get_guid(data.source.id);
  result.source.parentID = get_guid(data.source.parentID);
  auto& dst = result.cellsListMetadata;
  const auto& from = data.cellsListMetadata;
  dst.listID = get_guid(from.listID);
  dst.updateElementID = get_guid(from.updateElementID);
  dst.startingElementID = get_guid(from.startingElementID);
  dst.size = from.size;
  if (from.updateElementTimestamp)
    dst.updateElementTimestamp = get_time(*from.updateElementTimestamp);
  verify_report(result);
  return result;
}

class ElementWire {
 public:
  // Cyclone's @optional typedef-to-array representation must be a pointer
  // to exactly one NumericGUID (16 bytes), not an unsized sequence.
  using NativeOptionalGuid = std::remove_pointer_t<decltype(NativeElement{}.nextElementID)>;
  static_assert(sizeof(NativeOptionalGuid) == 16,
                "official optional NumericGUID ABI changed: re-review compiler mapping");

  explicit ElementWire(const host::ListElement& src) {
    verify_cell(src);
    data_.element.current = src.element.current;
    data_.element.temperature = src.element.temperature;
    data_.element.voltage = src.element.voltage;
    set_guid(data_.listID, src.listID);
    set_guid(data_.elementID, src.elementID);
    set_time(data_.elementTimestamp, src.elementTimestamp);
    if (src.nextElementID) {
      set_guid(next_, *src.nextElementID);
      data_.nextElementID = &next_;
    }
  }
  ElementWire(const ElementWire&) = delete;
  ElementWire& operator=(const ElementWire&) = delete;
  ElementWire(ElementWire&&) = delete;
  ElementWire& operator=(ElementWire&&) = delete;
  const NativeElement& get() const noexcept { return data_; }
 private:
  NativeElement data_{};
  NativeOptionalGuid next_{};
};

inline host::ListElement decode_element(const NativeElement& data) {
  host::ListElement result{};
  result.element.current = data.element.current;
  result.element.temperature = data.element.temperature;
  result.element.voltage = data.element.voltage;
  result.listID = get_guid(data.listID);
  result.elementID = get_guid(data.elementID);
  result.elementTimestamp = get_time(data.elementTimestamp);
  if (data.nextElementID)
    result.nextElementID = get_guid(*data.nextElementID);
  verify_cell(result);
  return result;
}

} // namespace umaa::cyclone::official_battery
