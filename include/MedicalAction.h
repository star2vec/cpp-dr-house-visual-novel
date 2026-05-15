#ifndef MEDICAL_ACTION_H
#define MEDICAL_ACTION_H

#include <string>

// --- CLASA DE BAZA ABSTRACTA ---
// Nu vei putea crea niciodata un obiect simplu "MedicalAction",
// trebuie sa fie neaparat un Test, Tratament, etc.
class MedicalAction {
protected:
    std::string name;

    // Non-trivial static attribute (rubric)
    inline static int totalActionsPerformed = 0;

public:
    explicit MedicalAction(const std::string& n) : name(n) {}
    virtual ~MedicalAction() = default;

    virtual std::string getActionType() const = 0;

    const std::string& getName() const { return name; }

    void recordPerformed() { ++totalActionsPerformed; }

    // Non-trivial static method (rubric)
    static int getTotalActionsPerformed() { return totalActionsPerformed; }
};


// --- CLASELE DERIVATE (Mostenirea) ---

// 1. Analize (Cresc Claritatea, risc mic)
class LabTest : public MedicalAction {
public:
    explicit LabTest(const std::string& n) : MedicalAction(n) {}

    // Suprascriem functia pura
    // cppcheck-suppress unusedFunction
    std::string getActionType() const override {
        return "Lab Test";
    }
};

// 2. Tratament (Modifica masiv Health, risc mediu)
class Treatment : public MedicalAction {
public:
    explicit Treatment(const std::string& n) : MedicalAction(n) {}

    std::string getActionType() const override {
        return "Treatment";
    }
};

// 3. Proceduri Riscante (Risc imens de Malpraxis, dar pot salva viata)
class RiskyProcedure : public MedicalAction {
public:
    explicit RiskyProcedure(const std::string& n) : MedicalAction(n) {}

    std::string getActionType() const override {
        return "Risky Procedure";
    }
};

// 4. Ingrijire Suportiva (Stabilizare simptomatica — nu vindeca, nu dauneaza)
class SupportiveCare : public MedicalAction {
public:
    explicit SupportiveCare(const std::string& n) : MedicalAction(n) {}

    std::string getActionType() const override {
        return "Supportive Care";
    }
};

#endif
