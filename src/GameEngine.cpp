#include "GameEngine.h"
#include "AIAgent.h"
#include "Exceptions.h"
#include "MedicalActionFactory.h"
#include "TerminalUI.h"
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

namespace {
// Plays one line of the form "Speaker: body". Colors the speaker prefix and
// typewrites the body so existing punctuation-pause + skip-on-keypress behavior
// is preserved. Falls back to plain typewriting if the line has no recognized prefix.
void playDialogueLine(const std::string& line) {
    struct Speaker { const char* prefix; size_t len; const char* color; };
    static const Speaker speakers[] = {
        {"House: ",  7, "\033[1;36m"},
        {"Wilson: ", 8, "\033[1;33m"},
        {"Cuddy: ",  7, "\033[1;35m"},
    };
    for (const auto& s : speakers) {
        if (line.rfind(s.prefix, 0) == 0) {
            std::string label(s.prefix, s.len - 2);  // strip ": "
            std::cout << s.color << label << ":\033[0m ";
            std::cout.flush();
            TerminalUI::typewrite(line.substr(s.len));
            return;
        }
    }
    TerminalUI::typewrite(line);
}
}  // namespace

GameEngine::GameEngine(const Patient& p) : patient(p) {}

void GameEngine::checkEndings() {
    if (patient.getHealth() <= 0)        throw PatientDeathException(turn);
    if (patient.getMalpractice() >= 100) throw FiredByHospitalException(turn);
    if (budget < 0)                      throw OutOfBudgetException(turn);
    if (vicodinLevel >= 5)               throw MayfieldWardException(turn);
}

// --- SUB-MENIURI ---

void GameEngine::showMedicalSubMenu() {
    MedicalActionFactory factory;
    const auto& allActions = factory.getAllActions();

    // Category labels and their corresponding action type strings
    const std::vector<std::string> catLabels = {
        "Run a Lab Test",
        "Order a Treatment",
        "Supportive Care          (stabilize, buy time)",
        "Risky Procedure",
        "Go Back"
    };
    const std::vector<std::string> catTypes = {
        "Lab Test",
        "Treatment",
        "Supportive Care",
        "Risky Procedure",
        ""
    };

    std::string actionName, actionType;

    // Outer loop: return here if player hits Go Back from level 2
    while (true) {
        // --- Level 1: Category selection ---
        int catIndex = 0;
        bool catChosen = false;
        while (!catChosen) {
            TerminalUI::clearScreen();
            std::cout << "\n--- [ MEDICAL INTERVENTION ] ---\n";
            std::cout << "Patient: " << patient.getName()
                      << "  |  Health: " << patient.getHealth()
                      << "  |  Clarity: " << patient.getClarity() << "%"
                      << "  |  Budget: $" << budget << "\n";
            if (budget < 1500)
                std::cout << "\033[1;31m!! WARNING: Budget nearly depleted ($" << budget << "). Pick spending carefully.\033[0m\n";
            else if (budget < 3000)
                std::cout << "\033[1;33m!  CAUTION: Budget running low ($" << budget << "). Pick spending carefully.\033[0m\n";
            std::cout << "\nWhat is your approach?\n\n";
            for (int i = 0; i < static_cast<int>(catLabels.size()); ++i) {
                if (i == catIndex)
                    std::cout << "  -> \033[1;36m" << catLabels[i] << "\033[0m\n";
                else
                    std::cout << "     " << catLabels[i] << "\n";
            }
            renderCaseBoard();

            int key = TerminalUI::getKeyPress();
            if (key == 1) { catIndex--; if (catIndex < 0) catIndex = static_cast<int>(catLabels.size()) - 1; }
            else if (key == 2) { catIndex++; if (catIndex >= static_cast<int>(catLabels.size())) catIndex = 0; }
            else if (key == 3) catChosen = true;
        }

        if (catIndex == static_cast<int>(catLabels.size()) - 1) return; // Go Back → main menu

        const std::string& chosenType = catTypes[catIndex];

        // Build filtered list for this category
        std::vector<std::pair<std::string, std::string>> filtered;
        for (const auto& [name, type] : allActions)
            if (type == chosenType) filtered.push_back({name, type});

        // --- Level 2: Action selection ---
        int selIndex = 0;
        bool actionChosen = false;
        while (!actionChosen) {
            TerminalUI::clearScreen();
            std::cout << "\n--- [ " << catLabels[catIndex] << " ] ---\n";
            std::cout << "Patient: " << patient.getName()
                      << "  |  Health: " << patient.getHealth()
                      << "  |  Clarity: " << patient.getClarity() << "%"
                      << "  |  Budget: $" << budget << "\n";
            if (budget < 1500)
                std::cout << "\033[1;31m!! WARNING: Budget nearly depleted ($" << budget << "). Pick spending carefully.\033[0m\n";
            else if (budget < 3000)
                std::cout << "\033[1;33m!  CAUTION: Budget running low ($" << budget << "). Pick spending carefully.\033[0m\n";
            std::cout << "\nSelect (UP/DOWN and ENTER):\n\n";
            for (size_t i = 0; i < filtered.size(); ++i) {
                int cost = factory.getCost(filtered[i].first);
                if (i == static_cast<size_t>(selIndex))
                    std::cout << "  -> \033[1;36m" << filtered[i].first << "  ($" << cost << ")\033[0m\n";
                else
                    std::cout << "     " << filtered[i].first << "  ($" << cost << ")\n";
            }
            if (selIndex == static_cast<int>(filtered.size()))
                std::cout << "  -> \033[1;31m<< Back to categories\033[0m\n";
            else
                std::cout << "     << Back to categories\n";

            renderCaseBoard();

            int key = TerminalUI::getKeyPress();
            if (key == 1) { selIndex--; if (selIndex < 0) selIndex = static_cast<int>(filtered.size()); }
            else if (key == 2) { selIndex++; if (selIndex > static_cast<int>(filtered.size())) selIndex = 0; }
            else if (key == 3) actionChosen = true;
        }

        if (selIndex == static_cast<int>(filtered.size())) continue; // Back to category selection

        actionName = filtered[selIndex].first;
        actionType = filtered[selIndex].second;

        // Risky Procedure honesty pass: surface the actual EV before the player commits.
        // Hidden odds breed rage; transparent odds breed decisions.
        if (actionType == "Risky Procedure") {
            TerminalUI::clearScreen();
            std::cout << "\n--- [ RISK ASSESSMENT ] ---\n\n";
            std::cout << "Procedure: \033[1m" << actionName << "\033[0m  (Cost: $"
                      << factory.getCost(actionName) << ")\n\n";
            std::cout << "  Risk assessment: \033[1;31m60% bad outcomes\033[0m "
                      << "/ \033[1;32m40% productive outcomes\033[0m\n";
            std::cout << "    \033[1;31m40%\033[0m  Catastrophe   (-30 HP, +30% malpractice)\n";
            std::cout << "    \033[2m40%  Nothing found (no clarity, +5% malpractice)\033[0m\n";
            std::cout << "    \033[1;32m20%\033[0m  Jackpot       (+50% clarity, +20 HP, +10% malpractice)\n\n";
            std::cout << "House's hand hovers over the consent form.\n\n";
            std::cout << "  [ENTER] Proceed     [UP/DOWN] Cancel and pick something else\n";
            int key = TerminalUI::getKeyPress();
            if (key != 3) {
                continue; // back to category selection without burning a turn
            }
        }
        break;
    }

    // --- Execute chosen action ---
    std::unique_ptr<MedicalAction> selectedAction = factory.create(actionName);
    int cost = factory.getCost(actionName);
    selectedAction->recordPerformed();

    TerminalUI::clearScreen();
    std::cout << "\n[ " << actionType << " ] " << actionName << "  (Cost: $" << cost << ")\n";
    std::cout << "\nDetermining outcome... (House is thinking)\n";

    MedicalOutcome outcome = aiBrain.evaluateMedicalAction(
        actionType,
        actionName,
        patient.getHealth(),
        patient.getClarity(),
        patient.getSymptom(),
        patient.getHiddenDiagnosis(),
        patient.getDiseaseSeverity()
    );

    // Risky Procedure variance roulette — high price tag is the gambler's tax.
    // 40% catastrophic / 40% nothing / 20% jackpot. EV on clarity is now break-even,
    // not positive: don't spam this as a late-game win button. Override the LLM's
    // deltas; the LLM narrative still prints first, then we tag the bucket explicitly.
    std::string riskyTag;
    if (actionType == "Risky Procedure") {
        int roll = std::rand() % 100;
        if (roll < 40) {
            outcome.healthDelta      = -30;
            outcome.clarityDelta     =   0;
            outcome.malpracticeDelta = +30;
            riskyTag = "\033[1;31m[ The procedure goes catastrophically. Bleeding won't stop. ]\033[0m";
        } else if (roll < 80) {
            outcome.healthDelta      =   0;
            outcome.clarityDelta     =   0;
            outcome.malpracticeDelta =  +5;
            riskyTag = "\033[2m[ Procedure complete. No useful findings. ]\033[0m";
        } else {
            outcome.healthDelta      = +20;
            outcome.clarityDelta     = +50;
            outcome.malpracticeDelta = +10;
            riskyTag = "\033[1;32m[ Jackpot. The procedure cracks the case wide open. ]\033[0m";
        }
    }

    patient.modifyHealth(outcome.healthDelta);
    patient.modifyClarity(outcome.clarityDelta);
    patient.modifyMalpractice(outcome.malpracticeDelta);
    budget -= cost;

    narrativeLog += std::string("[") + actionType + ": " + actionName + "] " + outcome.narrative;
    ActionRecord record;
    record.actionName        = actionName;
    record.actionType        = actionType;
    record.brief             = outcome.brief;
    record.gloss             = outcome.gloss;
    record.healthDelta       = outcome.healthDelta;
    record.clarityDelta      = outcome.clarityDelta;
    record.malpracticeDelta  = outcome.malpracticeDelta;
    record.budgetSpent       = cost;
    actionLog += record;

    TerminalUI::clearScreen();
    std::cout << "\n=== PROCEDURE RESULTS ===\n\n";
    TerminalUI::typewrite(outcome.narrative);
    std::cout << "\n\n";
    if (!outcome.gloss.empty())
        std::cout << "\033[2m  -> " << outcome.gloss << "\033[0m\n\n";
    if (!riskyTag.empty())
        std::cout << riskyTag << "\n\n";
    std::cout << "------------------------------------\n";
    std::cout << "Health:       " << (outcome.healthDelta > 0 ? "+" : "") << outcome.healthDelta << "  (Current: " << patient.getHealth() << "/100)\n";
    std::cout << "Clarity:      " << (outcome.clarityDelta > 0 ? "+" : "") << outcome.clarityDelta << "% (Current: " << patient.getClarity() << "%)\n";
    std::cout << "Malpractice:  " << (outcome.malpracticeDelta > 0 ? "+" : "") << outcome.malpracticeDelta << "% (Current: " << patient.getMalpractice() << "%)\n";
    std::cout << "Budget:       -$" << cost << "  (Remaining: $" << budget << ")\n";
    std::cout << "Total procedures this session: " << MedicalAction::getTotalActionsPerformed() << "\n";
    std::cout << "------------------------------------\n";

    std::cout << "\n(Press ENTER to return to the main room...)";
    while(TerminalUI::getKeyPress() != 3);
}

