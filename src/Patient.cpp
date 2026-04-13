#include "Patient.h"

// Constructor de initializare (Default)
Patient::Patient() : name("Unknown"), health(100), diagnosticCurent(new std::string("Nediagnosticat")), diagnosticClarity(0), malpracticeRisk(0) {}

// Constructor supraincarcat
Patient::Patient(std::string n, int h, std::string diag) : name(n), health(h), diagnosticCurent(new std::string(diag)), diagnosticClarity(0), malpracticeRisk(0) {}

// --- REGULA CELOR 3 ---

// 1. Destructorul
Patient::~Patient() {
    delete diagnosticCurent;
}

// 2. Constructorul de Copiere (Facem Deep Copy)
Patient::Patient(const Patient& other) : name(other.name), health(other.health), diagnosticClarity(other.diagnosticClarity), malpracticeRisk(other.malpracticeRisk) {
    this->diagnosticCurent = new std::string(*(other.diagnosticCurent));
}

// 3. Operatorul = (Atribuire)
Patient& Patient::operator=(const Patient& other) {
    if (this == &other) {
        return *this;
    }

    this->name = other.name;
    this->health = other.health;
    this->diagnosticClarity = other.diagnosticClarity; // Copiem noile staturi
    this->malpracticeRisk = other.malpracticeRisk;     // Copiem noile staturi

    delete this->diagnosticCurent;
    this->diagnosticCurent = new std::string(*(other.diagnosticCurent));

    return *this;
}

// --- OPERATORII PENTRU TERMINAL ---

std::ostream& operator<<(std::ostream& os, const Patient& p) {
    os << "=== DOSAR PACIENT ===\n";
    os << "Nume: " << p.name << "\n";
    os << "Sanatate: " << p.health << "/100\n";
    os << "Claritate Diagnostic: " << p.diagnosticClarity << "%\n";
    os << "Risc Malpraxis: " << p.malpracticeRisk << "%\n";
    os << "Simptom/Diagnostic: " << *(p.diagnosticCurent) << "\n";
    os << "=====================\n";
    return os;
}

std::istream& operator>>(std::istream& is, Patient& p) {
    std::cout << "Introduceti Numele Pacientului: ";
    is >> p.name;
    std::cout << "Introduceti Nivelul de Sanatate (0-100): ";
    is >> p.health;

    std::string diag;
    std::cout << "Ce simptom are?: ";
    is >> diag;

    delete p.diagnosticCurent;
    p.diagnosticCurent = new std::string(diag);

    // Resetam la venirea unui pacient nou
    p.diagnosticClarity = 0;
    p.malpracticeRisk = 0;

    return is;
}

// Getteri
int Patient::getHealth() const { return health; }
std::string Patient::getName() const { return name; }
std::string Patient::getSymptom() const { return *diagnosticCurent; }
int Patient::getClarity() const { return diagnosticClarity; }
int Patient::getMalpractice() const { return malpracticeRisk; }

// Modificatori (Cu "Clamping" - impiedicam valorile sa iasa din [0, 100])
void Patient::modifyHealth(int amount) {
    health += amount;
    if (health > 100) health = 100;
    if (health < 0) health = 0;
}

void Patient::modifyClarity(int amount) {
    diagnosticClarity += amount;
    if (diagnosticClarity > 100) diagnosticClarity = 100;
    if (diagnosticClarity < 0) diagnosticClarity = 0;
}

void Patient::modifyMalpractice(int amount) {
    malpracticeRisk += amount;
    if (malpracticeRisk > 100) malpracticeRisk = 100;
    if (malpracticeRisk < 0) malpracticeRisk = 0;
}