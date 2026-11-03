// Development transport: two independent physical DDS topics (Elements and
// Metadata) and an application-layer commit fence. NOT DDS GROUP coherence or
// official UMAA 6.0 TypeSupport. No partial revision is ever exposed.
#pragma once
#include "umaa/collection_transport.hpp"
#include "umaa/collection_frame_codec.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace umaa {
template<class Service> class SplitCollectionJoiner final {
public:
  using Value=typename Service::Value;
  using Batch=CoherentCollectionBatch<Value>;
  explicit SplitCollectionJoiner(std::size_t max=1024)
    :receiver_(Service::kind,max) {}
  void expect_snapshot(std::uint64_t generation) {
    receiver_.expect_snapshot(generation);
    expected_generation_=generation;
    next_sequence_=1;
    elements_.reset();metadata_.reset();
    elements_disposed_=metadata_disposed_=false;
    faulted_=disposed_=false;
  }
  void ingest_elements(Batch part) {stage(std::move(part),false);}
  void ingest_metadata(Batch part) {stage(std::move(part),true);}
  // A unilateral dispose hides the old snapshot immediately. Only both
  // explicit DISPOSE events confirm final retirement. UNREGISTER is not proof.
  void dispose_part(bool metadata) {
    if(faulted_||disposed_)throw std::logic_error("split collection not live");
    if(metadata) {
      if(metadata_disposed_)return fail("repeated Metadata Dispose");
      metadata_disposed_=true;
    } else {
      if(elements_disposed_)return fail("repeated Elements Dispose");
      elements_disposed_=true;
    }
    receiver_.fail_unknown_delivery();
    if(metadata_disposed_&&elements_disposed_)disposed_=true;
  }
  std::optional<std::vector<Value>> snapshot() const {
    if(faulted_||disposed_||elements_||metadata_||
       elements_disposed_||metadata_disposed_)return std::nullopt;
    return receiver_.snapshot();
  }
  std::uint64_t sequence() const noexcept {return receiver_.sequence();}
  bool disposed() const noexcept{return disposed_;}
  bool faulted() const noexcept{return faulted_;}
  bool pending() const noexcept {return elements_.has_value()||metadata_.has_value();}
private:
  [[noreturn]] void fail(const char* reason) {
    faulted_=true;receiver_.fail_unknown_delivery();
    throw std::runtime_error(reason);
  }
  static bool equal_metadata(const CollectionMetadata& a,const CollectionMetadata& b){
    return a.collection_id==b.collection_id && a.size==b.size &&
           a.update_element_id==b.update_element_id &&
           a.update_element_timestamp==b.update_element_timestamp &&
           a.starting_element_id==b.starting_element_id;
  }
  void stage(Batch part,bool is_metadata){
    if(faulted_||disposed_||elements_disposed_||metadata_disposed_ ||
       !expected_generation_||next_sequence_==0)
      fail("split collection requires trusted resynchronization");
    if(part.generation!=expected_generation_||part.sequence!=next_sequence_||
       part.metadata.collection_id!=Service::collection_id ||
       (is_metadata&&!part.operations.empty()))
      fail("split collection generation/sequence/type or Metadata frame mismatch");
    auto& slot=is_metadata?metadata_:elements_;
    if(slot)fail("duplicate split collection half");
    slot=std::move(part);
    if(!elements_||!metadata_)return;
    if(!equal_metadata(elements_->metadata,metadata_->metadata))
      fail("split Elements and Metadata disagree");
    try {
      // The existing coherent transaction receiver validates all element
      // mutations on a shadow copy before exposing the committed snapshot.
      if(next_sequence_==1)receiver_.apply_snapshot(*elements_);
      else receiver_.apply_delta(*elements_);
    }catch(...){faulted_=true;receiver_.fail_unknown_delivery();throw;}
    elements_.reset();metadata_.reset();
    if(next_sequence_==std::numeric_limits<std::uint64_t>::max())
      fail("split collection sequence exhausted");
    ++next_sequence_;
  }
  CoherentCollectionReceiver<Value> receiver_;
  std::optional<Batch> elements_,metadata_;
  std::uint64_t expected_generation_{},next_sequence_{};
  bool elements_disposed_{},metadata_disposed_{},disposed_{},faulted_{};
};
} // namespace umaa
