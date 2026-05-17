# Dr. House: The Visual Novel

> *"Everybody lies."* — A terminal-native diagnostic puzzle game built in C++23, with dialogue, team banter, and an entire episode script generated live by Claude.

You are Gregory House. A patient is bleeding from somewhere they shouldn't be bleeding from. You have ~8000 hospital dollars, four hostile colleagues, a Vicodin bottle, and roughly fifteen turns before something kills them. Run labs. Order treatments. Break into their apartment. Don't take the fifth pill.

---

<!--
  TODO: drop a screenshot, GIF, or asciinema cast here.
  Suggested shots:
    - docs/screenshots/morning.png  — the cold-open montage with the team
    - docs/screenshots/menu.png     — main menu + case board mid-game
    - docs/screenshots/eureka.png   — the eureka finale dialogue
  Recommended: a single 20–30s GIF of a representative turn beats any number of stills.
-->

## Highlights

- **LLM-driven dialogue, not LLM-driven game logic.** Claude (Haiku 4.5) writes the lines; deterministic C++ owns the stats, the exceptions, and the win/lose conditions. The model can be wrong about medicine without breaking the simulation.
- **Async team brainstorms.** Chase, Cameron, and Foreman each get their own personality prompt and respond in parallel via `std::async` — three opinions land in the time of one.
- **Exception-driven endings.** Four distinct game-overs (`PatientDeathException`, `FiredByHospitalException`, `OutOfBudgetException`, `MayfieldWardException`) thrown from deep game logic and caught at the top of `GameEngine::run()`. The control flow *is* the narrative.
- **Director's Cut.** When the case ends, the model writes a complete episode-style script of what just happened to `episodes/Episode_<PatientName>.txt`. Every run produces a unique short story.
- **Foreman's post-mortem.** Optional post-game debrief: the model reviews the actual game log against the actual `MedicalActionFactory` registry and tells you what the optimal workup would have been.
- **Eureka gate.** Reach 100% clarity *and* keep the patient at ≥10 HP — you trigger a multi-round bedside dialogue where the player roleplays the patient's perspective while House lands the diagnosis. Hit 100% clarity while the patient is dying and the game forces you to stabilize first.
- **Stub fallbacks everywhere.** Lose the network? Every LLM call has a hardcoded fallback so the game stays playable offline (with much less personality, obviously).

## Quickstart

### Prerequisites

