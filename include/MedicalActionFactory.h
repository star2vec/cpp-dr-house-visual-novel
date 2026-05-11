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

        // Lab Tests
        reg("Blood Panel",                    "Lab Test",         300);
        reg("Chest X-ray",                    "Lab Test",         250);
        reg("Urine Culture",                  "Lab Test",         200);
        reg("Comprehensive Metabolic Panel",  "Lab Test",         400);
        reg("Thyroid Panel",                  "Lab Test",         350);
        reg("ANA / Autoantibody Panel",       "Lab Test",         500);
        reg("Heavy Metal Screen",             "Lab Test",         600);
        reg("Nerve Conduction Study",         "Lab Test",         700);
        reg("Echocardiogram",                 "Lab Test",         900);
        reg("EEG",                            "Lab Test",         600);
        reg("CT Scan",                        "Lab Test",        1500);
        reg("Full Body MRI",                  "Lab Test",        2000);
        reg("Skin / Tissue Biopsy",           "Lab Test",        1800);
        reg("Bone Marrow Biopsy",             "Lab Test",        2200);
        reg("Genetic Panel",                  "Lab Test",        3000);
        reg("Lumbar Puncture",                "Lab Test",         800);
        // Treatments
        reg("High-dose Vitamin Infusion",     "Treatment",        500);
        reg("Broad-spectrum Antibiotics",     "Treatment",        800);
        reg("Antiparasitics",                 "Treatment",       1200);
        reg("Antifungals",                    "Treatment",       1500);
        reg("Anticoagulants",                 "Treatment",       1500);
        reg("Antivirals",                     "Treatment",       2000);
        reg("High-dose Steroids",             "Treatment",       1200);
        reg("Hormone Replacement Therapy",    "Treatment",       1800);
        reg("Immunosuppressants",             "Treatment",       2500);
        reg("Chelation Therapy",              "Treatment",       3000);
        reg("Dialysis",                       "Treatment",       4500);
        reg("Enzyme Replacement Therapy",     "Treatment",       4000);
        reg("Targeted Biologic Therapy",      "Treatment",       6000);
        reg("Plasmapheresis",                 "Treatment",       5000);
        // Risky Procedures
        reg("Experimental Drug",              "Risky Procedure",10000);
        reg("Brain Biopsy",                   "Risky Procedure",15000);
        reg("Emergency Surgery",              "Risky Procedure",25000);
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
