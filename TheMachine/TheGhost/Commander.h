#include "Commands/ICommand"

struct CommandNameMap
{
public:
	string Name;
	ICommand Command;
}

static class Commander
{
public:
	Command GetInstance()
	{
		static Commander _instance = new Commander();
		return _instance;
	}

	void HandleRequest(const char[] requestMessage);
	
private:
	Commander();
	std::vector<ICommand> knownCommands;

	void InitializeCommands();
}