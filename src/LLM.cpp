#include "LLM.h"
#include <array>
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

// Returns `count` unique "First Last" names, drawn from intentionally diverse pools
// so patient rosters don't keep regressing to the same 4-5 Anglo names the model favors
// by default. Names are post-applied to each PatientProfile in generatePatientFiles —
// the LLM only generates clinical content. Pools are wide enough that 3 names per
// session almost never repeat across runs.
static std::vector<std::string> pickDiverseNames(int count) {
    static const std::vector<std::string> firstNames = {
        "Sarah",    "Michael",  "Jennifer", "David",   "Emily",   "James",
        "Rachel",   "Daniel",   "Laura",    "Thomas",  "Jessica", "Andrew",
        "Megan",    "Kevin",    "Hannah",   "Brian",   "Olivia",  "Steven",
        "Natalie",  "Ryan",     "Karen",    "Adam",    "Lauren",  "Patrick",
        "Caroline", "Jason",    "Allison",  "Eric",    "Heather", "Paul",
        "Christina","Tim",      "Vanessa",  "Scott",   "Erin",    "Greg",
        "Mark",     "Linda",    "Robert",   "Diana",   "Joel",    "Erica",
        "Stephanie","Maria",    "Carlos",   "Sofia",   "Diego",   "Anjali"
    };
    static const std::vector<std::string> surnames = {
        "Carter",   "Bennett",  "Reed",     "Powell",  "Foster",  "Walsh",
        "Mitchell", "Sullivan", "Hayes",    "Russell", "Bishop",  "Harper",
        "Lawson",   "Park",     "Patel",    "Singh",   "Wong",    "Murphy",
        "Cohen",    "Schwartz", "Brennan",  "Holloway","Newman",  "Caldwell",
        "Vaughn",   "Cole",     "Brooks",   "Lopez",   "Garza",   "Holt",
        "Chambers", "Becker",   "Dawson",   "Pierce",  "Lambert", "Hicks",
        "Goodwin",  "Pratt",    "Kim",      "Nguyen",  "Sanchez", "Fischer"
    };

    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));

    std::vector<size_t> firstIdx(firstNames.size()), surIdx(surnames.size());
    for (size_t i = 0; i < firstNames.size(); ++i) firstIdx[i] = i;
    for (size_t i = 0; i < surnames.size(); ++i)   surIdx[i] = i;
    std::shuffle(firstIdx.begin(), firstIdx.end(), rng);
    std::shuffle(surIdx.begin(), surIdx.end(), rng);

    std::vector<std::string> out;
    out.reserve(static_cast<size_t>(count));
    for (size_t i = 0; i < static_cast<size_t>(count) && i < firstIdx.size() && i < surIdx.size(); ++i)
        out.push_back(firstNames[firstIdx[i]] + " " + surnames[surIdx[i]]);
    return out;
}

