#pragma once
namespace Keys {
struct ActivationKey {
    bool previousDown = false, latched = false;
    int previousKey = 0;
    bool previousMode = false;
    bool Update(bool enabled, bool toggle, int key, bool down, bool allow) {
        if (!enabled || !allow || key != previousKey || toggle != previousMode) {
            latched = false;
            previousDown = down;
        }
        if (enabled && allow && key > 0 && toggle && down && !previousDown)
            latched = !latched;
        previousDown = down;
        previousKey = key;
        previousMode = toggle;
        return enabled && allow && key > 0 && (toggle ? latched : down);
    }
};
}