void GameEngine::showWhiteboardMenu() {
    TerminalUI::clearScreen();
    std::cout << "\n";
    std::cout << "  +------------------------------------------+\n";
    std::cout << "  |           [ THE WHITEBOARD ]             |\n";
    std::cout << "  +------------------------------------------+\n\n";
    std::cout << "House uncaps the marker. His eyes fix on the board.\n";
    std::cout << "Patient: \033[1m" << patient.getName() << "\033[0m";
    std::cout << "  |  Symptom: " << patient.getSymptom() << "\n\n";
    std::cout << "What's the theory burning in his skull?\n\n";
    std::cout << "> ";
    std::cout.flush();

    std::string question;
    std::getline(std::cin, question);

    if (question.empty()) return;

    std::cout << "\nHouse stares at the board. The marker squeaks.\n\n";

    std::string thought = aiBrain.generateWhiteboardThought(
        question, patient.getSymptom(), patient.getHiddenDiagnosis()
    );

    TerminalUI::typewrite(thought);
    std::cout << "\n\n";

    // +5% clarity reward if House's thought references any ≥4-char token from the
    // hidden diagnosis. Tokens that short are filtered to avoid false positives on
    // words like "the", "of", "and", "type". Naturally rate-limited: the LLM rarely
    // names the disease outright, so the bonus only fires when it genuinely lands.
    {
        std::string thoughtLow = thought;
        std::string diagLow    = patient.getHiddenDiagnosis();
        for (char& c : thoughtLow) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (char& c : diagLow)    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        bool hit = false;
        size_t i = 0;
        while (i < diagLow.size() && !hit) {
            while (i < diagLow.size() && diagLow[i] == ' ') ++i;
            size_t j = i;
            while (j < diagLow.size() && diagLow[j] != ' ') ++j;
            if (j - i >= 4) {
                std::string token = diagLow.substr(i, j - i);
                if (thoughtLow.find(token) != std::string::npos) hit = true;
            }
            i = j;
        }
        if (hit) {
            patient.modifyClarity(5);
            std::cout << "\033[1;32m  -> House zeroed in on something. (+5% clarity, now "
                      << patient.getClarity() << "%)\033[0m\n\n";
        }
    }

    narrativeLog += std::string("[WHITEBOARD] ") + question;

    std::cout << "(Press ENTER to return to the main room...)";
    while (TerminalUI::getKeyPress() != 3);
}