// Strips *stage directions*, normalizes whitespace, collapses runs of spaces.
// Used by scenes (Wilson, Cuddy) whose models occasionally leak `*action*` blocks.
static std::string cleanDialogueLine(const std::string& raw) {
    std::string clean;
    bool inStage = false;
    for (char c : raw) {
        if (c == '*') { inStage = !inStage; }
        else if (!inStage) clean += c;
    }
    for (char& c : clean) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    size_t start = clean.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    clean = clean.substr(start);
    std::string collapsed;
    bool lastSp = false;
    for (char c : clean) {
        if (c == ' ') { if (!lastSp) collapsed += c; lastSp = true; }
        else { collapsed += c; lastSp = false; }
    }
    size_t endTrim = collapsed.find_last_not_of(' ');
    if (endTrim != std::string::npos) collapsed = collapsed.substr(0, endTrim + 1);
    return collapsed;
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
        "'narrative' (string), 'brief' (string), 'gloss' (string), "
        "'health_change' (integer, can be negative), 'clarity_change' (integer), 'malpractice_change' (integer). "
        "The 'brief' key: 6 words max summarizing the key clinical finding only (e.g. 'elevated WBC, bilateral infiltrates'). "
        "The 'gloss' key: ONE plain-English sentence, 14 words max, translating the 'brief' for a viewer with NO medical background. "
        "No jargon, no acronyms, no Latin, no drug names, no test names. Describe the DIRECTION the finding points "
        "(infection? inflammation? bleeding? toxin? metabolic?) — not the diagnosis. "
        "Example: brief 'elevated WBC, bilateral infiltrates' -> gloss 'Body's fighting something in both lungs.' "
        "NEVER include the diagnosis name in 'brief' or 'gloss'. "
        "Do NOT wrap the JSON in markdown blocks. Output absolutely NOTHING else.\n"
        "[GAME MASTER SECRET — NOT SHOWN TO PLAYER]: The patient's true diagnosis is: " + hiddenDiagnosis + ". "
        "Use this to judge whether the action is medically appropriate. "
        "For treatments, distinguish between plausibly wrong (reasonable guess, mild harm) and contraindicated (actively harmful). "
        "CRITICAL: NEVER write the diagnosis name in the narrative, brief, or gloss — not even partially. "
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
            outcome.gloss = outJson.value("gloss", "");
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
        "'story' (string — 2-sentence cynical backstory that MUST be self-consistent with the symptom cluster and "
        "the hidden_diagnosis. Sentence 1: the SPECIFIC circumstance under which the patient first realised something "
        "was wrong — must reference at least ONE symptom from the 'symptom' field by name or near-synonym (e.g. if "
        "the symptom is 'joint pain and rash', sentence 1 could be 'Collapsed at her own wedding rehearsal when her "
        "knees gave out and a rash crept up her arms.'). Sentence 2: ONE specific lifestyle / occupation / exposure / "
        "diet / travel / hobby detail that a sharp diagnostician would treat as a soft clue toward the hidden_diagnosis "
        "(without naming it). The story must NOT describe a generic 'found unconscious' — it must lead the reader "
        "from the presentation INTO the case.), "
        "'hidden_diagnosis' (string — the ONE real disease name, a genuine specific obscure condition within the assigned category), "
        "'disease_severity' (integer — 1 mild, 2 moderate, 3 critical). "
        "Derive the symptom FROM the hidden_diagnosis — medically consistent but not diagnostic on their own. "
        "Derive the story FROM the symptom AND the hidden_diagnosis — the backstory must echo the same case. "
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

            // Override LLM-chosen names with C++-rolled diverse names. The LLM gravitates
            // to the same handful of Anglo names ("Marcus ..." over and over); a wide pool
            // shuffled per session removes the repetition without complicating the prompt.
            auto names = pickDiverseNames(static_cast<int>(profiles.size()));
            for (size_t i = 0; i < profiles.size() && i < names.size(); ++i)
                profiles[i].name = names[i];
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
                                        int clarity,
                                        const std::string& playerCategory) {
    WilsonResult result;

    std::string apiKey;
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are writing a short dialogue scene between Dr. Gregory House and Dr. James Wilson "
        "(oncologist, House's best friend). House has just limped into Wilson's office about a patient. "
        "House SPEAKS FIRST. The scene then strictly alternates Wilson, House, Wilson, House, ... — "
        "each speaker getting 2 to 3 turns. The PREAMBLE is 3 or 5 strictly-alternating lines starting "
        "with House (so it ends on House when length is 3 or 5, or on Wilson when length is 4 — all are "
        "valid; pick the rhythm that feels right). Then Wilson delivers the final 'insight' line on top "
        "of the preamble. Total scene: 4 to 6 spoken turns.\n"
        "Length: most lines ≤25 words. ONE or TWO lines in the scene may run to ~40 words for a banter "
        "beat. Lines must have continuity (each responds to the one before). Show personality through "
        "the RHYTHM and the JABS — not by restating clinical info.\n"
        "House's voice: sardonic, self-interested, mining Wilson for clinical leverage AND for personal "
        "needling. Wilson's voice: warm but exasperated, tries to humanize the patient, sometimes "
        "returns fire.\n"
        "REQUIRED BEAT — at least ONE House line must land a punchy personal jab at Wilson. "
        "VARY the target across sessions — don't always reach for the same joke. Show-accurate options "
        "(don't copy verbatim, paraphrase freely):\n"
        "  • mocking Wilson's serial marriages, his latest divorce, or his alimony payments\n"
        "  • calling out Wilson's saint complex / moral high ground / fixer impulse\n"
        "  • oncology guilt — his patients always dying, the survivor's-guilt of being the kind one\n"
        "  • House stealing Wilson's lunch / sandwich / Reuben from the cafeteria again\n"
        "  • Wilson's self-help-book energy, his apartment decor, his cardigan, his earnestness\n"
        "  • Wilson's compulsion to fix everyone else's relationship while his own collapse\n"
        "  • Wilson's hair / face / cancer-doctor sad eyes\n"
        "Wilson may return fire when natural — a dry deadpan, a reluctant grin, a needling callback to "
        "House's pill habit, his cane, his refusal to wear a coat, his piano at 2 a.m., his Cuddy "
        "obsession, anything show-accurate.\n"
        "After the preamble, Wilson delivers one final 'insight' line: he names ONE concrete "
        "lifestyle/context factor (occupation, diet, travel, family stress, hobbies, environment, "
        "toxic exposure, recent changes, medications) SUBTLY relevant to the hidden diagnosis '" +
        hiddenDiagnosis + "' WITHOUT naming the disease, a synonym, or any medical label for it.\n";

    // Pre-commit category mechanic: if the player chose a hypothesis axis before walking
    // in, Wilson's tone shifts to reflect alignment, and we ask the model to return a
    // verdict the caller can score against (+8% on match, +2% on miss).
    if (!playerCategory.empty()) {
        systemPrompt +=
            "BEFORE consulting Wilson, House has pre-committed to a hypothesis CATEGORY: '" +
            playerCategory + "' (one of: Lab Test / Treatment / Risky Procedure / Supportive Care). "
            "Decide whether that category is the RIGHT primary axis for the hidden diagnosis '" +
            hiddenDiagnosis + "':\n"
            "  • Lab Test fits autoimmune, endocrine, metabolic, genetic, hematologic cases where "
            "the diagnosis hinges on workup results.\n"
            "  • Treatment fits infectious (bacterial/viral/fungal/parasitic) or pharmacologically-"
            "reversible cases where the right drug resolves it.\n"
            "  • Risky Procedure fits masses, tumors, structural lesions, or cases requiring "
            "biopsy / surgery / interventional confirmation.\n"
            "  • Supportive Care fits toxicology, withdrawal, intoxication, or self-limiting cases "
            "where stabilization + observation is the answer.\n"
            "If the player's category is right (MATCHED), Wilson's tone is supportive-but-pointed: "
            "he validates the axis through the lifestyle insight, sharpening their direction. "
            "If wrong (MISS), Wilson is gently corrective without naming the disease: the insight "
            "should nudge them toward the correct axis, and the dialogue can include a soft pushback "
            "from Wilson on their current plan. NEVER reveal the diagnosis itself either way.\n"
            "Add a fourth JSON key: 'matched' (boolean). True if the player's category is the right "
            "axis; false otherwise.\n";
    }

    systemPrompt +=
        "No stage directions, no asterisks — only spoken words.\n"
        "Return a JSON object with the keys: "
        "'preamble' (array of 3 to 5 strings, each prefixed exactly 'House: ' or 'Wilson: ', "
        "first entry MUST start with 'House: ', strictly alternating from there), "
        "'insight' (string starting with 'Wilson: ' — the final payoff line), "
        "'brief' (string, 6 words max, summarizing what Wilson probed, e.g. 'asked about dust and family stress')";
    if (!playerCategory.empty()) {
        systemPrompt += ", 'matched' (boolean as described above)";
    }
    systemPrompt +=
        ". Do NOT wrap in markdown. Output only the raw JSON.";

    std::string userPrompt =
        "Patient context for the scene: " + patientName + ", presenting with " + symptom +
        ". Diagnostic clarity " + std::to_string(clarity) + "%. ";
    if (!playerCategory.empty()) {
        userPrompt += "House walked in already committed to category: " + playerCategory + ". ";
    }
    userPrompt += "Write the scene now.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 500},
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

    auto stubFallback = [&] {
        result.preamble = {
            "House: I'm stuck. Patient won't tell me anything useful.",
            "Wilson: You barged in just to complain? Or is this about the third marriage I'm not currently in?",
            "House: Cute. Tell me what people lie about when they think nobody's watching."
        };
        result.insight = "Wilson: Ask about his job — people hide where they work when it's where they're getting sick.";
        result.brief = "asked about occupation";
        result.matched = false; // conservative when the network call fails
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");
    if (!res || res->status != 200) {
        stubFallback();
        return result;
    }

    try {
        json responseJson = json::parse(res->body);
        std::string text = responseJson["content"][0]["text"].get<std::string>();

        size_t firstBrace = text.find('{');
        size_t lastBrace  = text.rfind('}');
        if (firstBrace != std::string::npos && lastBrace != std::string::npos)
            text = text.substr(firstBrace, lastBrace - firstBrace + 1);

        json parsed = json::parse(text);

        std::vector<std::string> preamble;
        if (parsed.contains("preamble") && parsed["preamble"].is_array()) {
            for (const auto& item : parsed["preamble"]) {
                if (!item.is_string()) continue;
                std::string line = cleanDialogueLine(item.get<std::string>());
                if (!line.empty()) preamble.push_back(line);
            }
        }
        std::string insight = cleanDialogueLine(parsed.value("insight", ""));
        std::string brief   = parsed.value("brief", "");
        bool matched = false;
        if (parsed.contains("matched") && parsed["matched"].is_boolean()) {
            matched = parsed["matched"].get<bool>();
        }

        // Loosened: 3-5 preamble lines (was 2-4), only require start=House + insight=Wilson.
        // The last preamble line can be either speaker — both rhythms work into Wilson's insight.
        bool ok = preamble.size() >= 3 && preamble.size() <= 5
               && preamble.front().rfind("House:", 0) == 0
               && insight.rfind("Wilson:", 0) == 0;
        if (!ok) {
            stubFallback();
            return result;
        }

        result.preamble = std::move(preamble);
        result.insight  = std::move(insight);
        result.brief    = std::move(brief);
        result.matched  = matched;
    } catch (...) {
        stubFallback();
    }
    return result;
}

CuddyDealResult LLM::generateCuddyDeal(int currentBudget,
                                       int requestAmount,
                                       int previousGrants,
                                       int malpractice,
                                       const std::string& patientName,
                                       const std::string& patientSymptom) {
    CuddyDealResult result;
    result.approved = (previousGrants < 2);

    std::string apiKey;
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string toneNote;
    if (previousGrants == 0) {
        toneNote = "Cuddy is wary but ultimately relents. She pushes back hard on cost, "
                   "questions the urgency, but agrees in the verdict.";
    } else if (previousGrants == 1) {
        toneNote = "Cuddy is ANGRY — House already used one emergency allocation. She invokes "
                   "the board, the budget overrun, the malpractice exposure. She still grants it "
                   "in the verdict, but tightly and with explicit warning that this is the last time.";
    } else {
        toneNote = "Cuddy REFUSES. House has already had two emergency allocations. Verdict denies "
                   "the funds outright. She tells him to walk out of her office.";
    }

    std::string systemPrompt =
        "You are writing a short negotiation scene between Dr. Gregory House and Dr. Lisa Cuddy "
        "(Dean of Medicine, House's boss). House has dropped into the chair across from her, "
        "asking for $" + std::to_string(requestAmount) + " in emergency funding for his current patient. "
        "House SPEAKS FIRST. The scene then strictly alternates Cuddy, House, Cuddy, House, ... — "
        "each speaker getting 2 to 3 turns. The PREAMBLE is 3 or 5 strictly-alternating lines starting "
        "with House (so it ends on House when length is 3 or 5, or on Cuddy when length is 4 — all are "
        "valid; pick the rhythm that feels right). Then Cuddy delivers the final 'verdict' line on top.\n"
        "Length: most lines ≤25 words. ONE or TWO lines in the scene may run to ~40 words for a banter "
        "beat. Don't waste the bit on the verdict — verdict stays tight. Lines must have continuity "
        "(each responds to the one before).\n"
        "House's voice: manipulative, urgent, frames the patient's life as the only thing that matters — "
        "BUT also takes shots at Cuddy personally for sport. Cuddy's voice: institutional pressure, cost "
        "concerns, board politics, malpractice exposure — AND she returns fire when she wants.\n"
        + toneNote + "\n"
        "REQUIRED BEAT — at least ONE House line must land a punchy personal jab at Cuddy. "
        "VARY the target across sessions — don't always reach for the same joke. Show-accurate options "
        "(don't copy verbatim, paraphrase freely):\n"
        "  • commenting on her blouse, cleavage, neckline, or outfit-of-the-day\n"
        "  • needling her dating life or pointed absence of one\n"
        "  • calling her 'Lisa' or 'Cuddles' for the power-play sting\n"
        "  • bringing up a specific past disaster — the MRI he wrecked, a lawsuit she handled, "
        "the time he stole from pharmacy, a fellowship he sabotaged\n"
        "  • mocking her workaholism / late-night office light\n"
        "  • the ticking clock on her wanting a kid, or her recent adoption attempts\n"
        "  • the board breathing down her neck about him specifically\n"
        "  • her micromanaging style, her clipboard, her endless tolerance for his nonsense\n"
        "Cuddy may FIRE BACK when natural — about his pills, his cane, his refusal to wear a coat, "
        "his piano at 2 a.m., his bills on her desk, his Wilson-shaped shadow, his clinic-hours dodging, "
        "anything show-accurate. The chemistry — flirty-hostile, twenty-years-of-grudges — carries the scene.\n"
        "Earn the banter beat AND land the funds verdict. After the preamble, Cuddy delivers the verdict — "
        "her final word.\n"
        "No stage directions, no asterisks — only spoken words.\n"
        "Return a JSON object with exactly two keys: "
        "'preamble' (array of 3 to 5 strings, each prefixed exactly 'House: ' or 'Cuddy: ', "
        "first entry MUST start with 'House: ', strictly alternating from there), "
        "'verdict' (string starting with 'Cuddy: ' — the final ruling). "
        "Do NOT wrap in markdown. Output only the raw JSON.";

    std::string userPrompt =
        "Scene context: Patient is " + patientName + ", presenting with " + patientSymptom + ". "
        "Current budget left: $" + std::to_string(currentBudget) + ". "
        "Malpractice risk: " + std::to_string(malpractice) + "%. "
        "Prior emergency allocations granted this case: " + std::to_string(previousGrants) + ". "
        "Write the scene now.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 500},
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

    auto stubFallback = [&] {
        if (previousGrants < 2) {
            result.preamble = {
                "House: Five thousand, Lisa. Patient's crashing and your accountant can wait.",
                "Cuddy: It's always 'patient's crashing'. The blouse comment was free though, thanks.",
                "House: Consider it professional feedback. The neckline is doing real work today."
            };
            result.verdict = "Cuddy: Fine. Five thousand. Don't make me regret this, House.";
        } else {
            result.preamble = {
                "House: One more allocation. Last one. I promise. Same way I promised last time.",
                "Cuddy: And the time before. And the MRI you wrecked. The promise stack is impressive.",
                "House: Fine. Don't fund it. I'll just sign the death certificate myself, save you the paperwork."
            };
            result.verdict = "Cuddy: No. Get out of my office, House.";
        }
    };

    auto res = cli.Post("/v1/messages", headers, requestBody.dump(), "application/json");
    if (!res || res->status != 200) {
        stubFallback();
        return result;
    }

    try {
        json responseJson = json::parse(res->body);
        std::string text = responseJson["content"][0]["text"].get<std::string>();

        size_t firstBrace = text.find('{');
        size_t lastBrace  = text.rfind('}');
        if (firstBrace != std::string::npos && lastBrace != std::string::npos)
            text = text.substr(firstBrace, lastBrace - firstBrace + 1);

        json parsed = json::parse(text);

        std::vector<std::string> preamble;
        if (parsed.contains("preamble") && parsed["preamble"].is_array()) {
            for (const auto& item : parsed["preamble"]) {
                if (!item.is_string()) continue;
                std::string line = cleanDialogueLine(item.get<std::string>());
                if (!line.empty()) preamble.push_back(line);
            }
        }
        std::string verdict = cleanDialogueLine(parsed.value("verdict", ""));

        // Loosened: 3-5 preamble lines (was 2-4), only require start=House + verdict=Cuddy.
        // Last preamble line can be either speaker — both rhythms flow into Cuddy's verdict.
        bool ok = preamble.size() >= 3 && preamble.size() <= 5
               && preamble.front().rfind("House:", 0) == 0
               && verdict.rfind("Cuddy:", 0) == 0;
        if (!ok) {
            stubFallback();
            return result;
        }

        result.preamble = std::move(preamble);
        result.verdict  = std::move(verdict);
    } catch (...) {
        stubFallback();
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
        "You are Dr. Gregory House, M.D., at his whiteboard. The player is a med student / a "
        "layperson trying to actually LEARN how diagnostic medicine works — they need real answers, "
        "not riffs. Teach FIRST, voice SECOND.\n"
        "TEACH FIRST: answer their question like a lecturer who happens to be House. If they ask "
        "'how do we test for brain disorders?' — name the actual tests (EEG measures electrical "
        "activity, MRI shows structure, lumbar puncture samples CSF for infection/inflammation, "
        "neuropsych testing maps cognitive deficits...), explain what each measures, what findings "
        "would mean. If they ask 'what is sarcoidosis?' — define it cleanly. If they ask 'why do "
        "we order a CBC?' — explain what blood counts reveal. Real mechanisms, real test names, "
        "real drug classes, real body systems. Be specific. Be useful.\n"
        "VOICE SECOND: after the teaching beat, close with ONE House-flavored aside — a sneer at "
        "an obvious answer, a snide observation about the question, a half-formed accusation. ONE "
        "line of voice. Not a monologue. The student came to learn.\n"
        "Hard rules:\n"
        "1. NEVER name '" + hiddenDiagnosis + "' — not directly, not as a synonym, not by "
        "unique description. You MAY name wrong differentials freely.\n"
        "2. Anchor on THEIR question. Quote or paraphrase a keyword from what they typed. Don't "
        "drift into a generic House monologue that ignores what they actually asked.\n"
        "3. Specific tests, drugs, scans, procedures, mechanisms are ALL allowed — this is "
        "the teaching tool. Just don't uniquely identify the hidden answer.\n"
        "Length: 100-200 words. Plain prose, no bullet points, no headers, no stage directions, "
        "no parentheticals.";

    std::string userPrompt =
        "The player just typed at the whiteboard: \"" + question + "\"\n"
        "Patient's known symptom: " + symptom + ".\n"
        "Respond as House — START from their question and engage with what they actually asked.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 350},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.7}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(15, 0);
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
                                            int clarity,
                                            const std::vector<std::string>& priorTranscript,
                                            const std::string& houseNudge) {
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
        "ORIENTATION RULE: The way you argue must make a DIRECTION readable — a body system, a mechanism, "
        "an exposure class, or a kind of workup. Carry that direction THROUGH your reasoning, in character. "
        "Do NOT tack on a summary sentence like 'this is an imaging question' or 'I'd run an autoimmune workup'. "
        "Do NOT name any specific test, drug, scan, or procedure. Do NOT name the disease. "
        "End your reply however feels natural for the character — a question, a flat assertion, a shrug, "
        "a confident wrong guess. Don't force a clean conclusion. The show's team rarely speaks in summaries.\n"
        "Length: 2 to 4 sentences. Target ≤25 words per sentence, but ONE sentence may run to ~40 words "
        "if a character beat needs the room. Total response 60-120 words. "
        "Stay punchy where you can, breathe where it earns it. Don't make every sentence a paragraph.\n"
        "One of your sentences should be a CHARACTER BEAT — a sneer at another team member, an aside, "
        "a callback to a prior round if there was one. Show personality. Don't be a checklist.\n"
        "Return a JSON object with exactly two keys: "
        "'opinion' (string — your spoken reply, in voice, 2-4 sentences) and "
        "'brief' (string — 8 words max capturing the thrust of YOUR argument in your voice, "
        "e.g. 'travel, common things first', 'maybe she's hiding something', 'rare autoimmune, biopsy now'). "
        "Do NOT wrap in markdown. Output only the raw JSON.";

    // Round 1: clean ask. Rounds 2/3: include transcript + nudge so agents react in character.
    std::string userPrompt =
        "Setting: whiteboard room, team diagnostic meeting. The patient is NOT present. "
        "Patient's known symptom: " + symptom + ".\n";
    if (!priorTranscript.empty()) {
        userPrompt += "Prior round(s) of this brainstorm:\n";
        for (const auto& line : priorTranscript) userPrompt += "  " + line + "\n";
        if (!houseNudge.empty())
            userPrompt += "House just nudged the team, saying: \"" + houseNudge + "\".\n"
                          "React to his nudge — push back, riff on it, or pivot — in your voice, "
                          "in your direction of thinking. Callbacks to prior rounds welcome.\n";
        else
            userPrompt += "Continue the brainstorm in your voice. Callbacks to prior rounds welcome.\n";
    } else {
        userPrompt += "Dr. House just asked: 'What are you idiots thinking?'\n";
    }
    userPrompt += "Respond as " + agentName + " speaking directly to House. "
                  "Be fast and opinionated — no preamble, no hedging. Stay in character.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 280},
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

BrainstormSteers LLM::generateBrainstormSteers(const std::string& symptom,
                                                const std::string& hiddenDiagnosis,
                                                const std::vector<std::string>& priorTranscript) {
    BrainstormSteers result;

    // Canned fallback — always usable when the API is unreachable or returns junk.
    // House's voice; subtle; one option per action type, fixed order.
    auto canned = [] {
        return BrainstormSteers{{
            {"You actually pull blood on her, or did everybody just stand around?", "Lab Test"},
            {"Stop testing. Treat something. Anything.", "Treatment"},
            {"She's circling the drain. Keep her stable while we argue.", "Supportive Care"},
            {"Time to do something stupid. Stupid wins.", "Risky Procedure"},
        }};
    };

    std::string apiKey;
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are writing four SHORT House M.D. interjections — Dr. Gregory House nudging his diagnostic team "
        "toward a CATEGORY of next move. Each line is in House's voice: sardonic, impatient, subtle, "
        "never literal. Do NOT say 'order more lab tests' or 'consider treatment options'. Instead show "
        "House pushing his team through tone — a question, a sneer, a half-mocking observation.\n"
        "Length: each houseLine is ≤16 words. Punchy. House never explains himself.\n"
        "[GAME MASTER SECRET — NOT SHOWN TO PLAYER]: The patient's true diagnosis is: " + hiddenDiagnosis + ". "
        "Let this faintly tilt your wording, but NEVER name the disease, a synonym, or any unique identifier. "
        "Do NOT name a specific test, drug, scan, or procedure.\n"
        "Style examples (do NOT copy verbatim):\n"
        "  Lab Test:        'You actually run blood on her, or just stare?'\n"
        "  Treatment:       'Stop testing. Try something. Anything.'\n"
        "  Supportive Care: 'She's circling the drain. Keep her warm while we argue.'\n"
        "  Risky Procedure: 'Time to do something stupid. Stupid wins.'\n"
        "Return EXACTLY this JSON shape, four entries in this fixed order:\n"
        "{\"options\": ["
        "{\"houseLine\": \"...\", \"actionType\": \"Lab Test\"}, "
        "{\"houseLine\": \"...\", \"actionType\": \"Treatment\"}, "
        "{\"houseLine\": \"...\", \"actionType\": \"Supportive Care\"}, "
        "{\"houseLine\": \"...\", \"actionType\": \"Risky Procedure\"}"
        "]}\n"
        "Do NOT wrap in markdown. Output only the raw JSON.";

    std::string userPrompt =
        "Patient's known symptom: " + symptom + ".\n"
        "Prior brainstorm transcript:\n";
    for (const auto& line : priorTranscript) userPrompt += "  " + line + "\n";
    userPrompt += "Write House's four nudges. Make them respond to what was just said — "
                  "callbacks to the team's lines welcome.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 280},
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
    if (!res || res->status != 200) return canned();

    try {
        json responseJson = json::parse(res->body);
        std::string text = responseJson["content"][0]["text"].get<std::string>();

        size_t firstBrace = text.find('{');
        size_t lastBrace  = text.rfind('}');
        if (firstBrace != std::string::npos && lastBrace != std::string::npos)
            text = text.substr(firstBrace, lastBrace - firstBrace + 1);

        json parsed = json::parse(text);
        if (!parsed.contains("options") || !parsed["options"].is_array()) return canned();

        const std::array<const char*, 4> expectedTypes = {
            "Lab Test", "Treatment", "Supportive Care", "Risky Procedure"
        };
        std::vector<BrainstormSteer> opts;
        opts.reserve(4);
        for (const auto& item : parsed["options"]) {
            if (!item.is_object()) continue;
            BrainstormSteer s;
            s.houseLine  = cleanDialogueLine(item.value("houseLine", ""));
            s.actionType = item.value("actionType", "");
            if (!s.houseLine.empty()) opts.push_back(std::move(s));
        }
        // Hard validation: exactly 4 entries, types match the expected fixed order.
        if (opts.size() != 4) return canned();
        for (size_t i = 0; i < 4; ++i) {
            if (opts[i].actionType != expectedTypes[i]) return canned();
        }
        result.options = std::move(opts);
    } catch (...) {
        return canned();
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
        "inner monologue and the House diagnosis dialogue VERBATIM as the final scene. "
        "Do NOT add a 'WE ARE NOW THE PATIENT' header, a perspective-shift framing, or any "
        "meta narration around the climax — let the monologue stand on its own, then cut to "
        "House. For loss outcomes, build to a tragic climax matching the log. "
        "Aim for roughly two pages and FINISH the script cleanly — do not cut off mid-scene. "
        "Use only characters and events present in the log.";

    std::string userPrompt =
        "Write the House M.D. episode script for patient " + patientName + ".\n\n" + gameLogSummary;

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 1800},
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

std::string LLM::generateForemanPostMortem(const std::string& hiddenDiagnosis,
                                           const std::string& patientSymptom,
                                           const std::vector<std::pair<std::string, std::string>>& allActions) {
    std::string apiKey;
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    // Bucket the action registry by type so the model sees a clean menu.
    // This is the SAME registry the player had access to in the game.
    std::string labs, treatments, supportive, risky;
    for (const auto& [name, type] : allActions) {
        if      (type == "Lab Test")        labs       += "  - " + name + "\n";
        else if (type == "Treatment")       treatments += "  - " + name + "\n";
        else if (type == "Supportive Care") supportive += "  - " + name + "\n";
        else if (type == "Risky Procedure") risky      += "  - " + name + "\n";
    }

    std::string systemPrompt =
        "You are Dr. Eric Foreman, M.D. — neurologist on Dr. House's diagnostic team. "
        "The case is over. You are giving a by-the-book post-mortem to the player "
        "(a med student / layperson). Voice: methodical, slightly smug, occasionally "
        "irritated that House made it harder than it had to be. 'Common things common.' "
        "You ARE the team's institutional memory.\n\n"
        "Your job: explain how this disease SHOULD have been worked up using ONLY the "
        "tests and treatments available in this game. The player needs to learn what to "
        "do next time they see this kind of presentation.\n\n"
        "HARD CONSTRAINT — READ CAREFULLY:\n"
        "Every test, treatment, or care action you name MUST appear VERBATIM in the lists "
        "below. Do not invent tests. Do not name real-world tests that aren't here (no "
        "'PET scan', no 'Western blot' unless they're listed). If something you would "
        "normally recommend isn't in the list, name the CLOSEST listed alternative and "
        "say so.\n\n"
        "AVAILABLE LAB TESTS:\n" + labs +
        "\nAVAILABLE TREATMENTS:\n" + treatments +
        "\nAVAILABLE SUPPORTIVE CARE:\n" + supportive +
        "\nAVAILABLE RISKY PROCEDURES:\n" + risky +
        "\nOUTPUT FORMAT — plain prose, no markdown, no bullet points, use these section "
        "headers exactly:\n"
        "CONFIRMATORY WORKUP: 2-4 Lab Tests from the list above, with one sentence each "
        "on WHAT a positive finding would have shown for this disease.\n"
        "RIGHT TREATMENT: 1-2 Treatments from the list above, with the mechanism of action "
        "in plain language (why this drug class helps THIS disease).\n"
        "SUPPORTIVE CARE: 1-2 Supportive Care items from the list above, with one line on "
        "what symptom they would have stabilized while the workup ran.\n"
        "RISKY PROCEDURES: only mention if this disease genuinely warrants one. Otherwise "
        "write one line: 'Not warranted. House would have ordered one anyway.'\n"
        "Close with ONE Foreman sentence — a dry observation, a callback to House's "
        "stubbornness, or a 'told you so' aside. ONE sentence. Not a monologue.\n"
        "Length total: 250-400 words. No stage directions, no parentheticals, no asterisks.";

    std::string userPrompt =
        "The case was: " + hiddenDiagnosis + ".\n"
        "Presenting symptoms: " + patientSymptom + ".\n"
        "Walk the player through how it SHOULD have been worked up, using only the "
        "tests and treatments listed in the system prompt. Stay in Foreman's voice.";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 700},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.6}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(20, 0);
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
            return "[Post-mortem generation failed — JSON parse error]";
        }
    }
    return "[Post-mortem generation failed — API error: " + (res ? std::to_string(res->status) : "Connection failed") + "]";
}
