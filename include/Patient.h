#ifndef PATIENT_H
#define PATIENT_H

#include <string>
#include <iostream>
#include <vector>

class Patient {
private:
    std::string name;
    int health;
    std::string* diagnosticCurent; // Visible symptom shown to player

    int diagnosticClarity; // 0-100%
    int malpracticeRisk;   // 0-100%

    // Hidden diagnosis system — Game Master context, never shown to player
    std::string hiddenDiagnosis;
    int diseaseSeverity;   // 1 (mild) / 2 (moderate) / 3 (critical)

public:
    Patient();
    Patient(std::string n, int h, std::string symptom);
    Patient(std::string n, int h, std::string symptom,
            std::string hiddenDiag, int severity);

    // --- REGULA CELOR 3 ---
    ~Patient();
    Patient(const Patient& other);
    Patient& operator=(const Patient& other);

    // Getteri
    int getHealth() const;
    std::string getName() const;
    std::string getSymptom() const;
    int getClarity() const;
    int getMalpractice() const;
    const std::string& getHiddenDiagnosis() const;
    int getDiseaseSeverity() const;

    // Modificatori
    void modifyHealth(int amount);
    void modifyClarity(int amount);
    void modifyMalpractice(int amount);

    // --- OPERATORI I/O ---
    friend std::ostream& operator<<(std::ostream& os, const Patient& p);
    friend std::istream& operator>>(std::istream& is, Patient& p);
};

#endif