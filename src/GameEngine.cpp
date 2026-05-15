#include "GameEngine.h"
#include "AIAgent.h"
#include "Exceptions.h"
#include "MedicalActionFactory.h"
#include "TerminalUI.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

GameEngine::GameEngine(const Patient& p) : patient(p) {}

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
                      << "  |  Budget: $" << budget << "\n\n";
            std::cout << "What is your approach?\n\n";
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
                      << "  |  Budget: $" << budget << "\n\n";
            std::cout << "Select (UP/DOWN and ENTER):\n\n";
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

    patient.modifyHealth(outcome.healthDelta);
    patient.modifyClarity(outcome.clarityDelta);
    patient.modifyMalpractice(outcome.malpracticeDelta);
    budget -= cost;

    narrativeLog += std::string("[") + actionType + ": " + actionName + "] " + outcome.narrative;
    ActionRecord record;
    record.actionName        = actionName;
    record.actionType        = actionType;
    record.brief             = outcome.brief;
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

    narrativeLog += std::string("[WHITEBOARD] ") + question;

    std::cout << "(Press ENTER to return to the main room...)";
    while (TerminalUI::getKeyPress() != 3);
}

void GameEngine::showSocialSubMenu() {
    std::vector<std::string> socialOptions = {
        "Team Brainstorm       (Chase / Cameron / Foreman)",
        "Consult Wilson        (Case Sounding Board)  [+5% clarity]",
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
            TeamOpinionResult result = agent->brainstorm(
                patient.getSymptom(), patient.getHiddenDiagnosis(),
                patient.getClarity(), aiBrain);

            std::cout << "\033[1;33m" << agent->getName() << ":\033[0m ";
            TerminalUI::typewrite(result.opinion);
            std::cout << "\n\n";

            narrativeLog += std::string("[BRAINSTORM: ") + agent->getName() + "] " + result.opinion;
            narrativeLog += std::string("[BRAINSTORM_BRIEF: ") + agent->getName() + "] " + result.brief;

            // Downcast: Chase's wild guess occasionally sparks something (rubric)
            if (auto* chase = dynamic_cast<ChaseAgent*>(agent.get())) {
                if (std::rand() % 5 == 0) {
                    patient.modifyClarity(15);
                    std::cout << "\033[2m[Chase's exotic theory accidentally points somewhere useful. Clarity +15%.]\033[0m\n\n";
                }
                (void)chase;
            }
        }

        std::cout << "\n(Press ENTER to return to the main room...)";
        while (TerminalUI::getKeyPress() != 3);
        return;
    }

    // ── Wilson Consult ────────────────────────────────────────────────────────
    if (selectedIndex == 1) {
        TerminalUI::clearScreen();
        std::cout << "\n=== WILSON'S OFFICE ===\n";
        std::cout << "House limps in without knocking. Wilson looks up from his paperwork.\n\n";
        std::cout << "  (Wilson is thinking...)\n";
        std::cout.flush();

        WilsonResult wilsonResult = aiBrain.generateWilsonConsult(
            patient.getSymptom(), patient.getHiddenDiagnosis(),
            patient.getName(), patient.getClarity()
        );

        patient.modifyClarity(5);
        narrativeLog += std::string("[WILSON] ") + wilsonResult.dialogue;
        narrativeLog += std::string("[WILSON_BRIEF] ") + wilsonResult.brief;

        std::cout << "\033[1;33mWilson:\033[0m ";
        TerminalUI::typewrite(wilsonResult.dialogue);
        std::cout << "\n\n";
        std::cout << "Clarity: +5%  (Current: " << patient.getClarity() << "%)\n";

        std::cout << "\n(Press ENTER to return to the main room...)";
        while (TerminalUI::getKeyPress() != 3);
        return;
    }

    // ── Cuddy dialogue ────────────────────────────────────────────────────────
    const std::string characterName = "Cuddy";
    int maxMessages = 3;
    std::vector<std::string> chatHistory;
    std::vector<std::string> houseOptions = aiBrain.getHouseIntents(characterName);

    // Inject emergency budget option at front — label reflects availability
    const std::string fundsLabel = (cuddyFundsGranted < 2)
        ? "Request Emergency Budget Approval  [+$5,000 / +8% malpractice]"
        : "Request Emergency Funding          [Cuddy already gave you enough]";
    houseOptions.insert(houseOptions.begin(), fundsLabel);

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

        // Walk out (last option)
        if (selectedLine == static_cast<int>(houseOptions.size()) - 1) {
            chatHistory.push_back("House: *Pops a pill and limps away.*");
            break;
        }

        // Emergency budget request (index 0)
        if (selectedLine == 0) {
            if (cuddyFundsGranted < 2) {
                budget += 5000;
                patient.modifyMalpractice(8);
                ++cuddyFundsGranted;
                chatHistory.push_back("House: I need emergency funds. Critical test.");
                chatHistory.push_back("[Cuddy]: Fine. Five thousand. Don't make me regret this, House.");
                std::cout << "\n\033[1;33m[Cuddy]: Fine. Five thousand. Don't make me regret this, House.\033[0m\n";
                std::cout << "Budget:      +$5,000  (Remaining: $" << budget << ")\n";
                std::cout << "Malpractice: +8%      (Current: " << patient.getMalpractice() << "%)\n";
                // Update label for next round if any remain
                houseOptions[0] = (cuddyFundsGranted < 2)
                    ? "Request Emergency Budget Approval  [+$5,000 / +8% malpractice]"
                    : "Request Emergency Funding          [Cuddy already gave you enough]";
            } else {
                chatHistory.push_back("House: More money.");
                chatHistory.push_back("[Cuddy]: No. I already gave you two emergency allocations. You're done, House.");
                std::cout << "\n\033[1;33m[Cuddy]: No. I already gave you two emergency allocations. You're done.\033[0m\n";
            }
            std::cout << "\n(Press ENTER to continue...)";
            while (TerminalUI::getKeyPress() != 3);
            continue;
        }

        // Regular dialogue (LLM options, shifted by 1 due to injected budget option)
        DialogueResponse aiDialogue = aiBrain.generateDialogue(
            characterName, chatHistory, houseOptions[selectedLine],
            patient.getName(), patient.getSymptom()
        );
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
        "Break Into Patient's House  (+22% clarity, +20% malpractice)",
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

    if (selectedIndex == 2) return; // Go Back

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
        checkEndings(); // throws MayfieldWardException if vicodinLevel >= 10

    } else if (selectedIndex == 1) {
        // ── Break Into Patient's House ────────────────────────────────────────
        std::cout << "\n[ ILLEGAL HOME VISIT ] House picks the lock without a second thought.\n";
        std::cout << "  (House is snooping...)\n";
        std::cout.flush();
        std::string clue = aiBrain.generateHouseClue(
            patient.getHiddenDiagnosis(), patient.getName(), patient.getSymptom());
        patient.modifyClarity(22);
        patient.modifyMalpractice(20);
        narrativeLog += std::string("[CLUE] ") + clue;
        std::cout << "\n\033[1;33m\"";
        TerminalUI::typewrite(clue);
        std::cout << "\"\033[0m\n\n";
        std::cout << "Clarity:     +22%  (Current: " << patient.getClarity() << "%)\n";
        std::cout << "Malpractice: +20%  (Current: " << patient.getMalpractice() << "%)\n";
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
            std::cout << "Actions: " << actionLog.size() << "  |  Risky: " << riskyActions << "  |  Vicodin: " << vicodinLevel << "/10\n";

            // Urgency warnings
            if (patient.getHealth() < 10)
                std::cout << "\033[1;31m  !! CRITICAL: " << patient.getName() << " is crashing — this turn may be your last chance.\033[0m\n";
            else if (patient.getHealth() < 20)
                std::cout << "\033[1;31m  !! WARNING: Patient is deteriorating rapidly.\033[0m\n";
            if (patient.getMalpractice() > 85)
                std::cout << "\033[1;33m  !! WARNING: Legal is watching. One more incident and you're done.\033[0m\n";
            else if (patient.getMalpractice() > 70)
                std::cout << "\033[1;33m  !  CAUTION: Malpractice risk elevated — tread carefully.\033[0m\n";
            if (budget < 3000)
                std::cout << "\033[1;31m  !! WARNING: Budget nearly depleted. Choose carefully.\033[0m\n";
            else if (budget < 8000)
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

                if (takeTurn) {
                    turn++;

                    // Probabilistic health decay per turn
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
                            decay = -(1 + std::rand() % 2) * patient.getDiseaseSeverity(); // 60%: normal decay
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
    }
    catch (const PatientDeathException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
        std::cout << "\033[2mIt was " << patient.getHiddenDiagnosis() << ".\033[0m\n";
    }
    catch (const FiredByHospitalException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
    }
    catch (const GameException& e) {
        TerminalUI::clearScreen();
        std::cout << "\n[ GAME OVER ] " << e.what() << "\n";
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

    // Tests come from actionLog (have name + LLM-generated brief)
    std::vector<std::pair<std::string, std::string>> testEntries; // {name, brief}
    for (const auto& rec : actionLog.getEntries())
        testEntries.push_back({rec.actionName, rec.brief});

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
    std::cout << row("Symptom: " + trunc(patient.getSymptom(), CW - 9)) << "\n";

    // TESTS — one line per action: "CT Scan: elevated WBC, bilateral infiltrates"
    if (!testEntries.empty()) {
        std::cout << sec("TESTS") << "\n";
        for (const auto& te : testEntries) {
            std::string entry = te.second.empty() ? te.first : te.first + ": " + te.second;
            std::cout << row(trunc(entry, CW)) << "\n";
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
    triggerDirectorsCut("WIN — Diagnosis revealed: " + hiddenDiagnosis);
}