// Optional isolated process hosting one Device Component ABI v2 module.
// Line framing is development-only IPC, NOT authenticated production RPC.
// The owning host never loads device .so/.dll into its address space.
#include "umaa/component_module.hpp"
#include "umaa/module_api_v2.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
int digit(char c) {
 if(c>='0'&&c<='9')return c-'0';
 if(c>='a'&&c<='f')return c-'a'+10;
 if(c>='A'&&c<='F')return c-'A'+10;
 return -1;
}
std::vector<std::uint8_t> decode(const std::string& hex) {
 if(hex.size()>524288||hex.size()%2)throw std::invalid_argument("bad payload size");
 std::vector<std::uint8_t> out;out.reserve(hex.size()/2);
 for(std::size_t i=0;i<hex.size();i+=2){
   const int a=digit(hex[i]),b=digit(hex[i+1]);
   if(a<0||b<0)throw std::invalid_argument("invalid payload hex");
   out.push_back(static_cast<std::uint8_t>((a<<4)|b));
 }
 return out;
}
std::string encode(const std::vector<std::uint8_t>& data) {
 static constexpr char h[]="0123456789abcdef";
 std::string out;out.reserve(data.size()*2);
 for(auto b:data){out.push_back(h[b>>4]);out.push_back(h[b&15]);}
 return out;
}
std::vector<std::string> fields(const std::string& line) {
 std::vector<std::string> out;std::size_t pos=0;
 while(true){
   auto end=line.find('\t',pos);
   out.push_back(line.substr(pos,end==std::string::npos?end:end-pos));
   if(end==std::string::npos)break;
   pos=end+1;
 }
 return out;
}
std::uint64_t number(const std::string& text){
 if(text.empty()||text.find_first_not_of("0123456789")!=std::string::npos)
   throw std::invalid_argument("bad integer");
 std::size_t used=0;
 const auto value=std::stoull(text,&used,10);
 if(used!=text.size())throw std::invalid_argument("bad integer");
 return static_cast<std::uint64_t>(value);
}
}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 try{
  umaa::ComponentModule module(argv[1]);
  std::cout<<"READY\n"<<std::flush;
  std::string line;bool faulted=false;
  while(std::getline(std::cin,line)){
   if(line.size()>1048576){std::cout<<"FAULT\tFRAME_TOO_LARGE\n";break;}
   try{
    const auto args=fields(line);
    if(args.size()==1&&args[0]=="PING"){
      std::cout<<"PONG\n";continue;
    }
#ifdef UMAA_WORKER_TEST_FAULT_INJECTION
    if(args.size()==1&&args[0]=="TEST_CRASH")
      std::_Exit(71); // isolated child crashes; NEVER a production command
#endif
    if(args.size()==1&&args[0]=="STOP"){
      if(module.stop()){std::cout<<"STOPPED\n";return 0;}
      std::cout<<"BUSY\n";continue;
    }
    if(faulted){
      std::cout<<"FAULT\tQUARANTINED\n";continue;
    }
    if(args.size()!=5||args[0]!="CALL"||args[1].empty())
      throw std::invalid_argument("invalid frame");
    const auto action=number(args[2]),session=number(args[3]);
    if(action<1||action>4 || action==4&&session==0)
      throw std::invalid_argument("invalid component action");
    const auto in=decode(args[4]);
    std::vector<std::uint8_t> out;
    std::int32_t result=-1;
    const auto rc=module.call(args[1],static_cast<std::uint32_t>(action),
                              in,out,result,session);
    std::cout<<"RESULT\t"<<rc<<"\t"<<result<<"\t"<<encode(out)<<"\n";
   }catch(const std::exception& e){
     // An exception may occur AFTER a device side effect. Fence this
     // worker, do not replay an ambiguous device Command.
     faulted=true;
     (void)e;
     std::cout<<"FAULT\tUNKNOWN_DELIVERY\n";
   }
  }
  return faulted?3:0;
 }catch(const std::exception&){std::cerr<<"component worker failed to start\n";return 4;}
}
