// Development-only UMAA 6.0 generalization/specialization synchronization core.
// ICD EO 6.0 section 3.9: distinct DDS topics joined by specializationID
// and specializationTimestamp. The SDK backend must supply reviewed topic,
// TypeSupport, instance key, QoS, and Dispose lifecycle separately.
// This header never creates DDS entities and claims no official wire support.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace umaa {

template<class General, class Specialization>
class GeneralizationSynchronizer final {
public:
  struct Joined {
    std::string specialization_id;
    std::uint64_t specialization_timestamp;
    General general;
    Specialization specialization;
  };

  explicit GeneralizationSynchronizer(std::string specialization_topic,
                                      std::size_t max_instances = 1024)
      : specialization_topic_(std::move(specialization_topic)),
        max_instances_(max_instances) {
    if (specialization_topic_.empty() || max_instances_ == 0)
      throw std::invalid_argument("specialization topic and instance bound required");
  }

  // Incoming generalization carries the specialization topic, identity, and
  // synchronization timestamp from the reviewed official IDL binding.
  bool accept_general(const std::string& topic, const std::string& id,
                      std::uint64_t timestamp, General value) {
    if (topic != specialization_topic_)
      throw std::invalid_argument("unexpected specialization topic");
    auto& state = instance(id);
    if (!prepare(state, timestamp)) return false;
    if (state.general_timestamp && timestamp <= *state.general_timestamp)
      return false;
    state.general_timestamp = timestamp;
    state.general = std::move(value);
    state.highwater = timestamp > state.highwater ? timestamp : state.highwater;
    return synchronize(state, id, timestamp);
  }

  // Incoming specialization comes from the already verified specialization
  // DDS topic; the adapter must not pass unrelated topic samples here.
  bool accept_specialization(const std::string& id, std::uint64_t timestamp,
                             Specialization value) {
    auto& state = instance(id);
    if (!prepare(state, timestamp)) return false;
    if (state.special_timestamp && timestamp <= *state.special_timestamp)
      return false;
    state.special_timestamp = timestamp;
    state.specialization = std::move(value);
    state.highwater = timestamp > state.highwater ? timestamp : state.highwater;
    return synchronize(state, id, timestamp);
  }

  // A single Dispose invalidates visibility immediately; the relationship
  // is not eligible for recreation until Dispose was observed on BOTH topics.
  // Tombstones preserve a timestamp floor so delayed old samples cannot
  // resurrect a disposed pair. They consume bounded instance slots until
  // explicitly cleared by the host after reviewed synchronization policy.
  bool dispose_general(const std::string& id) {
    auto& state = instance(id);
    state.disposed_general = true;
    state.general.reset();
    state.visible.reset();
    return state.disposed_general && state.disposed_special;
  }
  bool dispose_specialization(const std::string& id) {
    auto& state = instance(id);
    state.disposed_special = true;
    state.specialization.reset();
    state.visible.reset();
    return state.disposed_general && state.disposed_special;
  }

  std::optional<Joined> latest(const std::string& id) const {
    const auto it = entries_.find(id);
    return it == entries_.end() ? std::nullopt : it->second.visible;
  }
  bool removal_pending(const std::string& id) const {
    const auto it = entries_.find(id);
    return it != entries_.end() &&
      (it->second.disposed_general != it->second.disposed_special);
  }
  std::size_t instance_count() const noexcept { return entries_.size(); }

  // Explicit resync removes a tombstone and its replay history. This must
  // be authorized only after the host re-establishes DDS reader state.
  void clear_after_resync(const std::string& id) { entries_.erase(id); }

private:
  struct Entry {
    std::optional<General> general;
    std::optional<Specialization> specialization;
    std::optional<std::uint64_t> general_timestamp;
    std::optional<std::uint64_t> special_timestamp;
    std::optional<Joined> visible;
    std::uint64_t highwater{0};
    bool disposed_general{false};
    bool disposed_special{false};
  };

  Entry& instance(const std::string& id) {
    if (id.empty()) throw std::invalid_argument("specialization ID is required");
    auto it = entries_.find(id);
    if (it != entries_.end()) return it->second;
    if (entries_.size() >= max_instances_)
      throw std::length_error("generalization instance capacity exhausted");
    return entries_.emplace(id, Entry{}).first->second;
  }
  bool prepare(Entry& state, std::uint64_t timestamp) {
    if (state.disposed_general && state.disposed_special) {
      if (timestamp <= state.highwater) return false;
      state = Entry{};
    } else if (state.disposed_general || state.disposed_special) {
      throw std::logic_error("partial generalization Dispose: resync required");
    }
    return true;
  }
  bool synchronize(Entry& state, const std::string& id,
                   std::uint64_t timestamp) {
    if (!state.general || !state.specialization || !state.general_timestamp ||
        !state.special_timestamp || *state.general_timestamp != *state.special_timestamp)
      return false;
    if (state.visible && timestamp <= state.visible->specialization_timestamp)
      return false;
    state.visible = Joined{id, timestamp, *state.general, *state.specialization};
    return true;
  }
  std::string specialization_topic_;
  std::size_t max_instances_;
  std::map<std::string, Entry> entries_;
};
} // namespace umaa
