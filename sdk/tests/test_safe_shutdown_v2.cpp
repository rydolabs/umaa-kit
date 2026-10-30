#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/component_module.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(int argc,char** argv) {
  if(argc!=2)return 2;
  int32_t rpm=1200,business=-1;
  std::vector<uint8_t> command(sizeof(rpm)),out;
  std::memcpy(command.data(),&rpm,sizeof(rpm));
  umaa::ComponentModule m(argv[1]);
  constexpr uint64_t session=7401;
  assert(m.call("Demo::EO::AsyncEngineControl",UMAA_COMPONENT_COMMAND,
                command,out,business,session)==UMAA_COMPONENT_OK);
  assert(business==2);
  assert(!m.stop()); // Never unload while device action is Pending
  bool refused=false;
  try {m.call("Demo::EO::AsyncEngineControl",UMAA_COMPONENT_COMMAND,
              command,out,business,session);}
  catch(const std::runtime_error&){refused=true;}
  assert(refused); // Duplicate command is never replayed
  for(int i=0;i<3;i++)
    assert(m.call("Demo::EO::AsyncEngineControl",UMAA_COMPONENT_COMMAND_POLL,
                  {},out,business,session)==UMAA_COMPONENT_OK);
  assert(business==0);
  assert(m.stop());
  assert(m.stop());
  refused=false;
  try {m.call("Demo::EO::EngineControl",UMAA_COMPONENT_COMMAND,command,out,business);}
  catch(const std::runtime_error&){refused=true;}
  assert(refused);
  umaa::ComponentModule n(argv[1]);
  assert(n.call("Demo::EO::AsyncEngineControl",UMAA_COMPONENT_COMMAND,
                command,out,business,7402)==UMAA_COMPONENT_OK);
  assert(!n.stop());
  assert(n.call("Demo::EO::AsyncEngineControl",UMAA_COMPONENT_CANCEL,
                {},out,business,7402)==UMAA_COMPONENT_OK);
  // Confirmed hardware cancellation is Completed, not an ambiguous Failed.
  assert(business==0);
  assert(n.stop());
  std::cout<<"PASS device session drain, duplicate-replay fence, safe reload gate\n";
}
