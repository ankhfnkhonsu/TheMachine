#include "ICommand.h"

class Login : ICommand
{
	void Execute(std::queue<std::string>* args) override;
}