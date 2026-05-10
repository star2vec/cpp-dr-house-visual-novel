#include "GameEngine.h"
#include "AIAgent.h"
#include "Exceptions.h"
#include "MedicalActionFactory.h"
#include "TerminalUI.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

GameEngine::GameEngine(Patient p) : patient(p) {}

void GameEngine::checkEndings() {
    if (patient.getHealth() <= 0)        throw PatientDeathException(turn);
    if (patient.getMalpractice() >= 100) throw FiredByHospitalException(turn);
    if (budget < 0)                      throw OutOfBudgetException(turn);
    if (vicodinLevel >= 10)              throw MayfieldWardException(turn);
}

// --- SUB-MENIURI ---

void GameEngine::showMedicalSubMenu() {
    MedicalActionFactory factory;
    const auto& allActions = factory.getAllActions();

    int selectedIndex = 0;
    bool actionChosen = false;

    while (!actionChosen) {
        TerminalUI::clearScreen();
        std::cout << "\n--- [ SUB-MENU: MEDICAL INTERVENTION ] ---\n";
        std::cout << "Patient: " << patient.getName()
                  << " | Health: " << patient.getHealth()
                  << " | Clarity: " << patient.getClarity() << "%"
                  << " | Budget: $" << budget << "\n\n";
        std::cout << "Select a procedure (UP/DOWN and ENTER):\n";

        for (size_t i = 0; i < allActions.size(); ++i) {
            const auto& [name, type] = allActions[i];
            int cost = factory.getCost(name);
            if (i == static_cast<size_t>(selectedIndex)) {
                std::cout << "  -> \033[1;36m[" << type << "] " << name << "  ($" << cost << ")\033[0m\n";
            } else {
                std::cout << "     [" << type << "] " << name << "  ($" << cost << ")\n";
            }
        }

        if (selectedIndex == static_cast<int>(allActions.size())) {
            std::cout << "  -> \033[1;31mGo Back\033[0m\n";
        } else {
            std::cout << "     Go Back\n";
        }

        int key = TerminalUI::getKeyPress();
        if (key == 1) {
            selectedIndex--;
            if (selectedIndex < 0) selectedIndex = static_cast<int>(allActions.size());
        }
        else if (key == 2) {
            selectedIndex++;
            if (selectedIndex > static_cast<int>(allActions.size())) selectedIndex = 0;
        }
        else if (key == 3) {
            actionChosen = true;
        }
    }

    if (selectedIndex == static_cast<int>(allActions.size())) return;

    const auto& [actionName, actionType] = allActions[selectedIndex];
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
        patient.getHiddenDiagnosis()
    );

    patient.modifyHealth(outcome.healthDelta);
    patient.modifyClarity(outcome.clarityDelta);
    patient.modifyMalpractice(outcome.malpracticeDelta);
    budget -= cost;

    narrativeLog += outcome.narrative;
    ActionRecord record;
    record.actionName        = actionName;
    record.actionType        = actionType;
    record.healthDelta       = outcome.healthDelta;
    record.clarityDelta      = outcome.clarityDelta;
    record.malpracticeDelta  = outcome.malpracticeDelta;
    record.budgetSpent       = cost;
    actionLog += record;

    TerminalUI::clearScreen();
    std::cout << "\n=== PROCEDURE RESULTS ===\n\n";
    TerminalUI::typewrite(outcome.narrative);
    std::cout << "\n\n";
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

