#include <iostream>
#include <string>
#if defined(_WIN32)
#include <windows.h>
int main(int argc,char** argv){
  if(argc!=3)return 2;
  for(int i=1;i<3;++i){
    auto module=LoadLibraryA(argv[i]);
    if(!module){std::cerr<<"Native TypeSupport DLL load failed: "<<argv[i]<<"\n";return 3;}
    FreeLibrary(module);
  }
  std::cout<<"NATIVE_VENDOR_TYPESUPPORT_LIBRARIES_LOADED\n";
  return 0;
}
#else
#include <dlfcn.h>
int main(int argc,char** argv){
  if(argc!=3)return 2;
  for(int i=1;i<3;++i){
    auto module=dlopen(argv[i],RTLD_NOW|RTLD_LOCAL);
    if(!module){std::cerr<<"Native TypeSupport SO load failed: "<<argv[i]
                        <<" "<<dlerror()<<"\n";return 3;}
    dlclose(module);
  }
  std::cout<<"NATIVE_VENDOR_TYPESUPPORT_LIBRARIES_LOADED\n";
  return 0;
}
#endif
