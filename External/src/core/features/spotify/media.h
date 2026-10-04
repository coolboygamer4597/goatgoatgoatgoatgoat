#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace media {

struct LyricLine {
    double      timeSec = -1.0;
    std::string text;
};

struct NowPlaying {
    bool         playing = false;
    bool         shuffleActive = false;
    bool         repeatActive = false;

    bool         sourceConnected = false;
    bool         sourceIsSpotify = false;
    char         sourceApp[96] = {};
    char         title[160]  = {};
    char         artist[160] = {};
    char         album[160]  = {};

    int          artW = 0;
    int          artH = 0;
    uint8_t*     artBgra = nullptr;
    bool         artDirty = false;

    double       positionSec = 0.0;
    double       durationSec = 0.0;
    double       playbackRate = 1.0;
    uint64_t     snapshotTickMs = 0;

    std::vector<LyricLine> lyrics;
    bool         lyricsLoading = false;
    bool         lyricsSynced = false;
    bool         instrumental = false;
    uint64_t     lyricsRevision = 0;
};

void Init();
void Shutdown();
void Tick();
const NowPlaying& Current();

bool TryAcquireSnapshot();
void ReleaseSnapshot();

void RequestTogglePlayPause();
void RequestSkipNext();
void RequestSkipPrevious();
void RequestToggleShuffle();
void RequestToggleRepeat();
void RequestSeek(double positionSec);
void RequestLyricsRefresh();

void SetSpotifyOnly(bool enabled);
bool IsSpotifyOnly();

}
