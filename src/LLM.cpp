#include "LLM.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <ctime>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "external/httplib.h"
#include "external/json.hpp"

using json = nlohmann::json;

// Trims a prose string to the last complete sentence (ending in . ! or ?).
// Prevents mid-sentence cutoffs when the model hits max_tokens.
static std::string trimToLastSentence(const std::string& text) {
    if (text.empty()) return text;
    size_t last = text.find_last_of(".!?");
    if (last == std::string::npos) return text;
    return text.substr(0, last + 1);
}

LLM::LLM() {}

std::vector<std::string> LLM::getHouseIntents(const std::string& character) {
    std::vector<std::string> intents;

    if (character == "The Team") {
        intents.push_back("[Insult] Mock their medical competence.");
        intents.push_back("[Hint] Suggest a crazy, rare autoimmune disease.");
    }
    else if (character == "Wilson") {
        intents.push_back("[Steal] Steal his lunch or demand money.");
        intents.push_back("[Deflect] Psychoanalyze Wilson's failed marriages.");
    }
    else if (character == "Cuddy") {
        intents.push_back("[Inappropriate] Make a comment about her outfit.");
        intents.push_back("[Demand] Ask for a dangerous, expensive medical test.");
    }
    else {
        intents.push_back("[Sarcasm] Say something mean.");
        intents.push_back("[Leave] Look at your watch and sigh.");
    }

    intents.push_back("Nevermind. Walk away. (Exit conversation)");
    return intents;
}

DialogueResponse LLM::generateDialogue(const std::string& character, const std::vector<std::string>& history, const std::string& chosenIntent, const std::string& patientName, const std::string& symptom) {
    DialogueResponse response;

    // AICI PUI CHEIA TA DE LA ANTHROPIC (Claude)
    std::string apiKey = "";
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        try {
            json secrets = json::parse(secretFile);
            apiKey = secrets.value("ANTHROPIC_API_KEY", "");
        } catch (...) {
            response.houseLine = "[SYSTEM ERROR]";
            response.characterReply = "Could not parse secrets.json. Is it formatted correctly?";
            return response;
        }
    } else {
        response.houseLine = "[SYSTEM ERROR]";
        response.characterReply = "secrets.json file not found! Create it in the root directory.";
        return response;
    }

    if (apiKey.empty()) {
        response.houseLine = "[SYSTEM ERROR]";
        response.characterReply = "API key is empty in secrets.json!";
        return response;
    }
    // ------------------------------------------------------------

    // 1. Contextul
    std::string historyContext = "Previous conversation context:\n";
    if (history.empty()) {
        historyContext += "(This is the start of the conversation)\n";
    } else {
        for (const auto& log : history) {
            historyContext += log + "\n";
        }
    }

    // 2. Impartim instructiunile pentru standardul Anthropic
    std::string systemPrompt =
        "You are a scriptwriter for House M.D. Dr. House is currently talking to " + character + ".\n"
        + (!patientName.empty() ? "Current case: patient named " + patientName + " presenting with: " + symptom + ".\n" : "")
        + historyContext +
        "\nYou MUST return strictly a JSON object with two keys: 'house' and 'reply'. "
        "Do NOT wrap the JSON in markdown blocks (no ```json). Output absolutely NOTHING else but the raw JSON brackets.";

    std::string userPrompt =
        "House's chosen intent for his NEXT line is: " + chosenIntent + "\n"
        "Write exactly 1 sarcastic, brilliant sentence for House, and 1 realistic reaction sentence for " + character + ".";

    // 3. Noul JSON specific pentru Claude
    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 512}, // OBLIGATORIU LA ANTHROPIC
        {"system", systemPrompt}, // System prompt e afara din array!
        {"messages", {
            {{"role", "user"}, {"content", userPrompt}}
        }},
        {"temperature", 0.7}
    };

    // 4. Connect to Anthropic with strict settings
    httplib::Client cli("https://api.anthropic.com");

    // Setăm timeout-uri (uneori Mac-ul stă prea mult să verifice certs)
    cli.set_connection_timeout(5, 0); // 5 secunde
    cli.set_read_timeout(10, 0);    // 10 secunde
    cli.set_follow_location(true);

    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"}
    };

    std::cout << "  (House is thinking...)";
    std::cout.flush();

    // 5. Trimitem request-ul.
    // IMPORTANT: Nu mai folosim ultimul parametru "application/json"
    // ca sa evitam orice dublura de header, il punem manual in headers.
    headers.emplace("Content-Type", "application/json");

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            // Anthropic are alta locatie pentru textul returnat
            std::string content = responseJson["content"][0]["text"];

            json dialogueJson = json::parse(content);

            response.houseLine = trimToLastSentence(dialogueJson.value("house", "[JSON Missing 'house']"));
            response.characterReply = trimToLastSentence(dialogueJson.value("reply", "[JSON Missing 'reply']"));
        } catch (const std::exception& e) {
            response.houseLine = "[JSON Parsing Error]";
            response.characterReply = e.what();
        }
    } else {
        response.houseLine = "[API ERROR]";
        if (res) {
            response.characterReply = "HTTP " + std::to_string(res->status) + " Server said: " + res->body;
        } else {
            response.characterReply = "Connection failed entirely.";
        }
    }

    return response;
}

