// SDK-side, vendor-neutral UMAA 6.0 EO BatteryStatus value types.
// This header does not import DDS vendor headers and is NOT a conformant
// ServiceProvider by itself. Values are encoded by reviewed vendor adapters.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace umaa::official_battery {

using Guid = std::array<std::uint8_t, 16>;

struct DateTime {
  std::int64_t seconds{};
  std::int32_t nanoseconds{};
};

struct Identifier {
  Guid id{};
  Guid parentID{};
};

enum class State : std::int32_t { FAULT = 0, OFF = 1, ON = 2 };

struct LargeListMetadata {
  Guid listID{};
  Guid updateElementID{};
  std::optional<DateTime> updateElementTimestamp{};
  Guid startingElementID{};
  std::int32_t size{};
};

struct Report {
  std::optional<double> chargeRemaining{};
  std::optional<double> current{};
  std::optional<double> energyUsageRate{};
  std::optional<double> hours{};
  State state{State::FAULT};
  std::optional<double> temp{};
  std::optional<double> voltage{};
  DateTime timeStamp{};
  Identifier source{};
  LargeListMetadata cellsListMetadata{};
};

struct Cell {
  double current{};
  double temperature{};
  double voltage{};
};

struct ListElement {
  Cell element{};
  Guid listID{};
  Guid elementID{};
  DateTime elementTimestamp{};
  std::optional<Guid> nextElementID{};
};

} // namespace umaa::official_battery
