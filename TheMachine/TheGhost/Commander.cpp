#include "Commander.h"

void Commander::HandleRequest(char ** requestMessage)
{
	// Loop through message, breaking words at blank spaces and putting them in a
	//  vector of strings.
	std::queue<std::string> commandArgs;
	char* c = requestMessage;
	std::string currentWord;
	int i = 0;

	while( c != '\0' ) // while the next char im going to check isnt EOF
	{
		// Look for the end of the current words
		if(c == ' ') // Space, last word is done, prep for new word
		{
			commandArgs.enqueue(new string(currentWord));
			currentWord.clear();
		}
		else // Valid letter, append to wordString
		{
			currentWord.append(c);
		}
		
		c++; // lewl
	}

	ICommand* commandToExecute = nullptr;

	for(int i = 0; i < knownCommands.size(); i++)
	{
		if(knownCommands[i]->Name == commandArgs[0]) // I hate string compare, but.. good for now
		{
			commandToExecute = knownCommands[i]->Command;
			break;
		}
	}

	commandToExecute->Execute(new std::queue<std::string>(commandArgs));
}

void Commander::InitializeCommands()
{
	// Add all recognized commands to this list for lookup
	knownCommands.emplace_back(new CommandNameMap("AUTH", new Login());
	knownCommands.emplace_back(new CommandNameMap("GRNBLT", new GreenBlit()));
}

Commander::~Commander()
{
	for (const auto& commandPair : knownCommands)
	{
		delete commandPair->Command;
	}

	// Vector handles its own memory clean up when it goes out of scope, right?
	//  else, free vector memory manually?  seems wrong, been in C# too long.
}