PatientBackstory LLM::generateAdmissionStory(const std::string& name, int health, const std::string& symptom) {
    PatientBackstory backstory;

    // Citim cheia iarasi (o poti muta intr-o functie separata pe viitor pentru a nu repeta codul)
    std::string apiKey = "";
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are Dr. House. You MUST return strictly a JSON object with one key: 'story'. "
        "Do NOT wrap the JSON in markdown blocks. Output absolutely NOTHING else.";

    std::string userPrompt =
        "Write a dark, sarcastic 2-sentence medical chart introduction for a new patient named " + name + ". "
        "Symptom: " + symptom + ". Current Health: " + std::to_string(health) + "/100.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 256},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.7}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(15, 0); // Am crescut putin timpul ca sa aiba timp sa scrie 3 pacienti

    // FARA "Content-Type" AICI PENTRU CA IL PUNE AUTOMAT cli.Post:
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            std::string content = responseJson["content"][0]["text"];
            json storyJson = json::parse(content);
            backstory.story = trimToLastSentence(storyJson.value("story", "[JSON Parse Error]"));
        } catch (...) {
            backstory.story = "[Eroare la parsarea povestii pacientului]";
        }
    } else {
        backstory.story = "[API Error - Nu am putut genera povestea]";
    }

    return backstory;
}

