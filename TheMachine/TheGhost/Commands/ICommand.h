#pragma once
#include <queue>
#include <string>

class Command
{
	virtual void Execute(std::queue<std::string>* args) = 0;

private:
	std::queue<std::string>* myArgs = nullptr;
}