void GameEngine::showSocialSubMenu() {
    std::vector<std::string> socialOptions = {
        "Team Brainstorm       (Chase / Cameron / Foreman)",
        "Consult Wilson        (Case Sounding Board)  [+8% on match / +2% on miss]",
        "Talk to Cuddy             (Budget / Favors)",
        "Go Back"
    };

    int selectedIndex = 0;
    bool chosen = false;
    while (!chosen) {
        TerminalUI::clearScreen();
        std::cout << "\n--- [ SUB-MENU: SOCIAL INTERACTION ] ---\n\n";
        for (size_t i = 0; i < socialOptions.size(); ++i) {
            if (i == static_cast<size_t>(selectedIndex))
                std::cout << "  -> \033[1;36m" << socialOptions[i] << "\033[0m\n";
            else
                std::cout << "     " << socialOptions[i] << "\n";
        }
        int key = TerminalUI::getKeyPress();
        if (key == 1) { --selectedIndex; if (selectedIndex < 0) selectedIndex = static_cast<int>(socialOptions.size()) - 1; }
        else if (key == 2) { ++selectedIndex; if (selectedIndex >= static_cast<int>(socialOptions.size())) selectedIndex = 0; }
        else if (key == 3) { chosen = true; }
    }

    if (selectedIndex == 3) return; // Go Back

    // ── Team Brainstorm (1-to-3 rounds, House-voice steering between rounds) ──
    if (selectedIndex == 0) {
        TerminalUI::clearScreen();
        std::cout << "\n=== TEAM BRAINSTORM ===\n";
        std::cout << "House rolls his eyes. \"Fine. What do you idiots think?\"\n\n";

        // Upcast: stored as base pointers (rubric)
        std::vector<std::unique_ptr<AIAgent>> team;
        team.push_back(std::make_unique<ChaseAgent>());
        team.push_back(std::make_unique<CameronAgent>());
        team.push_back(std::make_unique<ForemanAgent>());

        std::vector<std::string> roundTranscript;
        std::string lastNudge;            // empty on round 1
        bool chaseLuckyApplied = false;   // gate the +15% clarity wildcard to once per session

        for (int round = 1; round <= 3; ++round) {
            // 1. Fan out: 3 agent calls in parallel. cpp-httplib client is created per-call
            //    inside generateTeamOpinion so each thread has its own connection.
            std::cout << "  (the team is thinking...)\n";
            std::cout.flush();

            std::vector<std::future<TeamOpinionResult>> futures;
            futures.reserve(team.size());
            for (auto& agent : team) {
                AIAgent* a = agent.get();
                std::vector<std::string> tscript = roundTranscript;
                std::string nudge = lastNudge;
                futures.push_back(std::async(std::launch::async,
                    [a, this, tscript = std::move(tscript), nudge = std::move(nudge)]() {
                        return a->brainstorm(patient.getSymptom(),
                                             patient.getHiddenDiagnosis(),
                                             patient.getClarity(),
                                             aiBrain, tscript, nudge);
                    }));
            }

            // 2. Drain in Chase→Cameron→Foreman order so typewriter playback feels orderly.
            for (size_t i = 0; i < team.size(); ++i) {
                TeamOpinionResult result = futures[i].get();

                std::cout << "\033[1;33m" << team[i]->getName() << ":\033[0m ";
                TerminalUI::typewrite(result.opinion);
                std::cout << "\n\n";

                roundTranscript.push_back(team[i]->getName() + ": " + result.opinion);
                narrativeLog += std::string("[BRAINSTORM: ") + team[i]->getName() + "] " + result.opinion;
                narrativeLog += std::string("[BRAINSTORM_BRIEF: ") + team[i]->getName() + "] " + result.brief;

                // Chase wildcard — fires at most once per session (gated)
                if (!chaseLuckyApplied) {
                    if (auto* chase = dynamic_cast<ChaseAgent*>(team[i].get())) {
                        if (std::rand() % 5 == 0) {
                            patient.modifyClarity(15);
                            std::cout << "\033[2m[Chase's exotic theory accidentally points somewhere useful. Clarity +15%.]\033[0m\n\n";
                            chaseLuckyApplied = true;
                        }
                        (void)chase;
                    }
                }
            }

            if (round == 3) break;

            // 3. Steering menu: 4 LLM-generated House-voice nudges + "Let it go for now".
            std::cout << "  (House paces...)\n";
            std::cout.flush();
            BrainstormSteers steers = aiBrain.generateBrainstormSteers(
                patient.getSymptom(), patient.getHiddenDiagnosis(), roundTranscript);

            const int totalOptions = static_cast<int>(steers.options.size()) + 1;  // +1 walk-out
            int sel = 0;
            bool picked = false;
            while (!picked) {
                TerminalUI::clearScreen();
                std::cout << "\n=== TEAM BRAINSTORM — Round " << round << " of 3 ===\n";
                std::cout << "House paces. \"Anything else, or are we done?\"\n\n";
                for (size_t j = 0; j < steers.options.size(); ++j) {
                    const auto& opt = steers.options[j];
                    bool isSel = (j == static_cast<size_t>(sel));
                    if (isSel)
                        std::cout << "  -> \033[1;36m\"" << opt.houseLine << "\"\033[0m\n";
                    else
                        std::cout << "     \"" << opt.houseLine << "\"\n";
                    // Dim grey category hint underneath, same indent
                    std::cout << "     \033[2m[\xE2\x86\x92 push for " << opt.actionType << "]\033[0m\n\n";
                }
                bool isSel = (sel == totalOptions - 1);
                if (isSel)
                    std::cout << "  -> \033[1;36mLet it go for now.\033[0m\n";
                else
                    std::cout << "     Let it go for now.\n";

                int key = TerminalUI::getKeyPress();
                if (key == 1) { --sel; if (sel < 0) sel = totalOptions - 1; }
                else if (key == 2) { ++sel; if (sel >= totalOptions) sel = 0; }
                else if (key == 3) { picked = true; }
            }

            if (sel == totalOptions - 1) break;   // walked out

            lastNudge = steers.options[sel].houseLine;
            TerminalUI::clearScreen();
            std::cout << "\n=== TEAM BRAINSTORM — Round " << (round + 1) << " of 3 ===\n\n";
            std::cout << "\033[1;36mHouse:\033[0m ";
            TerminalUI::typewrite(lastNudge);
            std::cout << "\n\n";
        }

        std::cout << "\n(Press ENTER to return to the main room...)";
        while (TerminalUI::getKeyPress() != 3);
        return;
    }

    // ── Wilson Consult ────────────────────────────────────────────────────────
    if (selectedIndex == 1) {
        // Uses 1-3: normal consult.
        if (wilsonUsesLeft > 0) {
            // Pre-commit category picker: force the player to form a hypothesis before
            // walking in. Wilson rewards alignment (+8% clarity) and gently corrects a
            // miss (+2%) — turning grinding into hypothesis-forming.
            const std::vector<std::string> categoryLabels = {
                "Lab Test          (workup / diagnostics)",
                "Treatment         (meds / drugs / therapy)",
                "Risky Procedure   (biopsy / surgery / intervention)",
                "Supportive Care   (stabilize / observe / wait it out)"
            };
            const std::vector<std::string> categoryValues = {
                "Lab Test", "Treatment", "Risky Procedure", "Supportive Care"
            };
            int catIdx = 0;
            bool catChosen = false;
            while (!catChosen) {
                TerminalUI::clearScreen();
                std::cout << "\n=== WILSON'S OFFICE ===\n\n";
                std::cout << "Before you knock, House pauses. What's the angle you're bringing in?\n";
                std::cout << "\033[2m(Wilson rewards a sharp hypothesis. Vague guesses get less.)\033[0m\n\n";
                for (size_t i = 0; i < categoryLabels.size(); ++i) {
                    if (i == static_cast<size_t>(catIdx))
                        std::cout << "  -> \033[1;36m" << categoryLabels[i] << "\033[0m\n";
                    else
                        std::cout << "     " << categoryLabels[i] << "\n";
                }
                int key = TerminalUI::getKeyPress();
                if      (key == 1) { --catIdx; if (catIdx < 0) catIdx = static_cast<int>(categoryLabels.size()) - 1; }
                else if (key == 2) { ++catIdx; if (catIdx >= static_cast<int>(categoryLabels.size())) catIdx = 0; }
                else if (key == 3) catChosen = true;
            }
            const std::string& playerCategory = categoryValues[catIdx];

            TerminalUI::clearScreen();
            std::cout << "\n=== WILSON'S OFFICE ===\n";
            std::cout << "House limps in without knocking. Wilson looks up from his paperwork.\n";
            std::cout << "\033[2mHouse's hypothesis: " << playerCategory << ".\033[0m\n\n";
            std::cout << "  (Wilson is thinking...)\n";
            std::cout.flush();

            WilsonResult wilsonResult = aiBrain.generateWilsonConsult(
                patient.getSymptom(), patient.getHiddenDiagnosis(),
                patient.getName(), patient.getClarity(),
                playerCategory
            );

            TerminalUI::clearScreen();
            std::cout << "\n=== WILSON'S OFFICE ===\n\n";
            for (const auto& line : wilsonResult.preamble) {
                playDialogueLine(line);
                std::cout << "\n\n";
                narrativeLog += std::string("[WILSON] ") + line;
            }
            playDialogueLine(wilsonResult.insight);
            std::cout << "\n\n";
            narrativeLog += std::string("[WILSON] ") + wilsonResult.insight;
            narrativeLog += std::string("[WILSON_BRIEF] ") + wilsonResult.brief;

            const int clarityGain = wilsonResult.matched ? 8 : 2;
            patient.modifyClarity(clarityGain);
            --wilsonUsesLeft;
            if (wilsonResult.matched) {
                std::cout << "\033[1;32m[Wilson confirms the axis. The hypothesis lands.]\033[0m\n";
                std::cout << "Clarity: +" << clarityGain << "%  (Current: " << patient.getClarity() << "%)\n";
                narrativeLog += std::string("[WILSON_MATCH] ") + playerCategory;
            } else {
                std::cout << "\033[2m[Wilson's tone says you're chasing the wrong axis.]\033[0m\n";
                std::cout << "Clarity: +" << clarityGain << "%  (Current: " << patient.getClarity() << "%)\n";
                narrativeLog += std::string("[WILSON_MISS] ") + playerCategory;
            }
            std::cout << "\033[2m(Wilson visits remaining: " << wilsonUsesLeft << ")\033[0m\n";

            std::cout << "\n(Press ENTER to return to the main room...)";
            while (TerminalUI::getKeyPress() != 3);
            return;
        }

        // Use #4: Wilson's revenge — one-time hardcoded payback scene, -3 clarity.
        if (wilsonUsesLeft == 0) {
            static const std::vector<std::string> paybackScenes = {
                "Wilson swapped your coffee with decaf this morning. You only realize at the third sip. "
                "Your concentration cracks. You stare at the case board and the letters move.",
                "Wilson hid your cane. You spent twenty minutes leaning on a supply cart pretending it was on purpose. "
                "You couldn't think about the patient for the entire search.",
                "Wilson forwarded your office phone to the OB-GYN clinic. Three pregnant women called you "
                "personally before you figured it out. The case board went unread.",
                "Wilson rewrote the labels on your Vicodin bottle. You spent six minutes confirming they were real. "
                "Train of thought: derailed."
            };
            const std::string& scene = paybackScenes[std::rand() % paybackScenes.size()];

            TerminalUI::clearScreen();
            std::cout << "\n=== WILSON STRIKES BACK ===\n\n";
            TerminalUI::typewrite(scene);
            std::cout << "\n\n";
            patient.modifyClarity(-3);
            std::cout << "\033[1;31mClarity: -3%  (Current: " << patient.getClarity() << "%)\033[0m\n";
            std::cout << "\033[2m(Wilson is now permanently unavailable. Earned it.)\033[0m\n";
            narrativeLog += std::string("[WILSON_REVENGE] ") + scene;
            wilsonUsesLeft = -1;

            std::cout << "\n(Press ENTER to return to the main room...)";
            while (TerminalUI::getKeyPress() != 3);
            return;
        }

        // Use #5+: locked out, no turn taken.
        static const std::vector<std::string> hidingLines = {
            "Wilson's office is dark. He's been ducking you all morning.",
            "Wilson's secretary says he's 'in a meeting.' For five hours.",
            "Wilson is conveniently on a coffee run. Two floors up. With no coffee shop.",
            "Wilson hears you coming and shuts the door. You hear the lock turn."
        };
        TerminalUI::clearScreen();
        std::cout << "\n=== WILSON'S OFFICE ===\n\n";
        TerminalUI::typewrite(hidingLines[std::rand() % hidingLines.size()]);
        std::cout << "\n\n\033[2m(No turn taken.)\033[0m\n";
        lastChoiceNoOp = true;

        std::cout << "\n(Press ENTER to return to the main room...)";
        while (TerminalUI::getKeyPress() != 3);
        return;
    }

    // ── Cuddy: emergency-funds negotiation ────────────────────────────────────
    // Single-purpose scene — Cuddy exists in this menu only to grant or deny funds.
    // No chit-chat options; the House↔Cuddy dynamic lives in the LLM-generated dialogue.
    if (selectedIndex == 2) {
        TerminalUI::clearScreen();
        std::cout << "\n=== CUDDY'S OFFICE ===\n";
        std::cout << "House drops into the chair across from her without being invited.\n\n";
        std::cout << "  (Cuddy puts down her pen.)\n";
        std::cout.flush();

        // Cuddy is still angry about Skip-Clinic — bypass the LLM, force refusal.
        if (cuddyDeniedNext) {
            cuddyDeniedNext = false;
            TerminalUI::clearScreen();
            std::cout << "\n=== CUDDY'S OFFICE ===\n\n";
            std::cout << "\033[1;35mCuddy:\033[0m ";
            TerminalUI::typewrite("Out. OUT. You ditched clinic hours yesterday and now you want five thousand dollars?");
            std::cout << "\n\n";
            std::cout << "\033[1;31mHouse:\033[0m ";
            TerminalUI::typewrite("It's for the patient.");
            std::cout << "\n\n";
            std::cout << "\033[1;35mCuddy:\033[0m ";
            TerminalUI::typewrite("Then the patient can fill out my clinic paperwork. Get out.");
            std::cout << "\n\n";
            narrativeLog += std::string("[CUDDY] Refused funds — still mad about skipped clinic hours.");
            std::cout << "[No funds granted.]\n";
            std::cout << "\n(Press ENTER to return to the main room...)";
            while (TerminalUI::getKeyPress() != 3);
            return;
        }

        CuddyDealResult deal = aiBrain.generateCuddyDeal(
            budget, 5000, cuddyFundsGranted,
            patient.getMalpractice(), patient.getName(), patient.getSymptom()
        );
        const bool grant = (cuddyFundsGranted < 2);  // authoritative gate
        deal.approved = grant;

        TerminalUI::clearScreen();
        std::cout << "\n=== CUDDY'S OFFICE ===\n\n";
        for (const auto& line : deal.preamble) {
            playDialogueLine(line);
            std::cout << "\n\n";
            narrativeLog += std::string("[CUDDY] ") + line;
        }
        playDialogueLine(deal.verdict);
        std::cout << "\n\n";
        narrativeLog += std::string("[CUDDY] ") + deal.verdict;

        if (grant) {
            budget += 5000;
            patient.modifyMalpractice(8);
            ++cuddyFundsGranted;
            std::cout << "Budget:      +$5,000  (Remaining: $" << budget << ")\n";
            std::cout << "Malpractice: +8%      (Current: " << patient.getMalpractice() << "%)\n";
        } else {
            std::cout << "[No funds granted.]\n";
        }

        std::cout << "\n(Press ENTER to return to the main room...)";
        while (TerminalUI::getKeyPress() != 3);
        return;
    }
}

