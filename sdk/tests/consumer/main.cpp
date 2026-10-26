#include <umaa/command_engine.hpp>
#include <umaa/command_transport.hpp>
#include <umaa/report_engine.hpp>
#include <umaa/large_collection.hpp>
int main() {
  umaa::CollectionAssembler<int> assembler(umaa::CollectionKind::Set);
  return assembler.snapshot().has_value() ? 1 : 0;
}
