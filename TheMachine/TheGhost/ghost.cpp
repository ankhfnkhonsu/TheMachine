#include "ghost.h"
// DO NOT ADD ANYTHING ABOVE THIS FUNCTION DEFINITION
extern "C" GHOST_API void ProcessCommand(const char[] command)
{
	// Parse command
	// First word should be Command Type (Login, Mischief, FreezeSystem)
	// Look up Command type <string, CommandType> Table/Dictionary/Map
	// requestedCommand.Execute( args ... )
}