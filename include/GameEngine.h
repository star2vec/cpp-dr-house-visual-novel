#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "Patient.h"
#include "LLM.h"

class GameEngine {
private:
    Patient patient;
    LLM aiBrain;

    void checkEndings();
    
    void showMedicalSubMenu();
    void showSocialSubMenu();
    void showMiscellaneousSubMenu();

public:
    // Acum primește un pacient din exterior!
    GameEngine(Patient p);
    void run();
};

#endif