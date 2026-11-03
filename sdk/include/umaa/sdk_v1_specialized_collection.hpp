// Composes the existing CollectionAssembler and SpecializationJoin. This
// does not pretend separate DDS Topics are a single atomic transaction.
#pragma once
#include "umaa/sdk_v1.hpp"
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
namespace umaa::sdk_v1 {
template<class Service,class Detail> class SpecializedCollectionConsumer final {
public:
  using General=typename ServiceTraits<Service>::Element;
  using Pair=typename SpecializationJoin<General,Detail>::Pair;
  explicit SpecializedCollectionConsumer(std::size_t maximum=10000)
      :collection_(maximum),join_(maximum) {
    if(!descriptor<Service>().has_specializations)
      throw std::invalid_argument("service has no specialization contract");
  }
  bool upsert(umaa::CollectionElement<General> element){
    return collection_.upsert(std::move(element));
  }
  bool dispose_element(const std::string& id,std::int64_t timestamp){
    return collection_.dispose(id,timestamp);
  }
  void metadata(umaa::CollectionMetadata metadata){
    collection_.metadata(std::move(metadata));
  }
  bool general(Guid128 id,std::int64_t time,General value){
    return join_.put_general(id,time,std::move(value));
  }
  bool specialization(Guid128 id,std::int64_t time,Detail value){
    return join_.put_specialization(id,time,std::move(value));
  }
  bool dispose_specialization(Guid128 id,std::int64_t time){
    return join_.dispose(id,time);
  }
  // The projector is an explicitly reviewed mapping from a general
  // element to the referenced specialization ID and revision.
  template<class Projector>
  std::optional<std::vector<Pair>> snapshot(Projector projector) const {
    return join_.resolve_collection(collection_.snapshot(),projector);
  }
  void reset_collection(){collection_.clear();}
  std::uint64_t collection_commits() const {return collection_.commit_count();}
  std::size_t tracked_specializations() const {return join_.tracked();}
private:
  CollectionConsumer<Service> collection_;
  SpecializationJoin<General,Detail> join_;
};
} // namespace umaa::sdk_v1
