// Test-only ABI component: in-flight callback and failed-stop injection.
#include "umaa/module_api_v2.h"
#include <atomic>
#include <chrono>
#include <new>
#include <thread>
namespace {
std::atomic<int> active{0};
std::atomic<int> stop_count{0};
std::atomic<bool> stop_fail{false};
struct State {};
void* create(){return new(std::nothrow)State;}
uint32_t count(void*){return 1;}
int32_t at(void*,uint32_t index,UmaaComponentServiceV2* desc){
 if(index!=0||!desc)return UMAA_COMPONENT_INVALID;
 *desc={"Demo::SlowControl","demo:slow-v1",1,0};return UMAA_COMPONENT_OK;
}
int32_t start(void*){return UMAA_COMPONENT_OK;}
int32_t call(void*,const UmaaComponentCallV2* c,UmaaComponentResponseV2* r){
 if(!c||!r||c->action!=UMAA_COMPONENT_COMMAND)return UMAA_COMPONENT_INVALID;
 active.fetch_add(1);
 std::this_thread::sleep_for(std::chrono::milliseconds(180));
 active.fetch_sub(1);
 r->size=0;r->business_result=0;return UMAA_COMPONENT_OK;
}
int32_t stop(void*){stop_count.fetch_add(1);return stop_fail.load()?UMAA_COMPONENT_INVALID:UMAA_COMPONENT_OK;}
void destroy(void* p){delete static_cast<State*>(p);}
const UmaaComponentModuleV2 api={2,sizeof(UmaaComponentModuleV2),"slow-stop-test",create,count,at,start,call,stop,destroy};
}
extern "C" UMAA_COMPONENT_EXPORT int umaa_test_inflight(){return active.load();}
extern "C" UMAA_COMPONENT_EXPORT int umaa_test_stop_count(){return stop_count.load();}
extern "C" UMAA_COMPONENT_EXPORT void umaa_test_fail_stop(int value){stop_fail.store(value!=0);}
extern "C" UMAA_COMPONENT_EXPORT const UmaaComponentModuleV2* umaa_get_component_v2(uint32_t v){return v==2?&api:nullptr;}