MedicalOutcome LLM::evaluateMedicalAction(const std::string& actionType, const std::string& actionName, int currentHealth, int currentClarity, const std::string& symptom, const std::string& hiddenDiagnosis, int diseaseSeverity) {
    MedicalOutcome outcome;

    std::string apiKey = "";
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are a Game Master for a medical simulator. You MUST return strictly a JSON object with EXACTLY these keys: "
        "'narrative' (string), 'brief' (string), 'health_change' (integer, can be negative), 'clarity_change' (integer), 'malpractice_change' (integer). "
        "The 'brief' key: 6 words max summarizing the key clinical finding only (e.g. 'elevated WBC, bilateral infiltrates'). "
        "NEVER include the diagnosis name in 'brief'. "
        "Do NOT wrap the JSON in markdown blocks. Output absolutely NOTHING else.\n"
        "[GAME MASTER SECRET — NOT SHOWN TO PLAYER]: The patient's true diagnosis is: " + hiddenDiagnosis + ". "
        "Use this to judge whether the action is medically appropriate. "
        "For treatments, distinguish between plausibly wrong (reasonable guess, mild harm) and contraindicated (actively harmful). "
        "CRITICAL: NEVER write the diagnosis name in the narrative — not even partially. "
        "Describe only lab findings, physiological effects, organ involvement, and House's clinical observations. "
        "The player must never learn the disease name from this text.";

    std::string userPrompt =
        "Patient visible symptom: " + symptom + ". Health: " + std::to_string(currentHealth) + "/100. Diagnostic Clarity: " + std::to_string(currentClarity) + "%.\n"
        "Dr. House orders a " + actionType + ": " + actionName + ".\n"
        "Evaluate this against the true (hidden) diagnosis. "
        "If it's a Lab Test, clarity goes up based on how relevant the test is to the real diagnosis. "
        "If it's a Treatment: if correct for the hidden diagnosis, it helps; if wrong, it causes significant harm and malpractice risk. "
        "Delta calibration — use these ranges:\n"
        "  Lab Test highly relevant: clarity +12 to +20, health 0 to -3.\n"
        "  Lab Test somewhat relevant: clarity +4 to +10, health 0 to -2.\n"
        "  Lab Test irrelevant: clarity 0 to +2, health 0 to -1.\n"
        "  Treatment — pick exactly one of three tiers based on the hidden diagnosis:\n"
        "    CORRECT (treatment directly targets the hidden diagnosis or its core mechanism):\n"
        "      health +5 to +12, clarity +8 to +15, malpractice 0.\n"
        "    PLAUSIBLE_WRONG (wrong diagnosis but a defensible guess — treats a related symptom\n"
        "      or adjacent mechanism without addressing root cause):\n"
        "      health -3 to -8, clarity +3 to +7, malpractice +3 to +10.\n"
        "      Narrative: patient shows partial, superficial, or temporary response — something moved,\n"
        "      but the underlying mechanism is untouched.\n"
        "    CONTRAINDICATED (wrong drug class, actively worsens the disease mechanism,\n"
        "      or immunologically dangerous given the hidden diagnosis):\n"
        "      health -15 to -25, clarity +1 to +3, malpractice +15 to +25.\n"
        "      Narrative: make clear HOW the patient deteriorated — which system, which sign worsened.\n"
        "  RiskyProcedure: always malpractice +15 to +25; if relevant clarity +15 to +25 and health -5 to -10; if not health -15 to -25.\n"
        "  Supportive Care (disease severity " + std::to_string(diseaseSeverity) + "/3):\n"
        "    First judge whether this action is relevant to the VISIBLE symptom presentation (ignore hidden diagnosis — use only the symptom field above).\n"
        "    If IRRELEVANT to visible symptoms: health 0, clarity 0, malpractice 0. Narrative: House observes the action has no discernible effect given what this patient is showing.\n"
        "    If RELEVANT to visible symptoms, apply the severity-based range:\n"
        "      Severity 1: health +4 to +8.  Severity 2: health +2 to +5.  Severity 3: health +1 to +3.\n"
        "    Always: clarity 0, malpractice 0. CRITICAL: health_change MUST be >= 0. Supportive Care NEVER harms.\n"
        "    Sentence 1: specific observable improvement (vital sign, pain level, oxygenation).\n"
        "    Sentence 2: House's sardonic remark that this buys time but solves nothing. At severity 3 make clear the patient is still declining.\n"
        "Write exactly 2 sentences for the narrative. "
        "Sentence 1 — a plain clinical observation accessible to any reader (what was measured, found, or seen: "
        "lab values, scan descriptions, physical signs, test results — no jargon). "
        "Sentence 2 — House's interpretation: mechanisms, body systems, what this means for the case, "
        "with his signature cynicism. NEVER name the diagnosis in either sentence.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 300},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.7}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);
    httplib::Headers headers = { {"x-api-key", apiKey}, {"anthropic-version", "2023-06-01"}, {"Content-Type", "application/json"} };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            std::string content = responseJson["content"][0]["text"];

            // --- MAGIA DE SANITIZARE ---
            // Taiem tot ce e in afara acoladelor { } (ex: "Aici este raspunsul tau: ```json ... ```")
            size_t firstBrace = content.find('{');
            size_t lastBrace = content.rfind('}');
            if (firstBrace != std::string::npos && lastBrace != std::string::npos && lastBrace >= firstBrace) {
                content = content.substr(firstBrace, lastBrace - firstBrace + 1);
            }
            // ---------------------------

            json outJson = json::parse(content);

            outcome.narrative = trimToLastSentence(outJson.value("narrative", "[Error parsing narrative]"));
            outcome.brief = outJson.value("brief", "");
            outcome.healthDelta = outJson.value("health_change", 0);
            outcome.clarityDelta = outJson.value("clarity_change", 0);
            outcome.malpracticeDelta = outJson.value("malpractice_change", 0);
        } catch (...) {
            outcome.narrative = "[JSON Parse Error: Model returned garbage data]";
            outcome.healthDelta = outcome.clarityDelta = outcome.malpracticeDelta = 0;
        }
    } else {
        outcome.narrative = "[API Error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
        outcome.healthDelta = outcome.clarityDelta = outcome.malpracticeDelta = 0;
    }

    return outcome;
}

