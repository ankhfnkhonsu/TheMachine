#include "Commander.h"

void Commander::HandleRequest(const char[] requestMessage)
{
	// Loop through message, breaking words at blank spaces and putting them in a
	//  vector of strings.
}

void Commander::InitializeCommands()
{
	// Add all recognized commands to this list for lookup
	knownCommands.emplace_back(new Login());
}