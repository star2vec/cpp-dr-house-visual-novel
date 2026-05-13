#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#define DEBUG_MODE false  // flip to true to reveal hidden diagnosis

#include "Patient.h"
#include "LLM.h"
#include "GameLog.h"

class GameEngine {
private:
    Patient patient;
    LLM aiBrain;
    int budget = 50000;
    int vicodinLevel = 0;
    int turn = 1;
    GameLog<std::string>     narrativeLog;
    GameLog<ActionRecord>    actionLog;

    void checkEndings();

    void showMedicalSubMenu();
    void showSocialSubMenu();
    void showMiscellaneousSubMenu();
    void showWhiteboardMenu();
    void renderCaseBoard(bool expanded = false);

    void runEurekaFinale();
    void triggerDirectorsCut(const std::string& outcome);

public:
    // Acum primește un pacient din exterior!
    GameEngine(Patient p);
    void run();
};

#endif