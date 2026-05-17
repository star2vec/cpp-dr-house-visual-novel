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
    int budget = 8000;
    int vicodinLevel = 0;
    int turn = 1;
    int cuddyFundsGranted = 0;

    int  wilsonUsesLeft    = 3;      // -1 once payback scene has fired
    bool brokeInOnce       = false;
    bool skippedClinicOnce = false;
    bool prankedOnce       = false;
    bool monsterTrucksOnce = false;
    bool cuddyDeniedNext   = false;  // set by Skip Clinic, consumed in the Cuddy branch
    bool eurekaPending     = false;  // clarity hit 100 but health < 10 — waiting for stabilization
    bool lastChoiceNoOp    = false;  // set by a sub-menu when it refuses to consume the turn

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
    void runForemanPostMortem(const std::string& outcome);
    void showPostGameMenu(const std::string& outcome);

public:
    // Acum primește un pacient din exterior!
    explicit GameEngine(const Patient& p);
    void run();
};

#endif