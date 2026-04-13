#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <string>

class GameException : public std::exception {
protected:
    std::string message;
public:
    GameException(const std::string& msg) : message(msg) {}

    const char* what() const noexcept override { 
        return message.c_str(); 
    }
};

class PatientDeathException : public GameException {
public:
    PatientDeathException()
        : GameException("BEEEEP. Patient flatlined. Foreman glares at you: 'Your arrogance killed a man, House.'") {}
};

#endif