// UMAA 6.0 original BatteryStatus IDL <-> SDK host values, Fast DDS-Gen edition.
//
// Real vendor-generated C++ TypeSupport owns CDR/wire encoding. This is a
// selected-service, prototype-QoS binding, NOT a production official Runtime,
// full Collection lifecycle, or a UMAA conformance certificate.
#pragma once
#include "umaa/official_battery_host.hpp"
#if __has_include("BatteryReportType.hpp") && __has_include("BatteryReportTypePubSubTypes.hpp")
  #include "BatteryReportType.hpp"
  #include "BatteryReportTypePubSubTypes.hpp"
#elif __has_include("BatteryReportType.h") && __has_include("BatteryReportTypePubSubTypes.h")
  #include "BatteryReportType.h"
  #include "BatteryReportTypePubSubTypes.h"
#else
  #error "Original Fast DDS-Gen Battery TypeSupport is required"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace umaa::fastdds::official_battery {
namespace host = ::umaa::official_battery;
using NativeReport = ::UMAA::EO::BatteryStatus::BatteryReportType;
using NativeElement = ::UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement;
using NativeTime = ::UMAA::Common::Measurement::DateTime;
using ReportTypeSupport = ::UMAA::EO::BatteryStatus::BatteryReportTypePubSubType;
using ElementTypeSupport = ::UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElementPubSubType;

template<typename T, typename = void> struct HasOptionalPresence : std::false_type {};
template<typename T>
struct HasOptionalPresence<T, std::void_t<
  decltype(std::declval<const T&>().has_value()),
  decltype(*std::declval<T&>())
>> : std::true_type {};

using ChargeField = std::remove_reference_t<decltype(std::declval<NativeReport&>().chargeRemaining())>;
using CheckpointField = std::remove_reference_t<
    decltype(std::declval<NativeReport&>().cellsListMetadata().updateElementTimestamp())>;
using NextElementField = std::remove_reference_t<decltype(std::declval<NativeElement&>().nextElementID())>;
static_assert(HasOptionalPresence<ChargeField>::value &&
              HasOptionalPresence<CheckpointField>::value &&
              HasOptionalPresence<NextElementField>::value,
              "Official Battery Fast DDS backend requires @optional-preserving "
              "native TypeSupport; older Fast DDS-Gen must fail closed");

inline void verify_time(const host::DateTime& value) {
  if (value.seconds == INT64_MIN || value.nanoseconds < 0 ||
      value.nanoseconds > 999999999)
    throw std::invalid_argument("Battery DateTime outside official IDL bounds");
}

inline void verify_scalar(double value, double min, double max, const char* field) {
  if (!std::isfinite(value) || value < min || value > max)
    throw std::invalid_argument(std::string("Battery scalar outside official IDL bound: ") + field);
}

inline void verify_report(const host::Report& value) {
  verify_time(value.timeStamp);
  if (value.cellsListMetadata.updateElementTimestamp)
    verify_time(*value.cellsListMetadata.updateElementTimestamp);
  if (value.cellsListMetadata.size < 0)
    throw std::invalid_argument("Battery metadata negative size");
  const int state = static_cast<int>(value.state);
  if (state < 0 || state > 2) throw std::invalid_argument("Battery state unknown");
  if (value.chargeRemaining) verify_scalar(*value.chargeRemaining, 0, 1000, "chargeRemaining");
  if (value.current) verify_scalar(*value.current, -100000, 100000, "current");
  if (value.energyUsageRate) verify_scalar(*value.energyUsageRate, 0, 100000000, "energyUsageRate");
  if (value.hours) verify_scalar(*value.hours, 0, 10505, "hours");
  if (value.temp) verify_scalar(*value.temp, -273, 1000, "temp");
  if (value.voltage) verify_scalar(*value.voltage, -100000, 100000, "voltage");
}

inline void verify_element(const host::ListElement& value) {
  verify_time(value.elementTimestamp);
  verify_scalar(value.element.current, -100000, 100000, "element.current");
  verify_scalar(value.element.temperature, -273, 1000, "element.temperature");
  verify_scalar(value.element.voltage, -100000, 100000, "element.voltage");
}

template<class NativeGuid>
inline void set_guid(NativeGuid& target, const host::Guid& src) {
  if (target.size() != src.size())
    throw std::logic_error("Fast DDS NumericGUID type no longer has 16 bytes");
  std::copy(src.begin(), src.end(), target.begin());
}

