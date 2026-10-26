// Prototype atomic collection assembler inspired by UMAA EO ICD 6.0 3.8.
// NOT a DDS writer/reader and not a conformance-certified LargeCollection.
#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace umaa {
enum class CollectionKind { Set, List };
template <class T> struct CollectionElement {
  std::string element_id;
  std::int64_t timestamp{};
  T value;
  std::string next_id; // used only for List
};
struct CollectionMetadata {
  std::string collection_id;
  std::size_t size{};
  std::string update_element_id;
  std::optional<std::int64_t> update_element_timestamp; // unset for DDS dispose
  std::string starting_element_id;
};
template <class T> class CollectionAssembler {
public:
  explicit CollectionAssembler(CollectionKind kind, std::size_t maximum = 10000)
      : kind_(kind), maximum_(maximum) {}

  bool upsert(CollectionElement<T> element) {
    if (element.element_id.empty()) throw std::invalid_argument("Missing element ID");
    auto it = elems_.find(element.element_id);
    if (it != elems_.end() && it->second.timestamp >= element.timestamp) return false;
    auto tomb = tombstone_clock_.find(element.element_id);
    if (tomb != tombstone_clock_.end() && tomb->second >= element.timestamp) return false;
    if (it == elems_.end() && elems_.size() >= maximum_)
      throw std::length_error("Exceeded collection memory cap");
    auto id = element.element_id;
    elems_[id] = std::move(element);
    disposed_.erase(id);
    tombstone_clock_.erase(id);
    maybe_commit();
    return true;
  }
  // Compatibility operation for DDS adapters that do not provide a valid
  // timestamp. It cannot establish a safe ordering watermark: use the timed
  // overload where the wire protocol makes disposal time available.
  void dispose(const std::string& id) {
    if (id.empty()) throw std::invalid_argument("Missing element ID");
    reserve_tombstone(id);
    elems_.erase(id);
    disposed_.insert(id);
    maybe_commit();
  }
  // A late data sample must not resurrect a previously disposed element.
  // Transport adapters must validate and normalize timestamps themselves.
  bool dispose(const std::string& id, std::int64_t timestamp) {
    if (id.empty()) throw std::invalid_argument("Missing element ID");
    auto live = elems_.find(id);
    if (live != elems_.end() && live->second.timestamp >= timestamp) return false;
    auto old = tombstone_clock_.find(id);
    if (old != tombstone_clock_.end() && old->second >= timestamp) return false;
    reserve_tombstone(id);
    if (old == tombstone_clock_.end() && tombstone_clock_.size() >= maximum_)
      throw std::length_error("Exceeded timestamped tombstone memory cap");
    elems_.erase(id);
    disposed_.insert(id);
    tombstone_clock_[id] = timestamp;
    maybe_commit();
    return true;
  }
  void metadata(CollectionMetadata m) {
    if (m.collection_id.empty()) throw std::invalid_argument("Missing collection ID");
    if (m.size > maximum_) throw std::length_error("Metadata exceeds collection memory cap");
    // One assembler represents one DDS collection instance. Never combine
    // two collection IDs into a single supposedly atomic snapshot.
    if (metadata_ && m.collection_id != metadata_->collection_id)
      throw std::invalid_argument("Collection ID changed; create/reset assembler");
    auto signature = token(m);
    if (metadata_ && signature == token(*metadata_)) return;
    metadata_ = std::move(m);
    maybe_commit();
  }
  std::optional<std::vector<T>> snapshot() const { return committed_; }
  std::uint64_t commit_count() const { return commits_; }
  std::size_t buffered_elements() const { return elems_.size(); }
  void clear() { elems_.clear(); disposed_.clear(); tombstone_clock_.clear(); metadata_.reset(); committed_.reset(); last_commit_.clear(); }

private:
  void reserve_tombstone(const std::string& id) const {
    if (!disposed_.count(id) && disposed_.size() >= maximum_)
      throw std::length_error("Exceeded disposed element memory cap");
  }
  static std::string token(const CollectionMetadata& m) {
    return m.collection_id + ":" + std::to_string(m.size) + ":" + m.update_element_id + ":" +
      (m.update_element_timestamp ? std::to_string(*m.update_element_timestamp) : "DISPOSE") + ":" + m.starting_element_id;
  }
  void maybe_commit() {
    if (!metadata_) return;
    const auto& m = *metadata_;
    const auto token_now = token(m);
    if (last_commit_ == token_now) return;
    if (m.size != elems_.size()) return;
    if (m.size == 0) {
      // Nil GUID represents an empty collection; simplify recognition by
      // accepting empty ID or all-zero UUID as the transport-canonical form.
      if (m.update_element_id != "00000000-0000-0000-0000-000000000000" &&
          !m.update_element_id.empty()) return;
      committed_ = std::vector<T>{}; last_commit_ = token_now; ++commits_; return;
    }
    if (m.update_element_timestamp) {
      auto it = elems_.find(m.update_element_id);
      if (it == elems_.end() || it->second.timestamp != *m.update_element_timestamp) return;
    } else if (!disposed_.count(m.update_element_id)) {
      return; // metadata declares last mutation to be a dispose
    }
    std::vector<T> vals;
    vals.reserve(m.size);
    if (kind_ == CollectionKind::Set) {
      std::vector<std::string> keys;
      for (const auto& kv : elems_) keys.push_back(kv.first);
      std::sort(keys.begin(), keys.end());
      for (const auto& key : keys) vals.push_back(elems_.at(key).value);
    } else {
      if (m.starting_element_id.empty()) return;
      std::unordered_set<std::string> visited;
      std::string cursor = m.starting_element_id;
      while (!cursor.empty()) {
        if (!visited.insert(cursor).second) return; // cycle
        auto it = elems_.find(cursor);
        if (it == elems_.end()) return;
        vals.push_back(it->second.value);
        cursor = it->second.next_id;
      }
      if (vals.size() != m.size) return; // dangling/unreachable element
    }
    committed_ = std::move(vals);
    last_commit_ = token_now;
    ++commits_;
  }
  CollectionKind kind_;
  std::size_t maximum_;
  std::unordered_map<std::string, CollectionElement<T>> elems_;
  std::unordered_set<std::string> disposed_;
  std::unordered_map<std::string, std::int64_t> tombstone_clock_;
  std::optional<CollectionMetadata> metadata_;
  std::optional<std::vector<T>> committed_;
  std::string last_commit_;
  std::uint64_t commits_{};
};
}  // namespace umaa
