#pragma once

#include "Commands/ICommand.h"
#include <vector>
#include <string>
#include <queue>

struct CommandNameMap
{
public:
	std::string Name;
	ICommand* Command;
}

static class Commander
{
public:
	static Commander GetInstance()
	{
		static Commander _instance = new Commander();
		return _instance;
	}

	void HandleRequest(char ** requestMessage);
	~Commander();
private:
	Commander();
	std::vector<CommandNameMap*> knownCommands;

	void InitializeCommands();
}