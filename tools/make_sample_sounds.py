# 예제(sandbox) 소리 만들기: 효과음·배경음을 코드로 합성해 assets/sounds/에 WAV로 쓴다 (CC0, 외부 파일 없음).
#   python tools/make_sample_sounds.py
# 모노 16비트 22050 Hz. 효과음은 짧게(메모리에 풀어 둔다), 배경음은 16초 루프(스트리밍).
# 같은 스크립트로 다시 만들면 같은 파일이 나온다 (난수 시드 고정).
import math
import os
import random
import struct
import wave

RATE = 22050
ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sounds")


def write(name, samples, peak=0.9):
    m = max(1e-9, max(abs(s) for s in samples))
    scale = peak / m
    os.makedirs(ROOT, exist_ok=True)
    path = os.path.join(ROOT, name)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples))
    print("wrote", os.path.relpath(path), f"{len(samples) / RATE:.2f}s")


def seconds(t):
    return int(t * RATE)


def env(i, n, attack=0.005, decay=None):
    t = i / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    if decay is None:
        return a
    return a * math.exp(-t / decay)


def lowpass(samples, cutoff):
    rc = 1.0 / (2 * math.pi * cutoff)
    alpha = (1.0 / RATE) / (rc + 1.0 / RATE)
    out, y = [], 0.0
    for s in samples:
        y += alpha * (s - y)
        out.append(y)
    return out


def highpass(samples, cutoff):
    low = lowpass(samples, cutoff)
    return [s - l for s, l in zip(samples, low)]


def sweep(f0, f1, length, shape=math.sin, curve=1.0):
    n = seconds(length)
    phase, out = 0.0, []
    for i in range(n):
        k = (i / n) ** curve
        f = f0 + (f1 - f0) * k
        phase += 2 * math.pi * f / RATE
        out.append(shape(phase))
    return out


def square(p):
    return 1.0 if math.sin(p) >= 0 else -1.0


def triangle(p):
    return 2.0 / math.pi * math.asin(math.sin(p))


rng = random.Random(7)


def noise(n):
    return [rng.uniform(-1, 1) for _ in range(n)]


# ---- 충돌: 낮은 쿵 + 짧은 잡음 (세 가지 색)
for k, (thump, color, length) in enumerate([(70, 1800, 0.18), (95, 2600, 0.15), (55, 1300, 0.22)], start=1):
    n = seconds(length)
    body = [math.sin(2 * math.pi * thump * (i / RATE) * (1.0 - 0.3 * i / n)) * env(i, n, 0.002, length * 0.25) for i in range(n)]
    click = lowpass(noise(n), color)
    click = [c * env(i, n, 0.001, 0.02) for i, c in enumerate(click)]
    write(f"hit_{k}.wav", [b + 0.6 * c for b, c in zip(body, click)])

# ---- 밀치기: 잡음 휙 (대역이 올라갔다 내려온다)
n = seconds(0.42)
src = noise(n)
out, y = [], 0.0
for i, s in enumerate(src):
    t = i / n
    cutoff = 400 + 3200 * math.sin(math.pi * t)
    rc = 1.0 / (2 * math.pi * cutoff)
    a = (1.0 / RATE) / (rc + 1.0 / RATE)
    y += a * (s - y)
    out.append(y * math.sin(math.pi * t) ** 1.5)
write("push.wav", highpass(out, 150))

# ---- 점프: 올라가는 사인
n = seconds(0.2)
s = sweep(260, 760, 0.2, curve=0.6)
write("jump.wav", [v * env(i, n, 0.003, 0.09) for i, v in enumerate(s)])

# ---- 점프대: 떨리는 스프링
n = seconds(0.42)
phase, out = 0.0, []
for i in range(n):
    t = i / RATE
    f = 180 + 520 * (i / n) ** 0.5 + 40 * math.sin(2 * math.pi * 26 * t)
    phase += 2 * math.pi * f / RATE
    out.append(triangle(phase) * env(i, n, 0.004, 0.16))
