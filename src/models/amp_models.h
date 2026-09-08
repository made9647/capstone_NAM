#pragma once

#include <memory>

namespace nam {
class DSP;
}

// Values match the USB amp-selection commands.
enum class AmpId { Fender = 1, Vox = 2, Marshall = 3 };

const char *AmpName(AmpId id);
// Returns nullptr for an unknown amp. Construct only while audio is stopped.
std::unique_ptr<nam::DSP> CreateAmpModel(AmpId id);