- C++23-capable compiler (GCC 13+ / Clang 16+ / MSVC 2022 / Apple Clang 15+)
- CMake 3.26+
- OpenSSL (Homebrew on macOS, `mingw-w64-x86_64-openssl` on MinGW, distro package on Linux)
- An [Anthropic API key](https://console.anthropic.com/) — the game calls Claude Haiku 4.5. Without one you get stub dialogue (still playable, much weaker).

### Build & run

```bash
# Clone
git clone https://github.com/<you>/cpp-dr-house-visual-novel.git
cd cpp-dr-house-visual-novel

# Add your API key
cat > secrets.json <<'EOF'
{ "ANTHROPIC_API_KEY": "sk-ant-..." }
EOF

# Configure + build (one-shot via the helper script)
./scripts/cmake.sh configure
./scripts/cmake.sh build -c Debug

# Or manual CMake if you prefer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Play
./build/oop          # macOS / Linux
./build/oop.exe      # Windows
```

### Smoke-test mode

Trigger every exception path without playing through the game:

```bash
./build/oop --test
```

This drives the four loss conditions in sequence so you can verify endings without a real run — useful when balancing or after a refactor.

## How it plays

Each session is one patient case. You see three randomly generated patient files at admission and pick one to diagnose. From there it's turn-based:

1. **Pick an action** from the main menu — Medical Intervention, Social Interaction, House Actions (chaos), or Whiteboard (free-text brainstorm).
2. **Each turn ticks two clocks**: the patient's health decays (probabilistically, scaled to disease severity), and your malpractice score creeps up if you took a risky action.
3. **Three resources to balance**: budget ($8000 starting), clarity (your diagnostic understanding, 0–100%), and malpractice (legal exposure, capped at 100%). Plus a Vicodin counter that game-overs at 5 pills.
4. **You win** when clarity hits 100% *and* the patient is alive enough (≥10 HP) for a confirmation scene.
5. **You lose** four ways: patient dies, you get fired for malpractice, you run out of money, or Cuddy ships you to Mayfield Psychiatric for the fifth pill.

### Things the game actually does well

- The team brainstorm doesn't just print three opinions — it lets you *steer*. Pick one of four House-voice nudges (one per action category) and the agents reactively shift in the next round.
- Wilson's office is a real mechanic. You pre-commit to a hypothesis category before you knock. If your axis matches the true workup direction, Wilson rewards it; if it doesn't, he gently corrects.
- Risky Procedure variance is *transparent* — the game shows the actual odds (40% catastrophe / 40% nothing / 20% jackpot) before you commit. No hidden EV.
- The morning montage at game start picks one of twelve hand-written cold-open scripts (Cuddy banter, Wilson sarcasm, Foreman dryness) so every run opens differently.
- Every case generates an `episodes/Episode_<PatientName>.txt` you can keep. They read like real *House M.D.* cold-opens-to-resolution shorts.

### Things the game doesn't do (yet, or maybe never)

- **You don't name the disease.** The game reveals the diagnosis in the eureka cutscene; you're playing stat optimization, not classic adventure-game deduction.
- **LLM latency is real.** A full turn round-trip is 5–15 seconds. Team brainstorms parallelize, but Wilson / Cuddy / single-action evaluations don't.
- **No save/load.** Each run is a session. The episode and post-mortem scripts on disk are the only persistent artifacts.
- **No difficulty levels.** Stats are tuned for a single curve.

## Notable engineering

- **`include/Exceptions.h`** — exception hierarchy `GameException : std::exception` with four derived end-state exceptions. Thrown from `GameEngine::checkEndings()` deep in the action handlers, caught at the top of `GameEngine::run()`. Game-over flow lives in the type system.
- **`include/AIAgent.h` + `src/GameEngine.cpp`** — `AIAgent` abstract base with `brainstorm()` template-method and `getPersonalityPrompt()` virtual hook. Chase / Cameron / Foreman are derivations that change the prompt only. Brainstorm rounds fan out `std::async` calls for parallel LLM execution.
- **`include/GameLog.h`** — templated `GameLog<T>` with `operator+=` (push), `operator+` (concat), and STL-algorithm-driven `filter` / `count` methods taking lambdas. Instantiated for both `std::string` (narrative log) and `ActionRecord` (structured action log).
- **`include/MedicalActionFactory.h`** — Factory pattern producing `std::unique_ptr<MedicalAction>` for ~40 named medical actions across 4 type categories, each with cost and the right subclass.
- **`src/LLM.cpp`** — Adapter over `cpp-httplib` + `nlohmann::json` for the Anthropic Messages API. Every endpoint has a `stubFallback` lambda so a missing network never crashes the game.

## Configuration

- **`secrets.json`** (gitignored) — holds `ANTHROPIC_API_KEY`. Required for the LLM features; stubs are used when missing.
- **`DEBUG_MODE`** macro in `include/GameEngine.h` — flip to `true` to reveal the hidden diagnosis in the HUD. Useful for development and balance testing.
- **`tastatura.txt`** — terminal raw-mode keyboard config, copied next to the executable at build time.

## Tech stack

- **C++23**, CMake 3.26+
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) (vendored under `include/external/`) for HTTPS to Anthropic
- [nlohmann/json](https://github.com/nlohmann/json) for request/response parsing
- OpenSSL for TLS
- [Anthropic Claude Haiku 4.5](https://www.anthropic.com/) as the dialogue model
- GoogleTest (fetched via CMake `FetchContent`) for the test harness

## Academic project

This was built for the OOP course at the Faculty of Mathematics and Computer Science, University of Bucharest. The full rubric checklist with file/line citations lives in [`RUBRIC.md`](RUBRIC.md). The README is kept rubric-free so it reads as a project page, not a coursework submission.

## Credits

- Dialogue style, character beats, and the whole premise are lovingly cribbed from *House M.D.* (FOX, 2004–2012). This is a fan project, not affiliated with anyone.
- Built and submitted by *<your name>*.

## License

*<pick one — MIT is the default if you'd like to show this off as a portfolio piece; otherwise "Academic use only" is fine.>*
