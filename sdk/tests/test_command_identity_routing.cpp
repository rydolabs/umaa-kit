#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/command_engine.hpp"
#include <cassert>
#include <iostream>
#include <vector>
struct SessionHandler final: umaa::Handler<int> {
 std::vector<std::string> executed,canceled;
 umaa::Outcome execute(const int&) override {return umaa::Outcome::Failed;}
 bool cancel(const int&) override {return false;}
 umaa::Outcome execute_session(const umaa::Identity& id,const int&) override {
   executed.push_back(id.source+":"+id.session+":"+std::to_string(id.timestamp));
   return umaa::Outcome::Pending;
 }
 bool cancel_session(const umaa::Identity& id,const int&) override {
   canceled.push_back(id.source+":"+id.session+":"+std::to_string(id.timestamp));
   return true;
 }
};
int main(){
 SessionHandler handler;
 umaa::CommandEngine<int> engine("host",handler);
 engine.receive({"one","host","s1",10},100);
 engine.receive({"two","host","s2",20},200);
 assert(handler.executed.size()==2);
 assert(handler.executed[0]=="one:s1:10");
 assert(handler.executed[1]=="two:s2:20");
 auto update=engine.receive({"one","host","s1",11},101);
 assert(handler.canceled.size()==1&&handler.canceled[0]=="one:s1:10");
 assert(handler.executed.back()=="one:s1:11");
 assert(update.size()>=3);
 auto disposed=engine.command_disposed("two","s2");
 assert(!disposed.empty());
 assert(handler.canceled.back()=="two:s2:20");
 assert(engine.contains("s1"));
 auto timeout=engine.timeout("s1");
 assert(!timeout.empty()&&timeout[0].reason=="TIMEOUT");
 assert(handler.canceled.back()=="one:s1:11");
 assert(handler.executed.size()==3);
 std::cout<<"PASS identity-aware execute/cancel preserves per-session routing\n";
}
