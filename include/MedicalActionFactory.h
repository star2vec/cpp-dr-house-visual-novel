#ifndef MEDICAL_ACTION_FACTORY_H
#define MEDICAL_ACTION_FACTORY_H

#include "MedicalAction.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

// Design Pattern 1: Factory Method
class MedicalActionFactory {
private:
    std::map<std::string, int>                       actionCosts;  // 2nd STL container (rubric)
    std::vector<std::pair<std::string, std::string>> orderedActions; // {name, type}

public:
    MedicalActionFactory() {
        auto reg = [&](const std::string& name, const std::string& type, int cost) {
            orderedActions.push_back({name, type});
            actionCosts[name] = cost;
        };

        reg("Lumbar Puncture",            "Lab Test",         800);
        reg("Full Body MRI",              "Lab Test",        2000);
        reg("Blood Panel",                "Lab Test",         300);
        reg("CT Scan",                    "Lab Test",        1500);
        reg("EEG",                        "Lab Test",         600);
        reg("Urine Culture",              "Lab Test",         200);
        reg("Echocardiogram",             "Lab Test",         900);
        reg("High-dose Steroids",         "Treatment",       1200);
        reg("Broad-spectrum Antibiotics", "Treatment",        800);
        reg("Antivirals",                 "Treatment",       2000);
        reg("Plasmapheresis",             "Treatment",       5000);
        reg("Brain Biopsy",               "Risky Procedure",15000);
        reg("Emergency Surgery",          "Risky Procedure",25000);
        reg("Experimental Drug",          "Risky Procedure",10000);
    }

    std::unique_ptr<MedicalAction> create(const std::string& name) const {
        for (const auto& [n, type] : orderedActions) {
            if (n == name) {
                if (type == "Lab Test")        return std::make_unique<LabTest>(name);
                if (type == "Treatment")       return std::make_unique<Treatment>(name);
                if (type == "Risky Procedure") return std::make_unique<RiskyProcedure>(name);
            }
        }
        return nullptr;
    }

    int getCost(const std::string& name) const {
        auto it = actionCosts.find(name);
        return (it != actionCosts.end()) ? it->second : 0;
    }

    const std::vector<std::pair<std::string, std::string>>& getAllActions() const {
        return orderedActions;
    }
};

#endif
