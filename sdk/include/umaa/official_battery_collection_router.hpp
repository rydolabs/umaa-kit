// Bounded host-side routing of independently keyed UMAA Battery Large Lists.
// Source: original EO ICD 6.0 sections 3.8.1--3.8.5; no original IDL bytes.
// This is an event router, NOT evidence of DDS QoS, cross-topic atomicity,
// Source<->List ownership, complete reconnect replay or official conformance.
#pragma once

#include "umaa/official_battery_collection.hpp"

#include <cstddef>
#include <map>
#include <stdexcept>
#include <utility>

namespace umaa::official_battery {

// The original Collection Element has listID/elementID keys but no Source
// Identifier. Consequently listID is the only safe *observed* dispatch key.
// If two sources reuse a listID the parent LargeListJoiner rejects the second
// Report; the caller must never infer a Source identity from an element.
// This router intentionally does not merge snapshots across lists.
class LargeListRouter final {
 public:
  using ElementOrdering = LargeListJoiner::ElementOrdering;

  explicit LargeListRouter(ElementOrdering ordering,
                           std::size_t max_lists = 16,
                           std::size_t max_elements_per_list = 4096,
                           std::size_t max_pending_reports_per_list = 32)
      : ordering_(ordering),
        max_lists_(max_lists),
        max_elements_(max_elements_per_list),
        max_pending_reports_(max_pending_reports_per_list) {
    // Limit aggregate buffering in addition to the per-list joiner limits.
    if (max_lists_ == 0 || max_lists_ > 128 ||
        max_elements_ == 0 || max_elements_ > 100000 ||
        max_pending_reports_ == 0 || max_pending_reports_ > 512 ||
        max_lists_ > 262144 / max_elements_)
      throw std::invalid_argument("invalid Battery multi-list memory limits");
  }

  void on_report(const Report& report) {
    dispatch(report.cellsListMetadata.listID,
             [&](LargeListJoiner& list) { list.on_report(report); });
  }

  void on_element(const ListElement& element) {
    dispatch(element.listID,
             [&](LargeListJoiner& list) { list.on_element(element); });
  }

  // The caller MUST independently extract the two original keys from a
  // genuine DDS NOT_ALIVE_DISPOSED event. Never infer a Dispose from payload.
  void on_dispose(const Guid& list_id, const Guid& element_id) {
    dispatch(list_id, [&](LargeListJoiner& list) {
      list.on_dispose(list_id, element_id);
    });
  }

  // Pointer is valid until this list is mutated, retired or reset.
  // nativeDdsCoherenceProved remains false, exactly as for LargeListJoiner.
  const StableList* latest_stable(const Guid& id) const noexcept {
    const auto it = lists_.find(id);
    if (it == lists_.end() || !it->second.latest_stable())
      return nullptr;
    return &*it->second.latest_stable();
  }

  std::size_t active_lists() const noexcept { return lists_.size(); }

  // Explicit lifecycle decision: caller must know that the list has ended.
  // Retiring a live list discards its buffered evidence; never auto-evict.
  bool retire_list(const Guid& id) noexcept {
    return lists_.erase(id) != 0;
  }

  // Any DDS disconnect makes ALL prior snapshots/buffers untrustworthy.
  // Reconstruct each list from a complete new stream after reconnection.
  void reset_after_disconnect() noexcept { lists_.clear(); }

 private:
  template<class Function>
  void dispatch(const Guid& id, Function&& action) {
    auto found = lists_.find(id);
    if (found != lists_.end()) {
      action(found->second);
      return;
    }
    if (lists_.size() >= max_lists_)
      throw std::runtime_error("Battery multi-list router capacity exceeded");
    // An invalid first event must not consume a capacity slot.
    LargeListJoiner candidate(ordering_, max_elements_, max_pending_reports_);
    action(candidate);
    lists_.emplace(id, std::move(candidate));
  }

  ElementOrdering ordering_;
  std::size_t max_lists_;
  std::size_t max_elements_;
  std::size_t max_pending_reports_;
  std::map<Guid, LargeListJoiner> lists_;
};

} // namespace umaa::official_battery
