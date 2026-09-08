#include "models/amp_models.h"

#include "models/a2_lite.h"
#include "embedded_a2_data.h"

const char *AmpName(AmpId id) {
    switch (id) {
    case AmpId::Fender:
        return "Fender Twin65";
    case AmpId::Vox:
        return "Vox AC30 Chimey";
    case AmpId::Marshall:
        return "Marshall JCM800 G5";
    }
    return "invalid";
}

std::unique_ptr<nam::DSP> CreateAmpModel(AmpId id) {
    switch (id) {
    case AmpId::Fender:
        return std::make_unique<A2Lite>(embedded_a2::kWeights0);
    case AmpId::Vox:
        return std::make_unique<A2Lite>(embedded_a2::kWeights1);
    case AmpId::Marshall:
        return std::make_unique<A2Lite>(embedded_a2::kWeights2);
    }
    return nullptr;
}
