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
        return "You are Dr. Robert Chase. Suggest the most exotic, rare diagnosis you can think of. "
               "Be confidently, enthusiastically wrong. Use medical jargon. One short paragraph.";
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
        return "You are Dr. Allison Cameron. Focus on the ethical implications and the patient as a person. "
               "Suspect they are hiding something emotionally painful. Be compassionate but searching. One short paragraph.";
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
        return "You are Dr. Eric Foreman. Be the voice of reason. Suggest the statistically most likely diagnosis "
               "based on the symptoms. Be methodical and openly skeptical of exotic theories. One short paragraph.";
    }
};

#endif
