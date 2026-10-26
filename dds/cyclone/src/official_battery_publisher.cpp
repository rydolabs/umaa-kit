/* Official AUVSI UMAA 6.0 BatteryReportType demo wire publisher.
 * This is a selected-service transport-binding proof, not a QoS declaration
 * or full UMAA service conformance claim. Official IDL stays external. */
#include <dds/dds.h>
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"
#include "umaa/module_loader.hpp"
#include "umaa/report_transport.hpp"
#include "umaa/cyclone_report_writer.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

struct BatteryHandler final : umaa::ReportHandler<double> {
  double value{20.0};
  double read_report() const override { return value; }
};

static void require_dds(dds_return_t result, const char* action) {
  if (result < 0)
    throw std::runtime_error(std::string(action) + ": " + dds_strretcode(-result));
}

// The verified SDK lifecycle now controls when the DDS Writer is allocated,
// used, disposed and deleted. Topic/QoS values below are demo test settings;
// mandatory, per-service UMAA QoS is not yet established by Component specs.
struct BatteryMapper {
  mutable double charge{};
  UMAA_EO_BatteryStatus_BatteryReportType make_report(
      const umaa::ReportSample<double>& report) const {
    charge = report.data;
    UMAA_EO_BatteryStatus_BatteryReportType sample{};
    sample.chargeRemaining = &charge;
    return sample;
  }
  UMAA_EO_BatteryStatus_BatteryReportType make_dispose(
      const std::string&, std::int64_t) const {
    UMAA_EO_BatteryStatus_BatteryReportType sample{};
    sample.chargeRemaining = &charge;
    return sample;
  }
};

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  dds_entity_t participant = 0;
  try {
    umaa::ServiceModule battery(argv[1], "Demo::EO::BatteryStatus", "demo-only");
    BatteryHandler handler;
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
    require_dds(participant, "create participant");
    const dds_entity_t topic =
        dds_create_topic(participant, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
                         "BatteryReportType", nullptr, nullptr);
    require_dds(topic, "create official BatteryReportType topic");
    const char* required = std::getenv("UMAA_EXPECT_READERS");
    const int readers = required ? std::atoi(required) : 1;
    if (readers < 1 || readers > 2) throw std::invalid_argument("invalid reader count");
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> qos(
        dds_create_qos(), &dds_delete_qos);
    if (!qos) throw std::runtime_error("cannot allocate DDS QoS");
    // Demonstration profile only: not reviewed per-service mandatory QoS.
    dds_qset_reliability(qos.get(), DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(qos.get(), DDS_HISTORY_KEEP_LAST, 16);
    const int attempts = std::getenv("UMAA_NETEM_RECOVERY") ? 1600 : 200;
    umaa::cyclone::ReportWriter<double, UMAA_EO_BatteryStatus_BatteryReportType,
                                BatteryMapper> transport(
        "battery-demo", participant, topic, qos.get(), BatteryMapper{}, readers, attempts);
    umaa::BoundReportProvider<double> provider("battery-demo", handler, transport);
    provider.start(1);
    dds_sleepfor(DDS_MSECS(150));
    handler.value = static_cast<double>(battery.invoke(2, 0));
    provider.update(2);
    dds_sleepfor(DDS_MSECS(150));
    provider.shutdown(3);
    if (provider.state() != umaa::BoundReportProvider<double>::State::Stopped)
      throw std::logic_error("Report provider not stopped");
    require_dds(dds_delete(participant), "delete participant");
    participant = 0;
    std::puts("OFFICIAL_UMAA_WIRE_PUBLISHER: transport-bound Report lifecycle PASS");
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "official module/transport error: %s\n", e.what());
    if (participant > 0) dds_delete(participant);
    return 4;
  }
}