std::vector<PatientProfile> LLM::generatePatientFiles(int count) {
    std::vector<PatientProfile> profiles;

    std::string apiKey = "";
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    // Pick `count` disease categories from a shuffled pool — guarantees variety across
    // patients and across runs. Each category forces a completely different body-system cluster.
    static const std::vector<std::string> allCategories = {
        "lysosomal or peroxisomal storage disorder (NOT Niemann-Pick or Gaucher — pick a rare variant)",
        "mitochondrial disease or oxidative phosphorylation defect",
        "ion channelopathy or neuromuscular junction disease (e.g. Lambert-Eaton, Brody, Isaac's syndrome)",
        "systemic vasculitis or granulomatous disease (NOT Wegener's/GPA — pick an unusual variant)",
        "paraneoplastic syndrome (cancer-induced neurological or endocrine effect)",
        "heavy metal or environmental toxin accumulation (thallium, arsenic, manganese, beryllium, etc.)",
        "hereditary periodic fever or autoinflammatory syndrome",
        "prion or slow-virus neurological disease",
        "rare coagulation or platelet disorder (NOT hemophilia or vWD)",
        "endocrine-secreting tumor causing systemic chaos (VIPoma, glucagonoma, somatostatinoma, etc.)",
        "rare nutritional deficiency causing multi-organ failure",
        "eosinophilic organ infiltration or hypereosinophilic syndrome",
        "hereditary connective tissue disorder affecting unexpected organs (NOT classic EDS)",
        "unusual infectious disease (rare parasite, atypical intracellular bacteria, endemic dimorphic fungus)",
        "complement system defect or rare primary immunodeficiency"
    };

    std::vector<std::string> categories = allCategories;
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::shuffle(categories.begin(), categories.end(), rng);

    // Build per-case category constraints
    std::string categoryConstraints;
    for (int i = 0; i < count && i < (int)categories.size(); ++i)
        categoryConstraints += "  Case " + std::to_string(i + 1) +
                               " MUST be a disease from this category: \"" + categories[i] + "\"\n";

    std::string systemPrompt =
        "You are Dr. House's diagnostic department coordinator. You MUST return strictly a JSON ARRAY of objects. "
        "Do NOT wrap the JSON in markdown blocks (no ```json). Output ONLY the raw array [ { ... }, { ... } ].";

    std::string userPrompt =
        "Generate " + std::to_string(count) + " unique, bizarre medical cases for Dr. House.\n"
        "CATEGORY ASSIGNMENTS (mandatory — each case must belong to its assigned category):\n"
        + categoryConstraints +
        "Each object in the array MUST have EXACTLY these keys: "
        "'name' (string), "
        "'health' (integer between 30 and 80), "
        "'symptom' (string — 2-3 symptoms clinically plausible for the hidden_diagnosis but non-specific and confusing. "
        "CRITICAL: the symptom cluster MUST span at least 2 different body systems, e.g. neurological + hepatic, "
        "cardiac + dermatological, renal + neuromuscular. Single-organ presentations are forbidden.), "
        "'story' (string — 2-sentence cynical backstory, no diagnosis named), "
        "'hidden_diagnosis' (string — the ONE real disease name, a genuine specific obscure condition within the assigned category), "
        "'disease_severity' (integer — 1 mild, 2 moderate, 3 critical). "
        "Derive the symptom FROM the hidden_diagnosis — medically consistent but not diagnostic on their own. "
        "The 'symptom' and 'story' fields must NOT name the hidden_diagnosis. "
        "AVOID: Wilson's Disease, Lupus, Cushing's, Addison's, Marfan, Huntington's, "
        "Acute Intermittent Porphyria, MCAS, Niemann-Pick type C, Gaucher, Wegener's/GPA, Classic EDS. "
        "Pick conditions a specialist might recognise but a general audience would never guess.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 1024},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 1.0}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(15, 0); // Am crescut putin timpul ca sa aiba timp sa scrie 3 pacienti

    // FARA "Content-Type" AICI PENTRU CA IL PUNE AUTOMAT cli.Post:
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            std::string content = responseJson["content"][0]["text"];

            // --- SANITIZARE PENTRU ARRAY [ ] ---
            size_t firstBracket = content.find('[');
            size_t lastBracket = content.rfind(']');
            if (firstBracket != std::string::npos && lastBracket != std::string::npos && lastBracket >= firstBracket) {
                content = content.substr(firstBracket, lastBracket - firstBracket + 1);
            }

            json jsonArray = json::parse(content);

            for (auto& item : jsonArray) {
                PatientProfile p;
                p.name = item.value("name", "Unknown Doe");
                p.health = item.value("health", 50);
                p.symptom = item.value("symptom", "Unexplained bleeding");
                p.story = item.value("story", "Found unconscious.");
                p.hiddenDiagnosis = item.value("hidden_diagnosis", "Unknown Pathology");
                p.diseaseSeverity = item.value("disease_severity", 1);
                if (p.diseaseSeverity < 1) p.diseaseSeverity = 1;
                if (p.diseaseSeverity > 3) p.diseaseSeverity = 3;
                profiles.push_back(p);
            }
        } catch (...) {
            // Fallback in caz de eroare grava
            profiles.push_back({"Error Doe", 50, "API Parsing Failed", "The lab lost the results.", "Unknown Pathology", 1});
        }
    } else {
        profiles.push_back({"Connection Error", 10, "No Wi-Fi", "Cuddy forgot to pay the internet bill.", "Unknown Pathology", 1});
    }

    return profiles;
}