void GameEngine::showMiscellaneousSubMenu() {
    // Each entry's label is appended with "  [used]" or "  [locked]" when its
    // 1/game flag has been spent — keeps the option visible so the player
    // remembers what they've already burned this run.
    auto label = [](const std::string& base, bool spent) {
        return spent ? base + "    \033[2m[used]\033[0m" : base;
    };

    std::vector<std::string> chaosOptions = {
        std::string("Pop Vicodin                    (+10% clarity, +1 pill)"),
        label("Send the Team to Break In       (+22% clarity, +20% malpractice)", brokeInOnce),
        label("Skip Clinic Hours               (+5% clarity, -15% malpractice, Cuddy mad)", skippedClinicOnce),
        label("Prank a Colleague               (+10% clarity, +5% malpractice)", prankedOnce),
        label("Watch Monster Trucks            (-1 Vicodin level, +3% clarity)", monsterTrucksOnce),
        "Go Back"
    };

    int selectedIndex = 0;
    bool chosen = false;
    while (!chosen) {
        TerminalUI::clearScreen();
        std::cout << "\n--- [ HOUSE CHAOS ] ---\n";
        std::cout << "Vicodin level: " << vicodinLevel << "/5\n\n";
        for (size_t i = 0; i < chaosOptions.size(); ++i) {
            if (i == static_cast<size_t>(selectedIndex))
                std::cout << "  -> \033[1;35m" << chaosOptions[i] << "\033[0m\n";
            else
                std::cout << "     " << chaosOptions[i] << "\n";
        }
        int key = TerminalUI::getKeyPress();
        if (key == 1) { --selectedIndex; if (selectedIndex < 0) selectedIndex = static_cast<int>(chaosOptions.size()) - 1; }
        else if (key == 2) { ++selectedIndex; if (selectedIndex >= static_cast<int>(chaosOptions.size())) selectedIndex = 0; }
        else if (key == 3) { chosen = true; }
    }

    if (selectedIndex == 5) return; // Go Back

    TerminalUI::clearScreen();

    if (selectedIndex == 0) {
        // ── Pop Vicodin ───────────────────────────────────────────────────────
        ++vicodinLevel;
        std::cout << "\n[ VICODIN ] House dry-swallows a pill without breaking eye contact.\n";
        std::cout << "Vicodin level: " << vicodinLevel << "/5\n";
        // Mayfield triggers on the 5th pill — fires BEFORE the clarity bailout so the
        // player doesn't get a +10% consolation prize on a game-over turn.
        checkEndings(); // throws MayfieldWardException if vicodinLevel >= 5
        patient.modifyClarity(7);
        std::cout << "The pain recedes. So does his filter.\n";
        std::cout << "Clarity:      +7%  (Current: " << patient.getClarity() << "%)\n";
        if (vicodinLevel >= 4)
            std::cout << "\033[1;31mWarning: One more pill and Cuddy ships you to Mayfield.\033[0m\n";

    } else if (selectedIndex == 1) {
        // ── Send the Team to Break In (1/game) ────────────────────────────────
        // House never does it himself in canon. He texts Chase. Cameron rides shotgun.
        if (brokeInOnce) {
            std::cout << "\n[ ILLEGAL HOME VISIT ] ";
            TerminalUI::typewrite("Security has Chase and Cameron on file now. Sending them back "
                                  "tonight means handcuffs for them and a malpractice suit for you.");
            std::cout << "\n\n\033[2m(No turn taken.)\033[0m\n";
            lastChoiceNoOp = true;
        } else {
            std::cout << "\n[ ILLEGAL HOME VISIT ] You text Chase: 'Patient's address. Now. Take Cameron.'\n";
            TerminalUI::typewrite("Chase picks the lock at 2 a.m. Cameron watches the street, looking guilty.");
            std::cout << "\n";
            std::cout << "  (The team is snooping...)\n";
            std::cout.flush();
            std::string clue = aiBrain.generateHouseClue(
                patient.getHiddenDiagnosis(), patient.getName(), patient.getSymptom());
            patient.modifyClarity(22);
            patient.modifyMalpractice(20);
            narrativeLog += std::string("[CLUE] ") + clue;
            std::cout << "\n\033[1;36mChase calls back:\033[0m \033[1;33m\"";
            TerminalUI::typewrite(clue);
            std::cout << "\"\033[0m\n\n";
            std::cout << "Clarity:     +22%  (Current: " << patient.getClarity() << "%)\n";
            std::cout << "Malpractice: +20%  (Current: " << patient.getMalpractice() << "%)\n";
            brokeInOnce = true;
        }

    } else if (selectedIndex == 2) {
        // ── Skip Clinic Hours (1/game) ────────────────────────────────────────
        if (skippedClinicOnce) {
            std::cout << "\n[ CLINIC ] ";
            TerminalUI::typewrite("Cuddy's already locked the clinic schedule. She's watching the door.");
            std::cout << "\n\n\033[2m(No turn taken.)\033[0m\n";
            lastChoiceNoOp = true;
        } else {
            std::cout << "\n[ CLINIC ] House ducks out the side door. He doesn't see a single patient.\n";
            TerminalUI::typewrite("Three hours of nobody's-snotty-nose later, the case finally has space "
                                  "to breathe in your head. Nothing reaches your malpractice file either.");
            std::cout << "\n\n";
            patient.modifyMalpractice(-15);
            patient.modifyClarity(5);
            skippedClinicOnce = true;
            cuddyDeniedNext   = true;
            narrativeLog += std::string("[SKIP_CLINIC] House ditched clinic hours. Cuddy will remember.");
            std::cout << "Clarity:     +5%   (Current: " << patient.getClarity() << "%)\n";
            std::cout << "Malpractice: -15%  (Current: " << patient.getMalpractice() << "%)\n";
            std::cout << "\033[2m(Cuddy will refuse your next funds request.)\033[0m\n";
        }

    } else if (selectedIndex == 3) {
        // ── Prank a Colleague (1/game) ────────────────────────────────────────
        if (prankedOnce) {
            std::cout << "\n[ PRANK ] ";
            TerminalUI::typewrite("Everyone's onto you today. The element of surprise is gone.");
            std::cout << "\n\n\033[2m(No turn taken.)\033[0m\n";
            lastChoiceNoOp = true;
        } else {
            static const std::vector<std::string> prankScenes = {
                "You forward Wilson's pager to the morgue line. Within ten minutes he's calling you "
                "with the most theatrical sigh of his career. You laugh harder than you have in a week. "
                "The case suddenly makes more sense.",
                "You swap Foreman's stethoscope earpieces with the ones from a teaching dummy. "
                "He spends forty-five minutes 'examining' silence. The team's open laughter clears your head.",
                "You leave a fake page on Chase's desk: 'Cuddy wants you. URGENT.' He sprints. "
                "You watch from the balcony with coffee. The endorphin spike unlocks a thought.",
                "You glue Foreman's coffee mug to his desk. He pretends not to care. "
                "You pretend not to watch. The mutual pretending is the funniest thing all month.",
                "You replace the differential-board markers with the dried-out ones from last year. "
                "Watching Cameron try to write while the marker squeaks gives you an idea about the patient."
            };
            const std::string& scene = prankScenes[std::rand() % prankScenes.size()];
            std::cout << "\n[ PRANK ] ";
            TerminalUI::typewrite(scene);
            std::cout << "\n\n";
            patient.modifyClarity(10);
            patient.modifyMalpractice(5);
            prankedOnce = true;
            narrativeLog += std::string("[PRANK] ") + scene;
            std::cout << "Clarity:     +10%  (Current: " << patient.getClarity() << "%)\n";
            std::cout << "Malpractice:  +5%  (Current: " << patient.getMalpractice() << "%)\n";
        }

    } else if (selectedIndex == 4) {
        // ── Watch Monster Trucks / General Hospital (1/game) ──────────────────
        // Stress relief: walks back one pill (Mayfield safety valve) + small clarity
        // bump from doing nothing. 25% Wilson interrupt stacks for +5 more clarity.
        if (monsterTrucksOnce) {
            std::cout << "\n[ TV ] ";
            TerminalUI::typewrite("The lounge TV is gone. Cuddy moved it after you 'borrowed' it last time.");
            std::cout << "\n\n\033[2m(No turn taken.)\033[0m\n";
            lastChoiceNoOp = true;
        } else {
            std::cout << "\n[ TV ] House props his feet on the lounge table. The remote clicks on.\n";
            TerminalUI::typewrite("Forty minutes of monster trucks crushing minivans. Your shoulders unclench.");
            std::cout << "\n\n";
            monsterTrucksOnce = true;
            int vicodinBefore = vicodinLevel;
            if (vicodinLevel > 0) --vicodinLevel;
            patient.modifyClarity(3);
            narrativeLog += std::string("[TV] Monster trucks. House decompressed. Pill count walked back.");
            if (vicodinBefore != vicodinLevel)
                std::cout << "Vicodin level: " << vicodinBefore << " -> " << vicodinLevel << "/5\n";
            std::cout << "Clarity: +3%  (Current: " << patient.getClarity() << "%)\n";

            if (std::rand() % 4 == 0) {
                std::cout << "\n\033[1;33mWilson:\033[0m ";
                TerminalUI::typewrite("You know what the patient's chart reminded me of? My uncle. Before the diagnosis.");
                std::cout << "\n";
                patient.modifyClarity(5);
                narrativeLog += std::string("[TV_LUCK] Wilson barged in mid-episode with a stray thought. +5% clarity.");
                std::cout << "\033[1;32mClarity: +5%  (Current: " << patient.getClarity() << "%)\033[0m\n";
            }
        }
    }

    std::cout << "\n(Press ENTER to return to the main room...)";
    while (TerminalUI::getKeyPress() != 3);
}

