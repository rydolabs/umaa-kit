#include "umaa/module_loader.hpp"
#include <cassert>
#include <iostream>
#include <string>
int main(int argc, char** argv) {
  if (argc != 4) return 2;
  std::string engine = argv[1], battery = argv[2], wrong_abi = argv[3];
  {
    umaa::ServiceModule e(engine, "Demo::EO::EngineControl", "demo-only");
    umaa::ServiceModule b(battery, "Demo::EO::BatteryStatus", "demo-only");
    assert(e.name() == "Demo::EO::EngineControl");
    assert(e.invoke(1, 1200) == 1200);
    assert(b.invoke(2, 0) == 80);
    bool rejected = false;
    try { (void)e.invoke(2, 1); } catch (const umaa::ModuleLoadError&) { rejected = true; }
    if (!rejected) return 3;
  } // two service modules stop/destroy and unload independently
  {
    umaa::ServiceModule e(engine, "Demo::EO::EngineControl", "demo-only");
    if (e.invoke(1, 900) != 900) return 4; // clean reload
  }
  for (const auto& wrong : {"Demo::EO::BatteryStatus", "Demo::EO::Other"}) {
    bool rejected = false;
    try { umaa::ServiceModule e(engine, wrong, "demo-only"); }
    catch (const umaa::ModuleLoadError&) { rejected = true; }
    if (!rejected) return 5;
  }
  {
    bool rejected = false;
    try { umaa::ServiceModule e(engine, "Demo::EO::EngineControl", "wrong-schema"); }
    catch (const umaa::ModuleLoadError&) { rejected = true; }
    if (!rejected) return 6;
  }
  {
    bool rejected = false;
    try { umaa::ServiceModule bad(wrong_abi, "Demo::EO::EngineControl", "demo-only"); }
    catch (const umaa::ModuleLoadError&) { rejected = true; }
    if (!rejected) return 7;
  }
  std::cout << "PASS dynamic service module load/invoke/unload, reload, mismatch rejection\n";
  return 0;
}
