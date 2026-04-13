#include "HospitalStaff.h"
#include <iostream>

// Constructor clasa de baza
HospitalStaff::HospitalStaff(std::string n) : name(n) {}

// Destructor virtual - OBLIGATORIU pentru a evita memory leak-uri la upcast
HospitalStaff::~HospitalStaff() {}

// Doctor apeleaza constructorul din HospitalStaff
Doctor::Doctor(std::string n, std::string spec) : HospitalStaff(n), specialty(spec) {}

// Administrator (Cuddy)
Administrator::Administrator() : HospitalStaff("Lisa Cuddy") {}
void Administrator::interact() {
    std::cout << "\n[CUDDY]: House, my office. NOW. I've seen the bill for that MRI.\n";
}

// drHouse - foloseste atribute private
drHouse::drHouse() : Doctor("Gregory House", "Diagnostic Medicine"), sarcasmLevel(85), vicodinPills(5) {}
void drHouse::interact() {
    std::cout << "\n[HOUSE]: Everybody lies. Especially you. Go away, I have a TV show to watch.\n";
    std::cout << "(Current Sarcasm: " << sarcasmLevel << " | Vicodin: " << vicodinPills << ")\n";
}

// TeamMember (Chase/Foreman/Cameron) - apeleaza constructorul din Doctor
TeamMember::TeamMember(std::string n, std::string spec) : Doctor(n, spec) {}
void TeamMember::interact() {
    std::cout << "\n[" << name << " - " << specialty << "]: We should probably check for Sarcoidosis or Lupus.\n";
}

Wilson::Wilson() : Doctor("James Wilson", "Oncology") {}
void Wilson::interact() {
    std::cout << "\n[WILSON]: You're being a jerk, House. Let's grab lunch and talk about your feelings.\n";
}