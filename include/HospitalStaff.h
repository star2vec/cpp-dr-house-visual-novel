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
    virtual void interact() = 0;
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
    void interact() override;
};

class drHouse : public Doctor {
private:
    int sarcasmLevel;
    int vicodinPills;
public:
    drHouse();
    void interact() override;
};

class TeamMember : public Doctor {
public:
    TeamMember(const std::string& n, const std::string& spec);
    void interact() override;
};

class Wilson : public Doctor {
public:
    Wilson();
    void interact() override;
};

#endif