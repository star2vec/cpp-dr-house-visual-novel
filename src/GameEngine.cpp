#include "GameEngine.h"
#include "Exceptions.h"
#include "TerminalUI.h"
#include "MedicalAction.h" // AICI e ierarhia ta OOP
#include <iostream>
#include <memory> // PENTRU SMART POINTERS

GameEngine::GameEngine(Patient p) : patient(p) {}

void GameEngine::checkEndings() {
    if (patient.getHealth() <= 0) {
        TerminalUI::clearScreen();
        std::cout << "\n[ FLATLINE ] The patient has died. Cuddy is furious.\nGAME OVER.\n";
        exit(0);
    }
    if (patient.getMalpractice() >= 100) {
        TerminalUI::clearScreen();
        std::cout << "\n[ FIRED ] You've crossed the line too many times. You lose your medical license.\nGAME OVER.\n";
        exit(0);
    }
    if (patient.getClarity() >= 100 && patient.getHealth() > 0) {
        TerminalUI::clearScreen();
        std::cout << "\n[ CURED ] You figured it out. It wasn't Lupus.\nYOU WIN!\n";
        exit(0);
    }
}

// --- SUB-MENIURI ---

void GameEngine::showMedicalSubMenu() {
    // Aici bifam Polimorfismul si Smart Pointers
    std::vector<std::unique_ptr<MedicalAction>> availableActions;
    availableActions.push_back(std::make_unique<LabTest>("Lumbar Puncture"));
    availableActions.push_back(std::make_unique<LabTest>("Full Body MRI"));
    availableActions.push_back(std::make_unique<LabTest>("Blood Panel"));
    availableActions.push_back(std::make_unique<Treatment>("High-dose Steroids"));
    availableActions.push_back(std::make_unique<Treatment>("Broad-spectrum Antibiotics"));
    availableActions.push_back(std::make_unique<RiskyProcedure>("Brain Biopsy"));

    int selectedIndex = 0;
    bool actionChosen = false;

    while (!actionChosen) {
        TerminalUI::clearScreen();
        std::cout << "\n--- [ SUB-MENU: MEDICAL INTERVENTION ] ---\n";
        std::cout << "Patient: " << patient.getName() << " | Health: " << patient.getHealth() << " | Clarity: " << patient.getClarity() << "%\n\n";
        std::cout << "Select a procedure (UP/DOWN and ENTER):\n";

        for (size_t i = 0; i < availableActions.size(); ++i) {
            if (i == static_cast<size_t>(selectedIndex)) {
                std::cout << "  -> \033[1;36m[" << availableActions[i]->getActionType() << "] " << availableActions[i]->getName() << "\033[0m\n";
            } else {
                std::cout << "     [" << availableActions[i]->getActionType() << "] " << availableActions[i]->getName() << "\n";
            }
        }

        // Optiunea de Back
        if (selectedIndex == static_cast<int>(availableActions.size())) {
            std::cout << "  -> \033[1;31mGo Back\033[0m\n";
        } else {
            std::cout << "     Go Back\n";
        }

        int key = TerminalUI::getKeyPress();
        if (key == 1) { // UP
            selectedIndex--;
            if (selectedIndex < 0) selectedIndex = static_cast<int>(availableActions.size());
        }
        else if (key == 2) { // DOWN
            selectedIndex++;
            if (selectedIndex > static_cast<int>(availableActions.size())) selectedIndex = 0;
        }
        else if (key == 3) { // ENTER
            actionChosen = true;
        }
    }

    // Daca a ales Go Back
    if (selectedIndex == static_cast<int>(availableActions.size())) return;

    // Daca a ales o actiune medicala
    auto& selectedAction = availableActions[selectedIndex];

    TerminalUI::clearScreen();
    std::cout << "\n[ " << selectedAction->getActionType() << " ] " << selectedAction->getName() << "\n";
    std::cout << "\nDetermining outcome... (House is thinking)\n";

    // AI-ul e Game Master-ul care calculeaza rezultatul
    MedicalOutcome outcome = aiBrain.evaluateMedicalAction(
        selectedAction->getActionType(),
        selectedAction->getName(),
        patient.getHealth(),
        patient.getClarity(),
        patient.getSymptom()
    );

    // Aplicam fizica jocului pe pacient
    patient.modifyHealth(outcome.healthDelta);
    patient.modifyClarity(outcome.clarityDelta);
    patient.modifyMalpractice(outcome.malpracticeDelta);

    TerminalUI::clearScreen();
    std::cout << "\n=== PROCEDURE RESULTS ===\n\n";
    std::cout << outcome.narrative << "\n\n";
    std::cout << "------------------------------------\n";
    std::cout << "Health:       " << (outcome.healthDelta > 0 ? "+" : "") << outcome.healthDelta << "  (Current: " << patient.getHealth() << "/100)\n";
    std::cout << "Clarity:      " << (outcome.clarityDelta > 0 ? "+" : "") << outcome.clarityDelta << "% (Current: " << patient.getClarity() << "%)\n";
    std::cout << "Malpractice:  " << (outcome.malpracticeDelta > 0 ? "+" : "") << outcome.malpracticeDelta << "% (Current: " << patient.getMalpractice() << "%)\n";
    std::cout << "------------------------------------\n";

    std::cout << "\n(Press ENTER to return to the main room...)";
    while(TerminalUI::getKeyPress() != 3);
}

