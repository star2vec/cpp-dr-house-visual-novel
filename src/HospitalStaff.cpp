#include "HospitalStaff.h"
#include "TerminalUI.h"
#include <iostream>

// Constructor clasa de baza
HospitalStaff::HospitalStaff(const std::string& n) : name(n) {}

// Destructor virtual - OBLIGATORIU pentru a evita memory leak-uri la upcast
HospitalStaff::~HospitalStaff() {}

// Doctor apeleaza constructorul din HospitalStaff
Doctor::Doctor(const std::string& n, const std::string& spec) : HospitalStaff(n), specialty(spec) {}

// Each say() method colors the speaker prefix in that character's palette, then
// runs the body of the line through TerminalUI::typewrite so it types in slowly
// with the visual-novel feel (any keypress skips the current line to its end).

// Administrator (Cuddy) — magenta
Administrator::Administrator() : HospitalStaff("Lisa Cuddy") {}
void Administrator::say(const std::string& line) const {
    std::cout << "\033[1;35m[Cuddy]\033[0m: ";
    std::cout.flush();
    TerminalUI::typewrite(line, 32);   // slower per-char pace for the visual-novel intro
    std::cout << "\n";
}

// drHouse — red, the centerpiece
drHouse::drHouse() : Doctor("Gregory House", "Diagnostic Medicine"), sarcasmLevel(85), vicodinPills(5) {}
void drHouse::say(const std::string& line) const {
    std::cout << "\033[1;31m[House]\033[0m: ";
    std::cout.flush();
    TerminalUI::typewrite(line, 32);   // slower per-char pace for the visual-novel intro
    std::cout << "\n";
}

// TeamMember (Chase / Foreman / Cameron) — cyan, includes specialty in the prefix
TeamMember::TeamMember(const std::string& n, const std::string& spec) : Doctor(n, spec) {}
void TeamMember::say(const std::string& line) const {
    std::cout << "\033[1;36m[" << name << " - " << specialty << "]\033[0m: ";
    std::cout.flush();
    TerminalUI::typewrite(line, 32);   // slower per-char pace for the visual-novel intro
    std::cout << "\n";
}

// Wilson — yellow
Wilson::Wilson() : Doctor("James Wilson", "Oncology") {}
void Wilson::say(const std::string& line) const {
    std::cout << "\033[1;33m[Wilson]\033[0m: ";
    std::cout.flush();
    TerminalUI::typewrite(line, 32);   // slower per-char pace for the visual-novel intro
    std::cout << "\n";
}
