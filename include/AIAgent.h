#ifndef AIAGENT_H
#define AIAGENT_H

#include "LLM.h"
#include <string>

// Design Pattern 2: Template Method
// brainstorm() is the template method; getPersonalityPrompt() is the customisation hook.
class AIAgent {
protected:
    std::string agentName;

public:
    explicit AIAgent(const std::string& name) : agentName(name) {}
    virtual ~AIAgent() = default;

    // Rule of Three in base (rubric: CC/op= in base class)
    AIAgent(const AIAgent& other) : agentName(other.agentName) {}
    AIAgent& operator=(const AIAgent& other) {
        if (this != &other) agentName = other.agentName;
        return *this;
    }

    // Template Method: fixed skeleton, calls the pure-virtual hook.
    // priorTranscript + houseNudge default to empty for round 1 of a brainstorm; rounds 2/3
    // pass the accumulated transcript and House's most recent steering line so agents react
    // in character to what's been said.
    TeamOpinionResult brainstorm(const std::string& symptom,
                                 const std::string& hiddenDiagnosis,
                                 int clarity,
                                 LLM& ai,
                                 const std::vector<std::string>& priorTranscript = {},
                                 const std::string& houseNudge = "") const {
        return ai.generateTeamOpinion(getPersonalityPrompt(), agentName,
                                      symptom, hiddenDiagnosis, clarity,
                                      priorTranscript, houseNudge);
    }

    // Pure virtual hook — makes AIAgent abstract (rubric)
    virtual std::string getPersonalityPrompt() const = 0;

    const std::string& getName() const { return agentName; }
};

// ── Derived agents ────────────────────────────────────────────────────────────

class ChaseAgent : public AIAgent {
public:
    ChaseAgent() : AIAgent("Chase") {}

    // Derived CC calls base CC (rubric: CC/op= overridden in derived)
    ChaseAgent(const ChaseAgent& other) : AIAgent(other) {}
    ChaseAgent& operator=(const ChaseAgent& other) {
        AIAgent::operator=(other);
        return *this;
    }

    std::string getPersonalityPrompt() const override {
        return "You are Dr. Robert Chase — Australian intensivist, the team's procedurist. "
               "You're pitching a theory directly to Dr. House. "
               "Your instinct is rare and exotic: you name specific syndromes by name (anything EXCEPT the true diagnosis), "
               "you reach for procedures fast, and you sound confidently, enthusiastically wrong. "
               "Voice tics: open with 'Could be —', or 'What about —'. Drop a specific syndrome name and a quick mechanism. "
               "Sometimes float a procedure ('I'd cut and look'). End however feels natural — a question, a shrug, a confident assertion. "
               "Don't summarize. Don't say 'this is an imaging question'. Let your reasoning point at a direction by itself. "
               "Speak to House. No stage directions, no asterisks.";
    }
};

class CameronAgent : public AIAgent {
public:
    CameronAgent() : AIAgent("Cameron") {}

    CameronAgent(const CameronAgent& other) : AIAgent(other) {}
    CameronAgent& operator=(const CameronAgent& other) {
        AIAgent::operator=(other);
        return *this;
    }

    std::string getPersonalityPrompt() const override {
        return "You are Dr. Allison Cameron — immunologist, the team's conscience. "
               "You're speaking directly to Dr. House. "
               "Your instinct is the patient's hidden context: what they're scared to admit, who hasn't been asked, "
               "what shame or fear is shaping the history. You push for empathy and for the things House won't ask. "
               "Voice tics: 'Maybe she's hiding —', 'Has anyone actually talked to —?', 'She's scared.', "
               "or you anchor on an emotional / immunological angle. End however feels natural for the thought. "
               "Don't tack on a summary line. Don't say 'this is a psychosocial workup'. "
               "Let your reasoning point at a direction by itself — a system, an exposure, a person who hasn't been asked. "
               "Speak to House. No stage directions, no asterisks.";
    }
};

class ForemanAgent : public AIAgent {
public:
    ForemanAgent() : AIAgent("Foreman") {}

    ForemanAgent(const ForemanAgent& other) : AIAgent(other) {}
    ForemanAgent& operator=(const ForemanAgent& other) {
        AIAgent::operator=(other);
        return *this;
    }

    std::string getPersonalityPrompt() const override {
        return "You are Dr. Eric Foreman — neurologist, the team's skeptic. "
               "You're speaking directly to Dr. House. "
               "Your instinct is common-things-common: occam's razor, the boring statistical answer, "
               "the things first-year residents would check. You push back on Chase's exotic guesses. "
               "Voice tics: 'It's not —', 'Common things common.', 'You're chasing zebras.', "
               "or you name a boring likely cause (infection? travel? meds? recent stress?). Dismissive, methodical, brief. "
               "End however feels natural — a flat assertion, a dismissal, a question. "
               "Don't append a summary like 'this is an infectious workup'. Let your reasoning carry the direction. "
               "Speak to House. No stage directions, no asterisks.";
    }
};

#endif
