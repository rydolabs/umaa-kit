// Business-only integration process for the scoped official AUVSI
// BatteryStatus SDK Cyclone DDS Reader. No native DDS calls in application.
// Actual original TypeSupport is generated in ephemeral CI and is owned
// by the vendor adapter, never by the user's BatteryStatusHandler.
#include "umaa/cyclone_official_battery_reader.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

namespace b = umaa::official_battery;
namespace cyclone = umaa::cyclone::official_battery;

static b::Guid guid(unsigned char value) {
  b::Guid out{};
  out[0] = value;
  return out;
}

class BusinessHandler final : public b::BatteryStatusHandler {
 public:
  bool stable_a{false};
  bool stable_b{false};
  std::set<b::Guid> source_disposed;
  std::set<b::Guid> invalidated;
  b::ReaderEpoch epoch{};

  void on_stable_list(const b::StableList& state,
                      b::ReaderEpoch callback_epoch) override {
    if (!callback_epoch || (epoch && callback_epoch != epoch))
      throw std::runtime_error("stale native DDS callback reached Handler");
    epoch = callback_epoch;
    if (state.nativeDdsCoherenceProved || state.generation != 1)
      throw std::runtime_error("native Reader claimed unsupported coherence or generation");
    const auto& report = state.report;
    if (report.source.id == guid(0x33)) {
      if (stable_a || report.cellsListMetadata.listID != guid(0x11) ||
          !report.current || *report.current != -3.25 ||
          state.ordered.size() != 3)
        throw std::runtime_error("incorrect source A Handler Report");
      const unsigned char ids[] = {0x21, 0x22, 0x23};
      const double volts[] = {3.75, 4.0, 4.25};
      for (std::size_t i = 0; i < 3; ++i)
        if (state.ordered[i].listID != guid(0x11) ||
            state.ordered[i].elementID != guid(ids[i]) ||
            state.ordered[i].element.voltage != volts[i])
          throw std::runtime_error("incorrect source A native Cell value");
      stable_a = true;
    } else if (report.source.id == guid(0x44)) {
      if (stable_b || report.cellsListMetadata.listID != guid(0x12) ||
          !report.current || *report.current != -2.5 ||
          state.ordered.size() != 1 ||
          state.ordered[0].listID != guid(0x12) ||
          state.ordered[0].elementID != guid(0x31) ||
          state.ordered[0].element.voltage != 5.0 ||
          state.ordered[0].element.temperature != 27.0 ||
          state.ordered[0].element.current != -0.5)
        throw std::runtime_error("incorrect source B native Cell/Report value");
      stable_b = true;
    } else {
      throw std::runtime_error("unknown Source in user business Handler");
    }
  }

  void on_list_invalidated(const b::Guid& list,
                           b::ReaderEpoch callback_epoch) override {
    if (callback_epoch != epoch)
      throw std::runtime_error("invalidation on wrong DDS Reader generation");
    invalidated.insert(list);
  }

  void on_source_disposed(const b::Identifier& source,
                          const b::Guid&, b::ReaderEpoch callback_epoch) override {
    if (callback_epoch != epoch ||
        !source_disposed.insert(source.id).second)
      throw std::runtime_error("unexpected/duplicate native Report Dispose");
  }
};

int main(int argc, char** argv) {
  const bool negative = argc == 2 &&
      std::string(argv[1]) == "--bad-checkpoint";
  if (argc != 1 && !negative) return 2;
  try {
    BusinessHandler handler;
    cyclone::OriginalBatteryStatusReader reader(handler, DDS_DOMAIN_DEFAULT,
                                                 2, 16);
    const auto epoch = reader.epoch();
    // A deterministic deadline, not an infinite wait or a synthetic on-wire
    // Status/Dispose. The sender uses genuine keyed DDS Disposes.
    for (int tick = 0; tick < 1200; ++tick) {
      const bool had_data = reader.poll_once();
      const auto& s = reader.stats();
      if (s.report_samples >= 2 && s.element_samples >= 4 &&
          s.report_disposes >= 2 && s.element_disposes >= 4)
        break;
      if (!had_data)
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    const auto stats = reader.stats();
    if (stats.report_samples != 2 || stats.element_samples != 4 ||
        stats.report_disposes != 2 || stats.element_disposes != 4)
      throw std::runtime_error("native SDK adapter missed keyed DDS data/Dispose event");
    if (!handler.stable_a || (negative ? handler.stable_b : !handler.stable_b))
      throw std::runtime_error("incomplete/corrupt Handler callback evidence");
    if (handler.source_disposed != std::set<b::Guid>{guid(0x33),guid(0x44)} ||
        reader.active_lists() != 0 ||
        reader.latest(guid(0x11)) || reader.latest(guid(0x12)))
      throw std::runtime_error("actual DDS Dispose left a stale SDK List");
    if (reader.epoch() != epoch || handler.epoch != epoch)
      throw std::runtime_error("native adapter used wrong Reader epoch");
    reader.restart();
    if (reader.epoch() == epoch || reader.active_lists() != 0)
      throw std::runtime_error("native SDK adapter failed explicit reader restart");
    reader.close();
    if (reader.active_lists() != 0)
      throw std::runtime_error("SDK adapter retained state after close");

    if (negative) {
      std::puts("SDK_BATTERY_NATIVE_READER_BAD_CHECKPOINT_ISOLATED");
      std::puts("BATTERY_TWO_LIST_BAD_CHECKPOINT_REJECTED");
      return 10; // explicit known-negative supervisor oracle
    }
    std::puts("SDK_BATTERY_NATIVE_READER_HANDLER_PASS");
    std::puts("BATTERY_TWO_LIST_NATIVE_ROUTER_PASS");
    return 0;
  } catch (const std::exception& exc) {
    std::fprintf(stderr, "official Battery native SDK Reader: %s\n",
                 exc.what());
    return 3;
  }
}
