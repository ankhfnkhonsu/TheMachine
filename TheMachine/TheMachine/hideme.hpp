// https://github.com/vmcall/latebros/blob/3305a4b34fcd8c2292f8ee49dfa721b2d51a266d/littlebro/detour.hpp

#pragma once

#include "stdafx.h"

namespace hideme
{
	std::array<uint8_t, 0xF> generate_shellcode(uintptr_t hook_pointer);
	void hook_function(uintptr_t function_address, uintptr_t hook_address);
	void remove_detour(uintptr_t function_address, char* original_bytes, size_t length);
}