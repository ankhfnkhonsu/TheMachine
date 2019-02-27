#include "ICommand.h"

class Login : public ICommand
{
	void Execute(std::queue<std::string>* args) override;
}