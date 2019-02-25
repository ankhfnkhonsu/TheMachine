#include "Login.h"

Login::Execute(string[] args)
{
	// Command string should look something like "Login <password>"
	// So, whether we strip the first word as we pass the command string
	//  or not, we want to find the <password> and check against the system.

	// SystemAdministrator.TryLogin(args[0]);
	// System should set a valid connection flag or disconnect on fail or success
}