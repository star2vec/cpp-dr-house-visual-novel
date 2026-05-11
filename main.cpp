/*
#include <iostream>
#include <array>

int main() {
    std::cout << "Hello, world!\n";
    std::array<int, 100> v{};
    int nr;
    std::cout << "Introduceți nr: ";
    /////////////////////////////////////////////////////////////////////////
    /// Observație: dacă aveți nevoie să citiți date de intrare de la tastatură,
    /// dați exemple de date de intrare folosind fișierul tastatura.txt
    /// Trebuie să aveți în fișierul tastatura.txt suficiente date de intrare
    /// (în formatul impus de voi) astfel încât execuția programului să se încheie.
    /// De asemenea, trebuie să adăugați în acest fișier date de intrare
    /// pentru cât mai multe ramuri de execuție.
    /// Dorim să facem acest lucru pentru a automatiza testarea codului, fără să
    /// mai pierdem timp de fiecare dată să introducem de la zero aceleași date de intrare.
    ///
    /// Pe GitHub Actions (bife), fișierul tastatura.txt este folosit
    /// pentru a simula date introduse de la tastatură.
    /// Bifele verifică dacă programul are erori de compilare, erori de memorie și memory leaks.
    ///
    /// Dacă nu puneți în tastatura.txt suficiente date de intrare, îmi rezerv dreptul să vă
    /// testez codul cu ce date de intrare am chef și să nu pun notă dacă găsesc vreun bug.
    /// Impun această cerință ca să învățați să faceți un demo și să arătați părțile din
    /// program care merg (și să le evitați pe cele care nu merg).
    ///
    /////////////////////////////////////////////////////////////////////////
    std::cin >> nr;
    /////////////////////////////////////////////////////////////////////////
    for(int i = 0; i < nr; ++i) {
        std::cout << "v[" << i << "] = ";
        std::cin >> v[i];
    }
    std::cout << "\n\n";
    std::cout << "Am citit de la tastatură " << nr << " elemente:\n";
    for(int i = 0; i < nr; ++i) {
        std::cout << "- " << v[i] << "\n";
    }
    ///////////////////////////////////////////////////////////////////////////
    /// Pentru date citite din fișier, NU folosiți tastatura.txt. Creați-vă voi
    /// alt fișier propriu cu ce alt nume doriți.
    /// Exemplu:
    /// std::ifstream fis("date.txt");
    /// for(int i = 0; i < nr2; ++i)
    ///     fis >> v2[i];
    return 0;
}
*/
#include "GameEngine.h"
#include "TerminalUI.h"
#include "LLM.h"
#include "Exceptions.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "--test") {
        std::cout << "=== DR. HOUSE AUTO-TEST MODE ===\n\n";
        int passed = 0;

        try {
            throw PatientDeathException(1);
        } catch (GameException& e) {
            std::cout << "[PASS] PatientDeathException caught as GameException: "
                      << e.what() << "\n";
            ++passed;
        }

        try {
            throw FiredByHospitalException(1);
        } catch (GameException& e) {
            std::cout << "[PASS] FiredByHospitalException caught as GameException: "
                      << e.what() << "\n";
            ++passed;
        }

        try {
            throw OutOfBudgetException(1);
        } catch (GameException& e) {
            std::cout << "[PASS] OutOfBudgetException caught as GameException: "
                      << e.what() << "\n";
            ++passed;
        }

        try {
            throw MayfieldWardException(1);
        } catch (GameException& e) {
            std::cout << "[PASS] MayfieldWardException caught as GameException: "
                      << e.what() << "\n";
            ++passed;
        }

        std::cout << "\nAll " << passed << "/4 exception tests PASSED.\n";
        return 0;
    }

    LLM aiBrain;

    TerminalUI::clearScreen();
    std::cout << "=======================================\n";
    std::cout << "   DR. HOUSE : THE VISUAL NOVEL        \n";
    std::cout << "=======================================\n\n";
    std::cout << "Cameron is preparing patient files... (Please wait)\n";

    // 1. Generam cei 3 pacienti
    std::vector<PatientProfile> profiles = aiBrain.generatePatientFiles(3);

    // 2. Meniul tip CARUSEL
    int selected = 0;
    bool chosen = false;

    while(!chosen) {
        TerminalUI::clearScreen();
        std::cout << "=== SELECT A PATIENT FILE (UP/DOWN to browse, ENTER to select) ===\n\n";

        std::cout << "   [ FILE " << (selected + 1) << " OF " << profiles.size() << " ]\n";
        std::cout << "--------------------------------------\n";
        std::cout << " Name:     \033[1;36m" << profiles[selected].name << "\033[0m\n";
        std::cout << " Health:   " << profiles[selected].health << "/100\n";
        std::cout << " Symptom:  \033[1;31m" << profiles[selected].symptom << "\033[0m\n";
        std::cout << "--------------------------------------\n";
        std::cout << " Notes: " << profiles[selected].story << "\n";
        std::cout << "--------------------------------------\n\n";

        std::cout << "(Use UP/DOWN arrows to flip folders, ENTER to take the case)";

        // Folosim 1 (UP) si 2 (DOWN) din sistemul tau curent, pentru siguranta
        int key = TerminalUI::getKeyPress();
        if (key == 1) { // UP
            selected--;
            if (selected < 0) selected = profiles.size() - 1;
        } else if (key == 2) { // DOWN
            selected++;
            if (selected >= (int)profiles.size()) selected = 0;
        } else if (key == 3) { // ENTER
            chosen = true;
        }
    }

    // 3. Cream pacientul C++ pe baza alegerii (cu hiddenDiagnosis si severity)
    Patient chosenPatient(
        profiles[selected].name,
        profiles[selected].health,
        profiles[selected].symptom,
        profiles[selected].hiddenDiagnosis,
        profiles[selected].diseaseSeverity
    );

    // 4. Pornim motorul de joc!
    GameEngine engine(chosenPatient);

    // (Opcional, dar util): Putem printa povestea chiar inainte de start
    TerminalUI::clearScreen();
    std::cout << "You took the file. Cuddy hands you a marker.\n\n";
    std::cout << "\033[1;33m" << profiles[selected].story << "\033[0m\n\n";
    std::cout << "(Press ENTER to enter the room...)";
    while(TerminalUI::getKeyPress() != 3);

    engine.run();

    return 0;
}