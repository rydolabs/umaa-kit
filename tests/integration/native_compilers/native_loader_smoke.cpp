#include <iostream>
#if defined(_WIN32)
#include <windows.h>
int main(int argc,char** argv){
  if(argc<2)return 2;
  for(int i=1;i<argc;++i){
    auto library=LoadLibraryA(argv[i]);
    if(!library){std::cerr<<"Native DDS TypeSupport DLL load failed: "<<argv[i]<<"\n";return 3;}
    FreeLibrary(library);
  }
  std::cout<<"NATIVE_VENDOR_TYPESUPPORT_LIBRARIES_LOADED count="<<(argc-1)<<"\n";
  return 0;
}
#else
#include <dlfcn.h>
int main(int argc,char** argv){
  if(argc<2)return 2;
  for(int i=1;i<argc;++i){
    auto library=dlopen(argv[i],RTLD_NOW|RTLD_LOCAL);
    if(!library){std::cerr<<"Native DDS TypeSupport SO load failed: "<<argv[i]
                        <<" "<<dlerror()<<"\n";return 3;}
    dlclose(library);
  }
  std::cout<<"NATIVE_VENDOR_TYPESUPPORT_LIBRARIES_LOADED count="<<(argc-1)<<"\n";
  return 0;
}
#endif
