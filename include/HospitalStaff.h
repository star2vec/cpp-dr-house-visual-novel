#ifndef HOSPITAL_STAFF_H
#define HOSPITAL_STAFF_H

#include <string>
#include <vector>

class HospitalStaff {
protected:
    std::string name;
public:
    explicit HospitalStaff(const std::string& n);
    virtual ~HospitalStaff();

    // Pure virtual: each derived staff type renders a line of dialogue in its own voice
    // (colored prefix + typewriter on the body). The scene script in main.cpp drives
    // the order of calls — this is just the per-character rendering hook.
    virtual void say(const std::string& line) const = 0;
};

class Doctor : public HospitalStaff {
protected:
    std::string specialty;
public:
    Doctor(const std::string& n, const std::string& spec);
};

class Administrator : public HospitalStaff {
public:
    Administrator();
    void say(const std::string& line) const override;
};

class drHouse : public Doctor {
private:
    int sarcasmLevel;
    int vicodinPills;
public:
    drHouse();
    void say(const std::string& line) const override;
    int getSarcasm() const { return sarcasmLevel; }
    int getVicodin() const { return vicodinPills; }
};

class TeamMember : public Doctor {
public:
    TeamMember(const std::string& n, const std::string& spec);
    void say(const std::string& line) const override;
};

class Wilson : public Doctor {
public:
    Wilson();
    void say(const std::string& line) const override;
};

#endif