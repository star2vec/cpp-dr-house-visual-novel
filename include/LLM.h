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
    std::string story;
    std::string hiddenDiagnosis; // True disease — never displayed to player
    int diseaseSeverity = 1;     // 1 (mild) / 2 (moderate) / 3 (critical)
};

class LLM {
public:
    LLM();

    // Functiile vechi (Meniul Social)
    std::vector<std::string> getHouseIntents(const std::string& character);
    DialogueResponse generateDialogue(const std::string& character, const std::vector<std::string>& history,
                                      const std::string& chosenIntent,
                                      const std::string& patientName = "",
                                      const std::string& symptom = "");

    // Functiile NOI (Meniul Medical)
    PatientBackstory generateAdmissionStory(const std::string& name, int health, const std::string& symptom);
    MedicalOutcome evaluateMedicalAction(const std::string& actionType, const std::string& actionName, int currentHealth, int currentClarity, const std::string& symptom, const std::string& hiddenDiagnosis);
    std::vector<PatientProfile> generatePatientFiles(int count = 3);

    // Phase 5: Team Brainstorm (AIAgent hierarchy)
    std::string generateTeamOpinion(const std::string& personality, const std::string& agentName,
                                    const std::string& symptom, const std::string& hiddenDiagnosis,
                                    int clarity);

    // Phase 6: Chaos menu — illegal home visit clue
    std::string generateHouseClue(const std::string& hiddenDiagnosis,
                                  const std::string& patientName,
                                  const std::string& symptom);

    // Wilson consult — lifestyle/context sounding board
    std::string generateWilsonConsult(const std::string& symptom,
                                      const std::string& hiddenDiagnosis,
                                      const std::string& patientName,
                                      int clarity);

    // Whiteboard — free-text medical brainstorm
    std::string generateWhiteboardThought(const std::string& question,
                                          const std::string& symptom,
                                          const std::string& hiddenDiagnosis);

    // Phase 7: Eureka Finale
    std::string generatePatientMonologue(const std::string& patientName,
                                         const std::string& symptom);
    std::string generateEurekaDialogue(const std::string& hiddenDiagnosis,
                                       const std::string& patientName,
                                       const std::string& patientComment,
                                       int round,
                                       const std::vector<std::string>& history);

    // Phase 8: Director's Cut
    std::string generateEpisodeScript(const std::string& gameLogSummary,
                                      const std::string& patientName);
};

#endif