// --- MAIN LOOP ---

void GameEngine::run() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    try {
        // INTRAM IN JOC
        bool running = true;
        int selectedIndex = 0;
        bool boardExpanded = false;

        std::vector<std::string> options = {
            "Medical Intervention   (Tests & Treatments)",
            "Social Interaction     (Team / Cuddy / Wilson)",
            "'House' Actions        (Chaos)",
            "Think Out Loud         [free-text brainstorm]",
            "Resign  (Quit Game)"
        };
        // Index 5 is a virtual non-turn item rendered separately after the main list.

        while (running) {
            TerminalUI::clearScreen();

            std::cout << "=======================================\n";
            std::cout << "   DR. HOUSE : THE VISUAL NOVEL        \n";
            std::cout << "=======================================\n";

            std::cout << "\n[TURN " << turn << "] \n";
            std::cout << "Health: " << patient.getHealth() << "%  |  Clarity: " << patient.getClarity() << "%  |  Malpractice: " << patient.getMalpractice() << "%  |  Budget: $" << budget << "\n";
            int riskyActions = actionLog.count([](const ActionRecord& r){ return r.malpracticeDelta > 0; });
            std::cout << "Actions: " << actionLog.size() << "  |  Risky: " << riskyActions << "  |  Vicodin: " << vicodinLevel << "/5\n";

            // Urgency warnings
            if (eurekaPending)
                std::cout << "\033[1;35m  !! Patient is too unstable for diagnosis. Stabilize before House can connect the dots. (need \033[1;32m>=10 HP\033[1;35m)\033[0m\n";
            if (patient.getHealth() < 10)
                std::cout << "\033[1;31m  !! CRITICAL: " << patient.getName() << " is crashing — this turn may be your last chance.\033[0m\n";
            else if (patient.getHealth() < 20)
                std::cout << "\033[1;31m  !! WARNING: Patient is deteriorating rapidly.\033[0m\n";
            if (patient.getMalpractice() > 85)
                std::cout << "\033[1;33m  !! WARNING: Legal is watching. One more incident and you're done.\033[0m\n";
            else if (patient.getMalpractice() > 70)
                std::cout << "\033[1;33m  !  CAUTION: Malpractice risk elevated — tread carefully.\033[0m\n";
            if (budget < 1500)
                std::cout << "\033[1;31m  !! WARNING: Budget nearly depleted. Choose carefully.\033[0m\n";
            else if (budget < 3000)
                std::cout << "\033[1;33m  !  CAUTION: Budget running low ($" << budget << " remaining).\033[0m\n";

#if DEBUG_MODE
            std::cout << "\033[2m[DEBUG: Disease = "
                      << patient.getHiddenDiagnosis()
                      << " | Severity: " << patient.getDiseaseSeverity()
                      << "]\033[0m\n";
#endif

            std::cout << "What do you do?\n";

            for (size_t i = 0; i < options.size(); ++i) {
                if (i == static_cast<size_t>(selectedIndex)) {
                    std::cout << "  -> \033[1;36m" << options[i] << "\033[0m\n";
                } else {
                    std::cout << "     " << options[i] << "\n";
                }
            }
            // Virtual 6th item — expands/collapses board clues, no turn cost.
            if (selectedIndex == 5)
                std::cout << "  -> \033[2;36m" << (boardExpanded ? "[-] Collapse board" : "[+] Expand board clues") << "\033[0m\n";
            else
                std::cout << "  \033[2m   " << (boardExpanded ? "[-] Collapse board" : "[+] Expand board clues") << "\033[0m\n";

            renderCaseBoard(boardExpanded);

            int key = TerminalUI::getKeyPress();

            if (key == 1) {
                selectedIndex--;
                if (selectedIndex < 0) selectedIndex = 5;
            }
            else if (key == 2) {
                selectedIndex++;
                if (selectedIndex > 5) selectedIndex = 0;
            }
            else if (key == 3) {
                bool takeTurn = true;

                if (selectedIndex == 0) {
                    showMedicalSubMenu();
                } else if (selectedIndex == 1) {
                    showSocialSubMenu();
                } else if (selectedIndex == 2) {
                    showMiscellaneousSubMenu();
                } else if (selectedIndex == 3) {
                    showWhiteboardMenu();
                } else if (selectedIndex == 4) {
                    std::cout << "\nHouse limped home to watch General Hospital. GAME OVER.\n";
                    running = false;
                    takeTurn = false;
                } else if (selectedIndex == 5) {
                    boardExpanded = !boardExpanded;
                    takeTurn = false;
                }

                // A sub-menu can set lastChoiceNoOp = true to refuse the turn
                // (e.g. Wilson is locked out, Break-in already used).
                if (lastChoiceNoOp) {
                    lastChoiceNoOp = false;
                    takeTurn = false;
                }

                if (takeTurn) {
                    turn++;

                    // Probabilistic health decay per turn.
                    {
                        int roll = std::rand() % 100;
                        int decay = 0;
                        if (roll < 30) {
                            decay = 0;                                              // 30%: stabilizes
                        } else if (roll < 40) {
                            decay = -(15 + std::rand() % 11);                       // 10%: sudden crash -15 to -25
                            // Crash can't kill outright — always leaves 2-5 HP so the player
                            // has one last turn to respond (death from inaction, not pure RNG).
                            int floor = 2 + std::rand() % 4;
                            if (patient.getHealth() + decay <= 0)
                                decay = floor - patient.getHealth();
                        } else {
                            // 60%: severity-scaled decay.
                            //   sev 1: -2..-3,  sev 2: -3..-5,  sev 3: -5..-8
                            int sev = patient.getDiseaseSeverity();
                            if      (sev <= 1) decay = -(2 + std::rand() % 2);
                            else if (sev == 2) decay = -(3 + std::rand() % 3);
                            else               decay = -(5 + std::rand() % 4);
                        }
                        if (decay != 0) patient.modifyHealth(decay);
                    }

                    checkEndings(); // loss conditions always take priority over win

                    // Eureka health gate: need a conscious patient to confirm the diagnosis.
                    if (patient.getClarity() >= 100 && patient.getHealth() >= 10) {
                        eurekaPending = false;
                        runEurekaFinale();
                        return;
                    }
                    if (patient.getClarity() >= 100 && patient.getHealth() < 10) {
                        // Surface the softlock immediately the first time it bites — relying
                        // on the next menu redraw alone has driven first-time rage quits.
                        if (!eurekaPending) {
                            std::cout << "\n\033[1;35m"
                                      << "Patient is too unstable for diagnosis. "
                                      << "Stabilize before House can connect the dots."
                                      << "\033[0m\n";
                            std::cout << "\n(Press ENTER to continue...)";
                            while (TerminalUI::getKeyPress() != 3);
                        }
                        eurekaPending = true;
                    } else {
                        eurekaPending = false;
                    }
                }
            }
        }
    }
    catch (const PatientDeathException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
        std::cout << "\033[2mIt was " << patient.getHiddenDiagnosis() << ".\033[0m\n\n";
        std::cout << "(Press ENTER to continue...)";
        while (TerminalUI::getKeyPress() != 3);
        showPostGameMenu(std::string("LOSS — ") + e.what());
    }
    catch (const FiredByHospitalException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
        std::cout << "\033[2mFor the record, it was " << patient.getHiddenDiagnosis() << ".\033[0m\n\n";
        std::cout << "(Press ENTER to continue...)";
        while (TerminalUI::getKeyPress() != 3);
        showPostGameMenu(std::string("LOSS — ") + e.what());
    }
    catch (const GameException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
        std::cout << "\033[2mFor the record, it was " << patient.getHiddenDiagnosis() << ".\033[0m\n\n";
        std::cout << "(Press ENTER to continue...)";
        while (TerminalUI::getKeyPress() != 3);
        showPostGameMenu(std::string("LOSS — ") + e.what());
    }
}

