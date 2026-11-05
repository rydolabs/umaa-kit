// Vendor-free C++17 ABI-v2 host preview codec negative/fuzz-boundary test.
// This is NOT a certification of original UMAA DDS CDR.
#include "umaa/official_battery_component_bridge.hpp"
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace b=umaa::official_battery;
static b::Guid guid(std::uint8_t v){b::Guid g{};g[0]=v;return g;}
static void check(bool value,const char* reason){
  if(!value)throw std::runtime_error(reason);
}
int main(){
  try{
    b::StableList v{};
    v.report.source.id=guid(0x33);
    v.report.source.parentID=guid(0x15);
    v.report.timeStamp={1700000123,456};
    v.report.state=b::State::ON;
    v.report.current=-3.25;
    v.report.chargeRemaining=42.5;
    v.report.energyUsageRate=0.5;
    v.report.hours=2.25;
    v.report.temp=28.5;
    v.report.voltage=25.0;
    auto& m=v.report.cellsListMetadata;
    m.listID=guid(0x11);
    m.updateElementID=guid(0x22);
    m.updateElementTimestamp=b::DateTime{1700001000,201};
    m.startingElementID=guid(0x21);
    m.size=2;
    b::ListElement a{};
    a.listID=m.listID;a.elementID=guid(0x21);
    a.nextElementID=guid(0x22);
    a.elementTimestamp={1700001000,100};
    a.element={-0.25,26.0,3.75};
    auto z=a;z.elementID=guid(0x22);z.nextElementID.reset();
    z.elementTimestamp={1700001000,201};
    z.element.voltage=4.25;
    v.ordered={a,z};v.generation=9;
    using Codec=b::BatteryStatusHostPreview;
    const auto bytes=Codec::encode_report(v);
    b::StableList out{};
    check(Codec::decode_report(bytes.data(),bytes.size(),out),
          "full synthetic host preview failed roundtrip");
    check(out.generation==9&&out.report.source.id==v.report.source.id&&
          out.report.source.parentID==v.report.source.parentID&&
          out.report.current==v.report.current&&
          out.report.chargeRemaining==v.report.chargeRemaining&&
          out.report.energyUsageRate==v.report.energyUsageRate&&
          out.report.hours==v.report.hours&&
          out.report.temp==v.report.temp&&
          out.report.voltage==v.report.voltage&&
          out.report.cellsListMetadata.updateElementTimestamp.has_value()&&
          out.report.cellsListMetadata.updateElementTimestamp->seconds==1700001000&&
          out.report.cellsListMetadata.updateElementTimestamp->nanoseconds==201&&
          out.ordered.size()==2&&out.ordered[1].element.voltage==4.25&&
          out.ordered[0].nextElementID==guid(0x22),
          "host preview silently dropped source/metadata/payload fields");
    for(std::size_t n=0;n<bytes.size();++n)
      check(!Codec::decode_report(bytes.data(),n,out),
            "truncated host ABI bytes accepted");
    auto forged=bytes;forged[3]=2;
    check(!Codec::decode_report(forged.data(),forged.size(),out),
          "incorrect host ABI version accepted");
    forged=bytes;forged.push_back(0);
    check(!Codec::decode_report(forged.data(),forged.size(),out),
          "appended bytes accepted");
    forged=bytes;forged[4]=0;
    check(!Codec::decode_report(forged.data(),forged.size(),out),
          "short Source identity unexpectedly accepted");
    // The full 16-octet Source would be NIL only if every octet is zero.
    for(int i=4;i<20;++i)forged[static_cast<std::size_t>(i)]=0;
    check(!Codec::decode_report(forged.data(),forged.size(),out),
          "NIL original Source accepted");
    check(!Codec::decode_report(nullptr,0,out),"null buffer accepted");
    // Every rejected encoding must fail before an ABI callback can
    // observe a partially valid snapshot. Restore the known-good fixture.
    const auto original=v;
    auto expect_bad=[&](const char* reason) {
      bool rejected=false;
      try {(void)Codec::encode_report(v);}
      catch(const std::invalid_argument&) {rejected=true;}
      check(rejected,reason);
      v=original;
    };
    v.report.state=static_cast<b::State>(999);
    expect_bad("unknown Battery state encoded");
    v.ordered[0].elementID={};
    expect_bad("NIL Battery List element encoded");
    v.ordered[1].elementID=v.ordered[0].elementID;
    expect_bad("duplicate Battery List element encoded");
    v.ordered[0].nextElementID=guid(0x77);
    expect_bad("broken Battery List chain encoded");
    v.ordered[0].nextElementID.reset();
    expect_bad("nonterminal missing successor encoded");
    v.ordered[1].nextElementID=guid(0x21);
    expect_bad("terminal Battery List cycle encoded");
    v.report.cellsListMetadata.startingElementID=guid(0x44);
    expect_bad("mismatched Battery List head encoded");
    v.ordered[0].elementTimestamp.nanoseconds=1000000000;
    expect_bad("invalid Battery List timestamp encoded");
    // The original record remains representable after all negative probes.
    check(Codec::encode_report(v)==bytes,"negative probes corrupted valid ABI encoding");
    v.nativeDdsCoherenceProved=true;
    bool refused=false;
    try{(void)Codec::encode_report(v);}catch(const std::invalid_argument&){refused=true;}
    check(refused,"host projection invented proven native DDS coherence");
    std::puts("UMAA_HOST_BATTERY_ABI_V2_VERSIONED_CODEC_PASS");
    return 0;
  }catch(const std::exception& e){
    std::fprintf(stderr,"Battery ABI host preview regression: %s\n",e.what());
    return 3;
  }
}
