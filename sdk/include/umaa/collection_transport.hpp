// Strict transport-neutral collection transaction boundary (C++17).
// The vendor DDS adapter MUST establish coherent+ordered delivery separately.
// This component does not claim official UMAA LargeCollection wire conformance.
#pragma once
#include "umaa/large_collection.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace umaa {
template<class T> struct CollectionOperation {
  enum class Kind { Upsert, Dispose } kind{Kind::Upsert};
  CollectionElement<T> element{};
  std::string disposed_id;
  std::int64_t dispose_timestamp{};
  static CollectionOperation upsert(CollectionElement<T> e) {
    CollectionOperation c; c.kind=Kind::Upsert;c.element=std::move(e);return c;
  }
  static CollectionOperation dispose(std::string id,std::int64_t timestamp) {
    CollectionOperation c;c.kind=Kind::Dispose;
    c.disposed_id=std::move(id);c.dispose_timestamp=timestamp;return c;
  }
};
template<class T> struct CoherentCollectionBatch {
  std::uint64_t generation{};
  std::uint64_t sequence{};
  CollectionMetadata metadata;
  std::vector<CollectionOperation<T>> operations;
};
template<class T> class CoherentCollectionReceiver final {
public:
  enum class State { Ready, ResyncRequired };
  explicit CoherentCollectionReceiver(CollectionKind kind,std::size_t maximum=10000)
    :kind_(kind),maximum_(maximum),assembler_(kind,maximum) {
    if(maximum==0)throw std::invalid_argument("collection memory cap must be positive");
  }
  // Start with an explicitly trusted FULL snapshot at sequence 1. Also
  // required after any gap, invalid transaction or unknown delivery result.
  void expect_snapshot(std::uint64_t generation) {
    if(generation==0 || generation<=last_generation_)
      throw std::invalid_argument("snapshot generation must strictly advance");
    assembler_=CollectionAssembler<T>(kind_,maximum_);
    generation_=0;sequence_=0;expected_generation_=generation;
    state_=State::Ready;
  }
  bool apply_snapshot(const CoherentCollectionBatch<T>& batch) {
    if(state_!=State::Ready || !expected_generation_ ||
       batch.generation!=expected_generation_ || batch.sequence!=1)
      throw std::logic_error("full snapshot generation/sequence mismatch");
    apply_batch(batch);
    expected_generation_=0;
    return true;
  }
  bool apply_delta(const CoherentCollectionBatch<T>& batch) {
    if(state_!=State::Ready || expected_generation_ || !generation_)
      throw std::logic_error("full snapshot required before deltas");
    apply_batch(batch);
    return true;
  }
  // Loss of coherent/ordered DDS delivery invalidates all prior snapshots.
  void fail_unknown_delivery() noexcept {state_=State::ResyncRequired;}
  State state() const noexcept{return state_;}
  std::uint64_t generation() const noexcept{return generation_;}
  std::uint64_t sequence() const noexcept{return sequence_;}
  std::optional<std::vector<T>> snapshot() const {
    if(state_!=State::Ready || expected_generation_)return std::nullopt;
    return assembler_.snapshot();
  }
private:
  void apply_batch(const CoherentCollectionBatch<T>& batch) {
    try {
      if(batch.generation==0 || batch.sequence==0 ||
         batch.metadata.collection_id.empty() ||
         batch.operations.size()>maximum_ ||
         (generation_ && (batch.generation!=generation_ ||
           sequence_==UINT64_MAX || batch.sequence!=sequence_+1)) ||
         (!generation_ && (batch.sequence!=1 ||
           batch.generation!=expected_generation_)))
        throw std::invalid_argument("incoherent collection sequence or resource bound");
      CollectionAssembler<T> candidate=assembler_;
      const auto before=candidate.commit_count();
      for(const auto& op:batch.operations) {
        if(op.kind==CollectionOperation<T>::Kind::Upsert) {
          if(!candidate.upsert(op.element))
            throw std::logic_error("stale collection sample");
        }else if(op.kind==CollectionOperation<T>::Kind::Dispose) {
          if(!candidate.dispose(op.disposed_id,op.dispose_timestamp))
            throw std::logic_error("stale collection disposal");
        }else throw std::invalid_argument("unknown collection operation");
      }
      candidate.metadata(batch.metadata);
      if(candidate.commit_count()!=before+1 || !candidate.snapshot())
        throw std::logic_error("incomplete atomic collection revision");
      assembler_=std::move(candidate);
      generation_=batch.generation;
      last_generation_=batch.generation;
      sequence_=batch.sequence;
    }catch(...){state_=State::ResyncRequired;throw;}
  }
  CollectionKind kind_;
  std::size_t maximum_;
  CollectionAssembler<T> assembler_;
  std::uint64_t generation_{};
  std::uint64_t sequence_{};
  std::uint64_t last_generation_{};
  std::uint64_t expected_generation_{};
  State state_{State::Ready};
};
} // namespace umaa