template<class NativeGuid>
inline host::Guid get_guid(const NativeGuid& src) {
  host::Guid result{};
  if (src.size() != result.size())
    throw std::logic_error("Fast DDS NumericGUID type no longer has 16 bytes");
  std::copy(src.begin(), src.end(), result.begin());
  return result;
}

inline void set_time(NativeTime& dest, const host::DateTime& source) {
  verify_time(source);
  dest.seconds(source.seconds);
  dest.nanoseconds(source.nanoseconds);
}

inline host::DateTime get_time(const NativeTime& source) {
  host::DateTime result{source.seconds(), source.nanoseconds()};
  verify_time(result);
  return result;
}

inline NativeReport encode_report(const host::Report& source) {
  verify_report(source);
  NativeReport result{};
  if (source.chargeRemaining) result.chargeRemaining() = *source.chargeRemaining;
  if (source.current) result.current() = *source.current;
  if (source.energyUsageRate) result.energyUsageRate() = *source.energyUsageRate;
  if (source.hours) result.hours() = *source.hours;
  if (source.temp) result.temp() = *source.temp;
  if (source.voltage) result.voltage() = *source.voltage;
  using EnumType = std::remove_cv_t<std::remove_reference_t<decltype(result.state())>>;
  result.state() = static_cast<EnumType>(static_cast<int>(source.state));
  set_time(result.timeStamp(), source.timeStamp);
  set_guid(result.source().id(), source.source.id);
  set_guid(result.source().parentID(), source.source.parentID);
  auto& meta = result.cellsListMetadata();
  set_guid(meta.listID(), source.cellsListMetadata.listID);
  set_guid(meta.updateElementID(), source.cellsListMetadata.updateElementID);
  set_guid(meta.startingElementID(), source.cellsListMetadata.startingElementID);
  meta.size(source.cellsListMetadata.size);
  if (source.cellsListMetadata.updateElementTimestamp) {
    meta.updateElementTimestamp() = NativeTime{};
    set_time(*meta.updateElementTimestamp(), *source.cellsListMetadata.updateElementTimestamp);
  }
  return result;
}

inline host::Report decode_report(const NativeReport& source) {
  host::Report result{};
  if (source.chargeRemaining()) result.chargeRemaining = *source.chargeRemaining();
  if (source.current()) result.current = *source.current();
  if (source.energyUsageRate()) result.energyUsageRate = *source.energyUsageRate();
  if (source.hours()) result.hours = *source.hours();
  if (source.temp()) result.temp = *source.temp();
  if (source.voltage()) result.voltage = *source.voltage();
  result.state = static_cast<host::State>(static_cast<int>(source.state()));
  result.timeStamp = get_time(source.timeStamp());
  result.source.id = get_guid(source.source().id());
  result.source.parentID = get_guid(source.source().parentID());
  const auto& meta = source.cellsListMetadata();
  auto& output = result.cellsListMetadata;
  output.listID = get_guid(meta.listID());
  output.updateElementID = get_guid(meta.updateElementID());
  output.startingElementID = get_guid(meta.startingElementID());
  output.size = meta.size();
  if (meta.updateElementTimestamp())
    output.updateElementTimestamp = get_time(*meta.updateElementTimestamp());
  verify_report(result);
  return result;
}

inline NativeElement encode_element(const host::ListElement& source) {
  verify_element(source);
  NativeElement result{};
  result.element().current(source.element.current);
  result.element().temperature(source.element.temperature);
  result.element().voltage(source.element.voltage);
  set_guid(result.listID(), source.listID);
  set_guid(result.elementID(), source.elementID);
  set_time(result.elementTimestamp(), source.elementTimestamp);
  if (source.nextElementID) result.nextElementID() = *source.nextElementID;
  return result;
}

inline host::ListElement decode_element(const NativeElement& source) {
  host::ListElement result{};
  result.element.current = source.element().current();
  result.element.temperature = source.element().temperature();
  result.element.voltage = source.element().voltage();
  result.listID = get_guid(source.listID());
  result.elementID = get_guid(source.elementID());
  result.elementTimestamp = get_time(source.elementTimestamp());
  if (source.nextElementID()) result.nextElementID = get_guid(*source.nextElementID());
  verify_element(result);
  return result;
}

} // namespace umaa::fastdds::official_battery
