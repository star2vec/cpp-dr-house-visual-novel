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
    AIAgent(const std::string& name) : agentName(name) {}
    virtual ~AIAgent() = default;

    // Rule of Three in base (rubric: CC/op= in base class)
    AIAgent(const AIAgent& other) : agentName(other.agentName) {}
    AIAgent& operator=(const AIAgent& other) {
        if (this != &other) agentName = other.agentName;
        return *this;
    }

    // Template Method: fixed skeleton, calls the pure-virtual hook
    std::string brainstorm(const std::string& symptom,
                           const std::string& hiddenDiagnosis,
                           int clarity,
                           LLM& ai) const {
        return ai.generateTeamOpinion(getPersonalityPrompt(), agentName,
                                      symptom, hiddenDiagnosis, clarity);
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
        return "You are Dr. Robert Chase in a diagnostic team meeting. The patient is NOT in the room. "
               "You are pitching a theory directly to Dr. House. Suggest the most exotic, rare diagnosis "
               "you can think of. Be confidently, enthusiastically wrong. Use medical jargon. "
               "Speak to House, not to the patient. No stage directions or action descriptions.";
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
        return "You are Dr. Allison Cameron in a diagnostic team meeting. The patient is NOT in the room. "
               "You are speaking directly to Dr. House and the team. Focus on the ethical implications "
               "and what the patient's history might be hiding emotionally. Be compassionate but analytical. "
               "Speak to House, not to the patient. No stage directions or action descriptions.";
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
        return "You are Dr. Eric Foreman in a diagnostic team meeting. The patient is NOT in the room. "
               "You are speaking directly to Dr. House. Be the voice of reason — suggest the statistically "
               "most likely diagnosis, be methodical, and push back on exotic theories. "
               "Speak to House, not to the patient. No stage directions or action descriptions.";
    }
};

#endif
