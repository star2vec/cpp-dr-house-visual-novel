#ifndef LLM_H
#define LLM_H

#include <string>
#include <vector>

// Structura pentru conversatii
struct DialogueResponse {
    std::string houseLine;
    std::string characterReply;
};

// Structura pentru povestea de la inceput
struct PatientBackstory {
    std::string story;
};

// Structura pentru consecintele medicale
struct MedicalOutcome {
    std::string narrative;
    int healthDelta;
    int clarityDelta;
    int malpracticeDelta;
};

struct PatientProfile {
    std::string name;
    int health;
    std::string symptom;
    std::string story; // Folosim povestea direct aici, sa nu mai incarcam AI-ul iar!
};

class LLM {
public:
    LLM();

    // Functiile vechi (Meniul Social)
    std::vector<std::string> getHouseIntents(const std::string& character);
    DialogueResponse generateDialogue(const std::string& character, const std::vector<std::string>& history, const std::string& chosenIntent);

    // Functiile NOI (Meniul Medical)
    PatientBackstory generateAdmissionStory(const std::string& name, int health, const std::string& symptom);
    MedicalOutcome evaluateMedicalAction(const std::string& actionType, const std::string& actionName, int currentHealth, int currentClarity, const std::string& symptom);
    std::vector<PatientProfile> generatePatientFiles(int count = 3);
};

#endif