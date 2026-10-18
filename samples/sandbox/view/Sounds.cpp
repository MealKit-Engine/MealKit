#include "view/Sounds.h"

namespace sandbox {

void playEventSound(mealkit::audio::Engine& audio, const GameEvent& e) {
    switch (e.kind) {
    case EventKind::Push: audio.play("push", e.position); break;
    case EventKind::Bounce: audio.play("bounce", e.position); break;
    case EventKind::Spawn: audio.play("spawn", e.position); break;
    case EventKind::Fall: audio.play("fall", e.position); break;
    case EventKind::Jump: audio.play("jump", e.position); break;
    case EventKind::Impact: audio.play("hit", e.position, {{"speed", e.value}}); break; // 세기 = 다가오던 속도
    }
}

} // namespace sandbox
