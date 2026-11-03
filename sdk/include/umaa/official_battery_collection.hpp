// Bounded two-Topic host-side assembler for the UMAA 6.0 EO Battery Large List.
//
// Source clauses: EO ICD 6.0 SS3.8.1--3.8.5 and the official BatteryReportType
// + BatteryReportTypeCellsListElement IDL. The caller MUST supply real DDS
// events and independently establish TOPIC/ordered Presentation on the
// Collection Element publisher/subscriber. This class cannot inspect DDS QoS,
// do DDS discovery or prove transport-level coherence/conformance.
#pragma once
#include "umaa/official_battery_host.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace umaa::official_battery {

struct StableList {
  Report report{};
  std::vector<ListElement> ordered{};
  std::uint64_t generation{};
  // This is deliberately never true: a host-side assembler cannot itself
  // attest to native DDS QoS, delivery completeness or formal conformance.
  bool nativeDdsCoherenceProved{false};
};

class LargeListJoiner final {
 public:
  // The external DDS receiver must have explicitly verified the element
  // subscriber/publisher TOPIC ordered-access QoS before selecting this mode.
  enum class ElementOrdering { NotVerified, CallerVerifiedTopicOrdered };

  explicit LargeListJoiner(ElementOrdering ordering, std::size_t max_elements = 4096,
                           std::size_t max_pending_reports = 32)
      : ordering_(ordering), max_elements_(max_elements),
        max_pending_reports_(max_pending_reports) {
    if (max_elements_ == 0 || max_elements_ > 100000 ||
        max_pending_reports_ == 0 || max_pending_reports_ > 512)
      throw std::invalid_argument("invalid Battery Large List memory limits");
  }

  void on_report(const Report& report) {
    verify_report(report);
    if (ordering_ != ElementOrdering::CallerVerifiedTopicOrdered)
      throw std::logic_error("cannot assemble without verified ordered element Topic QoS");
    const auto& meta = report.cellsListMetadata;
    select_list(meta.listID);
    if (source_ && (source_->id != report.source.id ||
                    source_->parentID != report.source.parentID))
      throw std::runtime_error("Battery Report changed its source Identifier");
    if (last_report_time_ && !after(report.timeStamp, *last_report_time_))
      throw std::runtime_error("non-monotonic/replayed Battery Report timestamp");
    if (pending_.size() == max_pending_reports_)
      throw std::runtime_error("Battery pending Report buffer limit exceeded");
    source_ = report.source;
    last_report_time_ = report.timeStamp;
    pending_.push_back(report);
    reconcile();
  }

  // Data samples may arrive before or after a Report/metadata, as described
  // in EO ICD 3.8.1--3.8.3. Updates for an existing element must advance the
  // elementTimestamp. No overwritten checkpoint may be treated as complete.
  void on_element(const ListElement& element) {
    verify_element(element);
    if (ordering_ != ElementOrdering::CallerVerifiedTopicOrdered)
      throw std::logic_error("cannot assemble without verified ordered element Topic QoS");
    select_list(element.listID);
    if (disposed_.count(element.elementID))
      throw std::runtime_error("disposed Battery element ID cannot be resurrected");
    auto it = elements_.find(element.elementID);
    if (it != elements_.end() && !after(element.elementTimestamp, it->second.elementTimestamp))
      throw std::runtime_error("stale/duplicate Battery element timestamp");
    if (it == elements_.end() && elements_.size() == max_elements_)
      throw std::runtime_error("Battery element buffer limit exceeded");
    elements_.insert_or_assign(element.elementID, element);
    reconcile();
  }

  // Called only after the real DDS reader identifies NOT_ALIVE_DISPOSED,
  // extracts BOTH official instance keys and associates the event to the
  // current source/list. A regular zero payload is not a Dispose.
  void on_dispose(const Guid& list_id, const Guid& element_id) {
    if (ordering_ != ElementOrdering::CallerVerifiedTopicOrdered)
      throw std::logic_error("cannot assemble without verified ordered element Topic QoS");
    select_list(list_id);
    if (disposed_.size() == max_elements_ && !disposed_.count(element_id))
      throw std::runtime_error("Battery tombstone memory limit exceeded");
    elements_.erase(element_id);
    disposed_.insert(element_id);
    reconcile();
  }

  const std::optional<StableList>& latest_stable() const noexcept {
    return committed_;
  }

  std::size_t pending_reports() const noexcept { return pending_.size(); }
  std::size_t buffered_elements() const noexcept { return elements_.size(); }

  // There is no safe implicit network reconnect: replay may be incomplete.
  // Explicitly destroy previous coherence and reconstruct from new DDS state.
  void reset_after_disconnect() noexcept {
    list_id_.reset();
    source_.reset();
    last_report_time_.reset();
    elements_.clear();
    disposed_.clear();
    pending_.clear();
    committed_.reset();
  }

