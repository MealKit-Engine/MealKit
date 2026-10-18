#pragma once

#include "sim/World.h"

#include "mealkit/audio/Audio.h"

// 틱 이벤트 → 소리 이벤트 (assets/sounds.table). 소리 자체(클립, 볼륨, 감쇠, 동시 재생 상한)는 표에서 고친다.
namespace sandbox {

void playEventSound(mealkit::audio::Engine& audio, const GameEvent& event);

} // namespace sandbox
