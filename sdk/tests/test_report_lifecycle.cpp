#include "umaa/report_engine.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

struct Sensor : umaa::ReportHandler<int> {
  mutable bool fails{false};
  int data{42};
  int read_report() const override {
    if (fails) throw std::runtime_error("sensor offline");
    return data;
  }
};
using Intent = umaa::ReportIntent<int>;
using Kind = Intent::Kind;
template <typename E, class F> static void expect_throw(F&& f) {
  bool threw = false;
  try { f(); } catch (const E&) { threw = true; }
  assert(threw);
}
int main() {
  Sensor sensor;
  umaa::ReportProviderLifecycle<int> provider("sensor-1", sensor);
  umaa::ReportConsumerLifecycle<int> consumer;
  assert(!consumer.receive({"sensor-1", 2, 4}, 0));
  auto sub = consumer.start();
  assert(sub.size() == 1 && sub[0].kind == Kind::Subscribe);
  auto epoch1 = consumer.epoch();
  expect_throw<std::logic_error>([&] { consumer.start(); });
  auto start = provider.start(10);
  assert(start.size() == 2 && start[0].kind == Kind::RegisterPublisher);
  assert(start[1].kind == Kind::Publish && start[1].report->data == 42);
  assert(consumer.receive(*start[1].report, epoch1));
  assert(!consumer.receive(*start[1].report, epoch1));
  assert(consumer.cache().latest("sensor-1")->data == 42);
  expect_throw<std::logic_error>([&] { provider.start(11); });
  expect_throw<std::invalid_argument>([&] { provider.update(10); });
  sensor.data = 58;
  auto update = provider.update(12);
  assert(update.kind == Kind::Publish && consumer.receive(*update.report, epoch1));
  assert(!consumer.receive_dispose("sensor-1", 11, epoch1));
  expect_throw<std::invalid_argument>([&] { provider.shutdown(12); });
  auto stop = provider.shutdown(13);
  assert(stop.size() == 2 && stop[0].kind == Kind::Dispose);
  assert(stop[1].kind == Kind::UnregisterPublisher && stop[0].timestamp == 13);
  assert(consumer.receive_dispose("sensor-1", *stop[0].timestamp, epoch1));
  assert(!consumer.cache().latest("sensor-1"));
  assert(!consumer.receive({"sensor-1", 12, 58}, epoch1));
  assert(!consumer.receive_dispose("sensor-1", 12, epoch1));
  assert(consumer.receive({"sensor-1", 14, 59}, epoch1));
  assert(consumer.receive({"sensor-2", 10, 100}, epoch1));
  assert(!consumer.receive_dispose("sensor-2", 9, epoch1));
  assert(consumer.receive_dispose("sensor-2", 11, epoch1));
  assert(!consumer.receive({"sensor-2", 10, 100}, epoch1));
  assert(!consumer.receive({"", 100, 1}, epoch1));
  auto unsub = consumer.shutdown();
  assert(unsub.size() == 1 && unsub[0].kind == Kind::Unsubscribe);
  assert(!consumer.receive({"sensor-1", 15, 60}, epoch1));
  assert(consumer.cache().size() == 0 && consumer.shutdown().empty());
  consumer.start();
  auto epoch2 = consumer.epoch();
  assert(epoch1 != epoch2);
  assert(!consumer.receive({"sensor-1", 16, 61}, epoch1));
  assert(consumer.receive({"sensor-1", 16, 61}, epoch2));
  sensor.fails = true;
  expect_throw<std::runtime_error>([&] { provider.start(20); });
  assert(!provider.running());
  auto recovery = provider.start_unrecoverable(20);
  assert(recovery.size() == 2 && recovery[1].kind == Kind::Dispose);
  assert(consumer.receive_dispose("sensor-1", 20, epoch2));
  assert(!consumer.receive({"sensor-1", 19, 1}, epoch2));
  sensor.fails = false;
  assert(consumer.receive(*provider.update(21).report, epoch2));
  assert(provider.shutdown(22).size() == 2);
  consumer.shutdown();
  expect_throw<std::logic_error>([&] { provider.update(30); });
}