std::string LLM::generateHouseClue(const std::string& hiddenDiagnosis,
                                    const std::string& patientName,
                                    const std::string& symptom) {
    std::string apiKey = "";
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are a terse crime-scene narrator. House has broken into a patient's home illegally. "
        "Describe ONE cryptic physical object or environmental detail he notices that subtly hints at "
        "the diagnosis '" + hiddenDiagnosis + "' WITHOUT naming the disease or any medical term for it. "
        "Two sentences maximum. Be literary and specific — a pill bottle label, a food item, a smell, a photo.";

    std::string userPrompt =
        "Patient name: " + patientName + ". Known symptom: " + symptom + ". What does House notice?";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 200},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.9}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            return trimToLastSentence(responseJson["content"][0]["text"].get<std::string>());
        } catch (...) {
            return "[Clue generation failed]";
        }
    }
    return "[API Error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
}

WilsonResult LLM::generateWilsonConsult(const std::string& symptom,
                                        const std::string& hiddenDiagnosis,
                                        const std::string& patientName,
                                        int clarity) {
    WilsonResult result;

    std::string apiKey = "";
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are Dr. James Wilson, oncologist and House's best friend. "
        "House has just barged into your office about a patient. "
        "You are perceptive about people and lifestyle patterns, not just disease. "
        "Ask 2-3 sharp questions about the patient's life — occupation, diet, travel history, "
        "family situation, stress, hobbies, home environment, chemical or toxic exposure, or recent life changes. "
        "These questions must be subtly relevant to the true diagnosis '" + hiddenDiagnosis + "' "
        "WITHOUT naming it or any medical label for it. "
        "Tone: warm but exasperated, genuinely invested in the patient as a person. "
        "2-3 sentences maximum. No medical jargon. Ask questions — don't answer them. "
        "CRITICAL: Output ONLY spoken words in 'dialogue'. No stage directions. No asterisks. "
        "Return a JSON object with exactly two keys: "
        "'dialogue' (string — Wilson's full spoken lines) and "
        "'brief' (string — 6 words max capturing what Wilson probed, e.g. 'asked about diet and occupation'). "
        "Do NOT wrap in markdown. Output only the raw JSON.";

    std::string userPrompt =
        "House storms in: \"I've got " + patientName + ". Presenting with: " + symptom + ". "
        "Diagnostic clarity at " + std::to_string(clarity) + "%. What am I missing?\"";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 220},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.8}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            std::string text = responseJson["content"][0]["text"].get<std::string>();

            size_t firstBrace = text.find('{');
            size_t lastBrace  = text.rfind('}');
            if (firstBrace != std::string::npos && lastBrace != std::string::npos)
                text = text.substr(firstBrace, lastBrace - firstBrace + 1);

            json parsed = json::parse(text);
            std::string dialogue = parsed.value("dialogue", "[Wilson is unreachable.]");
            result.brief = parsed.value("brief", "");

            // Strip any *stage directions* the model sneaked into dialogue
            std::string clean;
            bool inStage = false;
            for (char c : dialogue) {
                if (c == '*') { inStage = !inStage; }
                else if (!inStage) clean += c;
            }
            for (char& c : clean) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            size_t start = clean.find_first_not_of(' ');
            if (start != std::string::npos) clean = clean.substr(start);
            std::string collapsed;
            bool lastSp = false;
            for (char c : clean) {
                if (c == ' ') { if (!lastSp) collapsed += c; lastSp = true; }
                else { collapsed += c; lastSp = false; }
            }
            result.dialogue = trimToLastSentence(collapsed);
        } catch (...) {
            result.dialogue = "[Wilson's office is empty — probably at a divorce attorney's.]";
            result.brief = "";
        }
    } else {
        result.dialogue = "[API Error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
        result.brief = "";
    }
    return result;
}

