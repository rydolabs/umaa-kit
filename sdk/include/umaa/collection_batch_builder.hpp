// Deterministic bounded LargeSet/LargeList batch composer (C++17).
// This produces SDK transactions, NOT native DDS coherent/ordered QoS.
// A real DDS sender MUST use a reviewed coherent/ordered vendor binding.
#pragma once
#include "umaa/collection_transport.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
namespace umaa {
template<class T> class CollectionBatchBuilder final {
public:
 using Element=CollectionElement<T>;
 using Batch=CoherentCollectionBatch<T>;
 using Operation=CollectionOperation<T>;
 CollectionBatchBuilder(CollectionKind kind,std::string collection_id,
                        std::size_t maximum=10000)
    :kind_(kind),id_(std::move(collection_id)),maximum_(maximum){
   if(id_.empty()||!maximum_)throw std::invalid_argument("invalid collection configuration");
 }
 // Recovered snapshots require a strictly newer generation; no implicit
 // reinstatement of previous unfinished actions or payloads.
 Batch snapshot(std::uint64_t generation,const std::vector<Element>& elements) {
   if(!generation||generation<=last_generation_||elements.size()>maximum_)
     throw std::invalid_argument("invalid collection snapshot generation/size");
   auto staged=validate_elements(elements);
   const auto start=kind_==CollectionKind::List ?
       (elements.empty()?std::string():elements.front().element_id) : std::string();
   validate_order(staged,start);
   Batch out;out.generation=generation;out.sequence=1;
   out.metadata.collection_id=id_;out.metadata.size=staged.size();
   out.metadata.starting_element_id=start;
   for(const auto& item:elements)out.operations.push_back(Operation::upsert(item));
   if(!elements.empty()){
     out.metadata.update_element_id=elements.back().element_id;
     out.metadata.update_element_timestamp=elements.back().timestamp;
   }
   entries_=std::move(staged);start_=start;
   last_generation_=generation;sequence_=1;
   return out;
 }
 // A revision contains one or more changes and declares the final mutation.
 // No partial batch can be committed by the receiving transaction boundary.
 Batch revise(const std::vector<Operation>& changes,
              const std::string& list_start="") {
   if(!sequence_||changes.empty()||changes.size()>maximum_||
      sequence_==UINT64_MAX)throw std::invalid_argument("invalid collection revision");
   auto staged=entries_;
   for(const auto& op:changes) {
     if(op.kind==Operation::Kind::Upsert){
       if(op.element.element_id.empty())throw std::invalid_argument("empty element ID");
       const auto it=staged.find(op.element.element_id);
       if(it!=staged.end()&&it->second.timestamp>=op.element.timestamp)
         throw std::invalid_argument("stale element revision");
       staged.insert_or_assign(op.element.element_id,op.element);
     }else if(op.kind==Operation::Kind::Dispose){
       const auto it=staged.find(op.disposed_id);
       if(it==staged.end()||it->second.timestamp>=op.dispose_timestamp)
         throw std::invalid_argument("invalid disposal timestamp");
       staged.erase(it);
     }else throw std::invalid_argument("unknown collection mutation");
     if(staged.size()>maximum_)throw std::length_error("collection exceeds cap");
   }
   const auto start=kind_==CollectionKind::List?list_start:std::string();
   validate_order(staged,start);
   Batch out;out.generation=last_generation_;out.sequence=sequence_+1;
   out.operations=changes;
   out.metadata.collection_id=id_;out.metadata.size=staged.size();
   out.metadata.starting_element_id=start;
   const auto& last=changes.back();
   if(last.kind==Operation::Kind::Upsert){
     out.metadata.update_element_id=last.element.element_id;
     out.metadata.update_element_timestamp=last.element.timestamp;
   }else{
     out.metadata.update_element_id=last.disposed_id;
     out.metadata.update_element_timestamp=std::nullopt;
   }
   entries_=std::move(staged);start_=start;++sequence_;
   return out;
 }
 std::uint64_t generation() const noexcept{return last_generation_;}
 std::uint64_t sequence() const noexcept{return sequence_;}
private:
 std::unordered_map<std::string,Element> validate_elements(
     const std::vector<Element>& data) const {
   std::unordered_map<std::string,Element> staged;
   for(const auto& item:data){
     if(item.element_id.empty()||item.timestamp<=0||
        !staged.emplace(item.element_id,item).second)
       throw std::invalid_argument("invalid/duplicate element in snapshot");
   }
   return staged;
 }
 void validate_order(const std::unordered_map<std::string,Element>& entries,
                     const std::string& start) const {
   if(kind_!=CollectionKind::List)return;
   if(entries.empty()){
     if(!start.empty())throw std::invalid_argument("nonempty start for empty List");
     return;
   }
   if(start.empty())throw std::invalid_argument("List start required");
   std::unordered_set<std::string> visited;std::string next=start;
   while(!next.empty()){
     const auto it=entries.find(next);
     if(it==entries.end()||!visited.insert(next).second)
       throw std::invalid_argument("cyclic/dangling List link");
     next=it->second.next_id;
   }
   if(visited.size()!=entries.size())
     throw std::invalid_argument("unreachable List element");
 }
 CollectionKind kind_;
 std::string id_,start_;
 std::size_t maximum_;
 std::uint64_t last_generation_{},sequence_{};
 std::unordered_map<std::string,Element> entries_;
};
} // namespace umaa
