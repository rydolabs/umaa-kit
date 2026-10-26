// Input transport for keyed UMAA Command DDS Topics. The service host must
// validate source/destination/session/timestamps and dispatch command/dispose
// actions through BoundCommandProvider. This reader never guesses those roles.
#pragma once
#include "umaa/cyclone_sample_reader.hpp"

namespace umaa::cyclone {
template<class WireSample> using CommandReader = SampleReader<WireSample>;
} // namespace umaa::cyclone
