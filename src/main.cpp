#include <iostream>
#include <string>
#include <vector>
#include "scheduler.h"
#include "utils.h"

int main() {
    int choice = 0, cores = 0, quantum = 1;
    std::cout << "1. Input from user\n2. Input from file (input.txt)\nChoice: ";
    if (!(std::cin >> choice) || (choice != 1 && choice != 2)) {
        std::cerr << "Choose 1 or 2.\n";
        return 1;
    }

    const std::vector<Process> processes =
        (choice == 1) ? ReadFromUser() : ReadFromFile("input.txt");

    std::string error;
    if (!ValidateProcesses(processes, error)) {
        std::cerr << "Input error: " << error << '\n';
        return 1;
    }
    if (processes.empty()) {
        std::cerr << "No processes were provided.\n";
        return 1;
    }

    std::cout << "Enter number of CPU cores: ";
    if (!(std::cin >> cores) || cores <= 0) {
        std::cerr << "Core count must be a positive integer.\n";
        return 1;
    }
    std::cout << "Enter Round Robin time quantum: ";
    if (!(std::cin >> quantum) || quantum <= 0) {
        std::cerr << "Time quantum must be a positive integer.\n";
        return 1;
    }

    SchedulingOptions options;
    int balanceChoice = 0;
    std::cout << "Enable load balancing? (0=no, 1=yes): ";
    if (!(std::cin >> balanceChoice) || (balanceChoice != 0 && balanceChoice != 1)) {
        std::cerr << "Enter 0 or 1 for load balancing.\n";
        return 1;
    }
    options.loadBalancing = balanceChoice == 1;
    if (options.loadBalancing) {
        std::cout << "Load-balancing interval (time units, usually 1): ";
        if (!(std::cin >> options.balanceInterval) || options.balanceInterval <= 0) {
            std::cerr << "Balance interval must be positive.\n";
            return 1;
        }
    }
    std::cout << "Migration overhead cost (time units, 0=disabled): ";
    if (!(std::cin >> options.migrationCost) || options.migrationCost < 0) {
        std::cerr << "Migration cost cannot be negative.\n";
        return 1;
    }

    try {
        PrintResult(Simulate(processes, cores, Policy::FCFS, 1, options));
        PrintResult(Simulate(processes, cores, Policy::SJF, 1, options));
        PrintResult(Simulate(processes, cores, Policy::Priority, 1, options));
        PrintResult(Simulate(processes, cores, Policy::RoundRobin, quantum, options));
    } catch (const std::exception& ex) {
        std::cerr << "Simulation failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
