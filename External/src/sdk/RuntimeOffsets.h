#pragma once
#define Offsets BundledOffsets
#include "offsets.h"
#undef Offsets
#include <vector>

namespace Offsets {
inline std::string ClientVersion = BundledOffsets::ClientVersion;
#define OFFSET_FIELD(group, name) namespace group { inline std::uintptr_t name = BundledOffsets::group::name; }
#include "offset_fields.inc"
#undef OFFSET_FIELD
}

namespace OffsetCatalog {
struct Field { const char* name; std::uintptr_t* active; std::uintptr_t bundled; };
inline const std::vector<Field>& Fields() {
    static const std::vector<Field> fields = {
#define OFFSET_FIELD(group, name) {#group "." #name, &Offsets::group::name, BundledOffsets::group::name},
#include "offset_fields.inc"
#undef OFFSET_FIELD
    };
    return fields;
}
}
