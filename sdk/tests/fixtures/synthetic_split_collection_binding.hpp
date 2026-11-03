// Synthetic split Elements/Metadata TypeBinding backed by the SAME fixture
// IDL type on two independent genuine DDS topics. Not official UMAA TypeSupport.
#pragma once
#include "fixtures/synthetic_collection_binding.hpp"
struct SyntheticSplitSet : SyntheticCollection {
  static constexpr const char* elements_topic="demo/CollectionElements";
  static constexpr const char* metadata_topic="demo/CollectionMetadata";
};
struct SyntheticSplitList : SyntheticList {
  static constexpr const char* elements_topic="demo/CollectionElements";
  static constexpr const char* metadata_topic="demo/CollectionMetadata";
};
