#include "LLM.h"
#include <iostream>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "external/httplib.h"
#include "external/json.hpp"

using json = nlohmann::json;

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

DialogueResponse LLM::generateDialogue(const std::string& character, const std::vector<std::string>& history, const std::string& chosenIntent) {
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

            response.houseLine = dialogueJson.value("house", "[JSON Missing 'house']");
            response.characterReply = dialogueJson.value("reply", "[JSON Missing 'reply']");
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
            backstory.story = storyJson.value("story", "[JSON Parse Error]");
        } catch (...) {
            backstory.story = "[Eroare la parsarea povestii pacientului]";
        }
    } else {
        backstory.story = "[API Error - Nu am putut genera povestea]";
    }

    return backstory;
}

MedicalOutcome LLM::evaluateMedicalAction(const std::string& actionType, const std::string& actionName, int currentHealth, int currentClarity, const std::string& symptom) {
    MedicalOutcome outcome;

    std::string apiKey = "";
    std::ifstream secretFile("secrets.json");
    if (secretFile.is_open()) {
        json secrets = json::parse(secretFile);
        apiKey = secrets.value("ANTHROPIC_API_KEY", "");
    }

    std::string systemPrompt =
        "You are a Game Master for a medical simulator. You MUST return strictly a JSON object with EXACTLY these keys: "
        "'narrative' (string), 'health_change' (integer, can be negative), 'clarity_change' (integer), 'malpractice_change' (integer). "
        "Do NOT wrap the JSON in markdown blocks. Output absolutely NOTHING else.";

    std::string userPrompt =
        "Patient symptom: " + symptom + ". Health: " + std::to_string(currentHealth) + "/100. Diagnostic Clarity: " + std::to_string(currentClarity) + "%.\n"
        "Dr. House orders a " + actionType + ": " + actionName + ".\n"
        "Determine what happens. If it's a Lab Test, clarity goes up but health might drop slightly from the procedure. "
        "If it's a Treatment and clarity is low, it might be the WRONG treatment causing a massive health drop and high malpractice risk. "
        "Write a 2-sentence cynical narrative of the result.";

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

            outcome.narrative = outJson.value("narrative", "[Error parsing narrative]");
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

    std::string systemPrompt =
        "You are Dr. House's diagnostic department coordinator. You MUST return strictly a JSON ARRAY of objects. "
        "Do NOT wrap the JSON in markdown blocks (no ```json). Output ONLY the raw array [ { ... }, { ... } ].";

    std::string userPrompt =
        "Generate " + std::to_string(count) + " unique, bizarre medical cases for Dr. House. "
        "Each object in the array MUST have EXACTLY these keys: "
        "'name' (string), 'health' (integer between 30 and 80), 'symptom' (string, a weird symptom), "
        "'story' (string, a 2-sentence cynical backstory).";

    json requestBody = {
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 512},
        {"system", systemPrompt},
        {"messages", {{{"role", "user"}, {"content", userPrompt}}}},
        {"temperature", 0.8} // Putem putin mai mare ca sa fie mai creativ!
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
                profiles.push_back(p);
            }
        } catch (...) {
            // Fallback in caz de eroare grava
            profiles.push_back({"Error Doe", 50, "API Parsing Failed", "The lab lost the results."});
        }
    } else {
        profiles.push_back({"Connection Error", 10, "No Wi-Fi", "Cuddy forgot to pay the internet bill."});
    }

    return profiles;
}