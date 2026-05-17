#ifndef MORNING_SCRIPTS_H
#define MORNING_SCRIPTS_H

#include <string>
#include <vector>

// Each of the 12 hardcoded morning montages is an ordered list of (speaker, line)
// pairs. The HospitalStaff hierarchy stays in charge of *rendering* each line
// (colored prefix + typewriter via HospitalStaff::say); these scripts are just the
// conversational beats. Speaker is a plain tag so the scripts themselves don't
// carry pointers — main.cpp resolves Speaker -> HospitalStaff* at dispatch time.
enum class Speaker { Cuddy, Wilson, Foreman, House };

struct ScriptLine {
    Speaker     who;
    std::string line;
};

using MorningScript = std::vector<ScriptLine>;

// Returns a const reference to one of 12 morning scripts, chosen at random per call.
// Pool is built once on first call and cached; RNG is seeded with std::time(nullptr)
// matching the existing pickDiverseNames pattern in src/LLM.cpp.
const MorningScript& pickRandomMorningScript();

#endif
