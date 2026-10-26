// Test-only DDS C shim: operation failure injection and lifecycle ordering.
// The official wire coverage is independently executed on hosted runners.
#include "umaa/cyclone_report_writer.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>
struct dds_qos {};
struct dds_topic_descriptor {};
struct Sample { int key; int value; };
struct Mapper {
  Sample make_report(const umaa::ReportSample<int>& report) const { return {7, report.data}; }
  Sample make_dispose(const std::string&, std::int64_t when) const { return {7, static_cast<int>(when)}; }
};
static std::vector<std::string> calls;
static const char* fail = nullptr;
static int current_readers = 1;
static bool should_fail(const char* op) { return fail && std::string(fail) == op; }
extern "C" dds_entity_t dds_create_writer(dds_entity_t p, dds_entity_t t,
                                            const dds_qos_t* q, const void*) {
  assert(p == 1 && t == 2 && q);
  calls.emplace_back("create");
  return should_fail("create") ? -1 : 11;
}
extern "C" dds_return_t dds_get_publication_matched_status(dds_entity_t writer,
                                                              dds_publication_matched_status_t* matched) {
  assert(writer == 11 && matched);
  calls.emplace_back("match");
  matched->current_count = current_readers;
  return should_fail("match") ? -2 : 0;
}
extern "C" void dds_sleepfor(int64_t n) {
  assert(n == DDS_MSECS(25)); calls.emplace_back("sleep");
}
extern "C" dds_return_t dds_write(dds_entity_t writer, const void* v) {
  assert(writer == 11 && static_cast<const Sample*>(v)->key == 7);
  calls.emplace_back("write");
  return should_fail("write") ? -3 : 0;
}
extern "C" dds_return_t dds_dispose(dds_entity_t writer, const void* v) {
  assert(writer == 11 && static_cast<const Sample*>(v)->key == 7);
  calls.emplace_back("dispose");
  return should_fail("dispose") ? -4 : 0;
}
extern "C" dds_return_t dds_wait_for_acks(dds_entity_t writer, int64_t duration) {
  assert(writer == 11 && duration == DDS_SECS(5));
  calls.emplace_back("acks");
  return should_fail("acks") ? -5 : 0;
}
extern "C" dds_return_t dds_delete(dds_entity_t writer) {
  assert(writer == 11); calls.emplace_back("delete");
  return should_fail("delete") ? -6 : 0;
}
extern "C" const char* dds_strretcode(int32_t) { return "simulated vendor failure"; }
template<class F> void throws(F f) {
  bool caught = false;
  try { f(); } catch (const std::exception&) { caught = true; }
  assert(caught);
}
struct Handler final : umaa::ReportHandler<int> {
  int value{20};
  int read_report() const override { return value; }
};
int main() {
  using Writer = umaa::cyclone::ReportWriter<int, Sample, Mapper>;
  dds_qos qos;
  throws([&] { Writer r("",1,2,&qos,Mapper{}); });
  throws([&] { Writer r("src",0,2,&qos,Mapper{}); });
  throws([&] { Writer r("src",1,0,&qos,Mapper{}); });
  throws([&] { Writer r("src",1,2,nullptr,Mapper{}); });
  throws([&] { Writer r("src",1,2,&qos,Mapper{},-1); });
  throws([&] { Writer r("src",1,2,&qos,Mapper{},1,0); });
  {
    Handler device;
    Writer writer("src",1,2,&qos,Mapper{},1);
    umaa::BoundReportProvider<int> bound("src",device,writer);
    bound.start(1);
    throws([&] { writer.register_source("src"); });
    device.value = 80;
    bound.update(2);
    bound.shutdown(3);
    assert(writer.state() == Writer::State::Stopped && writer.entity() == 0);
    assert((calls == std::vector<std::string>{"create","match","write","write","dispose","acks","delete"}));
  }
  calls.clear();
  {
    Writer writer("src",1,2,&qos,Mapper{},1);
    fail = "create";
    throws([&] { writer.register_source("src"); });
    assert(writer.state() == Writer::State::Faulted && writer.entity() == 0);
    fail = nullptr;
    throws([&] { writer.register_source("src"); });
  }
  calls.clear();
  {
    Writer writer("src",1,2,&qos,Mapper{},1);
    current_readers = 0;
    throws([&] { Writer w("src",1,2,&qos,Mapper{},1,1); w.register_source("src"); });
    current_readers = 1;
    // A writer with successful discovery now faults on a negative publish.
    writer.register_source("src");
    fail = "write";
    throws([&] { writer.publish({"src",1,20}); });
    assert(writer.state() == Writer::State::Faulted);
    fail = nullptr;
    throws([&] { writer.publish({"src",2,80}); });
  }
  for (auto op : {"dispose", "acks", "delete", "match"}) {
    fail = nullptr;
    Writer writer("src",1,2,&qos,Mapper{},1);
    if (std::string(op) == "match") {
      fail = op; throws([&] { writer.register_source("src"); });
    } else {
      writer.register_source("src");
      fail = op;
      if (std::string(op) == "dispose") throws([&] { writer.dispose("src", 3); });
      else throws([&] { writer.unregister_source("src"); });
    }
    assert(writer.state() == Writer::State::Faulted);
    fail = nullptr;
    throws([&] { writer.register_source("src"); });
  }
}
