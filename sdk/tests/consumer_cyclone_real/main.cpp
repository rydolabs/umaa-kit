// External consumer built only against a genuine installed CycloneDDS package.
// Unlike sdk/tests/consumer_cyclone, there is no fake DDS ABI or CMake target.
#include <umaa/cyclone_command_transport.hpp>
#include <umaa/cyclone_report_reader.hpp>
#include <umaa/cyclone_report_writer.hpp>
#include <dds/dds.h>
#include <cstdint>
#include <stdexcept>
#include <string>

struct Wire { int value; };
struct Mapper {
  Wire make_report(const umaa::ReportSample<int>& data) const { return {data.data}; }
  Wire make_dispose(const std::string&, std::int64_t) const { return {0}; }
};
int main() {
  dds_qos_t* qos = dds_create_qos();
  if (!qos) return 2;
  int result = 0;
  try {
    // Creation validates SDK API and links genuine DDS. Invalid IDs are never
    // submitted to the vendor; no synthetic TypeSupport masquerades as UMAA.
    using Writer = umaa::cyclone::ReportWriter<int, Wire, Mapper>;
    Writer writer("probe", 1, 2, qos, Mapper{});
    if (writer.state() != Writer::State::Idle) result = 3;
    bool rejected = false;
    try {
      umaa::cyclone::ReportReader<Wire> reader(1, 2, nullptr, qos);
    } catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) result = 4;
  } catch (...) { result = 5; }
  dds_delete_qos(qos);
  return result;
}
