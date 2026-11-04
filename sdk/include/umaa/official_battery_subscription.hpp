// SDK-owned BatteryStatus host subscription / business-Handler boundary.
//
// Narrow original EO ICD 6.0 §3.8.1-3.8.5 List event contract.
// Requires an upstream DDS receiver to authenticate the original native
// Report/Element TypeSupport, both instance Keys and effective Collection
// Element TOPIC ordered-access QoS. This class does NOT create DDS entities,
// verify Component-specific QoS, or authorize generic official Runtime codegen.
#pragma once

#include "umaa/official_battery_collection_router.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace umaa::official_battery {

using ReaderEpoch = std::uint64_t;

struct BatteryStatusHandler {
  virtual ~BatteryStatusHandler() = default;

  // A fully joined source-backed host snapshot, NOT a DDS GROUP-coherence
  // or overall UMAA conformity assertion. Copy any data retained by Handler.
  virtual void on_stable_list(const StableList& stable, ReaderEpoch epoch) = 0;

  // Previous stable snapshot is no longer current while an update/deletion
  // lacks its complete Report/Element checkpoint.
  virtual void on_list_invalidated(const Guid&, ReaderEpoch) {}

  // Invoked only from a genuine original DDS Report keyed Dispose callback.
  virtual void on_source_disposed(const Identifier&, const Guid&,
                                  ReaderEpoch) {}
};

class BatteryStatusSubscription final {
 public:
  using ElementOrdering = LargeListJoiner::ElementOrdering;
  enum class State { Idle, Active, Stopped, Faulted };

  explicit BatteryStatusSubscription(
      BatteryStatusHandler& handler, ElementOrdering ordering,
      std::size_t max_lists = 16, std::size_t max_elements = 4096,
      std::size_t max_pending_reports = 32)
      : handler_(handler),
        router_(ordering, max_lists, max_elements, max_pending_reports),
        max_lists_(max_lists) {}

  // Call only after the native reader's source closure, Key mapping and
  // effective element subscriber/publisher QoS have been verified.
  ReaderEpoch start() {
    if (state_ != State::Idle && state_ != State::Stopped)
      throw std::logic_error("Battery subscription requires stopped reader");
    if (epoch_ == std::numeric_limits<ReaderEpoch>::max())
      throw std::overflow_error("Battery reader epoch exhausted");
    clear();
    ++epoch_;
    state_ = State::Active;
    return epoch_;
  }

  bool on_report(const Report& report, ReaderEpoch epoch) {
    if (!accept(epoch)) return false;
    try {
      const Guid& id = report.cellsListMetadata.listID;
      const Guid& source = report.source.id;
      if (source == Guid{} || id == Guid{})
        throw std::invalid_argument("Battery source/list identity must be non-NIL");
      if (retired_sources_.count(source) || retired_lists_.count(id))
        throw std::runtime_error("retired Battery DDS identity cannot be replayed");
      const auto known_source = sources_.find(source);
      if (known_source != sources_.end() &&
          (known_source->second.first.id != source ||
           known_source->second.first.parentID != report.source.parentID ||
           known_source->second.second != id))
        throw std::runtime_error("Battery source reused a different List or Parent ID");
      const auto owner = list_owners_.find(id);
      if (owner != list_owners_.end() && owner->second != source)
        throw std::runtime_error("two Battery sources share the same List ID");
      if (known_source == sources_.end() && sources_.size() == max_lists_)
        throw std::runtime_error("Battery Source binding capacity exceeded");
      router_.on_report(report);
      sources_.emplace(source, std::make_pair(report.source, id));
      list_owners_.emplace(id, source);
      notify(id);
      return true;
    } catch (...) { fault(); throw; }
  }

  bool on_element(const ListElement& element, ReaderEpoch epoch) {
    if (!accept(epoch)) return false;
    try {
      if (element.listID == Guid{} || retired_lists_.count(element.listID))
        throw std::runtime_error("NIL or retired Battery List element");
      router_.on_element(element);
      notify(element.listID);
      return true;
    } catch (...) { fault(); throw; }
  }

