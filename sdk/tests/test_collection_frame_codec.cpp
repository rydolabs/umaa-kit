#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/collection_batch_builder.hpp"
#include "umaa/collection_frame_codec.hpp"
#include "fixtures/synthetic_collection_binding.hpp"
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>
template<class F>bool rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
 using C=umaa::CollectionFrameCodec<SyntheticCollection>;
 using O=umaa::CollectionOperation<int>;
 umaa::CollectionBatchBuilder<int> writer(umaa::CollectionKind::Set,"demo-collection",8);
 umaa::CoherentCollectionReceiver<int> reader(umaa::CollectionKind::Set,8);
 auto a=writer.snapshot(1,{{"a",1,10,""},{"b",2,20,""}});
 auto bytes=C::encode(a);auto decoded=C::decode(bytes.data(),bytes.size());
 reader.expect_snapshot(1);
 assert(reader.apply_snapshot(decoded)&&reader.snapshot()->size()==2);
 auto b=writer.revise({O::upsert({"a",3,13,""}),O::dispose("b",4)});
 bytes=C::encode(b);decoded=C::decode(bytes.data(),bytes.size());
 assert(reader.apply_delta(decoded)&&reader.snapshot()->at(0)==13);
 // An explicit gap never exposes a partial collection to user code.
 auto wrong=decoded;wrong.sequence=3;
 bytes=C::encode(wrong);
 assert(rejects([&]{(void)reader.apply_delta(C::decode(bytes.data(),bytes.size()));}));
 assert(!reader.snapshot());
 reader.expect_snapshot(2);
 auto empty=writer.snapshot(2,{});
 bytes=C::encode(empty);
 assert(reader.apply_snapshot(C::decode(bytes.data(),bytes.size())));
 assert(reader.snapshot()&&reader.snapshot()->empty());
 assert(rejects([&]{(void)C::decode(bytes.data(),bytes.size()-1);}));
 auto malformed=bytes;malformed[0]^=0xff;
 assert(rejects([&]{(void)C::decode(malformed.data(),malformed.size());}));
 malformed=bytes;malformed.push_back(0);
 assert(rejects([&]{(void)C::decode(malformed.data(),malformed.size());}));
 assert(rejects([&]{(void)umaa::CollectionFrameCodec<SyntheticList>::decode(bytes.data(),bytes.size());}));
 auto large=a;large.operations.resize(1025);
 assert(rejects([&]{(void)C::encode(large);}));
 umaa::CollectionBatchBuilder<int> list(umaa::CollectionKind::List,"demo-collection",8);
 auto lst=list.snapshot(1,{{"a",1,11,"b"},{"b",2,22,""}});
 auto packed=umaa::CollectionFrameCodec<SyntheticList>::encode(lst);
 auto restored=umaa::CollectionFrameCodec<SyntheticList>::decode(packed.data(),packed.size());
 umaa::CoherentCollectionReceiver<int> ordered(umaa::CollectionKind::List,8);
 ordered.expect_snapshot(1);
 assert(ordered.apply_snapshot(restored)&&ordered.snapshot()->at(1)==22);
 return 0;
}
