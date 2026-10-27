// Bounded, official-type AnchorSpecs Report payload and lifecycle proof.
// The values are illustrative technical test data, not equipment ratings.
#include <dds/dds.h>
#include "UMAA/EO/AnchorSpecs/AnchorSpecsReportType.h"
#include "umaa/cyclone_service_context.hpp"
#include "umaa/cyclone_report_writer.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>

using Sample=UMAA_EO_AnchorSpecs_AnchorSpecsReportType;
static void ck(dds_return_t r, const char* op){
  if(r<0)throw std::runtime_error(std::string(op)+": "+dds_strretcode(-r));
}
static void uuid(uint8_t id[16],uint8_t seed){
  std::memset(id,0,16);id[0]=seed;id[6]=0x40;id[8]=0x80;id[15]=seed;
}
static void value(Sample& s, bool updated){
  uuid(s.source.id,0x45);
  s.timeStamp.seconds=static_cast<decltype(s.timeStamp.seconds)>(std::time(nullptr));
  s.timeStamp.nanoseconds=updated?2000:1000;
  s.anchorDescription=const_cast<char*>(updated?"Proof anchor revised":"Proof bow anchor");
  s.anchorSize=updated?175.0:125.0;
  s.anchorHoldingPower=updated?2900.0:2150.0;
  s.anchorHoldingPowerRatio=updated?16.571428:17.2;
  s.rodeLength=updated?115.0:85.5;
  s.rodeSize=0.016;
  s.rodeWorkingLoadLimit=updated?1600.0:1400.0;
}
int main(){
 try {
  umaa::cyclone::ServiceContext context("UMAA::EO::AnchorSpecs",
    umaa::cyclone::ServiceContext::ReviewMode::ExplicitPrototypeOnly);
  const auto topic=context.topic(
    "UMAA::EO::AnchorSpecs::AnchorSpecsReportType",
    "UMAA::EO::AnchorSpecs::AnchorSpecsReportType",
    umaa::reviewed::Role::Report,"AnchorSpecsReportType",
    &UMAA_EO_AnchorSpecs_AnchorSpecsReportType_desc);
  auto* qos=dds_create_qos(); if(!qos)throw std::runtime_error("qos");
  dds_qset_reliability(qos,DDS_RELIABILITY_RELIABLE,DDS_SECS(1));
  dds_qset_history(qos,DDS_HISTORY_KEEP_ALL,0);
  const auto writer=dds_create_writer(context.participant(),topic,qos,nullptr);
  dds_delete_qos(qos); ck(writer,"create writer");
  bool discovered=false;
  for(int i=0;i<360;++i){
    dds_publication_matched_status_t status{};
    ck(dds_get_publication_matched_status(writer,&status),"matched");
    if(status.current_count>0){discovered=true;break;}
    dds_sleepfor(DDS_MSECS(25));
  }
  if(!discovered)throw std::runtime_error("consumer not discovered");
  Sample sample{};
  value(sample,false);
  ck(dds_write(writer,&sample),"send first meaningful AnchorSpecs");
  ck(dds_wait_for_acks(writer,DDS_SECS(3)),"first reliable ack");
  dds_sleepfor(DDS_MSECS(75));
  value(sample,true);
  ck(dds_write(writer,&sample),"send updated meaningful AnchorSpecs");
  ck(dds_wait_for_acks(writer,DDS_SECS(3)),"updated reliable ack");
  dds_sleepfor(DDS_MSECS(100));
  ck(dds_dispose(writer,&sample),"dispose AnchorSpecs instance");
  ck(dds_wait_for_acks(writer,DDS_SECS(3)),"dispose reliable ack");
  dds_sleepfor(DDS_MSECS(1500));
  context.close();
  std::puts("ANCHOR_SPECS_HOST_PASS meaningful_fields=2 dispose=1");
  return 0;
 }catch(const std::exception& e){
  std::fprintf(stderr,"ANCHOR_SPECS_HOST_FAIL %s\n",e.what());return 2;
 }
}
