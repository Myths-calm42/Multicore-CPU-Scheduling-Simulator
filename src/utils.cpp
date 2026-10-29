#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include "utils.h"

std::vector<Process> ReadFromUser() {
    int n = 0;
    std::cout << "Enter number of processes: ";
    if (!(std::cin >> n) || n < 0) {
        std::cerr << "Invalid process count.\n";
        return {};
    }

    std::vector<Process> processes;
    processes.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        Process p;
        p.pid = i + 1;
        std::cout << "P" << p.pid << " arrival burst priority preferredCore(-1=any) pinnedCore(-1=no pin): ";
        if (!(std::cin >> p.arrivalTime >> p.burstTime >> p.priority >> p.preferredCore >> p.pinnedCore)) {
            std::cerr << "Invalid process input.\n";
            return {};
        }
        processes.push_back(p);
    }
    return processes;
}

std::vector<Process> ReadFromFile(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin) {
        std::cerr << "Could not open input file: " << filename << '\n';
        return {};
    }

    std::vector<Process> processes;
    std::string line;
    int lineNumber = 0;
    while (std::getline(fin, line)) {
        ++lineNumber;
        std::istringstream row(line);
        std::string firstToken;
        if (!(row >> firstToken)) continue; // blank or whitespace-only line
        if (!firstToken.empty() && firstToken[0] == '#') continue;
        std::istringstream values(line);
        Process p;
        if (!(values >> p.pid >> p.arrivalTime >> p.burstTime >> p.priority)) {
            std::cerr << "Malformed input at line " << lineNumber
                      << ". Expected: PID Arrival Burst Priority [PreferredCore] [PinnedCore]\n";
            return {};
        }
        if (!(values >> p.preferredCore)) {
            values.clear();
            p.preferredCore = -1;
        } else if (!(values >> p.pinnedCore)) {
            values.clear();
            p.pinnedCore = -1;
        }
        std::string extra;
        if (values >> extra) {
            std::cerr << "Too many columns at line " << lineNumber << ".\n";
            return {};
        }
        processes.push_back(p);
    }
    return processes;
}

bool ValidateProcesses(const std::vector<Process>& processes, std::string& error) {
    std::unordered_set<int> seen;
    for (const auto& p : processes) {
        if (p.pid < 0) {
            error = "Process IDs must be non-negative.";
            return false;
        }
        if (!seen.insert(p.pid).second) {
            error = "Duplicate process ID: " + std::to_string(p.pid);
            return false;
        }
        if (p.arrivalTime < 0) {
            error = "Arrival times must be non-negative.";
            return false;
        }
        if (p.burstTime <= 0) {
            error = "Burst times must be positive.";
            return false;
        }
        if (p.preferredCore < -1 || p.pinnedCore < -1) {
            error = "Preferred and pinned core values must be -1 or non-negative.";
            return false;
        }
        if (p.pinnedCore >= 0 && p.preferredCore >= 0 && p.pinnedCore != p.preferredCore) {
            error = "A process cannot prefer a different core from its pinned core.";
            return false;
        }
    }
    return true;
}
