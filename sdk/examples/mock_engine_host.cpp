// Development-only line-transport host. NOT DDS and NOT UMAA wire format.
// Uses generated typed interface; all protocol sequencing is inside SDK.
#include "umaa/generated/EngineControl.hpp"
#include "umaa/command_transport.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class UserEngine final : public umaa::generated::EngineControlHandler {
public:
  bool validate(const umaa::generated::EngineControlPayload& cmd) override {
    return cmd.targetRpm >= 0 && cmd.targetRpm <= 10000;
  }
  umaa::Outcome execute(const umaa::generated::EngineControlPayload& cmd) override {
    return cmd.targetRpm == 0 ? umaa::Outcome::Completed : umaa::Outcome::Pending;
  }
  bool cancel(const umaa::generated::EngineControlPayload&) override {
    return true; // Mock can always stop; real adapter must confirm actual stop.
  }
};

// Synthetic stdio adapter implementing the same checked operations that a
// real DDS transport must supply. This executable is NOT on-wire UMAA DDS.
class StdioCommandTransport : public umaa::CommandTransport<umaa::generated::EngineControlPayload> {
 public:
  void publish_status(const umaa::Identity& id, const umaa::Event& e) override {
    std::cout << "STATUS " << id.session << " " << e.detail << " " << e.reason << '\n';
    std::cout.flush();
  }
  void publish_ack(const umaa::Identity& id, const umaa::generated::EngineControlPayload&) override {
    std::cout << "ACK " << id.session << '\n'; std::cout.flush();
  }
  void dispose_status(const umaa::Identity& id) override {
    std::cout << "DISPOSE_STATUS " << id.session << '\n'; std::cout.flush();
  }
  void dispose_ack(const umaa::Identity& id, const umaa::generated::EngineControlPayload&) override {
    std::cout << "DISPOSE_ACK " << id.session << '\n'; std::cout.flush();
  }
};
static void emit_ignored(const std::vector<umaa::Event>& events) {
  for (const auto& e : events) {
    if (e.kind == umaa::Event::Kind::Ignored)
      std::cout << "IGNORED " << e.session << " " << e.detail << '\n';
  }
  std::cout.flush();
}
int main() {
  UserEngine impl;
  StdioCommandTransport output;
  umaa::BoundCommandProvider<umaa::generated::EngineControlPayload> provider("p1", impl, output);
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string op; in >> op;
    if (op == "COMMAND") {
      umaa::Identity i;
      double rpm;
      if (!(in >> i.source >> i.destination >> i.session >> i.timestamp >> rpm)) return 3;
      umaa::generated::EngineControlPayload typed;
      typed.sessionID = i.session;
      typed.targetRpm = rpm;
      emit_ignored(provider.receive(i, typed));
    } else if (op == "FINISH") {
      std::string session, answer; if (!(in >> session >> answer)) return 3;
      emit_ignored(provider.complete(session, answer == "SUCCESS"));
    } else if (op == "DISPOSE" || op == "LOST_CONSUMER") {
      std::string source, session; if (!(in >> source >> session)) return 3;
      emit_ignored(provider.command_disposed(source, session));
    } else if (op == "RESTART") {
      // Emulates recovery policy for existing persisted commands. A real DDS
      // adapter must inspect DURABILITY samples and restore state at startup.
      emit_ignored(provider.fail_unrecoverable());
    } else if (op == "EXIT") {
      break;
    } else if (!op.empty()) {
      std::cerr << "Unknown fixture command" << std::endl; return 4;
    }
  }
}
