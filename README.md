[Episode_Paul_Garza.txt](https://github.com/user-attachments/files/27902886/Episode_Paul_Garza.txt)# Dr. House: The Visual Novel

A C++ terminal game where you play Dr. Gregory House trying to diagnose a patient before they die, you end up at Mayfield Psychiatric, or you bankrupt Princeton-Plainsboro Teaching Hospital. You order tests, argue with the team, lie to Cuddy for money, and occasionally pop a Vicodin.

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

<table>
  <tr>
    <td align="center">
      <img width="658" height="343" alt="Screenshot 2026-05-17 at 17 46 21" src="https://github.com/user-attachments/assets/ce2da29d-c22a-4e0b-a1a7-6e5935b9f9cb" />
      <br>
    </td>
    <td align="center">
      <img width="691" height="347" alt="Screenshot 2026-05-17 at 17 41 55" src="https://github.com/user-attachments/assets/2b808d32-3616-4efe-82d3-a365f5e3c7b1" />
      <br>
    </td>
  </tr>
</table>

There are four critical stats. Lose control of them, and it's Game Over.

| Stat |  Starting Value | Win/Loss Condition |
| :---: | :---: | :--- |
| **Health** | 30-80 | Drops to 0 → Patient dies (PatientDeathException) |
| **Clarity** | 0% | Hits 100% AND Health ≥ 10 → You Win (Eureka Finale) |
| **Malpractice** | 0% | Hits 100% → Tritter gets you (FiredByHospitalException) |
| **Budget** | $8,000 | Drops below $0 → Hospital pulls the plug (OutOfBudgetException) |
| **Vicodin Level** | 0 / 5 | Hits 5 → Sent to Mayfield Psychiatric (MayfieldWardException) |

### The Core Mechanics

<table>
  <tr>
    <td align="center">
      <img width="601" height="437" alt="Screenshot 2026-05-17 at 17 45 23" src="https://github.com/user-attachments/assets/53586d09-159c-4800-a11e-b3e58a461afd" />
      <br>
    </td>
  </tr> 
</table>

* **Medical Intervention:** Order from a list of Lab Tests, Treatments, or Risky Procedures. The LLM evaluates your choice against the hidden canonical diagnosis to award Clarity or penalize you.

<table>
  <tr>
    <td align="center">
      <img width="565" height="252" alt="Screenshot 2026-05-17 at 17 47 26" src="https://github.com/user-attachments/assets/d45bf249-e337-4935-b72e-aca84c4df0d4" />
      <br>
    </td>
     <td align="center">
      <img width="815" height="281" alt="Screenshot 2026-05-17 at 17 43 01" src="https://github.com/user-attachments/assets/79c921f0-429d-4b01-a8dc-ad314de8579a" />
      <br>
    </td>
  </tr> 
</table>
<table>
   <tr>
    <td align="center">
      <img width="565" height="419" alt="Screenshot 2026-05-17 at 17 47 33" src="https://github.com/user-attachments/assets/88d5ca30-ea55-4422-8c15-fdfe4f87f7bd" />
      <br>
    </td>
    <td align="center">
      <img width="564" height="400" alt="Screenshot 2026-05-17 at 17 47 43" src="https://github.com/user-attachments/assets/88530b80-b4d1-4744-adb0-24621ef2860a" />
      <br>
    </td>
    <td align="center">
      <img width="560" height="301" alt="Screenshot 2026-05-17 at 17 47 55" src="https://github.com/user-attachments/assets/e35da911-20cf-46cd-9021-ea6c66e2e2d7" />
      <br>
    </td>
  </tr>
</table>


* **Social Interaction:** Send the team (Chase, Cameron, Foreman) to brainstorm (triggers parallel LLM calls for unique perspectives), ask Wilson for a hint, or beg Cuddy for emergency funding.

<table>
  <tr>
    <td align="center">
      <img width="581" height="149" alt="Screenshot 2026-05-17 at 18 01 30" src="https://github.com/user-attachments/assets/826b2895-b9f2-49da-a13e-6691bc134778" />
      <br>
    </td>
     <td align="center">
      <img width="995" height="611" alt="Screenshot 2026-05-16 at 17 02 26" src="https://github.com/user-attachments/assets/073e47e8-b3c5-4727-b3e8-9b9dcffaba3d" />
      <br>
    </td>
  </tr> 
</table>

* **The "Chaos" Menu:** Break into the patient's house for clues, skip clinic duty, or pop a Vicodin for a quick Clarity boost.

<table>
  <tr>
    <td align="center">
      <img width="571" height="187" alt="Screenshot 2026-05-17 at 18 05 42" src="https://github.com/user-attachments/assets/272eba25-0643-46d3-9656-2926da76c6f0" />
      <br>
    </td>
     <td align="center">
      <img width="820" height="229" alt="Screenshot 2026-05-17 at 17 44 49" src="https://github.com/user-attachments/assets/265d4949-832a-4e13-96d9-440006797c5c" />
      <br>
    </td>
  </tr> 
</table>


* **The Whiteboard (Free-Text):** Type your own medical theories into a prompt. The AI acts as House's internal monologue to guide you. **Hidden Easter Egg:** If your text input contains a substring of the actual hidden disease, you trigger a "Eureka" spark and gain a massive +5% Clarity bonus!
* **The Eureka Finale:** Once you hit 100% Clarity, the POV shifts to the patient for a 3-round conversational boss fight where House delivers the final diagnosis.


https://github.com/user-attachments/assets/49739ade-fb95-44c9-a2b8-6e7545da6034


### Post-Game Analysis
Win or lose, the game serializes every action you took and offers two distinct AI-generated wrap-ups:
1. **Foreman's Post-Mortem:** The LLM reviews your exact `ActionRecord` log, reviews your medical errors, and explains the optimal clinical path you *should* have taken. Saved to `Notes_<PatientName>.txt`.

<table>
  <tr>
    <td align="center">
      <img width="819" height="565" alt="Screenshot 2026-05-17 at 18 08 05" src="https://github.com/user-attachments/assets/c19dc451-3989-4f8a-97f4-8743e5efe92d" />
      <br>
    </td>
  </tr> 
</table>

2. **Director's Cut:** The AI takes your entire playthrough log and writes a 2-page dramatic TV script. Saved to `Episode_<PatientName>.txt`.

## Have fun!! :D
