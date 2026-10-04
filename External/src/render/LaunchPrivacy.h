#pragma once
#include "../core/variables/variables.h"

namespace LaunchPrivacy {
inline bool streamproof = true;
inline bool hasLoaderChoice = false;
inline void Begin() { streamproof = true; hasLoaderChoice = true; }
inline void ApplyToSettings() {
    if (!hasLoaderChoice) return;
    variables::Misc::streamProof = streamproof;
    variables::Misc::streamKey = 0;
    variables::Misc::streamKeyMode = 2;
}
}