void GameEngine::renderCaseBoard(bool expanded) {
    // Box: 76 chars total. "| " (2) + 72 content + " |" (2) = 76.
    // All section headers use only ASCII chars to guarantee width accuracy.
    const size_t TW = 76;
    const size_t CW = TW - 4;  // 72 usable content chars

    // Truncate at last word boundary before maxLen, appending "..." if cut.
    auto trunc = [](const std::string& s, size_t maxLen) -> std::string {
        if (s.size() <= maxLen) return s;
        size_t pos = s.rfind(' ', maxLen - 3);
        return (pos != std::string::npos && pos > maxLen / 2)
               ? s.substr(0, pos) + "..."
               : s.substr(0, maxLen - 3) + "...";
    };
    // First sentence, then trunc.
    auto brief = [&trunc](const std::string& s, size_t maxLen) -> std::string {
        size_t end = s.find_first_of(".!?");
        std::string r = (end != std::string::npos && end < maxLen) ? s.substr(0, end + 1) : s;
        return trunc(r, maxLen);
    };
    auto row = [&](const std::string& s) -> std::string {
        std::string c = (s.size() > CW) ? s.substr(0, CW) : s + std::string(CW - s.size(), ' ');
        return "| " + c + " |";
    };
    auto sec = [&](const std::string& title) -> std::string {
        // Only ASCII in title — no multi-byte chars that would break width.
        std::string h = "+--[ " + title + " ]";
        while (h.size() < TW - 1) h += '-';
        return h + "+";
    };
    std::string hline = "+" + std::string(TW - 2, '-') + "+";

    auto sw = [](const std::string& s, const std::string& p) -> bool {
        return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
    };

    // Word-wrap a string into lines of at most CW chars (used in expanded mode).
    auto wrapLines = [&](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string rem = s;
        while (!rem.empty()) {
            if (rem.size() <= CW) { lines.push_back(rem); break; }
            size_t pos = rem.rfind(' ', CW);
            if (pos == std::string::npos || pos < CW / 2) pos = CW;
            lines.push_back(rem.substr(0, pos));
            size_t skip = (pos < rem.size() && rem[pos] == ' ') ? 1u : 0u;
            rem = rem.substr(pos + skip);
        }
        return lines;
    };

    // Tests come from actionLog (have name + LLM-generated brief + layperson gloss)
    struct TestEntry { std::string name, brief, gloss; };
    std::vector<TestEntry> testEntries;
    for (const auto& rec : actionLog.getEntries())
        testEntries.push_back({rec.actionName, rec.brief, rec.gloss});

    // Team briefs, clues, Wilson briefs come from narrativeLog tags
    std::vector<std::pair<std::string, std::string>> agentBriefs; // {name, brief}
    std::vector<std::string> clues, wilsonItems;

    for (const auto& entry : narrativeLog.getEntries()) {
        if (sw(entry, "[BRAINSTORM_BRIEF: ")) {
            size_t nameEnd = entry.find(']');
            if (nameEnd != std::string::npos && nameEnd + 2 < entry.size()) {
                std::string name  = entry.substr(18, nameEnd - 18);
                std::string briefText = entry.substr(nameEnd + 2);
                bool found = false;
                for (auto& kv : agentBriefs) {
                    if (kv.first == name) { kv.second = briefText; found = true; break; }
                }
                if (!found) agentBriefs.push_back({name, briefText});
            }
        } else if (sw(entry, "[CLUE] ")) {
            clues.push_back(entry.substr(7));
        } else if (sw(entry, "[WILSON_BRIEF] ")) {
            wilsonItems.push_back(entry.substr(15));
        }
    }

    std::cout << "\n";

    // Header — use ASCII colon, never an em dash, to keep byte count == display width
    std::string title = "WHITEBOARD: " + patient.getName();
    std::cout << sec(title) << "\n";
    if (expanded) {
        for (const auto& line : wrapLines("Symptom: " + patient.getSymptom()))
            std::cout << row(line) << "\n";
    } else {
        std::cout << row("Symptom: " + trunc(patient.getSymptom(), CW - 9)) << "\n";
    }

    // TESTS — one line per action: "CT Scan: elevated WBC, bilateral infiltrates"
    // followed by an optional indented layperson gloss: "  -> Body's fighting something in the lungs."
    if (!testEntries.empty()) {
        std::cout << sec("TESTS") << "\n";
        for (const auto& te : testEntries) {
            std::string entry = te.brief.empty() ? te.name : te.name + ": " + te.brief;
            std::cout << row(trunc(entry, CW)) << "\n";
            if (!te.gloss.empty()) {
                std::string indent = "  -> ";
                if (expanded) {
                    auto wrapped = wrapLines(indent + te.gloss);
                    for (const auto& line : wrapped)
                        std::cout << row(line) << "\n";
                } else {
                    std::cout << row(trunc(indent + te.gloss, CW)) << "\n";
                }
            }
        }
    }

    // TEAM — one line per agent: "Chase:    suspects autoimmune, wants ANA panel"
    if (!agentBriefs.empty()) {
        std::cout << sec("TEAM") << "\n";
        for (const auto& kv : agentBriefs) {
            std::string prefix = kv.first + ": ";
            while (prefix.size() < 10) prefix += ' ';
            std::cout << row(prefix + trunc(kv.second, CW - prefix.size())) << "\n";
        }
    }

    // FIELD CLUE — break-in finds only
    if (!clues.empty()) {
        std::cout << sec(expanded ? "FIELD CLUE  [ expanded ]" : "FIELD CLUE") << "\n";
        for (const auto& c : clues) {
            if (expanded) {
                for (const auto& line : wrapLines(c))
                    std::cout << row(line) << "\n";
            } else {
                std::cout << row(brief(c, CW)) << "\n";
            }
        }
    }

    // WILSON — brief of what he probed
    if (!wilsonItems.empty()) {
        std::cout << sec(expanded ? "WILSON  [ expanded ]" : "WILSON") << "\n";
        for (const auto& w : wilsonItems) {
            if (expanded) {
                for (const auto& line : wrapLines(w))
                    std::cout << row(line) << "\n";
            } else {
                std::cout << row(trunc(w, CW)) << "\n";
            }
        }
    }

    if (testEntries.empty() && agentBriefs.empty() && clues.empty() && wilsonItems.empty()) {
        std::string sev = (patient.getDiseaseSeverity() == 3) ? "CRITICAL" :
                          (patient.getDiseaseSeverity() == 2) ? "moderate" : "mild";
        std::string status = "Admitted: " + patient.getName() + "  |  Health " +
                             std::to_string(patient.getHealth()) + "/100  |  Severity: " + sev +
                             "  |  No findings yet.";
        std::cout << row(trunc(status, CW)) << "\n";
    }

    std::cout << hline << "\n";
}