std::string LLM::generateWhiteboardThought(const std::string& question,
                                            const std::string& symptom,
                                            const std::string& hiddenDiagnosis) {
    std::string apiKey;
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are the unfiltered internal monologue of Dr. Gregory House, M.D., standing at his whiteboard mid-case. "
        "The player is accessing your subconscious reasoning. "
        "Think out loud: make medical associations, challenge assumptions, propose 2-4 differential diagnoses, "
        "ask rhetorical questions. Use real medical terminology — mechanisms, body systems, lab findings. "
        "Be brilliant, sarcastic, dismissive of obvious answers.\n"
        "Hard output rules:\n"
        "1. NEVER name '" + hiddenDiagnosis + "' — not directly, not as a synonym, not by unique description.\n"
        "2. You MAY freely name wrong differentials that are not the answer — House chasing bad leads is realistic.\n"
        "3. Speak in mechanisms and body systems, not labels that uniquely identify the true answer.\n"
        "4. End on a question or unresolved tension, never a conclusion.\n"
        "Under 180 words. No stage directions. No parentheticals.";

    std::string userPrompt =
        "Patient's known symptom: " + symptom + ".\n"
        "House's burning question at the whiteboard: " + question;

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 250},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.9}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(12, 0);
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            return trimToLastSentence(responseJson["content"][0]["text"].get<std::string>());
        } catch (...) {
            return "[Whiteboard static — House can't form a coherent thought.]";
        }
    }
    return "[API Error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
}

