#pragma once
#include "RuntimePaths.h"
#include "CustomModelData.h"
#include <array>
#include <algorithm>
#include <fstream>
#include <string>
namespace CustomTextureData {
enum class Channel {Color,Material,Normal};
struct Preset {const char* name;const char* skin;int model;int groups;std::array<const wchar_t*,4> stems;const wchar_t* modelFile=nullptr;int category=0;const wchar_t* nativeFile=nullptr;};
inline constexpr std::array<Preset,231> Presets{{
    {"Bloodsport","X5",0,1,{L"ak-bloodsport"},nullptr,0},
    {"Asiimov","X5",0,1,{L"ak-asiimov"},nullptr,0},
    {"Crane Flight","X5",0,1,{L"ak-crane-flight"},nullptr,0},
    {"Printstream","Fade",1,3,{L"m4-printstream-receiver",L"m4-printstream-barrel",L"m4-printstream-stock"},nullptr,0},
    {"Asiimov","Fade",1,3,{L"m4-asiimov-receiver",L"m4-asiimov-barrel",L"m4-asiimov-stock"},nullptr,0},
    {"Inheritance","X5",0,1,{L"ak-inheritance"},nullptr,0},
    {"Nouveau Rouge","X5",0,1,{L"ak-nouveau-rouge"},nullptr,0},
    {"The Oligarch","X5",0,1,{L"ak-the-oligarch"},nullptr,0},
    {"Chantico's Fire","Fade",1,3,{L"m4-chanticos-fire-receiver",L"m4-chanticos-fire-barrel",L"m4-chanticos-fire-stock"},nullptr,0},
    {"Mecha Industries","Fade",1,3,{L"m4-mecha-industries-receiver",L"m4-mecha-industries-barrel",L"m4-mecha-industries-stock"},nullptr,0},
    {"Stratosphere","Fade",1,3,{L"m4-stratosphere-receiver",L"m4-stratosphere-barrel",L"m4-stratosphere-stock"},nullptr,0},
    {"Wash me plz","Fade",1,3,{L"m4-wash-me-plz-receiver",L"m4-wash-me-plz-barrel",L"m4-wash-me-plz-stock"},nullptr,0},
    {"AWP Printstream","Fade",1,1,{L"native-awp-printstream"},L"host-fade-1-cs-awp-hd-preview.gwm",2,L"host-fade-1-cs-awp-hd-native.gwm"},
    {"AWP Lightning Strike","Fade",1,1,{L"native-awp-lightning-strike"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"AWP Desert Hydra","Fade",1,1,{L"native-awp-desert-hydra"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"AWP Gungnir","Fade",1,1,{L"native-awp-gungnir"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"AWP Queen's Gambit","Fade",1,1,{L"native-awp-queen-s-gambit"},L"host-fade-1-cs-awp-hd-preview.gwm",2,L"host-fade-1-cs-awp-hd-native.gwm"},
    {"Blaze","Gold",3,1,{L"deagle-blaze"},nullptr,0},
    {"Blue Ply","Gold",3,1,{L"deagle-blue-ply"},nullptr,0},
    {"Cobalt Disruption","Gold",3,1,{L"deagle-cobalt-disruption"},nullptr,0},
    {"Eastern Enigma","Gold",3,1,{L"deagle-eastern-enigma"},nullptr,0},
    {"Emerald J\u00f6rmungandr","Gold",3,1,{L"deagle-emerald-j-rmungandr"},nullptr,0},
    {"Firebreathing","Gold",3,1,{L"deagle-firebreathing"},nullptr,0},
    {"Oxide Blaze","Gold",3,1,{L"deagle-oxide-blaze"},nullptr,0},
    {"Printstream","Gold",3,1,{L"deagle-printstream"},nullptr,0},
    {"The Daily Deagle","Gold",3,1,{L"deagle-the-daily-deagle"},nullptr,0},
    {"Trigger Discipline","Gold",3,1,{L"deagle-trigger-discipline"},nullptr,0},
    {"Franklin","Fade",4,4,{L"glock-franklin",L"glock-franklin-slide",L"glock-franklin-mag",L"glock-franklin-barrel"},nullptr,0},
    {"Fully Tuned","Fade",4,4,{L"glock-fully-tuned",L"glock-fully-tuned-slide",L"glock-fully-tuned-mag",L"glock-fully-tuned-barrel"},nullptr,0},
    {"Gold Toof","Fade",4,4,{L"glock-gold-toof",L"glock-gold-toof-slide",L"glock-gold-toof-mag",L"glock-gold-toof-barrel"},nullptr,0},
    {"Moonrise","Fade",4,4,{L"glock-moonrise",L"glock-moonrise-slide",L"glock-moonrise-mag",L"glock-moonrise-barrel"},nullptr,0},
    {"Ramese's Reach","Fade",4,4,{L"glock-ramese-s-reach",L"glock-ramese-s-reach-slide",L"glock-ramese-s-reach-mag",L"glock-ramese-s-reach-barrel"},nullptr,0},
    {"Trace Lock","Fade",4,4,{L"glock-trace-lock",L"glock-trace-lock-slide",L"glock-trace-lock-mag",L"glock-trace-lock-barrel"},nullptr,0},
    {"Umbral Rabbit","Fade",4,4,{L"glock-umbral-rabbit",L"glock-umbral-rabbit-slide",L"glock-umbral-rabbit-mag",L"glock-umbral-rabbit-barrel"},nullptr,0},
    {"Vogue","Fade",4,4,{L"glock-vogue",L"glock-vogue-slide",L"glock-vogue-mag",L"glock-vogue-barrel"},nullptr,0},
    {"Water Elemental","Fade",4,4,{L"glock-water-elemental",L"glock-water-elemental-slide",L"glock-water-elemental-mag",L"glock-water-elemental-barrel"},nullptr,0},
    {"Consequence of the Jinn","X5",0,1,{L"ak-consequence-of-the-jinn"},nullptr,0},
    {"Frontside Misty","X5",0,1,{L"ak-frontside-misty"},nullptr,0},
    {"Gold Arabesque","X5",0,1,{L"ak-gold-arabesque"},nullptr,0},
    {"Legion of Anubis","X5",0,1,{L"ak-legion-of-anubis"},nullptr,0},
    {"Neon Revolution","X5",0,1,{L"ak-neon-revolution"},nullptr,0},
    {"Point Disarray","X5",0,1,{L"ak-point-disarray"},nullptr,0},
    {"Rat Rod","X5",0,1,{L"ak-rat-rod"},nullptr,0},
    {"Redline","X5",0,1,{L"ak-redline"},nullptr,0},
    {"Safety Net","X5",0,1,{L"ak-safety-net"},nullptr,0},
    {"Searing Rage","X5",0,1,{L"ak-searing-rage"},nullptr,0},
    {"The Empress","X5",0,1,{L"ak-the-empress"},nullptr,0},
    {"X-Ray","X5",0,1,{L"ak-x-ray"},nullptr,0},
    {"AWP Atheris","Fade",1,1,{L"native-awp-atheris"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"AWP Black Nile","Fade",1,1,{L"native-awp-black-nile"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"AWP Crakow!","Fade",1,1,{L"native-awp-crakow"},L"host-fade-1-cs-awp-hd-preview.gwm",2,L"host-fade-1-cs-awp-hd-native.gwm"},
    {"AWP Exothermic","Fade",1,1,{L"native-awp-exothermic"},L"host-fade-1-cs-awp-hd-preview.gwm",2,L"host-fade-1-cs-awp-hd-native.gwm"},
    {"AWP LongDog","Fade",1,1,{L"native-awp-longdog"},L"host-fade-1-cs-awp-hd-preview.gwm",2,L"host-fade-1-cs-awp-hd-native.gwm"},
    {"AWP Medusa","Fade",1,1,{L"native-awp-medusa"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"AWP Oni Taiji","Fade",1,1,{L"native-awp-oni-taiji"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"AWP Sovereign Flame","Fade",1,1,{L"native-awp-sovereign-flame"},L"host-fade-1-cs-awp-hd-preview.gwm",2,L"host-fade-1-cs-awp-hd-native.gwm"},
    {"AWP The End","Fade",1,1,{L"native-awp-the-end"},L"host-fade-1-cs-awp-hd-preview.gwm",2,L"host-fade-1-cs-awp-hd-native.gwm"},
    {"AWP The Prince","Fade",1,1,{L"native-awp-the-prince"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"AWP Wildfire","Fade",1,1,{L"native-awp-wildfire"},L"host-fade-1-cs-awp-legacy-preview.gwm",2,L"host-fade-1-cs-awp-legacy-native.gwm"},
    {"Decimator","Fade",1,3,{L"m4-decimator-receiver",L"m4-decimator-barrel",L"m4-decimator-stock"},nullptr,0},
    {"Flashback","Fade",1,3,{L"m4-flashback-receiver",L"m4-flashback-barrel",L"m4-flashback-stock"},nullptr,0},
    {"Guardian","Fade",1,3,{L"m4-guardian-receiver",L"m4-guardian-barrel",L"m4-guardian-stock"},nullptr,0},
    {"Hot Rod","Fade",1,3,{L"m4-hot-rod-receiver",L"m4-hot-rod-barrel",L"m4-hot-rod-stock"},nullptr,0},
    {"Hyper Beast","Fade",1,3,{L"m4-hyper-beast-receiver",L"m4-hyper-beast-barrel",L"m4-hyper-beast-stock"},nullptr,0},
    {"Imminent Danger","Fade",1,3,{L"m4-imminent-danger-receiver",L"m4-imminent-danger-barrel",L"m4-imminent-danger-stock"},nullptr,0},
    {"Knight","Fade",1,3,{L"m4-knight-receiver",L"m4-knight-barrel",L"m4-knight-stock"},nullptr,0},
    {"Leaded Glass","Fade",1,3,{L"m4-leaded-glass-receiver",L"m4-leaded-glass-barrel",L"m4-leaded-glass-stock"},nullptr,0},
    {"Master Piece","Fade",1,3,{L"m4-master-piece-receiver",L"m4-master-piece-barrel",L"m4-master-piece-stock"},nullptr,0},
    {"Night Terror","Fade",1,3,{L"m4-night-terror-receiver",L"m4-night-terror-barrel",L"m4-night-terror-stock"},nullptr,0},
    {"Player Two","Fade",1,3,{L"m4-player-two-receiver",L"m4-player-two-barrel",L"m4-player-two-stock"},nullptr,0},
    {"Solitude","Fade",1,3,{L"m4-solitude-receiver",L"m4-solitude-barrel",L"m4-solitude-stock"},nullptr,0},
    {"Vaporwave","Fade",1,3,{L"m4-vaporwave-receiver",L"m4-vaporwave-barrel",L"m4-vaporwave-stock"},nullptr,0},
    {"VariCamo","Fade",1,3,{L"m4-varicamo-receiver",L"m4-varicamo-barrel",L"m4-varicamo-stock"},nullptr,0},
    {"Cirno MK14","0416",2,4,{L"other-cirno-mk14-body",L"other-cirno-mk14-scope",L"other-cirno-mk14-mag",L"other-cirno-mk14-bolt"},nullptr,1},
    {"Nekoha Shizuku","X5",0,1,{L"other-nekoha-shizuku"},nullptr,1},
    {"Banshee","X5",0,1,{L"other-banshee"},nullptr,1},
    {"Vanilla NEKOPARA","0416",2,4,{L"other-vanilla-nekopara-body",L"other-vanilla-nekopara-scope",L"other-vanilla-nekopara-mag",L"other-vanilla-nekopara-bolt"},nullptr,1},
    {"Blood Splatter Camo","Fade",1,3,{L"other-blood-splatter-camo-receiver",L"other-blood-splatter-camo-barrel",L"other-blood-splatter-camo-stock"},nullptr,1},
    {"Nahida MK110","Fade",1,3,{L"other-nahida-mk110-receiver",L"other-nahida-mk110-barrel",L"other-nahida-mk110-stock"},nullptr,1},
    {"Sakuya M4A4","Fade",1,3,{L"other-sakuya-m4a4-receiver",L"other-sakuya-m4a4-barrel",L"other-sakuya-m4a4-stock"},nullptr,1},
    {"Senko","Fade",4,4,{L"other-senko-glock",L"other-senko-glock-slide",L"other-senko-glock-mag",L"other-senko-glock-barrel"},nullptr,1},
    {"Gawr Gura M4","Fade",1,3,{L"other-gawr-gura-m4-receiver",L"other-gawr-gura-m4-barrel",L"other-gawr-gura-m4-stock"},nullptr,1},
    {"Hot Blood AK74","X5",0,1,{L"other-hot-blood-ak74"},nullptr,1},
    {"Yakumo Yukari","X5",0,1,{L"other-yakumo-yukari"},nullptr,1},
    {"Skadi M4A1-S","Fade",1,3,{L"other-skadi-m4a1s-receiver",L"other-skadi-m4a1s-barrel",L"other-skadi-m4a1s-stock"},nullptr,1},
    {"Reimu M4A1-S","Fade",1,3,{L"other-reimu-m4a1s-receiver",L"other-reimu-m4a1s-barrel",L"other-reimu-m4a1s-stock"},nullptr,1},
    {"Gray Raven XXI M4A1","Fade",1,3,{L"other-gray-raven-m4a1-receiver",L"other-gray-raven-m4a1-barrel",L"other-gray-raven-m4a1-stock"},nullptr,1},
    {"Rem & Ram M4A1-S","Fade",1,3,{L"other-rem-ram-m4a1s-receiver",L"other-rem-ram-m4a1s-barrel",L"other-rem-ram-m4a1s-stock"},nullptr,1},
    {"Kokoro Amamiya M4A1-S","Fade",1,3,{L"other-kokoro-m4a1s-receiver",L"other-kokoro-m4a1s-barrel",L"other-kokoro-m4a1s-stock"},nullptr,1},
    {"Sweet Cat Ear M4A1-S","Fade",1,3,{L"other-sweet-cat-m4a1s-receiver",L"other-sweet-cat-m4a1s-barrel",L"other-sweet-cat-m4a1s-stock"},nullptr,1},
    {"Enterprise M4A1-S","Fade",1,3,{L"other-enterprise-m4a1s-receiver",L"other-enterprise-m4a1s-barrel",L"other-enterprise-m4a1s-stock"},nullptr,1},
    {"Chloe M4A1-S","Fade",1,3,{L"other-chloe-m4a1s-receiver",L"other-chloe-m4a1s-barrel",L"other-chloe-m4a1s-stock"},nullptr,1},
    {"Nekoha Shizuku M4A1","Fade",1,3,{L"other-nekoha-m4a1-receiver",L"other-nekoha-m4a1-barrel",L"other-nekoha-m4a1-stock"},nullptr,1},
    {"Enterprise White M4A1-S","Fade",1,3,{L"other-enterprise-white-m4a1s-receiver",L"other-enterprise-white-m4a1s-barrel",L"other-enterprise-white-m4a1s-stock"},nullptr,1},
    {"M4 Bloodsport","Fade",1,3,{L"cs-m4-bloodsport-receiver",L"cs-m4-bloodsport-barrel",L"cs-m4-bloodsport-stock"},nullptr,0},
    {"Nailuo M4A1-S","Fade",1,3,{L"other-nailuo-m4a1s-receiver",L"other-nailuo-m4a1s-barrel",L"other-nailuo-m4a1s-stock"},nullptr,1},
    {"Graf Zeppelin M4A1-S","Fade",1,3,{L"other-graf-zeppelin-m4a1s-receiver",L"other-graf-zeppelin-m4a1s-barrel",L"other-graf-zeppelin-m4a1s-stock"},nullptr,1},
    {"Ryo AK-47","X5",0,1,{L"other-ryo-ak47"},nullptr,1},
    {"Saratoga AK-47","X5",0,1,{L"mapped2-98-0"},L"mapped-template-0.gwm",1},
    {"Koneko","X5",0,1,{L"other-koneko-ak"},nullptr,1},
    {"Koneko","Fade",1,3,{L"other-koneko-m4-receiver",L"other-koneko-m4-barrel",L"other-koneko-m4-stock"},nullptr,1},
    {"Karin AK-47","X5",0,1,{L"other-karin-ak47"},nullptr,1},
    {"Hibiki AK-12","X5",0,1,{L"other-hibiki-ak12"},nullptr,1},
    {"Yoimiya AK-47","X5",0,1,{L"other-yoimiya-ak47"},nullptr,1},
    {"Azhe AKM","X5",0,1,{L"mapped2-104-0"},L"mapped-template-0.gwm",1},
    {"Cherry Blossom AK-47","X5",0,1,{L"other-cherry-blossom-ak47"},nullptr,1},
    {"Minato Aqua AK-47","X5",0,1,{L"other-minato-aqua-ak47"},nullptr,1},
    {"Hellfire AK-47","X5",0,1,{L"other-hellfire-ak47"},nullptr,1},
    {"Wastelander","X5",0,4,{L"wastelander-body",L"wastelander-sight",L"wastelander-lens",L"wastelander-dot"},L"wastelander-preview.gwm",2,L"wastelander-native.gwm"},
    {"Kafka SCAR-H","Man",6,3,{L"native-scar-kafka-0",L"native-scar-kafka-1",L"native-scar-kafka-2"},L"native-scar-kafka-preview.gwm",2,L"native-scar-kafka-native.gwm"},
    {"Miyako SCAR","Man",6,4,{L"native-scar-miyako-0",L"native-scar-miyako-1",L"native-scar-miyako-2",L"native-scar-miyako-3"},L"native-scar-miyako-preview.gwm",2,L"native-scar-miyako-native.gwm"},
    {"Harukaze SCAR","Man",6,3,{L"native-scar-harukaze-0",L"native-scar-harukaze-1",L"native-scar-harukaze-2"},L"native-scar-harukaze-preview.gwm",2,L"native-scar-harukaze-native.gwm"},
    {"Printstream SCAR","Man",6,1,{L"mapped2-112-0"},L"mapped-template-6.gwm",0},
    {"Among Us SCAR","Man",6,1,{L"native-scar-among-us-0"},L"native-scar-among-us-preview.gwm",2,L"native-scar-among-us-native.gwm"},
    {"Yuni SCAR","Man",6,2,{L"native-scar-yuni-0",L"native-scar-yuni-1"},L"native-scar-yuni-preview.gwm",2,L"native-scar-yuni-native.gwm"},
    {"Arubis M4A1","Fade",1,3,{L"mapped2-115-0",L"mapped2-115-1",L"mapped2-115-2"},L"mapped-template-1.gwm",1},
    {"RE3 USP9 (Anime Finish)","Smaug",7,4,{L"mapped2-116-0",L"mapped2-116-1",L"mapped2-116-2",L"mapped2-116-3"},L"mapped-template-7.gwm",1},
    {"Amagi AWP","Fade",1,1,{L"library-aa693301670e-0"},L"host-fade-1-library-aa693301670e-preview.gwm",2,L"host-fade-1-library-aa693301670e-native.gwm"},
    {"King\u2019s Heart AK-47","X5",0,1,{L"library-d543dd8accdf-0"},L"library-d543dd8accdf-preview.gwm",2,L"library-d543dd8accdf-native.gwm"},
    {"ak dragon","X5",0,1,{L"library-6bdc5a005bbd-0"},L"library-6bdc5a005bbd-preview.gwm",2,L"library-6bdc5a005bbd-native.gwm"},
    {"Bazaar Bargain","Fade",1,1,{L"library-ccc2e0cc293d-0"},L"host-fade-1-library-ccc2e0cc293d-preview.gwm",2,L"host-fade-1-library-ccc2e0cc293d-native.gwm"},
    {"Brass Beast","X5",0,1,{L"library-4bcf5359b493-0"},L"library-4bcf5359b493-preview.gwm",2,L"library-4bcf5359b493-native.gwm"},
    {"Custom Tactical SCAR","Man",6,1,{L"library-9249a806db2c-0"},L"library-9249a806db2c-preview.gwm",2,L"library-9249a806db2c-native.gwm"},
    {"Fable Parasite","X5",0,2,{L"library-254f01c2f263-0",L"library-254f01c2f263-1"},L"library-254f01c2f263-preview.gwm",2,L"library-254f01c2f263-native.gwm"},
    {"Fable S5 Black","X5",0,2,{L"library-570d314defb1-0",L"library-570d314defb1-1"},L"library-570d314defb1-preview.gwm",2,L"library-570d314defb1-native.gwm"},
    {"Golden AWP","Fade",1,1,{L"library-d7ed131bca2a-0"},L"host-fade-1-library-d7ed131bca2a-preview.gwm",2,L"host-fade-1-library-d7ed131bca2a-native.gwm"},
    {"Golden SCAR-20","Man",6,1,{L"library-5cb6b4011a89-0"},L"library-5cb6b4011a89-preview.gwm",2,L"library-5cb6b4011a89-native.gwm"},
    {"Golden USP-S","Smaug",7,4,{L"mapped2-127-0",L"mapped2-127-1",L"mapped2-127-2",L"mapped2-127-3"},L"mapped-template-7.gwm",1},
    {"Neptunia Green heart scar","Man",6,2,{L"library-f0ea1d041626-0",L"library-f0ea1d041626-1"},L"library-f0ea1d041626-preview.gwm",2,L"library-f0ea1d041626-native.gwm"},
    {"[AK] Kalashnibren","Man",6,1,{L"library-ae157e948520-0"},L"library-ae157e948520-preview.gwm",2,L"library-ae157e948520-native.gwm"},
    {"USP-S CYBORG KILL CONFIRMED","Smaug",7,4,{L"mapped2-130-0",L"mapped2-130-1",L"mapped2-130-2",L"mapped2-130-3"},L"mapped-template-7.gwm",1},
    {"X-Ray Dark Metal AK","X5",0,2,{L"library-0367aa753942-0",L"library-0367aa753942-1"},L"library-0367aa753942-preview.gwm",2,L"library-0367aa753942-native.gwm"},
    {"AWP 02","Fade",1,1,{L"library-64f6d65b49c8-0"},L"host-fade-1-library-64f6d65b49c8-preview.gwm",2,L"host-fade-1-library-64f6d65b49c8-native.gwm"},
    {"Alice .44 Magnum","Opulent",9,1,{L"mapped2-133-0"},L"mapped-template-9.gwm",1},
    {"CFHD Monster AK-47","X5",0,1,{L"library-9ec9613f1563-0"},L"library-9ec9613f1563-preview.gwm",2,L"library-9ec9613f1563-native.gwm"},
    {"[Swarovski AK47]","X5",0,1,{L"mapped2-135-0"},L"mapped-template-0.gwm",1},
    {"Killing Floor 2 AK-12","X5",0,1,{L"mapped2-136-0"},L"mapped-template-0.gwm",1},
    {"Vulcan AK-47","X5",0,1,{L"mapped2-137-0"},L"mapped-template-0.gwm",0},
    {"Classic Prestige AK-47","X5",0,1,{L"mapped2-138-0"},L"mapped-template-0.gwm",1},
    {"AK-47 Black Snake from A.V.A","X5",0,1,{L"library-0bf622036668-0"},L"library-0bf622036668-preview.gwm",2,L"library-0bf622036668-native.gwm"},
    {"AK-47 Gold Snake from A.V.A","X5",0,1,{L"library-e0b38f1d1322-0"},L"library-e0b38f1d1322-preview.gwm",2,L"library-e0b38f1d1322-native.gwm"},
    {"AK-47 Custom Finish","X5",0,1,{L"mapped2-141-0"},L"mapped-template-0.gwm",1},
    {"AK-47 White Snake from A.V.A","X5",0,1,{L"library-c163acb9b4b5-0"},L"library-c163acb9b4b5-preview.gwm",2,L"library-c163acb9b4b5-native.gwm"},
    {"Hinata AWP","Fade",1,1,{L"library-5810d72ebfd1-0"},L"host-fade-1-library-5810d72ebfd1-preview.gwm",2,L"host-fade-1-library-5810d72ebfd1-native.gwm"},
    {"Minato Aqua AWP","Fade",1,1,{L"library-404e56316715-0"},L"host-fade-1-library-404e56316715-preview.gwm",2,L"host-fade-1-library-404e56316715-native.gwm"},
    {"AWP","Fade",1,1,{L"library-c9b2e9432a0e-0"},L"host-fade-1-library-c9b2e9432a0e-preview.gwm",2,L"host-fade-1-library-c9b2e9432a0e-native.gwm"},
    {"Arknights - Bibeak FN SCAR-17 skin (scar) glow","Man",6,2,{L"library-110517bf8589-0",L"library-110517bf8589-1"},L"library-110517bf8589-preview.gwm",2,L"library-110517bf8589-native.gwm"},
    {"usp-s Eagle using Arby animation","Smaug",7,4,{L"mapped2-147-0",L"mapped2-147-1",L"mapped2-147-2",L"mapped2-147-3"},L"mapped-template-7.gwm",1},
    {"Sapientia Revolver","Warden",8,2,{L"mapped2-148-0",L"mapped2-148-1"},L"mapped-template-8.gwm",1},
    {"AWP |Formidable Theme","Fade",1,1,{L"library-7f61269b1c80-0"},L"host-fade-1-library-7f61269b1c80-preview.gwm",2,L"host-fade-1-library-7f61269b1c80-native.gwm"},
    {"AWP Bloodsport","Fade",1,1,{L"library-798cc71dc5e2-0"},L"host-fade-1-library-798cc71dc5e2-preview.gwm",2,L"host-fade-1-library-798cc71dc5e2-native.gwm"},
    {"USP-S Tarot","Smaug",7,4,{L"mapped2-151-0",L"mapped2-151-1",L"mapped2-151-2",L"mapped2-151-3"},L"mapped-template-7.gwm",1},
    {"Mona AWP","Fade",1,1,{L"library-ee1fb496e3f1-0"},L"host-fade-1-library-ee1fb496e3f1-preview.gwm",2,L"host-fade-1-library-ee1fb496e3f1-native.gwm"},
    {"Royalty Rose AK-47","X5",0,1,{L"mapped2-153-0"},L"mapped-template-0.gwm",1},
    {"pizza ak47","X5",0,1,{L"mapped2-154-0"},L"mapped-template-0.gwm",1},
    {"R8 Revolver | Junk Yard","Opulent",9,1,{L"mapped2-155-0"},L"mapped-template-9.gwm",0},
    {"Golden Iron Beast AK-47","X5",0,1,{L"library-da94502166bc-0"},L"library-da94502166bc-preview.gwm",2,L"library-da94502166bc-native.gwm"},
    {"Kal'tsit SCAR-L","Man",6,1,{L"library-6b947b572e7a-0"},L"library-6b947b572e7a-preview.gwm",2,L"library-6b947b572e7a-native.gwm"},
    {"Ganyu M16A4","Fade",1,3,{L"mapped2-158-0",L"mapped2-158-1",L"mapped2-158-2"},L"mapped-template-1.gwm",1},
    {"Iron Beast AK-47","X5",0,1,{L"library-a530aa4c2050-0"},L"library-a530aa4c2050-preview.gwm",2,L"library-a530aa4c2050-native.gwm"},
    {"NMW2 AK Battleworn","X5",0,1,{L"mapped2-160-0"},L"mapped-template-0.gwm",1},
    {"MW2023-RAM9(SCAR) HATSUNE MIKU","Man",6,2,{L"library-a4471f94f0b1-0",L"library-a4471f94f0b1-1"},L"library-a4471f94f0b1-preview.gwm",2,L"library-a4471f94f0b1-native.gwm"},
    {"Sakuya Izayoi USP-S","Smaug",7,4,{L"mapped2-162-0",L"mapped2-162-1",L"mapped2-162-2",L"mapped2-162-3"},L"mapped-template-7.gwm",1},
    {"saratoga usp45","Smaug",7,4,{L"mapped2-163-0",L"mapped2-163-1",L"mapped2-163-2",L"mapped2-163-3"},L"mapped-template-7.gwm",1},
    {"SCAR-H Custom","Man",6,2,{L"library-60dadb929a5f-0",L"library-60dadb929a5f-1"},L"library-60dadb929a5f-preview.gwm",2,L"library-60dadb929a5f-native.gwm"},
    {"Armas Evolutiva SCAR - Megalod\u00f3n Alfa Free Fire","Man",6,1,{L"library-06dbfb9ec581-0"},L"library-06dbfb9ec581-preview.gwm",2,L"library-06dbfb9ec581-native.gwm"},
    {"Reed SCAR","Man",6,1,{L"library-c689a66da04e-0"},L"library-c689a66da04e-preview.gwm",2,L"library-c689a66da04e-native.gwm"},
    {"Hatsune Miku USP-S","Smaug",7,4,{L"mapped2-167-0",L"mapped2-167-1",L"mapped2-167-2",L"mapped2-167-3"},L"mapped-template-7.gwm",1},
    {"Silenced HK USP.40 Shiroko (ChaichanaThai66 Anim)","Smaug",7,4,{L"mapped2-168-0",L"mapped2-168-1",L"mapped2-168-2",L"mapped2-168-3"},L"mapped-template-7.gwm",1},
    {"Alice Mononobe Pistol","Gold",3,1,{L"mapped2-169-0"},L"native-deagle.gwm",1},
    {"USP-S CORTEX","Smaug",7,4,{L"mapped2-170-0",L"mapped2-170-1",L"mapped2-170-2",L"mapped2-170-3"},L"mapped-template-7.gwm",1},
    {"BEATLESS Crimson AK-47","X5",0,1,{L"mapped2-171-0"},L"mapped-template-0.gwm",1},
    {"BEATLESS SCAR-H","Man",6,1,{L"library-982c06c1d009-0"},L"library-982c06c1d009-preview.gwm",2,L"library-982c06c1d009-native.gwm"},
    {"USP-S Kou-Glow","Smaug",7,4,{L"mapped2-173-0",L"mapped2-173-1",L"mapped2-173-2",L"mapped2-173-3"},L"mapped-template-7.gwm",1},
    {"Tactical SCAR-H (Custom Stock)","Man",6,2,{L"library-824d1c6f1a43-0",L"library-824d1c6f1a43-1"},L"library-824d1c6f1a43-preview.gwm",2,L"library-824d1c6f1a43-native.gwm"},
    {"Choco Shark AWP","Fade",1,1,{L"library-facafa4db8e3-0"},L"host-fade-1-library-facafa4db8e3-preview.gwm",2,L"host-fade-1-library-facafa4db8e3-native.gwm"},
    {"Snow SCAR","Man",6,1,{L"library-d11ebb75ab83-0"},L"library-d11ebb75ab83-preview.gwm",2,L"library-d11ebb75ab83-native.gwm"},
    {"AWP Printstream","Fade",4,1,{L"native-awp-printstream"},L"host-fade-4-cs-awp-hd-preview.gwm",2,L"host-fade-4-cs-awp-hd-native.gwm"},
    {"AWP Lightning Strike","Fade",4,1,{L"native-awp-lightning-strike"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"AWP Desert Hydra","Fade",4,1,{L"native-awp-desert-hydra"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"AWP Gungnir","Fade",4,1,{L"native-awp-gungnir"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"AWP Queen's Gambit","Fade",4,1,{L"native-awp-queen-s-gambit"},L"host-fade-4-cs-awp-hd-preview.gwm",2,L"host-fade-4-cs-awp-hd-native.gwm"},
    {"AWP Atheris","Fade",4,1,{L"native-awp-atheris"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"AWP Black Nile","Fade",4,1,{L"native-awp-black-nile"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"AWP Crakow!","Fade",4,1,{L"native-awp-crakow"},L"host-fade-4-cs-awp-hd-preview.gwm",2,L"host-fade-4-cs-awp-hd-native.gwm"},
    {"AWP Exothermic","Fade",4,1,{L"native-awp-exothermic"},L"host-fade-4-cs-awp-hd-preview.gwm",2,L"host-fade-4-cs-awp-hd-native.gwm"},
    {"AWP LongDog","Fade",4,1,{L"native-awp-longdog"},L"host-fade-4-cs-awp-hd-preview.gwm",2,L"host-fade-4-cs-awp-hd-native.gwm"},
    {"AWP Medusa","Fade",4,1,{L"native-awp-medusa"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"AWP Oni Taiji","Fade",4,1,{L"native-awp-oni-taiji"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"AWP Sovereign Flame","Fade",4,1,{L"native-awp-sovereign-flame"},L"host-fade-4-cs-awp-hd-preview.gwm",2,L"host-fade-4-cs-awp-hd-native.gwm"},
    {"AWP The End","Fade",4,1,{L"native-awp-the-end"},L"host-fade-4-cs-awp-hd-preview.gwm",2,L"host-fade-4-cs-awp-hd-native.gwm"},
    {"AWP The Prince","Fade",4,1,{L"native-awp-the-prince"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"AWP Wildfire","Fade",4,1,{L"native-awp-wildfire"},L"host-fade-4-cs-awp-legacy-preview.gwm",2,L"host-fade-4-cs-awp-legacy-native.gwm"},
    {"Amagi AWP","Fade",4,1,{L"library-aa693301670e-0"},L"host-fade-4-library-aa693301670e-preview.gwm",2,L"host-fade-4-library-aa693301670e-native.gwm"},
    {"Bazaar Bargain","Fade",4,1,{L"library-ccc2e0cc293d-0"},L"host-fade-4-library-ccc2e0cc293d-preview.gwm",2,L"host-fade-4-library-ccc2e0cc293d-native.gwm"},
    {"Golden AWP","Fade",4,1,{L"library-d7ed131bca2a-0"},L"host-fade-4-library-d7ed131bca2a-preview.gwm",2,L"host-fade-4-library-d7ed131bca2a-native.gwm"},
    {"AWP 02","Fade",4,1,{L"library-64f6d65b49c8-0"},L"host-fade-4-library-64f6d65b49c8-preview.gwm",2,L"host-fade-4-library-64f6d65b49c8-native.gwm"},
    {"Hinata AWP","Fade",4,1,{L"library-5810d72ebfd1-0"},L"host-fade-4-library-5810d72ebfd1-preview.gwm",2,L"host-fade-4-library-5810d72ebfd1-native.gwm"},
    {"Minato Aqua AWP","Fade",4,1,{L"library-404e56316715-0"},L"host-fade-4-library-404e56316715-preview.gwm",2,L"host-fade-4-library-404e56316715-native.gwm"},
    {"AWP","Fade",4,1,{L"library-c9b2e9432a0e-0"},L"host-fade-4-library-c9b2e9432a0e-preview.gwm",2,L"host-fade-4-library-c9b2e9432a0e-native.gwm"},
    {"AWP |Formidable Theme","Fade",4,1,{L"library-7f61269b1c80-0"},L"host-fade-4-library-7f61269b1c80-preview.gwm",2,L"host-fade-4-library-7f61269b1c80-native.gwm"},
    {"AWP Bloodsport","Fade",4,1,{L"library-798cc71dc5e2-0"},L"host-fade-4-library-798cc71dc5e2-preview.gwm",2,L"host-fade-4-library-798cc71dc5e2-native.gwm"},
    {"Mona AWP","Fade",4,1,{L"library-ee1fb496e3f1-0"},L"host-fade-4-library-ee1fb496e3f1-preview.gwm",2,L"host-fade-4-library-ee1fb496e3f1-native.gwm"},
    {"Choco Shark AWP","Fade",4,1,{L"library-facafa4db8e3-0"},L"host-fade-4-library-facafa4db8e3-preview.gwm",2,L"host-fade-4-library-facafa4db8e3-native.gwm"},
    {"Natsuki Mini Uzi","X3",10,4,{L"mapped-new-4f4bd910efbc-0",L"mapped-new-4f4bd910efbc-1",L"mapped-new-4f4bd910efbc-2",L"mapped-new-4f4bd910efbc-3"},L"mapped-x3.gwm",1,nullptr},
    {"Natsuki Rifle","Man",6,2,{L"library-81cbb37aecc9-0",L"library-81cbb37aecc9-1"},L"library-81cbb37aecc9-preview.gwm",2,L"library-81cbb37aecc9-native.gwm"},
    {"Micro Uzi","X3",10,4,{L"mapped-new-666e9c8bfc82-0",L"mapped-new-666e9c8bfc82-1",L"mapped-new-666e9c8bfc82-2",L"mapped-new-666e9c8bfc82-3"},L"mapped-x3.gwm",1,nullptr},
    {"Crimson Blade","X5",0,1,{L"library-37b650309f94-0"},L"library-37b650309f94-preview.gwm",2,L"library-37b650309f94-native.gwm"},
    {"[CODOL]MINIUZI GRAFFITI","X3",10,4,{L"mapped-new-1e9491864a74-0",L"mapped-new-1e9491864a74-1",L"mapped-new-1e9491864a74-2",L"mapped-new-1e9491864a74-3"},L"mapped-x3.gwm",1,nullptr},
    {".50 Beowulf Automatic Rifle","Man",6,2,{L"library-5e7e70127f70-0",L"library-5e7e70127f70-1"},L"library-5e7e70127f70-preview.gwm",2,L"library-5e7e70127f70-native.gwm"},
    {"Bocchi Glock Carbine","X3",10,2,{L"library-6a8985a6f3f2-0",L"library-6a8985a6f3f2-1"},L"library-6a8985a6f3f2-preview.gwm",2,L"library-6a8985a6f3f2-native.gwm"},
    {"Blood Splatter AK","X5",0,1,{L"mapped-new-e5b51825b75f-0"},L"mapped-template-0.gwm",1,nullptr},
    {"Raiden Shogun AKS-74U","X5",0,1,{L"mapped-new-8d1056a28ca7-0"},L"mapped-template-0.gwm",1,nullptr},
    {"Hu Tao CZ-512","Man",6,1,{L"library-63ddc257f823-0"},L"library-63ddc257f823-preview.gwm",2,L"library-63ddc257f823-native.gwm"},
    {"MK14 EBR","Man",6,1,{L"library-a389b65f06ae-0"},L"library-a389b65f06ae-preview.gwm",2,L"library-a389b65f06ae-native.gwm"},
    {"MW FAMAS F1 SD","Man",6,1,{L"library-000c1be7de78-0"},L"library-000c1be7de78-preview.gwm",2,L"library-000c1be7de78-native.gwm"},
    {"Pink Sweetheart P90","X3",10,1,{L"library-2f14b2c31db9-0"},L"library-2f14b2c31db9-preview.gwm",2,L"library-2f14b2c31db9-native.gwm"},
    {"Plum Blossom M1 Garand","Man",6,1,{L"library-2163e0b10483-0"},L"library-2163e0b10483-preview.gwm",2,L"library-2163e0b10483-native.gwm"},
    {"Shenhe Uzi","X3",10,4,{L"mapped-new-0224431c1a9b-0",L"mapped-new-0224431c1a9b-1",L"mapped-new-0224431c1a9b-2",L"mapped-new-0224431c1a9b-3"},L"mapped-x3.gwm",1,nullptr},
    {"Arcaea Kou P90","X3",10,1,{L"library-46c7df2184bc-0"},L"library-46c7df2184bc-preview.gwm",2,L"library-46c7df2184bc-native.gwm"},
    {"CSO Skull-3","Man",6,1,{L"library-f1677ccb8243-0"},L"library-f1677ccb8243-preview.gwm",2,L"library-f1677ccb8243-native.gwm"},
    {"Goldenglow DSA SA58","Man",6,1,{L"library-db68f432c8b7-0"},L"library-db68f432c8b7-preview.gwm",2,L"library-db68f432c8b7-native.gwm"},
    {"Kokoro Amamiya Mini Uzi","X3",10,4,{L"mapped-new-3e6604e1fd61-0",L"mapped-new-3e6604e1fd61-1",L"mapped-new-3e6604e1fd61-2",L"mapped-new-3e6604e1fd61-3"},L"mapped-x3.gwm",1,nullptr},
    {"Qiye Uzi","X3",10,4,{L"mapped-new-1f489594b9ed-0",L"mapped-new-1f489594b9ed-1",L"mapped-new-1f489594b9ed-2",L"mapped-new-1f489594b9ed-3"},L"mapped-x3.gwm",1,nullptr},
    {"Jia Ran Doll Uzi","X3",10,2,{L"library-2efeb45eb273-0",L"library-2efeb45eb273-1"},L"library-2efeb45eb273-preview.gwm",2,L"library-2efeb45eb273-native.gwm"},
    {"Holy Judgement","X5",0,1,{L"library-180b10312bf3-0"},L"library-180b10312bf3-preview.gwm",2,L"library-180b10312bf3-native.gwm"},
    {"Cirno MK19","Man",6,2,{L"library-41e1ff22d282-0",L"library-41e1ff22d282-1"},L"library-41e1ff22d282-preview.gwm",2,L"library-41e1ff22d282-native.gwm"},
    {"XR-3 Ion","Man",6,2,{L"library-b585116fbaa5-0",L"library-b585116fbaa5-1"},L"library-b585116fbaa5-preview.gwm",2,L"library-b585116fbaa5-native.gwm"},
    {"Lacia MP7","X3",10,1,{L"library-ccc5c938f3f1-0"},L"library-ccc5c938f3f1-preview.gwm",2,L"library-ccc5c938f3f1-native.gwm"},
    {"Arona Uzi","X3",10,2,{L"library-beeb182f9909-0",L"library-beeb182f9909-1"},L"library-beeb182f9909-preview.gwm",2,L"library-beeb182f9909-native.gwm"},
    {"Mizuki P90","X3",10,1,{L"library-4fb3bd9e7acb-0"},L"library-4fb3bd9e7acb-preview.gwm",2,L"library-4fb3bd9e7acb-native.gwm"}
}};
inline constexpr std::array<std::array<int,2>,27> AwpHosts{{{12,177},{13,178},{14,179},{15,180},{16,181},{48,182},{49,183},{50,184},{51,185},{52,186},{53,187},{54,188},{55,189},{56,190},{57,191},{58,192},{117,193},{120,194},{125,195},{132,196},{143,197},{144,198},{145,199},{149,200},{150,201},{152,202},{175,203}}};
inline int ForWeapon(int index,bool glock){for(const auto& pair:AwpHosts)if(index==pair[0]||index==pair[1])return pair[glock?1:0];return index;}
inline bool IsAwpHost(int index){for(const auto& pair:AwpHosts)if(index==pair[0]||index==pair[1])return true;return false;}
inline bool Valid(int index){return index>=0&&index<static_cast<int>(Presets.size());}
inline bool Available(int index){return Valid(index)&&index!=73&&index!=76&&index!=79&&index!=81&&index!=86&&index!=104&&index!=112&&index!=133&&index!=160;}
inline int ActiveCount(){int count=0;for(int i=0;i<static_cast<int>(Presets.size());++i)count+=Available(i)?1:0;return count;}
inline bool Supports(int index,bool glock){return Available(index)&&(glock?((Presets[index].model>=2&&Presets[index].model<=4)||(Presets[index].model>=7&&Presets[index].model<=9)):(Presets[index].model<=2||Presets[index].model==6||Presets[index].model==10));}
inline std::wstring Name(int index,Channel channel,int group=0){
    if(!Valid(index)||group<0||group>=Presets[index].groups)return {};
    if(channel==Channel::Normal)return L"neutral-normal.png";
    return std::wstring(Presets[index].stems[group])+(channel==Channel::Material?L"-material":channel==Channel::Normal?L"-normal":L"")+L".png";
}
inline std::filesystem::path File(int index,Channel channel,int group=0){const auto name=Name(index,channel,group);return name.empty()?std::filesystem::path{}:RuntimePaths::File(L"custom-textures")/name;}
inline std::filesystem::path File(int index,bool material=false){return File(index,material?Channel::Material:Channel::Color);}
inline std::wstring SourceName(int index,Channel channel=Channel::Color){return Name(index,channel);}
inline std::filesystem::path SourceFile(int index,Channel channel=Channel::Color){return File(index,channel);}
inline bool RequiresModel(int index){return Valid(index)&&Presets[index].nativeFile!=nullptr;}
inline bool NativeMetal(int index){index=ForWeapon(index,false);return index==125||index==126||index==127||index==138||index==140||index==156||(Valid(index)&&(std::strcmp(Presets[index].name,"CSO Skull-3")==0||std::strcmp(Presets[index].name,"Crimson Blade")==0));}
inline float NativeExposure(int index){index=ForWeapon(index,false);if(index==125)return 1.f;if(index==164||index==165||index==207||index==209||index==214||index==220)return .85f;if(index>=116&&Valid(index))return .55f;switch(index){case 12:case 14:case 16:case 98:case 109:case 110:case 111:case 114:return .55f;case 115:return .7f;default:return 1.f;}}
inline std::wstring NativeModelName(int index){return RequiresModel(index)?Presets[index].nativeFile:L"";}
inline std::wstring ModelName(int index){if(!Valid(index))return {};if(Presets[index].modelFile)return Presets[index].modelFile;const wchar_t* names[]={L"native-x5.gwm",L"native-m4a1.gwm",L"native-0416.gwm",L"native-deagle.gwm",L"native-glock.gwm",L"native-m4a4.gwm",L"native-scar.gwm",L"mapped-template-7.gwm",L"mapped-template-8.gwm",L"mapped-template-9.gwm",L"mapped-x3.gwm"};return names[Presets[index].model];}
inline std::wstring ShadowName(int index,int group){if(!Valid(index)||RequiresModel(index)||group<0||group>=Presets[index].groups)return {};const wchar_t* names[]={L"x5",L"m4a1",L"0416",L"deagle",L"glock",L"m4a4",L"scar",L"smaug",L"warden",L"opulent",L"x3"};return L"shadow-"+std::wstring(names[Presets[index].model])+L"-"+std::to_wstring(group)+L".png";}
inline std::filesystem::path ShadowFile(int index,int group){return RuntimePaths::File(L"custom-textures")/ShadowName(index,group);}
inline std::filesystem::path ModelFile(int index){return RuntimePaths::File(L"custom-textures")/ModelName(index);}
inline std::wstring PreviewName(int index,bool=false){return Valid(index)?std::wstring(Presets[index].stems[0])+L"-preview.png":std::wstring{};}
inline std::filesystem::path PreviewFile(int index,bool=false){return RuntimePaths::File(L"custom-textures")/PreviewName(index);}
inline bool KnownFile(const std::wstring& name){for(int i=0;i<static_cast<int>(Presets.size());++i){if(name==ModelName(i)||(!NativeModelName(i).empty()&&name==NativeModelName(i))||name==PreviewName(i))return true;for(int group=0;group<Presets[i].groups;++group)for(auto channel:{Channel::Color,Channel::Material,Channel::Normal})if(name==Name(i,channel,group))return true;}return false;}

inline std::string AssetId(std::string mesh){
    const auto start=mesh.find("rbxassetid://");const auto query=mesh.find("id=");
    if(start!=mesh.npos)mesh=mesh.substr(start+13);else if(query!=mesh.npos)mesh=mesh.substr(query+3);
    const auto end=mesh.find_first_not_of("0123456789");if(end!=mesh.npos)mesh=mesh.substr(0,end);return mesh;
}
inline int SourceGroup(int index,const std::string& value){
    if(!Valid(index))return -1;const auto mesh=AssetId(value);
    if(Presets[index].model==0){for(auto id:{"105137249332183","81766375295784","93054331126203","95891610475658","118641859394860","71700100568681","108003260543980","86598522525666","110878660637855","74915530422476","86071090387346","77729921739121"})if(mesh==id)return 0;}
    if(Presets[index].model==1){if(mesh=="130533951042187"||mesh=="89088865339034")return 0;if(mesh=="92394911895072"||mesh=="140300391074568")return 1;if(mesh=="88483816698490"||mesh=="109005072716981")return 2;}
    if(Presets[index].model==2){if(mesh=="72915978896741")return 0;if(mesh=="88673729944263")return 1;if(mesh=="112855877398692")return 2;if(mesh=="121498307784399")return 3;}
    if(Presets[index].model==3)for(auto id:{"17788240085","5165152169","5165153537","5165062009","5160693646","5165147746","5160693225","5160689305","5160695315"})if(mesh==id)return 0;
    if(Presets[index].model==4){if(mesh=="139968168789097")return 0;if(mesh=="88526102437226")return 1;if(mesh=="95490897912075")return 2;if(mesh=="113543892953755"||mesh=="105223351972520")return 3;}

    if(Presets[index].model==6)for(const auto* id:{"104706438039964","83735677478023","127562261097545","107096041514151","93249792517931","73759587259346"})if(mesh==id)return 0;
    if(Presets[index].model==7){if(mesh=="94331848862717")return 0;if(mesh=="120716431310202")return RequiresModel(index)?0:1;if(mesh=="78504609503399")return RequiresModel(index)?0:2;if(mesh=="106191314817294")return RequiresModel(index)?0:3;if(mesh=="116973769079872")return RequiresModel(index)?0:-1;}
    if(Presets[index].model==8){if(mesh=="117367344623803")return RequiresModel(index)?0:1;if(mesh=="132190622933096")return RequiresModel(index)?0:-1;for(const auto* id:{"109601921301900","113234163647046","119993528275279","71267402738897","91480723083730","96436296265834"})if(mesh==id)return 0;}
    if(Presets[index].model==9)for(const auto* id:{"7193334919","7193336383","7193336841"})if(mesh==id)return 0;
    if(Presets[index].model==10){if(mesh=="77445984432790")return 0;if(mesh=="139436279660125")return RequiresModel(index)?0:1;if(mesh=="86752229938291")return RequiresModel(index)?0:2;if(mesh=="87705771580055")return RequiresModel(index)?0:3;if(mesh=="125155548870667"||mesh=="103021762273494")return 0;}
    return -1;
}
inline int Group(int index,const std::string& value){const int group=SourceGroup(index,value);return group<0?-1:RequiresModel(index)?0:group;}
inline bool NativeAnchor(int index,const std::string& value){if(!RequiresModel(index))return false;const auto id=AssetId(value);switch(Presets[index].model){case 0:return id=="105137249332183";case 1:return id=="130533951042187";case 2:return id=="72915978896741";case 3:return id=="5165062009";case 4:return id=="139968168789097";case 6:return id=="127562261097545";case 7:return id=="94331848862717";case 8:return id=="119993528275279";case 9:return id=="7193336841";case 10:return id=="77445984432790";default:return false;}}
inline int Anchor(int index,const std::string& value){
    if(!Valid(index))return -1;const auto mesh=AssetId(value);const auto model=Presets[index].model;
    if(model==0){if(mesh=="105137249332183")return 0;if(mesh=="74915530422476")return 1;if(mesh=="86071090387346")return 2;if(mesh=="81766375295784")return 3;}
    if(model==1){if(mesh=="130533951042187")return 0;if(mesh=="89088865339034")return 1;if(mesh=="109005072716981")return 2;if(mesh=="92394911895072")return 3;}
    if(model==2)return SourceGroup(index,value);
    if(model==3){if(mesh=="5165062009")return 0;if(mesh=="17788240085")return 1;if(mesh=="5165153537")return 2;if(mesh=="5160689305")return 3;}
    if(model==4){if(mesh=="139968168789097")return 0;if(mesh=="88526102437226")return 1;if(mesh=="95490897912075")return 2;if(mesh=="113543892953755")return 3;}
    return -1;
}
inline bool ModelPart(int index,const std::string& value){
    if(!Valid(index))return false;const auto mesh=AssetId(value);
    if(Presets[index].model==3){for(const auto* id:{"17788240085","5165152169","5165153537","5165062009","5160693646","5165147746","5160693225","5160689305","5160695315"})if(mesh==id)return true;}
    if(Presets[index].model==4){for(const auto* id:{"113543892953755","139968168789097","88526102437226","105223351972520","95490897912075"})if(mesh==id)return true;}
    return(Presets[index].model<=2||Presets[index].model>=6)&&Group(index,value)>=0;
}
inline bool Ornament(int index,const std::string& value){
    if(!Valid(index))return false;const auto mesh=AssetId(value);
    if(Presets[index].model==0)return mesh=="90519117045622"||mesh=="120365066408341"||mesh=="110502438098991";
    if(Presets[index].model==10)return mesh=="72057152892249";
    return(Presets[index].model==1||Presets[index].model==4)&&mesh=="77537260328217";
}
inline bool ImagePresent(const std::filesystem::path& file){
    try{if(!std::filesystem::is_regular_file(file))return false;std::ifstream f(file,std::ios::binary);std::array<unsigned char,24> b{};if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))return false;
        const unsigned char signature[]={137,80,78,71,13,10,26,10};if(!std::equal(std::begin(signature),std::end(signature),b.begin()))return false;
        auto u32=[&](int offset){return(unsigned(b[offset])<<24)|(unsigned(b[offset+1])<<16)|(unsigned(b[offset+2])<<8)|b[offset+3];};
        return b[12]=='I'&&b[13]=='H'&&b[14]=='D'&&b[15]=='R'&&u32(16)>0&&u32(16)<=4096&&u32(20)>0&&u32(20)<=4096;
    }catch(...){return false;}
}
inline bool Installed(int index,const std::filesystem::path& directory){if(!Valid(index))return false;for(int g=0;g<Presets[index].groups;++g)for(auto c:{Channel::Color,Channel::Material,Channel::Normal})if(!ImagePresent(directory/Name(index,c,g)))return false;if(RequiresModel(index)&&(!CustomModelData::Present(directory/ModelName(index))||!CustomModelData::Present(directory/NativeModelName(index))))return false;return true;}
inline bool Present(const std::filesystem::path& directory){for(int i=0;i<static_cast<int>(Presets.size());++i)if(Available(i)&&!Installed(i,directory))return false;return true;}
}