// --- EUREKA FINALE ---

void GameEngine::triggerDirectorsCut(const std::string& outcome) {
    // Build summary for the LLM
    std::ostringstream summary;
    summary << "PATIENT: " << patient.getName() << "\n";
    summary << "OUTCOME: " << outcome << "\n";
    summary << "TURNS PLAYED: " << turn << "\n\n";

    summary << "=== NARRATIVE LOG ===\n";
    for (const auto& entry : narrativeLog.getEntries())
        summary << "  " << entry << "\n";

    summary << "\n=== ACTION LOG ===\n";
    for (const auto& record : actionLog.getEntries()) {
        std::ostringstream rec;
        rec << record;
        summary << "  " << rec.str() << "\n";
    }

    std::cout << "\n\033[2m[ Generating Director's Cut episode script... ]\033[0m\n";
    std::cout.flush();

    std::string script = aiBrain.generateEpisodeScript(summary.str(), patient.getName());

    // Sanitise patient name for use in a filename
    std::string safeName = patient.getName();
    for (char& c : safeName)
        if (c == ' ' || c == '/') c = '_';

    std::filesystem::create_directories("episodes");
    std::string filename = "episodes/Episode_" + safeName + ".txt";
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << "HOUSE M.D. — \"" << patient.getName() << "\"\n";
        outFile << std::string(60, '=') << "\n\n";
        outFile << script << "\n\n";
        outFile << std::string(60, '=') << "\n";
        outFile << "[ Game log summary ]\n";
        outFile << summary.str();
        outFile.close();
        std::cout << "\033[1;32m[ Director's Cut saved to " << filename << " ]\033[0m\n";
    } else {
        std::cout << "\033[1;31m[ Could not write " << filename << " ]\033[0m\n";
        std::cout << script << "\n";
    }
}

void GameEngine::runForemanPostMortem(const std::string& outcome) {
    std::cout << "\n\033[2m[ Foreman is writing up the post-mortem... ]\033[0m\n";
    std::cout.flush();

    // Pass the game's actual action registry into the prompt so Foreman can only
    // reference tests/treatments the player could have used. The factory is cheap
    // to construct (40-entry map + vector) — no need to cache it on the engine.
    MedicalActionFactory factory;
    std::string notes = aiBrain.generateForemanPostMortem(
        patient.getHiddenDiagnosis(),
        patient.getSymptom(),
        factory.getAllActions()
    );

    TerminalUI::clearScreen();
    std::cout << "\n\033[1;36m=== FOREMAN'S POST-MORTEM ===\033[0m\n";
    std::cout << "Patient: " << patient.getName() << "\n";
    std::cout << "Diagnosis: \033[1;33m" << patient.getHiddenDiagnosis() << "\033[0m\n\n";
    std::cout << notes << "\n\n";

    // Save to file — same pattern as triggerDirectorsCut for consistency.
    std::string safeName = patient.getName();
    for (char& c : safeName)
        if (c == ' ' || c == '/') c = '_';

    std::filesystem::create_directories("episodes");
    std::string filename = "episodes/Notes_" + safeName + ".txt";
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << "FOREMAN'S POST-MORTEM — \"" << patient.getName() << "\"\n";
        outFile << std::string(60, '=') << "\n\n";
        outFile << "Diagnosis: " << patient.getHiddenDiagnosis() << "\n";
        outFile << "Presenting symptoms: " << patient.getSymptom() << "\n";
        outFile << "Outcome: " << outcome << "\n\n";
        outFile << notes << "\n";
        outFile.close();
        std::cout << "\033[1;32m[ Notes saved to " << filename << " ]\033[0m\n";
    } else {
        std::cout << "\033[1;31m[ Could not write " << filename << " ]\033[0m\n";
    }
}