void GameEngine::showSocialSubMenu() {
    std::vector<std::string> socialOptions = {
        "Team Brainstorm (Chase / Cameron / Foreman)",
        "Talk to The Team",
        "Talk to Wilson",
        "Talk to Cuddy",
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

    if (selectedIndex == 4) return; // Go Back

    // ── Team Brainstorm ───────────────────────────────────────────────────────
    if (selectedIndex == 0) {
        TerminalUI::clearScreen();
        std::cout << "\n=== TEAM BRAINSTORM ===\n";
        std::cout << "House rolls his eyes. \"Fine. What do you idiots think?\"\n\n";

        // Upcast: stored as base pointers (rubric)
        std::vector<std::unique_ptr<AIAgent>> team;
        team.push_back(std::make_unique<ChaseAgent>());
        team.push_back(std::make_unique<CameronAgent>());
        team.push_back(std::make_unique<ForemanAgent>());

        for (auto& agent : team) {
            std::cout << "  (asking " << agent->getName() << "...)\n";
            std::cout.flush();

            // Polymorphic dispatch via template method (rubric)
            std::string opinion = agent->brainstorm(
                patient.getSymptom(), patient.getHiddenDiagnosis(),
                patient.getClarity(), aiBrain);

            std::cout << "\033[1;33m" << agent->getName() << ":\033[0m ";
            TerminalUI::typewrite(opinion);
            std::cout << "\n\n";

            // Downcast: Chase's wild guess occasionally sparks something (rubric)
            if (auto* chase = dynamic_cast<ChaseAgent*>(agent.get())) {
                if (std::rand() % 5 == 0) {
                    patient.modifyClarity(15);
                    std::cout << "\033[2m[Chase's exotic theory accidentally points somewhere useful. Clarity +15%.]\033[0m\n\n";
                }
                (void)chase; // suppress unused-variable warning
            }
        }

        narrativeLog += std::string("Team brainstorm on turn ") + std::to_string(turn);
        std::cout << "\n(Press ENTER to return to the main room...)";
        while (TerminalUI::getKeyPress() != 3);
        return;
    }

    // ── Individual dialogue (existing behaviour) ──────────────────────────────
    std::string characterName;
    if (selectedIndex == 1) characterName = "The Team";
    else if (selectedIndex == 2) characterName = "Wilson";
    else characterName = "Cuddy";

    int maxMessages = 3;
    std::vector<std::string> chatHistory;
    std::vector<std::string> houseOptions = aiBrain.getHouseIntents(characterName);

    for (int i = 0; i < maxMessages; ++i) {
        int selectedLine = 0;
        bool lineChosen = false;

        while (!lineChosen) {
            TerminalUI::clearScreen();
            std::cout << "\n--- Talking to " << characterName << " (Message " << (i + 1) << "/3) ---\n\n";
            for (const auto& log : chatHistory) std::cout << log << "\n";
            std::cout << "\n";
            std::cout << "Choose House's action (UP/DOWN and ENTER):\n";
            for (size_t j = 0; j < houseOptions.size(); ++j) {
                if (j == static_cast<size_t>(selectedLine))
                    std::cout << "  -> \033[1;36m" << houseOptions[j] << "\033[0m\n";
                else
                    std::cout << "     " << houseOptions[j] << "\n";
            }
            int key = TerminalUI::getKeyPress();
            if (key == 1) { --selectedLine; if (selectedLine < 0) selectedLine = static_cast<int>(houseOptions.size()) - 1; }
            else if (key == 2) { ++selectedLine; if (selectedLine >= static_cast<int>(houseOptions.size())) selectedLine = 0; }
            else if (key == 3) { lineChosen = true; }
        }

        if (selectedLine == static_cast<int>(houseOptions.size()) - 1) {
            chatHistory.push_back("House: *Pops a pill and limps away.*");
            break;
        }

        DialogueResponse aiDialogue = aiBrain.generateDialogue(characterName, chatHistory, houseOptions[selectedLine]);
        chatHistory.push_back("House: " + aiDialogue.houseLine);
        chatHistory.push_back("[" + characterName + "]: " + aiDialogue.characterReply);
    }

    TerminalUI::clearScreen();
    std::cout << "\n--- Conversation Ended ---\n\n";
    for (const auto& log : chatHistory) std::cout << log << "\n";
    std::cout << "\n(Press ENTER to return to the main room...)";
    while (TerminalUI::getKeyPress() != 3);
}

void GameEngine::showMiscellaneousSubMenu() {
    std::vector<std::string> chaosOptions = {
        "Pop Vicodin            (+10% clarity, +1 Vicodin level)",
        "Break Into Patient's House  (+15% clarity, +20% malpractice)",
        "Steal Equipment Budget (+25% clarity, +15% malpractice, -$20000)",
        "Go Back"
    };

    int selectedIndex = 0;
    bool chosen = false;
    while (!chosen) {
        TerminalUI::clearScreen();
        std::cout << "\n--- [ HOUSE CHAOS ] ---\n";
        std::cout << "Vicodin level: " << vicodinLevel << "/10\n\n";
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

    if (selectedIndex == 3) return; // Go Back

    TerminalUI::clearScreen();

    if (selectedIndex == 0) {
        // ── Pop Vicodin ───────────────────────────────────────────────────────
        ++vicodinLevel;
        patient.modifyClarity(10);
        std::cout << "\n[ VICODIN ] House dry-swallows a pill without breaking eye contact.\n";
        std::cout << "The pain recedes. So does his filter.\n\n";
        std::cout << "Clarity:      +10%  (Current: " << patient.getClarity() << "%)\n";
        std::cout << "Vicodin level: " << vicodinLevel << "/10\n";
        if (vicodinLevel >= 9)
            std::cout << "\033[1;31mWarning: One more pill and Cuddy ships you to Mayfield.\033[0m\n";
        checkEndings(); // throws MayfieldWardException if vicodinLevel > 10

    } else if (selectedIndex == 1) {
        // ── Break Into Patient's House ────────────────────────────────────────
        std::cout << "\n[ ILLEGAL HOME VISIT ] House picks the lock without a second thought.\n";
        std::cout << "  (House is snooping...)\n";
        std::cout.flush();
        std::string clue = aiBrain.generateHouseClue(
            patient.getHiddenDiagnosis(), patient.getName(), patient.getSymptom());
        patient.modifyClarity(15);
        patient.modifyMalpractice(20);
        std::cout << "\n\033[1;33m\"";
        TerminalUI::typewrite(clue);
        std::cout << "\"\033[0m\n\n";
        std::cout << "Clarity:     +15%  (Current: " << patient.getClarity() << "%)\n";
        std::cout << "Malpractice: +20%  (Current: " << patient.getMalpractice() << "%)\n";

    } else if (selectedIndex == 2) {
        // ── Steal Equipment Budget ────────────────────────────────────────────
        budget -= 20000;
        patient.modifyClarity(25);
        patient.modifyMalpractice(15);
        std::cout << "\n[ BUDGET RAID ] House redirects the MRI department's quarterly fund.\n";
        std::cout << "Cuddy will notice. She always notices.\n\n";
        std::cout << "Clarity:     +25%  (Current: " << patient.getClarity() << "%)\n";
        std::cout << "Malpractice: +15%  (Current: " << patient.getMalpractice() << "%)\n";
        std::cout << "Budget:      -$20000  (Remaining: $" << budget << ")\n";
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

        std::vector<std::string> options = {
            "Medical Intervention (Tests & Treatment)",
            "Social Interaction (Team / Cuddy / Wilson)",
            "'House' Actions (Chaos)",
            "Resign (Quit Game)"
        };

        while (running) {
            TerminalUI::clearScreen();

            std::cout << "=======================================\n";
            std::cout << "   DR. HOUSE : THE VISUAL NOVEL        \n";
            std::cout << "=======================================\n";

            std::cout << "\n[TURN " << turn << "] \n";
            std::cout << "Health: " << patient.getHealth() << "%  |  Clarity: " << patient.getClarity() << "%  |  Malpractice: " << patient.getMalpractice() << "%  |  Budget: $" << budget << "\n";
            int riskyActions = actionLog.count([](const ActionRecord& r){ return r.malpracticeDelta > 0; });
            std::cout << "Actions: " << actionLog.size() << "  |  Risky: " << riskyActions << "  |  Vicodin: " << vicodinLevel << "/10\n\n";

            std::cout << "What category of action do you take?\n";

            for (size_t i = 0; i < options.size(); ++i) {
                if (i == static_cast<size_t>(selectedIndex)) {
                    std::cout << "  -> \033[1;36m" << options[i] << "\033[0m\n";
                } else {
                    std::cout << "     " << options[i] << "\n";
                }
            }

            int key = TerminalUI::getKeyPress();

            if (key == 1) {
                selectedIndex--;
                if (selectedIndex < 0) selectedIndex = static_cast<int>(options.size()) - 1;
            }
            else if (key == 2) {
                selectedIndex++;
                if (selectedIndex >= static_cast<int>(options.size())) selectedIndex = 0;
            }
            else if (key == 3) {
                if (selectedIndex == 0) {
                    showMedicalSubMenu();
                } else if (selectedIndex == 1) {
                    showSocialSubMenu();
                } else if (selectedIndex == 2) {
                    showMiscellaneousSubMenu();
                } else if (selectedIndex == 3) {
                    std::cout << "\nHouse limped home to watch General Hospital. GAME OVER.\n";
                    running = false;
                }

                turn++;

                // Probabilistic health decay per turn
                {
                    int roll = std::rand() % 100;
                    int decay = 0;
                    if (roll < 30) {
                        decay = 0;                                              // 30%: stabilizes
                    } else if (roll < 40) {
                        decay = -(15 + std::rand() % 11);                       // 10%: sudden crash -15 to -25
                    } else {
                        decay = -(3 + std::rand() % 5) * patient.getDiseaseSeverity(); // 60%: normal decay
                    }
                    if (decay != 0) patient.modifyHealth(decay);
                }

                checkEndings(); // loss conditions always take priority over win

                if (patient.getClarity() >= 100 && patient.getHealth() > 0) {
                    runEurekaFinale();
                    return;
                }
            }
        }
    }
    catch (const PatientDeathException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
        triggerDirectorsCut(e.what());
    }
    catch (const FiredByHospitalException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
        triggerDirectorsCut(e.what());
    }
    catch (const GameException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
        triggerDirectorsCut(e.what());
    }
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

    std::string filename = "Episode_" + safeName + ".txt";
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

    // --- Beat 3: House enters ---
    TerminalUI::clearScreen();
    std::cout << "\n\033[2m[ The door swings open. A cane hits the floor. ]\033[0m\n\n";
    std::cout << "  (House is thinking...)\n";
    std::cout.flush();
    std::string openingLine = aiBrain.generateEurekaDialogue(hiddenDiagnosis, patientName, "", 0, {});
    narrativeLog += std::string("[EUREKA — HOUSE ENTERS] House: ") + openingLine;
    std::cout << "\033[1;33mHouse:\033[0m ";
    TerminalUI::typewrite(openingLine);
    std::cout << "\n";
    std::cout << "\n(Press ENTER to continue...)\n";
    while (TerminalUI::getKeyPress() != 3);

    // --- Beat 4: Diagnosis loop ---
    std::vector<std::string> history; // alternates [playerComment, houseResponse, ...]

    const std::vector<std::vector<std::string>> commentOptions = {
        {"What do you mean?",          "I just want to go home.",             "..."},
        {"That doesn't make sense.",    "Are you even listening to me?",       "Please just tell me."},
        {"My heart?",                   "What body system?",                   "I don't understand any of this."},
        {"Is it serious?",              "Will I be okay?",                     "I knew something was wrong."}
    };

    for (int round = 1; round <= 4; ++round) {
        const std::vector<std::string>& opts = commentOptions[round - 1];

        int sel = 0;
        bool chosen = false;
        while (!chosen) {
            TerminalUI::clearScreen();
            std::cout << "\n\033[1;35m[ ROUND " << round << "/4 — How do you respond? ]\033[0m\n\n";
            for (int i = 0; i < (int)opts.size(); ++i) {
                if (i == sel)
                    std::cout << "  -> \033[1;36m" << opts[i] << "\033[0m\n";
                else
                    std::cout << "     " << opts[i] << "\n";
            }
            int key = TerminalUI::getKeyPress();
            if (key == 1) { sel--; if (sel < 0) sel = (int)opts.size() - 1; }
            else if (key == 2) { sel++; if (sel >= (int)opts.size()) sel = 0; }
            else if (key == 3) chosen = true;
        }

        std::string comment = opts[sel];

        TerminalUI::clearScreen();
        std::cout << "\n\033[2mYou: " << comment << "\033[0m\n\n";
        std::cout << "  (House is thinking...)\n";
        std::cout.flush();

        std::string houseResponse = aiBrain.generateEurekaDialogue(
            hiddenDiagnosis, patientName, comment, round, history);

        narrativeLog += std::string("[EUREKA — ROUND ") + std::to_string(round) + "/4]"
                      + " Patient: \"" + comment + "\""
                      + " | House: \"" + houseResponse + "\"";

        std::cout << "\033[1;33mHouse:\033[0m ";
        TerminalUI::typewrite(houseResponse);
        std::cout << "\n";

        history.push_back(comment);
        history.push_back(houseResponse);

        std::cout << "\n(Press ENTER to continue...)\n";
        while (TerminalUI::getKeyPress() != 3);
    }

    // --- Beat 5: Resolution ---
    narrativeLog += std::string("[EUREKA — RESOLUTION] House limps out. Diagnosis confirmed: ") + hiddenDiagnosis;
    TerminalUI::clearScreen();
    std::cout << "\n\033[2mHouse limps out without looking back.\033[0m\n";
    std::cout << "\033[2m[ You have a diagnosis. Whether you want it is another question. ]\033[0m\n\n";
    triggerDirectorsCut("WIN — Diagnosis revealed: " + hiddenDiagnosis);
}