 private:
  static bool after(const DateTime& a, const DateTime& b) noexcept {
    return a.seconds > b.seconds ||
           (a.seconds == b.seconds && a.nanoseconds > b.nanoseconds);
  }
  static bool same_time(const DateTime& a, const DateTime& b) noexcept {
    return a.seconds == b.seconds && a.nanoseconds == b.nanoseconds;
  }
  static void verify_time(const DateTime& t) {
    if (t.seconds == std::numeric_limits<std::int64_t>::min() ||
        t.nanoseconds < 0 || t.nanoseconds > 999999999)
      throw std::invalid_argument("Battery DateTime outside IDL range");
  }
  static void verify_double(double n, double lo, double hi) {
    if (!std::isfinite(n) || n < lo || n > hi)
      throw std::invalid_argument("Battery element scalar outside source IDL bounds");
  }
  static void verify_element(const ListElement& e) {
    verify_time(e.elementTimestamp);
    verify_double(e.element.current, -100000, 100000);
    verify_double(e.element.temperature, -273, 1000);
    verify_double(e.element.voltage, -100000, 100000);
  }
  static void verify_report(const Report& r) {
    verify_time(r.timeStamp);
    const auto& m = r.cellsListMetadata;
    if (m.updateElementTimestamp) verify_time(*m.updateElementTimestamp);
    if (m.size < 0) throw std::invalid_argument("negative Battery list size");
    if (static_cast<int>(r.state) < 0 || static_cast<int>(r.state) > 2)
      throw std::invalid_argument("invalid Battery PowerPlantState");
    if (r.chargeRemaining) verify_double(*r.chargeRemaining, 0, 1000);
    if (r.current) verify_double(*r.current, -100000, 100000);
    if (r.energyUsageRate) verify_double(*r.energyUsageRate, 0, 100000000);
    if (r.hours) verify_double(*r.hours, 0, 10505);
    if (r.temp) verify_double(*r.temp, -273, 1000);
    if (r.voltage) verify_double(*r.voltage, -100000, 100000);
  }
  static Guid nil_guid() noexcept { return Guid{}; }

  void select_list(const Guid& id) {
    if (list_id_ && *list_id_ != id)
      throw std::runtime_error("Battery Collection event belongs to another list");
    list_id_ = id;
  }

  void reconcile() {
    while (!pending_.empty()) {
      const auto& report = pending_.front();
      const auto& meta = report.cellsListMetadata;
      if (static_cast<std::size_t>(meta.size) > max_elements_)
        throw std::runtime_error("Battery collection exceeds configured maximum");

      const bool empty_initial = meta.size == 0 &&
                                 meta.updateElementID == nil_guid() &&
                                 !meta.updateElementTimestamp;
      if (empty_initial) {
        if ((committed_ && !committed_->ordered.empty()) || !elements_.empty())
          throw std::runtime_error("NIL checkpoint cannot erase a nonempty list");
        commit(report, {});
        continue;
      }

      if (meta.updateElementTimestamp) {
        const auto checkpoint = elements_.find(meta.updateElementID);
        if (checkpoint == elements_.end() ||
            !same_time(checkpoint->second.elementTimestamp,
                       *meta.updateElementTimestamp))
          break; // metadata may precede element data on another Topic
      } else {
        // An unset updateElementTimestamp is allowed only after a genuine
        // element Dispose, NOT after a zero/default value publication.
        if (!disposed_.count(meta.updateElementID)) break;
      }

      if (elements_.size() != static_cast<std::size_t>(meta.size))
        break; // still buffering an in-progress update or deletes

      std::vector<ListElement> ordered;
      std::set<Guid> visited;
      Guid cursor = meta.startingElementID;
      for (std::int32_t i = 0; i < meta.size; ++i) {
        const auto it = elements_.find(cursor);
        if (it == elements_.end() || !visited.insert(cursor).second)
          throw std::runtime_error("Battery Large List has missing link or cycle");
        ordered.push_back(it->second);
        if (i + 1 < meta.size) {
          if (!it->second.nextElementID)
            throw std::runtime_error("Battery list ends before declared size");
          cursor = *it->second.nextElementID;
        } else if (it->second.nextElementID) {
          throw std::runtime_error("Battery list has trailing linked element");
        }
      }
      if (visited.size() != elements_.size())
        throw std::runtime_error("Battery list has disconnected elements");
      commit(report, std::move(ordered));
    }
  }

  void commit(const Report& report, std::vector<ListElement> ordered) {
    committed_ = StableList{report, std::move(ordered), ++generation_, false};
    pending_.pop_front();
  }

  ElementOrdering ordering_;
  std::size_t max_elements_;
  std::size_t max_pending_reports_;
  std::optional<Guid> list_id_;
  std::optional<Identifier> source_;
  std::optional<DateTime> last_report_time_;
  std::map<Guid, ListElement> elements_;
  std::set<Guid> disposed_;
  std::deque<Report> pending_;
  std::optional<StableList> committed_;
  std::uint64_t generation_{0};
};

} // namespace umaa::official_battery
