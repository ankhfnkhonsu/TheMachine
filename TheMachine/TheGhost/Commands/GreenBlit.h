#pragma once

#include "ICommand.h"

//  Green blit will plaster a green pixel to the screen
//    This will remain until the system is reset.
//    This routine should continute to run, at x time units, blit another pixel
//    Max pixels blited ~= Screen.Width / 100
class GreenBlit : public ICommand
{
public:
	void Execute(std::queue<std::string>* args) override;

private:
	float lastTimeBlit = 0f;
	static unsigned int numPixelsBlit = 0U;

}