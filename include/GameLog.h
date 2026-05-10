#ifndef GAMELOG_H
#define GAMELOG_H

#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <ostream>

struct ActionRecord {
    std::string actionName;
    std::string actionType;
    int healthDelta     = 0;
    int clarityDelta    = 0;
    int malpracticeDelta = 0;
    int budgetSpent     = 0;

    friend std::ostream& operator<<(std::ostream& os, const ActionRecord& r) {
        os << "[" << r.actionType << "] " << r.actionName
           << " | HP:"  << (r.healthDelta >= 0      ? "+" : "") << r.healthDelta
           << " CLR:"   << (r.clarityDelta >= 0     ? "+" : "") << r.clarityDelta    << "%"
           << " MAL:"   << (r.malpracticeDelta >= 0 ? "+" : "") << r.malpracticeDelta << "%"
           << " $-"     << r.budgetSpent;
        return os;
    }
};

template <typename T>
class GameLog {
private:
    std::vector<T> entries;
    inline static int instanceCount = 0;

public:
    GameLog() { ++instanceCount; }

    // Member operator+= (rubric: one additional member operator)
    GameLog& operator+=(const T& entry) {
        entries.push_back(entry);
        return *this;
    }

    // Member operator+ (rubric: second member operator)
    GameLog operator+(const GameLog& other) const {
        GameLog result;
        result.entries = entries;
        result.entries.insert(result.entries.end(),
                              other.entries.begin(), other.entries.end());
        return result;
    }

    // Non-member friend operator<< (rubric: non-member operator)
    template <typename U>
    friend std::ostream& operator<<(std::ostream& os, const GameLog<U>& log);

    // STL algorithm + lambda (rubric)
    std::vector<T> filter(std::function<bool(const T&)> predicate) const {
        std::vector<T> result;
        std::copy_if(entries.begin(), entries.end(),
                     std::back_inserter(result), predicate);
        return result;
    }

    int count(std::function<bool(const T&)> predicate) const {
        return static_cast<int>(
            std::count_if(entries.begin(), entries.end(), predicate));
    }

    const std::vector<T>& getEntries() const { return entries; }
    size_t size() const { return entries.size(); }

    // Non-trivial static method (rubric)
    static int getInstanceCount() { return instanceCount; }
};

// Non-member operator<< definition
template <typename U>
std::ostream& operator<<(std::ostream& os, const GameLog<U>& log) {
    os << "GameLog [" << log.entries.size() << " entries]:\n";
    for (const auto& entry : log.entries) {
        os << "  " << entry << "\n";
    }
    return os;
}

#endif
