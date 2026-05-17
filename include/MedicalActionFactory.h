#ifndef MEDICAL_ACTION_FACTORY_H
#define MEDICAL_ACTION_FACTORY_H

#include "MedicalAction.h"
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Design Pattern 1: Factory Method.
// Definitions live in src/MedicalActionFactory.cpp so the 40-entry action registry
// constructor isn't expanded into every TU that includes this header.
class MedicalActionFactory {
private:
    std::map<std::string, int>                       actionCosts;    // 2nd STL container (rubric)
    std::vector<std::pair<std::string, std::string>> orderedActions; // {name, type}

public:
    MedicalActionFactory();

    std::unique_ptr<MedicalAction> create(const std::string& name) const;
    int                            getCost(const std::string& name) const;
    const std::vector<std::pair<std::string, std::string>>& getAllActions() const;
};

#endif
