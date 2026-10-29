#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/remote_command_monitor.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
int main(){
 umaa::RemoteCommandMonitor tracker(2);
 tracker.issue({"operator","vehicle","s1",10});
 tracker.issue({"operator","vehicle","s2",20});
 assert(tracker.result("s1")==umaa::RemoteCommandResult::Pending);
 assert(!tracker.ack("s1","attacker",11));
 assert(!tracker.ack("s1","vehicle",9));
 assert(tracker.status("s1","vehicle",umaa::State::ISSUED,11));
 assert(!tracker.status("s1","vehicle",umaa::State::ISSUED,11));
 assert(tracker.status("s1","vehicle",umaa::State::COMMANDED,12));
 assert(tracker.status("s1","vehicle",umaa::State::EXECUTING,13));
 assert(tracker.status("s1","vehicle",umaa::State::COMPLETED,14));
 assert(tracker.result("s1")==umaa::RemoteCommandResult::Pending);
 assert(tracker.ack("s1","vehicle",15));
 assert(tracker.result("s1")==umaa::RemoteCommandResult::Completed);
 assert(!tracker.status("s1","vehicle",umaa::State::FAILED,16));
 assert(tracker.needs_recovery("s1"));
 assert(tracker.result("s1")==umaa::RemoteCommandResult::Pending);
 assert(tracker.disposed_ack("s1","vehicle"));
 assert(!tracker.release("s1"));
 assert(tracker.disposed_status("s1","vehicle"));
 assert(tracker.release("s1"));
 assert(tracker.size()==1);
 assert(tracker.status("s2","vehicle",umaa::State::ISSUED,21));
 assert(tracker.status("s2","vehicle",umaa::State::FAILED,22));
 assert(tracker.result("s2")==umaa::RemoteCommandResult::Pending);
 assert(tracker.ack("s2","vehicle",23));
 assert(tracker.result("s2")==umaa::RemoteCommandResult::Failed);
 tracker.issue({"operator","vehicle","s3",30});
 bool full=false;
 try{tracker.issue({"operator","vehicle","s4",40});}catch(const std::length_error&){full=true;}
 assert(full);
 assert(tracker.status("s3","vehicle",umaa::State::ISSUED,31));
 assert(!tracker.status("s3","vehicle",umaa::State::COMPLETED,32));
 assert(tracker.needs_recovery("s3"));
 tracker.ambiguous_delivery("s2");
 assert(tracker.result("s2")==umaa::RemoteCommandResult::Pending);
 std::cout<<"PASS correlated ACK/status, ordering, unknown delivery, bounded sessions\n";
}
