#include "Commands/ICommand.h"

static class Commander
{
public:
	static Command GetInstance()
	{
		static Commander _instance = new Commander();
		return _instance;
	}

	void HandleRequest( requestMessage);
	
private:
	struct CommandNameMap
	{
	public:
		string Name;
		ICommand Command;
	}

	Commander();
	std::vector<CommandNameMap> knownCommands;

	void InitializeCommands();

	// TODO:  Implement a IsOffCooldown() that checks these times.
	float lastCommandExecutionTime = 0f;      // [systemTime]
	float commandExecutionCooldownTime = 10f; // [minutes]
}