write("bounce.wav", out)

# ---- 스폰: 퐁 + 종소리 두 음
n = seconds(0.45)
out = []
for i in range(n):
    t = i / RATE
    pop = math.sin(2 * math.pi * 440 * t * (1 + 2 * math.exp(-t / 0.01))) * math.exp(-t / 0.04)
    bell = (math.sin(2 * math.pi * 880 * t) + 0.6 * math.sin(2 * math.pi * 1320 * t)) * math.exp(-t / 0.18) * (t > 0.05)
    out.append(pop + 0.5 * bell)
write("spawn.wav", out)

# ---- 낙사: 내려가는 휘파람
n = seconds(0.9)
s = sweep(1300, 260, 0.9, curve=1.4)
write("fall.wav", [v * env(i, n, 0.02) * (1 - i / n) ** 0.6 for i, v in enumerate(s)], peak=0.7)

# ---- UI: 누름, 확인, 뒤로
n = seconds(0.05)
write("ui_click.wav", [math.sin(2 * math.pi * 1500 * i / RATE) * env(i, n, 0.001, 0.012) for i in range(n)], peak=0.6)
n = seconds(0.22)
out = []
for i in range(n):
    t = i / RATE
    f = 660 if t < 0.08 else 990
    out.append(square(2 * math.pi * f * t) * 0.5 * env(i, n, 0.002, 0.08))
write("ui_confirm.wav", lowpass(out, 4000), peak=0.6)
n = seconds(0.18)
out = []
for i in range(n):
    t = i / RATE
    f = 700 if t < 0.07 else 470
    out.append(square(2 * math.pi * f * t) * 0.5 * env(i, n, 0.002, 0.07))
write("ui_back.wav", lowpass(out, 3500), peak=0.6)

# ---- 배경음: 120 BPM, 8마디(16초) 루프. 베이스(삼각파) + 아르페지오(사각파) + 하이햇(잡음)
bpm = 120
beat = 60.0 / bpm
bars = 8
total = seconds(bars * 4 * beat)
music = [0.0] * total
chords = [(57, 60, 64), (53, 57, 60), (48, 52, 55), (55, 59, 62)]  # Am F C G (MIDI)


def midi(n):
    return 440.0 * 2 ** ((n - 69) / 12)


def add_note(start, length, freq, shape, volume, decay):
    s0 = seconds(start)
    n = seconds(length)
    for i in range(n):
        if s0 + i >= total:
            break
        t = i / RATE
        music[s0 + i] += shape(2 * math.pi * freq * t) * volume * env(i, n, 0.004, decay) * (1 - i / n) ** 0.3


for bar in range(bars):
    chord = chords[bar % 4]
    t0 = bar * 4 * beat
    for b in range(4):  # 베이스: 박마다 근음, 2·4박은 옥타브 위
        root = chord[0] - 24 + (12 if b % 2 else 0)
        add_note(t0 + b * beat, beat * 0.9, midi(root), triangle, 0.5, beat * 0.6)
    for step in range(16):  # 아르페지오: 16분음표
        note = chord[step % 3] + (12 if (step // 3) % 2 else 0)
        add_note(t0 + step * beat / 4, beat / 4 * 0.8, midi(note), square, 0.12, beat * 0.12)
    for step in range(8):  # 하이햇: 8분음표
        s0 = seconds(t0 + step * beat / 2)
        n = seconds(0.04)
        hat = highpass(noise(n), 6000)
        for i in range(n):
            if s0 + i < total:
                music[s0 + i] += hat[i] * 0.15 * math.exp(-i / RATE / 0.012)
# 루프 이음새: 끝 10 ms를 처음과 섞는다
fade = seconds(0.01)
for i in range(fade):
    a = i / fade
    music[total - fade + i] = music[total - fade + i] * (1 - a) + music[i] * a
write("music_loop.wav", lowpass(music, 7000), peak=0.8)
