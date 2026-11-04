// Independent Fast DDS C++ oracle. Uses only vendor-generated original IDL,
// never the SDK host representation nor its conversion/validation code.
#if __has_include("BatteryReportType.hpp")
#include "BatteryReportType.hpp"
#include "BatteryReportTypePubSubTypes.hpp"
#else
#include "BatteryReportType.h"
#include "BatteryReportTypePubSubTypes.h"
#endif
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>

namespace fd = eprosima::fastdds::dds;
using Report = UMAA::EO::BatteryStatus::BatteryReportType;
using Element = UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement;

template<typename Guid>
static bool guid(const Guid& id, uint8_t prefix) {
  if (id.size() != 16 || id[0] != prefix) return false;
  for (unsigned i = 1; i < 16; ++i) if (id[i]) return false;
  return true;
}

static void ok(fd::ReturnCode_t code, const char* op) {
  if (code != fd::RETCODE_OK)
    throw std::runtime_error(std::string(op) + " failed");
}

static bool verify_report(const Report& x, std::int64_t& checkpoint_seconds,
                          std::int32_t& checkpoint_nanos) {
  const auto& m = x.cellsListMetadata();
  if (!x.chargeRemaining() || *x.chargeRemaining() != 80.0 ||
      !x.current() || *x.current() != -3.25 ||
      !x.energyUsageRate() || *x.energyUsageRate() != 120.5 ||
      !x.hours() || *x.hours() != 8.0 ||
      !x.temp() || *x.temp() != 25.0 ||
      !x.voltage() || *x.voltage() != 24.5 ||
      static_cast<int>(x.state()) != 2 ||
      x.timeStamp().seconds() != 1700000000 ||
      x.timeStamp().nanoseconds() != 250 ||
      !guid(x.source().id(), 0x33) ||
      !guid(x.source().parentID(), 0) ||
      !guid(m.listID(), 0x11) ||
      !guid(m.updateElementID(), 0x22) ||
      !guid(m.startingElementID(), 0x22) ||
      m.size() != 1 ||
      !m.updateElementTimestamp()) return false;
  checkpoint_seconds = m.updateElementTimestamp()->seconds();
  checkpoint_nanos = m.updateElementTimestamp()->nanoseconds();
  return true;
}

static bool verify_element(const Element& x, std::int64_t& seconds,
                           std::int32_t& nanos) {
  if (!guid(x.listID(), 0x11) || !guid(x.elementID(), 0x22) ||
      static_cast<bool>(x.nextElementID()) ||
      x.element().current() != -0.25 ||
      x.element().temperature() != 26.0 ||
      x.element().voltage() != 3.75 ||
      x.elementTimestamp().seconds() != 1700000001 ||
      x.elementTimestamp().nanoseconds() != 110) return false;
  seconds = x.elementTimestamp().seconds();
  nanos = x.elementTimestamp().nanoseconds();
  return true;
}

int main() {
  auto* factory = fd::DomainParticipantFactory::get_instance();
  fd::DomainParticipant* p = nullptr;
  try {
    p = factory->create_participant(0, fd::PARTICIPANT_QOS_DEFAULT);
    if (!p) throw std::runtime_error("participant failed");
    fd::TypeSupport t1(new UMAA::EO::BatteryStatus::BatteryReportTypePubSubType());
    fd::TypeSupport t2(new UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElementPubSubType());
    ok(t1.register_type(p), "report registration");
    ok(t2.register_type(p), "cell registration");
    std::fprintf(stderr, "FASTDDS_ORIGINAL_TYPES report=%s element=%s\n",
        t1.get_type_name().c_str(), t2.get_type_name().c_str());
    std::fflush(stderr);
    auto* topic1 = p->create_topic("BatteryReportType", t1.get_type_name(), fd::TOPIC_QOS_DEFAULT);
    auto* topic2 = p->create_topic("BatteryReportTypeCellsListElement", t2.get_type_name(),
                                   fd::TOPIC_QOS_DEFAULT);
    if (!topic1 || !topic2) throw std::runtime_error("topics failed");
    auto subq = fd::SUBSCRIBER_QOS_DEFAULT;
    subq.presentation().access_scope = fd::TOPIC_PRESENTATION_QOS;
    subq.presentation().ordered_access = true;
    auto* sub = p->create_subscriber(subq);
    if (!sub) throw std::runtime_error("subscriber failed");
    auto q = fd::DATAREADER_QOS_DEFAULT;
    q.reliability().kind = fd::RELIABLE_RELIABILITY_QOS;
    q.history().kind = fd::KEEP_LAST_HISTORY_QOS;
    q.history().depth = 16;
    auto* report_reader = sub->create_datareader(topic1, q);
    auto* element_reader = sub->create_datareader(topic2, q);
    if (!report_reader || !element_reader) throw std::runtime_error("readers failed");

    bool got_report = false, got_element = false, disposed_report = false, disposed_element = false;
    std::int64_t rsec = 0, esec = 0;
    std::int32_t rns = 0, ens = 0;
    int code = 9;
    for (int i = 0; i < 450 && !(got_report && got_element &&
                                   disposed_report && disposed_element); ++i) {
      for (int topic_id = 0; topic_id < 2; ++topic_id) {
        fd::SampleInfo info{};
        if (topic_id == 0) {
          Report sample{};
          auto res = report_reader->take_next_sample(&sample, &info);
          if (res == fd::RETCODE_NO_DATA) continue;
          ok(res, "take Report");
          if (info.valid_data) {
            if (got_report || !verify_report(sample, rsec, rns))
              throw std::runtime_error("invalid or duplicate nested Battery Report");
            got_report = true;
          } else if (info.instance_state == fd::NOT_ALIVE_DISPOSED_INSTANCE_STATE) {
            if (!got_report) throw std::runtime_error("Report disposed before data");
            disposed_report = true;
          }
        } else {
          Element sample{};
          auto res = element_reader->take_next_sample(&sample, &info);
          if (res == fd::RETCODE_NO_DATA) continue;
          ok(res, "take Cell");
          if (info.valid_data) {
            if (got_element || !verify_element(sample, esec, ens))
              throw std::runtime_error("invalid or duplicate nested Cell");
            got_element = true;
          } else if (info.instance_state == fd::NOT_ALIVE_DISPOSED_INSTANCE_STATE) {
            if (!got_element) throw std::runtime_error("Cell disposed before data");
            disposed_element = true;
          }
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!got_report || !got_element || !disposed_report || !disposed_element) {
      std::fprintf(stderr, "FAST_BATTERY_INCOMPLETE report=%d element=%d disposeReport=%d disposeCell=%d\n",
                   got_report, got_element, disposed_report, disposed_element);
    } else if (rsec != esec || rns != ens) {
      std::puts("FAST_BATTERY_CHECKPOINT_REJECTED");
      code = 10;
    } else {
      std::puts("FAST_BATTERY_INDEPENDENT_WIRE_PASS");
      code = 0;
    }
    ok(p->delete_contained_entities(), "close entities");
    ok(factory->delete_participant(p), "close participant");
    return code;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "independent Fast DDS Battery oracle: %s\n", e.what());
    if (p) {
      (void)p->delete_contained_entities();
      (void)factory->delete_participant(p);
    }
    return 3;
  }
}