TeamOpinionResult LLM::generateTeamOpinion(const std::string& personality, const std::string& agentName,
                                            const std::string& symptom, const std::string& hiddenDiagnosis,
                                            int clarity) {
    TeamOpinionResult result;

    std::string apiKey = "";
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        personality + "\n"
        "[GAME MASTER SECRET — NOT SHOWN TO PLAYER]: The patient's true diagnosis is: " + hiddenDiagnosis + ". "
        "Let this subtly colour your instinct — speak of body systems, mechanisms, and clinical patterns. "
        "NEVER use the disease name or any phrase that uniquely identifies it. "
        "Current diagnostic clarity: " + std::to_string(clarity) + "%.\n"
        "Return a JSON object with exactly two keys: "
        "'opinion' (string — your full spoken response, 2-3 punchy sentences) and "
        "'brief' (string — 6 words max capturing your key clinical suspicion, e.g. 'suspects autoimmune, recommends ANA panel'). "
        "Do NOT wrap in markdown. Output only the raw JSON.";

    std::string userPrompt =
        "Setting: whiteboard room, team diagnostic meeting. The patient is NOT present. "
        "Dr. House just asked: 'What are you thinking?' "
        "Patient's known symptom: " + symptom + ". "
        "Respond as " + agentName + " speaking directly to House. "
        "Be fast and opinionated — no preamble, no hedging. Stay in character.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 200},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.8}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            std::string text = responseJson["content"][0]["text"].get<std::string>();

            size_t firstBrace = text.find('{');
            size_t lastBrace  = text.rfind('}');
            if (firstBrace != std::string::npos && lastBrace != std::string::npos)
                text = text.substr(firstBrace, lastBrace - firstBrace + 1);

            json parsed = json::parse(text);
            result.opinion = trimToLastSentence(parsed.value("opinion", "[JSON Parse Error]"));
            result.brief   = parsed.value("brief", "");
        } catch (...) {
            result.opinion = "[JSON Parse Error]";
            result.brief   = "";
        }
    } else {
        result.opinion = "[API Error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
        result.brief   = "";
    }
    return result;
}

// --- Phase 7: Eureka Finale ---

std::string LLM::generatePatientMonologue(const std::string& patientName,
                                           const std::string& symptom) {
    std::string apiKey;
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are a patient lying in a hospital room. Write 3-4 first-person sentences "
        "describing your experience right now: confusion, fear, the smell of the sterile room, "
        "the long waiting. Do NOT use any medical terminology. Do NOT name any disease. "
        "Do NOT mention the doctor yet.";

    std::string userPrompt =
        "Your name is " + patientName + ". You are experiencing: " + symptom + ".";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 250},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.85}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            return trimToLastSentence(responseJson["content"][0]["text"].get<std::string>());
        } catch (...) {
            return "[JSON Parse Error]";
        }
    }
    return "[API Error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
}

