# Dr. House: The Visual Novel

A C++ terminal game where you play Dr. Gregory House trying to diagnose a patient before they die, get you fired, or bankrupt the hospital. You order tests, argue with the team, lie to Cuddy for money, and occasionally pop a Vicodin.

> You are Gregory House. A patient is bleeding from somewhere they shouldn't be bleeding from. You have ~8000 hospital dollars, four hostile colleagues, a Vicodin bottle, and roughly fifteen turns before something kills them. Run labs. Order treatments. Break into their apartment. Don't take the fifth pill.

---

## Quickstart Guide

### Prerequisites
* **C++23** compatible compiler (GCC/Clang/MSVC)
* **CMake** (v3.20+)
* **OpenSSL**
* An **Anthropic API Key** (for the Claude integration)

### Setup & Build

1. Clone the repository:
   ```bash
   git clone [https://github.com/ec4t3rina/cpp-dr-house-agentic-ai.git](https://github.com/ec4t3rina/cpp-dr-house-agentic-ai.git)
   cd cpp-dr-house-agentic-ai
   ```
CRITICAL - API Key: Create a file named secrets.json in the root directory and add your Anthropic API key:

```JSON
{
    "anthropic_api_key": "sk-ant-api03-YOUR_KEY_HERE"
}
```
(Note: secrets.json is gitignored by default for security).

2. Build the project:

```bash
cmake -S . -B build
cmake --build build
```

3. Run the game:

```bash
./build/oop
```
(To skip gameplay and auto-test all C++ exception loss conditions, run: ./build/oop --test)

## Gameplay Features & Walkthrough

There are four critical stats. Lose control of them, and it's Game Over.

| Stat |  Starting Value | Win/Loss Condition |
| :---: | :---: | :--- |
| **Health** | 30-80 | Drops to 0 → Patient dies (PatientDeathException) |
| **Clarity** | 0% | Hits 100% AND Health ≥ 10 → You Win (Eureka Finale) |
| **Malpractice** | 0% | Hits 100% → Tritter gets you (FiredByHospitalException) |
| **Budget** | $8,000 | Drops below $0 → Hospital pulls the plug (OutOfBudgetException) |
| **Vicodin Level** | 0 / 5 | Hits 5 → Sent to Mayfield Psychiatric (MayfieldWardException) |

### The Core Mechanics
* **Medical Intervention:** Order from a list of Lab Tests, Treatments, or Risky Procedures. The LLM evaluates your choice against the hidden canonical diagnosis to award Clarity or penalize you.
* **Social Interaction:** Send the team (Chase, Cameron, Foreman) to brainstorm (triggers parallel LLM calls for unique perspectives), ask Wilson for a hint, or beg Cuddy for emergency funding.
* **The "Chaos" Menu:** Break into the patient's house for clues, skip clinic duty, or pop a Vicodin for a quick Clarity boost.
* **The Whiteboard (Free-Text):** Type your own medical theories into a prompt. The AI acts as House's internal monologue to guide you. **Hidden Easter Egg:** If your text input contains a substring of the actual hidden disease, you trigger a "Eureka" spark and gain a massive +5% Clarity bonus!
* **The Eureka Finale:** Once you hit 100% Clarity, the POV shifts to the patient for a 3-round conversational boss fight where House delivers the final diagnosis.

### Post-Game Analysis
Win or lose, the game serializes every action you took and offers two distinct AI-generated wrap-ups:
1. **Foreman's Post-Mortem:** The LLM reviews your exact `ActionRecord` log, reviews your medical errors, and explains the optimal clinical path you *should* have taken. Saved to `Notes_<PatientName>.txt`.
2. **Director's Cut:** The AI takes your entire playthrough log and writes a 2-page dramatic TV script. Saved to `Episode_<PatientName>.txt`.

## Have fun!! :D
