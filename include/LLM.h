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
    std::string brief;       // <=8 words for case board, e.g. "elevated WBC, bilateral infiltrates"
    std::string gloss;       // <=14 words layperson translation of `brief`, no medical jargon.
                             //   Example: "Body's fighting something in the lungs."
                             //   Empty string if the model didn't produce one (renderer skips).
    int healthDelta;
    int clarityDelta;
    int malpracticeDelta;
};

struct WilsonResult {
    // 2-4 alternating "House: …" / "Wilson: …" lines. preamble[0] is always House
    // (he initiates with a brief context line). Each line <=15 words; lines must
    // reference each other for continuity. preamble ends on a Wilson line so the
    // payoff `insight` (always "Wilson: …") follows naturally.
    std::vector<std::string> preamble;
    std::string insight;     // final Wilson line — the lifestyle/context payoff
    std::string brief;       // <=8 words for case board, e.g. "probed diet and toxic exposure"
    bool matched = false;    // true if the player's pre-committed category aligns with the true workup
};

struct CuddyDealResult {
    // 2-4 alternating "House: …" / "Cuddy: …" lines. preamble[0] is always House
    // (he's the one asking for money). House works angles (urgency, patient leverage);
    // Cuddy pushes back (cost, board pressure, prior grants).
    std::vector<std::string> preamble;
    std::string verdict;     // final Cuddy line — grants or refuses the funds
    bool approved;           // mirrors cuddyFundsGranted < 2 (deterministic gate)
};

struct TeamOpinionResult {
    std::string opinion;     // full spoken text (typewriter-printed)
    std::string brief;       // <=8 words for case board, e.g. "suspects autoimmune, wants ANA"
};

// One subtle House-voice nudge surfaced in the post-round steering menu.
// Each option is tied to one of the 4 medical-action types so the menu doubles
// as a category prompt for layperson players.
struct BrainstormSteer {
    std::string houseLine;   // <=16 words, in House's voice, subtle
    std::string actionType;  // exactly one of: "Lab Test" / "Treatment" / "Supportive Care" / "Risky Procedure"
};

struct BrainstormSteers {
    std::vector<BrainstormSteer> options;  // exactly 4 entries, one per action type
};

struct EurekaRoundResult {
    std::string houseLine;
    std::vector<std::string> patientOptions; // 3 contextual response options for player
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
    MedicalOutcome evaluateMedicalAction(const std::string& actionType, const std::string& actionName, int currentHealth, int currentClarity, const std::string& symptom, const std::string& hiddenDiagnosis, int diseaseSeverity = 1);
    std::vector<PatientProfile> generatePatientFiles(int count = 3);

    // Phase 5: Team Brainstorm (AIAgent hierarchy).
    // priorTranscript + houseNudge are empty on round 1 and populated for follow-up rounds
    // when the player has steered the team toward a category. Agents see prior beats so
    // round 2/3 callbacks and reactions to House's nudge feel natural.
    TeamOpinionResult generateTeamOpinion(const std::string& personality, const std::string& agentName,
                                          const std::string& symptom, const std::string& hiddenDiagnosis,
                                          int clarity,
                                          const std::vector<std::string>& priorTranscript = {},
                                          const std::string& houseNudge = "");

    // Post-round steering menu: 4 House-voice nudges, one per action type.
    BrainstormSteers generateBrainstormSteers(const std::string& symptom,
                                              const std::string& hiddenDiagnosis,
                                              const std::vector<std::string>& priorTranscript);

    // Phase 6: Chaos menu — illegal home visit clue
    std::string generateHouseClue(const std::string& hiddenDiagnosis,
                                  const std::string& patientName,
                                  const std::string& symptom);

    // Wilson consult — House-initiated multi-line scene ending in a lifestyle/context payoff.
    // playerCategory: the medical-action category the player pre-committed to before consulting
    //   (one of "Lab Test" / "Treatment" / "Risky Procedure" / "Supportive Care", or "" if the
    //   caller skipped the commitment step). When non-empty, Wilson reacts to whether that axis
    //   aligns with the diagnosis; result.matched signals the verdict back to the caller.
    WilsonResult generateWilsonConsult(const std::string& symptom,
                                       const std::string& hiddenDiagnosis,
                                       const std::string& patientName,
                                       int clarity,
                                       const std::string& playerCategory = "");

    // Cuddy emergency-funds negotiation. House-initiated; tone scales with previousGrants
    // (0 = cautious-but-relenting, 1 = angry-and-tight, >=2 = refusal scene).
    CuddyDealResult generateCuddyDeal(int currentBudget,
                                      int requestAmount,
                                      int previousGrants,
                                      int malpractice,
                                      const std::string& patientName,
                                      const std::string& patientSymptom);

    // Whiteboard — free-text medical brainstorm
    std::string generateWhiteboardThought(const std::string& question,
                                          const std::string& symptom,
                                          const std::string& hiddenDiagnosis);

    // Phase 7: Eureka Finale
    std::string generatePatientMonologue(const std::string& patientName,
                                         const std::string& symptom);
    EurekaRoundResult generateEurekaDialogue(const std::string& hiddenDiagnosis,
                                             const std::string& patientName,
                                             const std::string& patientComment,
                                             int round,
                                             const std::vector<std::string>& history);

    // Phase 8: Director's Cut
    std::string generateEpisodeScript(const std::string& gameLogSummary,
                                      const std::string& patientName);

    // Foreman's Post-Mortem — generic optimal-workup debrief, grounded in the
    // game's actual action registry. The model is told to ONLY name tests /
    // treatments / supportive care from the provided list, verbatim.
    std::string generateForemanPostMortem(const std::string& hiddenDiagnosis,
                                          const std::string& patientSymptom,
                                          const std::vector<std::pair<std::string, std::string>>& allActions);
};

#endif