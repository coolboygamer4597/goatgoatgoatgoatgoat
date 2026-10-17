#pragma once
namespace Keys {
struct ActivationKey {
    bool previousDown = false, latched = false;
    int previousKey = 0;
    bool previousMode = false, previousAllow = false, previousEnabled = false;
    bool Update(bool enabled, bool toggle, int key, bool down, bool allow) {
        if (toggle != previousMode)
            latched = false;
        if (!enabled || !allow || !previousAllow || !previousEnabled || key != previousKey || toggle != previousMode) {
            previousDown = down;
        }
        if (enabled && allow && key > 0 && toggle && down && !previousDown)
            latched = !latched;
        previousDown = down;
        previousKey = key;
        previousMode = toggle;
        previousAllow = allow;
        previousEnabled = enabled;
        return enabled && allow && key > 0 && (toggle ? latched : down);
    }
};
}
