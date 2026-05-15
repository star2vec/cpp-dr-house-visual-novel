#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <string>

class GameException : public std::exception {
protected:
    std::string message;
public:
    explicit GameException(const std::string& msg) : message(msg) {}

    const char* what() const noexcept override {
        return message.c_str();
    }
};

class PatientDeathException : public GameException {
public:
    explicit PatientDeathException(int turn)
        : GameException("Turn " + std::to_string(turn) +
          ": BEEEEP. Patient flatlined. Foreman: 'Your arrogance killed a man, House.'") {}
};

class FiredByHospitalException : public GameException {
public:
    explicit FiredByHospitalException(int turn)
        : GameException("Turn " + std::to_string(turn) +
          ": Cuddy slides your termination letter across the desk. You're fired.") {}
};

class OutOfBudgetException : public GameException {
public:
    explicit OutOfBudgetException(int turn)
        : GameException("Turn " + std::to_string(turn) +
          ": The hospital accountant pulls the plug. Budget exhausted.") {}
};

class MayfieldWardException : public GameException {
public:
    explicit MayfieldWardException(int turn)
        : GameException("Turn " + std::to_string(turn) +
          ": House checks himself into Mayfield Psychiatric Hospital. Vicodin wins.") {}
};

#endif