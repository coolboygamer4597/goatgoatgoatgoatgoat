#pragma once
namespace SkinLibraryProfile {
#ifdef GOAT_SKIN_TEST_BUILD
inline constexpr bool Testing=true;
inline constexpr const wchar_t* CatalogPath=L"/coolboygamer4597/Ekte-lidenskap-har-et-navn/main/skin-catalog-test-20261025.json";
inline constexpr const char* TagPrefix="skins-test-";
#else
inline constexpr bool Testing=false;
inline constexpr const wchar_t* CatalogPath=L"/coolboygamer4597/Ekte-lidenskap-har-et-navn/main/skin-catalog-v4.json";
inline constexpr const char* TagPrefix="skins-";
#endif
}