void GameEngine::showPostGameMenu(const std::string& outcome) {
    bool postMortemDone   = false;
    bool directorsCutDone = false;

    while (true) {
        std::vector<std::string> opts = {
            std::string("Foreman's Post-Mortem  (optimal tests & treatments for this disease)")
                + (postMortemDone   ? "    \033[2m[done]\033[0m" : ""),
            std::string("Director's Cut         (full episode script saved to file)")
                + (directorsCutDone ? "    \033[2m[done]\033[0m" : ""),
            std::string("Exit")
        };

        int sel = 0;
        bool picked = false;
        while (!picked) {
            TerminalUI::clearScreen();
            std::cout << "\n=======================================\n";
            std::cout << "             POST-GAME\n";
            std::cout << "=======================================\n\n";
            std::cout << "\033[2m" << outcome << "\033[0m\n\n";
            std::cout << "Want to know more before you go?\n\n";
            for (size_t i = 0; i < opts.size(); ++i) {
                if (i == static_cast<size_t>(sel))
                    std::cout << "  -> \033[1;36m" << opts[i] << "\033[0m\n";
                else
                    std::cout << "     " << opts[i] << "\n";
            }

            int key = TerminalUI::getKeyPress();
            if (key == 1) { --sel; if (sel < 0) sel = static_cast<int>(opts.size()) - 1; }
            else if (key == 2) { ++sel; if (sel >= static_cast<int>(opts.size())) sel = 0; }
            else if (key == 3) picked = true;
        }

        if (sel == 0) {
            if (postMortemDone) {
                std::cout << "\n\033[2m(Already generated this session. Notes file is in episodes/.)\033[0m\n";
            } else {
                runForemanPostMortem(outcome);
                postMortemDone = true;
            }
            std::cout << "\n(Press ENTER to return to the post-game menu...)";
            while (TerminalUI::getKeyPress() != 3);
        } else if (sel == 1) {
            if (directorsCutDone) {
                std::cout << "\n\033[2m(Already generated this session. Episode file is in episodes/.)\033[0m\n";
            } else {
                triggerDirectorsCut(outcome);
                directorsCutDone = true;
            }
            std::cout << "\n(Press ENTER to return to the post-game menu...)";
            while (TerminalUI::getKeyPress() != 3);
        } else {
            // Exit
            std::cout << "\n\033[2mHouse limps off. End of episode.\033[0m\n";
            return;
        }
    }
}

void GameEngine::runEurekaFinale() {
    const std::string patientName     = patient.getName();
    const std::string symptom         = patient.getSymptom();
    const std::string hiddenDiagnosis = patient.getHiddenDiagnosis();

    // --- Beat 1: Perspective shift ---
    TerminalUI::clearScreen();
    std::cout << "\n";
    std::cout << "  \033[1;35m╔══════════════════════════════════════════╗\033[0m\n";
    std::cout << "  \033[1;35m║   PERSPECTIVE SHIFT: YOU ARE NOW         ║\033[0m\n";
    std::cout << "  \033[1;35m║   THE PATIENT.                           ║\033[0m\n";
    std::cout << "  \033[1;35m╚══════════════════════════════════════════╝\033[0m\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // --- Beat 2: Patient monologue ---
    TerminalUI::clearScreen();
    std::cout << "\n  (Generating patient monologue...)\n";
    std::cout.flush();
    std::string monologue = aiBrain.generatePatientMonologue(patientName, symptom);
    narrativeLog += std::string("[EUREKA — PATIENT PERSPECTIVE] ") + patientName + " thinks: " + monologue;
    TerminalUI::clearScreen();
    std::cout << "\n\033[3m";
    TerminalUI::typewrite(monologue);
    std::cout << "\033[0m\n";
    std::cout << "\n(Press ENTER to continue...)\n";
    while (TerminalUI::getKeyPress() != 3);

    // --- Beat 3: House enters (round 0 — also fetches options for round 1) ---
    TerminalUI::clearScreen();
    std::cout << "\n\033[2m[ The door swings open. A cane hits the floor. ]\033[0m\n\n";
    std::cout << "  (House is thinking...)\n";
    std::cout.flush();
    EurekaRoundResult openingResult = aiBrain.generateEurekaDialogue(hiddenDiagnosis, patientName, "", 0, {});
    narrativeLog += std::string("[EUREKA — HOUSE ENTERS] House: ") + openingResult.houseLine;
    std::cout << "\033[1;33mHouse:\033[0m ";
    TerminalUI::typewrite(openingResult.houseLine);
    std::cout << "\n";
    std::cout << "\n(Press ENTER to continue...)\n";
    while (TerminalUI::getKeyPress() != 3);

    // --- Beat 4: Diagnosis loop (3 rounds) ---
    std::vector<std::string> history;
    std::vector<std::string> currentOptions = openingResult.patientOptions;
    if (currentOptions.empty())
        currentOptions = {"What do you mean?", "I'm scared.", "Just tell me."};

    for (int round = 1; round <= 3; ++round) {
        int sel = 0;
        bool chosen = false;
        while (!chosen) {
            TerminalUI::clearScreen();
            std::cout << "\n\033[1;35m[ ROUND " << round << "/3 — How do you respond? ]\033[0m\n\n";
            for (int i = 0; i < (int)currentOptions.size(); ++i) {
                if (i == sel)
                    std::cout << "  -> \033[1;36m" << currentOptions[i] << "\033[0m\n";
                else
                    std::cout << "     " << currentOptions[i] << "\n";
            }
            int key = TerminalUI::getKeyPress();
            if (key == 1) { sel--; if (sel < 0) sel = (int)currentOptions.size() - 1; }
            else if (key == 2) { sel++; if (sel >= (int)currentOptions.size()) sel = 0; }
            else if (key == 3) chosen = true;
        }

        std::string comment = currentOptions[sel];

        TerminalUI::clearScreen();
        std::cout << "\n\033[2mYou: " << comment << "\033[0m\n\n";
        std::cout << "  (House is thinking...)\n";
        std::cout.flush();

        EurekaRoundResult roundResult = aiBrain.generateEurekaDialogue(
            hiddenDiagnosis, patientName, comment, round, history);

        narrativeLog += std::string("[EUREKA — ROUND ") + std::to_string(round) + "/3]"
                      + " Patient: \"" + comment + "\""
                      + " | House: \"" + roundResult.houseLine + "\"";

        std::cout << "\033[1;33mHouse:\033[0m ";
        TerminalUI::typewrite(roundResult.houseLine);
        std::cout << "\n";

        history.push_back(comment);
        history.push_back(roundResult.houseLine);

        if (round < 3) {
            currentOptions = roundResult.patientOptions;
            if (currentOptions.empty())
                currentOptions = {"What does that mean?", "I'm listening.", "Is it serious?"};
        }

        std::cout << "\n(Press ENTER to continue...)\n";
        while (TerminalUI::getKeyPress() != 3);
    }

    // --- Beat 5: Resolution ---
    narrativeLog += std::string("[EUREKA — RESOLUTION] House limps out. Diagnosis confirmed: ") + hiddenDiagnosis;
    TerminalUI::clearScreen();
    std::cout << "\n\033[2mHouse limps out without looking back.\033[0m\n";
    std::cout << "\033[2m[ You have a diagnosis. Whether you want it is another question. ]\033[0m\n\n";
    std::cout << "(Press ENTER to continue...)";
    while (TerminalUI::getKeyPress() != 3);
    showPostGameMenu("WIN — Diagnosis revealed: " + hiddenDiagnosis);
}