void GameEngine::showSocialSubMenu() {
    // ... [Pastreaza EXACT codul de meniu social cu "The Team / Wilson / Cuddy" pe care il aveai inainte, care functioneaza perfect!] ...
    TerminalUI::clearScreen();
    std::cout << "\n--- [ SUB-MENU: SOCIAL INTERACTION ] ---\n";
    std::cout << "Type the name of who you want to bother (The Team / Wilson / Cuddy) or 'exit': ";

    std::string characterName;
    std::getline(std::cin, characterName);

    if (characterName == "exit" || characterName.empty()) return;

    int maxMessages = 3;
    std::vector<std::string> chatHistory;
    std::vector<std::string> houseOptions = aiBrain.getHouseIntents(characterName);

    for (int i = 0; i < maxMessages; ++i) {
        int selectedLine = 0;
        bool lineChosen = false;

        while (!lineChosen) {
            TerminalUI::clearScreen();
            std::cout << "\n--- Talking to " << characterName << " (Message " << (i + 1) << "/3) ---\n\n";

            for (const auto& log : chatHistory) {
                std::cout << log << "\n";
            }
            std::cout << "\n";

            std::cout << "Choose House's action (UP/DOWN and ENTER):\n";
            for (size_t j = 0; j < houseOptions.size(); ++j) {
                if (j == static_cast<size_t>(selectedLine)) {
                    std::cout << "  -> \033[1;36m" << houseOptions[j] << "\033[0m\n";
                } else {
                    std::cout << "     " << houseOptions[j] << "\n";
                }
            }

            int key = TerminalUI::getKeyPress();
            if (key == 1) {
                selectedLine--;
                if (selectedLine < 0) selectedLine = static_cast<int>(houseOptions.size()) - 1;
            }
            else if (key == 2) {
                selectedLine++;
                if (selectedLine >= static_cast<int>(houseOptions.size())) selectedLine = 0;
            }
            else if (key == 3) {
                lineChosen = true;
            }
        }

        if (selectedLine == static_cast<int>(houseOptions.size()) - 1) {
            chatHistory.push_back("House: *Pops a pill and limps away.*");
            break;
        }

        std::string chosenIntent = houseOptions[selectedLine];
        DialogueResponse aiDialogue = aiBrain.generateDialogue(characterName, chatHistory, chosenIntent);

        chatHistory.push_back("House: " + aiDialogue.houseLine);
        chatHistory.push_back("[" + characterName + "]: " + aiDialogue.characterReply);
    }

    TerminalUI::clearScreen();
    std::cout << "\n--- Conversation Ended ---\n\n";
    for (const auto& log : chatHistory) {
        std::cout << log << "\n";
    }
    std::cout << "\n(Press ENTER to return to the main room...)";
    while(TerminalUI::getKeyPress() != 3);
}

void GameEngine::showMiscellaneousSubMenu() {
    TerminalUI::clearScreen();
    std::cout << "\n--- [ SUB-MENU: HOUSE CHAOS ] ---\n";
    std::cout << "House takes a Vicodin. It doesn't help the patient, but his leg feels better.\n";
    std::cout << "(Press ENTER to go back...)";
    while(TerminalUI::getKeyPress() != 3);
}

// --- MAIN LOOP ---

void GameEngine::run() {
    // 1. GENERAM POVESTEA PACIENTULUI LA START
    TerminalUI::clearScreen();
    std::cout << "Reading patient file...\n(House is complaining about Clinic Duty)\n";

    PatientBackstory intro = aiBrain.generateAdmissionStory(patient.getName(), patient.getHealth(), patient.getSymptom());

    TerminalUI::clearScreen();
    std::cout << "=======================================\n";
    std::cout << "   DR. HOUSE : THE VISUAL NOVEL        \n";
    std::cout << "=======================================\n\n";
    std::cout << "\033[1;33m" << intro.story << "\033[0m\n\n";
    std::cout << "(Press ENTER to start your shift...)";
    while(TerminalUI::getKeyPress() != 3);

    // 2. INTRAM IN JOC
    bool running = true;
    int turn = 1;
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

        // Acum afisam si cele 3 statistici vitale!
        std::cout << "\n[TURN " << turn << "] \n";
        std::cout << "Health: " << patient.getHealth() << "%  |  Clarity: " << patient.getClarity() << "%  |  Malpractice Risk: " << patient.getMalpractice() << "%\n\n";

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
            checkEndings(); // Aici verificam daca pacientul a murit, te-a dat afara, sau a fost vindecat
        }
    }
}