  // A DDS NOT_ALIVE_DISPOSED Cell sample MUST be resolved to BOTH native
  // List and Element Keys by the vendor reader before calling this function.
  bool on_element_dispose(const Guid& list, const Guid& element,
                          ReaderEpoch epoch) {
    if (!accept(epoch)) return false;
    try {
      if (retired_lists_.count(list)) return false; // late Cell tombstone
      if (list == Guid{} || element == Guid{})
        throw std::invalid_argument("DDS Cell Dispose requires actual Keys");
      router_.on_dispose(list, element);
      notify(list);
      return true;
    } catch (...) { fault(); throw; }
  }

  // Only a DDS NOT_ALIVE_DISPOSED on the original keyed Report Topic may
  // trigger this method. It immediately invalidates and retires the list;
  // a late Cell Dispose must not allocate a ghost list afterwards.
  bool on_source_dispose(const Identifier& source, ReaderEpoch epoch) {
    if (!accept(epoch)) return false;
    try {
      const auto it = sources_.find(source.id);
      if (it == sources_.end() ||
          it->second.first.parentID != source.parentID)
        throw std::runtime_error("DDS Report Dispose has no matching Source Keys");
      const Guid list_id = it->second.second;
      if (retired_lists_.size() == 128 || retired_sources_.size() == 128)
        throw std::runtime_error("Battery tombstone budget exhausted; restart reader");
      const bool was_visible = delivered_.count(list_id) &&
                               !invalidated_.count(list_id);
      router_.retire_list(list_id);
      sources_.erase(it);
      list_owners_.erase(list_id);
      delivered_.erase(list_id);
      invalidated_.erase(list_id);
      retired_lists_.insert(list_id);
      retired_sources_.insert(source.id);
      if (was_visible) handler_.on_list_invalidated(list_id, epoch_);
      handler_.on_source_disposed(source, list_id, epoch_);
      return true;
    } catch (...) { fault(); throw; }
  }

  // This is the last **current** accepted snapshot, not historical cache.
  // Any incomplete update invalidates it until a full new checkpoint commits.
  const StableList* latest(const Guid& list_id) const noexcept {
    if (state_ != State::Active || invalidated_.count(list_id))
      return nullptr;
    return router_.latest_stable(list_id);
  }

  // Disconnect/recreation is an explicit action. Never silently reuse a
  // prior reader's caches or callback epoch when DDS discovery returns.
  void reset_after_disconnect() noexcept {
    clear();
    state_ = State::Stopped;
  }

  void stop() noexcept { reset_after_disconnect(); }
  State state() const noexcept { return state_; }
  ReaderEpoch epoch() const noexcept { return epoch_; }
  std::size_t active_lists() const noexcept { return router_.active_lists(); }

 private:
  bool accept(ReaderEpoch epoch) const noexcept {
    return state_ == State::Active && epoch == epoch_;
  }

  void notify(const Guid& list_id) {
    const auto* stable = router_.latest_stable(list_id);
    const auto found = delivered_.find(list_id);
    const auto generation = found == delivered_.end() ? 0 : found->second;
    if (stable && stable->generation > generation) {
      const StableList copy = *stable;  // survives even a Handler reentrant use
      delivered_.insert_or_assign(list_id, copy.generation);
      invalidated_.erase(list_id);
      handler_.on_stable_list(copy, epoch_);
    } else if (generation != 0 && invalidated_.insert(list_id).second) {
      handler_.on_list_invalidated(list_id, epoch_);
    }
  }

  void clear() noexcept {
    router_.reset_after_disconnect();
    sources_.clear();
    list_owners_.clear();
    delivered_.clear();
    invalidated_.clear();
    retired_lists_.clear();
    retired_sources_.clear();
  }

  void fault() noexcept {
    clear();
    state_ = State::Faulted;
  }

  BatteryStatusHandler& handler_;
  LargeListRouter router_;
  std::size_t max_lists_;
  ReaderEpoch epoch_{0};
  State state_{State::Idle};
  // No inferred Element Source ownership: only Report data supplies it.
  std::map<Guid, std::pair<Identifier, Guid>> sources_;
  std::map<Guid, Guid> list_owners_;
  std::map<Guid, std::uint64_t> delivered_;
  std::set<Guid> invalidated_;
  std::set<Guid> retired_lists_;
  std::set<Guid> retired_sources_;
};

} // namespace umaa::official_battery
