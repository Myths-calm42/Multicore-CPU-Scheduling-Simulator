#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include "process.h"

std::vector<Process> ReadFromUser();
std::vector<Process> ReadFromFile(const std::string& filename);
bool ValidateProcesses(const std::vector<Process>& processes, std::string& error);

#endif
