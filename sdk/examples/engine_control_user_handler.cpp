// USER-OWNED C++ example. SDK code generator never modifies this file.
// Real integrations replace these two device operations with hardware work.
#include "engine_control_handler.hpp"
#include <memory>
namespace {
class VirtualEngine final : public umaa::generated::EngineControlHandler {
 public:
  bool set_ignition(umaa::generated::IgnitionState state) override {
    running_ = state == umaa::generated::IgnitionState::Run;
    return true;
  }
  bool stop() override { running_ = false; return true; }
 private:
  bool running_{false};
};
}
namespace umaa::generated {
std::unique_ptr<EngineControlHandler> make_engine_handler() {
  return std::make_unique<VirtualEngine>();
}
}
