// DO NOT ADD ANYTHING BETWEEN HERE AND THE PROCESS COMMAND FUNCTION DECLARATION!!!!!!!!!!!!!
#pragma once

#define GHOST_API __declspec(dllexport)

extern "C" GHOST_API void ProcessCommand(const char[] command);
// DO NOT ADD ANYTHING ABOVE THE PROCESS COMMAND FUNCTION DECLARATION!!!!!!!!!!!!!

// All new functionality and features to the ghost should be added below this line