EurekaRoundResult LLM::generateEurekaDialogue(const std::string& hiddenDiagnosis,
                                               const std::string& patientName,
                                               const std::string& patientComment,
                                               int round,
                                               const std::vector<std::string>& history) {
    std::string apiKey;
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    // All rounds return JSON: {"house_line": "...", "patient_options": ["...", "...", "..."]}
    // Round 3 returns empty patient_options (final reveal, no next round).
    std::string systemPrompt;
    if (round == 0) {
        systemPrompt =
            "You are Dr. House who has just solved a difficult medical mystery. You enter the patient's room.\n"
            "Write ONE cryptic, sarcastic opening line — don't name the diagnosis, but let it subtly colour your words.\n"
            "True diagnosis: " + hiddenDiagnosis + "\n\n"
            "Also generate 3 short patient response options the player can choose (each under 10 words).\n"
            "Range: one confused, one scared/emotional, one deflecting.\n\n"
            "Respond ONLY with valid JSON, no extra text:\n"
            "{\"house_line\": \"...\", \"patient_options\": [\"...\", \"...\", \"...\"]}";
    } else if (round == 1) {
        systemPrompt =
            "You are Dr. House. Round 1 of 3 in a diagnosis reveal scene with " + patientName + ".\n"
            "True diagnosis: " + hiddenDiagnosis + " — do NOT name it yet.\n"
            "Make a cryptic observation about a lifestyle detail, environmental clue, or physical finding.\n"
            "It should seem off-topic but will make sense in hindsight. Dismiss the patient's comment in\n"
            "one clause, then barrel forward. 2-3 sentences. Voice: brilliant, sarcastic, relentless.\n\n"
            "Also generate 3 patient response options for round 2 (under 10 words each).\n"
            "Range: confused, pushing back, asking a specific question.\n\n"
            "Patient said: '" + patientComment + "'\n\n"
            "Respond ONLY with valid JSON:\n"
            "{\"house_line\": \"...\", \"patient_options\": [\"...\", \"...\", \"...\"]}";
    } else if (round == 2) {
        systemPrompt =
            "You are Dr. House. Round 2 of 3. The diagnosis is almost here.\n"
            "True diagnosis: " + hiddenDiagnosis + " — do NOT name it yet.\n"
            "Point at the specific body system, enzyme, or pathological mechanism central to this disease.\n"
            "Make it feel like the last piece clicking — specific, not vague. 2-3 sentences.\n\n"
            "Also generate 3 patient response options for round 3 (under 10 words each).\n"
            "Range: scared but starting to grasp it, asking if it's treatable, practical/direct.\n\n"
            "Patient said: '" + patientComment + "'\n\n"
            "Respond ONLY with valid JSON:\n"
            "{\"house_line\": \"...\", \"patient_options\": [\"...\", \"...\", \"...\"]}";
    } else {
        // round == 3: full reveal
        systemPrompt =
            "You are Dr. House. Round 3 of 3. THIS IS THE FULL REVEAL.\n"
            "Name '" + hiddenDiagnosis + "' explicitly.\n"
            "Explain the single most important diagnostic clue that cracked the case.\n"
            "Be definitive, not cryptic. 2-3 sentences. Voice: certain, direct, still very House.\n\n"
            "Patient said: '" + patientComment + "'\n\n"
            "Respond ONLY with valid JSON:\n"
            "{\"house_line\": \"...\", \"patient_options\": []}";
    }

    // Build message history
    json messagesArray = json::array();
    for (size_t i = 0; i + 1 < history.size(); i += 2) {
        messagesArray.push_back({{"role", "user"},      {"content", history[i]}});
        messagesArray.push_back({{"role", "assistant"}, {"content", history[i + 1]}});
    }
    std::string currentMsg = patientComment.empty() ? "You enter the room." : patientComment;
    messagesArray.push_back({{"role", "user"}, {"content", currentMsg}});

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 350},
        {"system", systemPrompt},
        {"messages", messagesArray},
        {"temperature", 0.9}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");

    EurekaRoundResult result;
    const std::vector<std::string> fallbackOptions = {"What do you mean?", "I'm scared.", "Please continue."};

    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            std::string rawText = responseJson["content"][0]["text"].get<std::string>();

            // Strip potential markdown code-block wrapping
            auto start = rawText.find('{');
            auto end   = rawText.rfind('}');
            std::string jsonStr = (start != std::string::npos && end > start)
                ? rawText.substr(start, end - start + 1) : "{}";

            json parsed = json::parse(jsonStr);
            result.houseLine = parsed.value("house_line", rawText);
            if (parsed.contains("patient_options") && parsed["patient_options"].is_array()) {
                for (const auto& opt : parsed["patient_options"])
                    if (opt.is_string()) result.patientOptions.push_back(opt.get<std::string>());
            }
            if (result.patientOptions.empty() && round < 3)
                result.patientOptions = fallbackOptions;
        } catch (...) {
            result.houseLine = "[Parse Error]";
            if (round < 3) result.patientOptions = fallbackOptions;
        }
    } else {
        result.houseLine = "[API Error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
        if (round < 3) result.patientOptions = fallbackOptions;
    }
    return result;
}
// --- Phase 8: Director's Cut ---

std::string LLM::generateEpisodeScript(const std::string& gameLogSummary,
                                        const std::string& patientName) {
    std::string apiKey;
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are an Emmy Award-winning House M.D. TV writer. "
        "Given a game log, write a dramatic TV script for a single episode. "
        "Include INT./EXT. scene headings, character dialogue, and stage directions. "
        "If the log contains [EUREKA] entries, those are the climax: reproduce the patient's "
        "inner monologue and the House diagnosis dialogue VERBATIM as the final scene, "
        "framed with a perspective-shift note ('WE ARE NOW THE PATIENT'). "
        "For loss outcomes, build to a tragic climax matching the log. "
        "Keep it to roughly two pages. Use only characters and events present in the log.";

    std::string userPrompt =
        "Write the House M.D. episode script for patient " + patientName + ".\n\n" + gameLogSummary;

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 1200},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.85}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(30, 0);
    httplib::Headers headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            json responseJson = json::parse(res->body);
            return responseJson["content"][0]["text"].get<std::string>();
        } catch (...) {
            return "[Script generation failed — JSON parse error]";
        }
    }
    return "[Script generation failed — API error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
}
