#ifndef PATIENT_H
#define PATIENT_H

#include <string>
#include <iostream>
#include <vector>

class Patient {
private:
    std::string name;
    int health;
    std::string* diagnosticCurent; // Folosit si pe post de simptom la inceput

    // NOILE ATRIBUTE PENTRU GAME MASTER-UL AI
    int diagnosticClarity; // 0% - 100% (La 100% stim boala)
    int malpracticeRisk;   // 0% - 100% (La 100% esti dat afara)

public:
    Patient();
    Patient(std::string n, int h, std::string diag);

    // --- REGULA CELOR 3 ---
    ~Patient();
    Patient(const Patient& other);
    Patient& operator=(const Patient& other);

    // Getteri
    int getHealth() const;
    std::string getName() const;
    std::string getSymptom() const; // Necesara pentru AI prompt
    int getClarity() const;
    int getMalpractice() const;

    // Modificatori
    void modifyHealth(int amount);
    void modifyClarity(int amount);
    void modifyMalpractice(int amount);

    // --- OPERATORI I/O ---
    friend std::ostream& operator<<(std::ostream& os, const Patient& p);
    friend std::istream& operator>>(std::istream& is, Patient& p);
};

#endif