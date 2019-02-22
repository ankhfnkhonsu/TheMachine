// https://github.com/vmcall/latebros/blob/3305a4b34fcd8c2292f8ee49dfa721b2d51a266d/littlebro/detour.cpp

#include "stdafx.h"
#include "hideme.hpp"


std::array<uint8_t, 0xF> hideme::generate_shellcode(uintptr_t hook_pointer)
{
	std::array<uint8_t, 0xF> hook_bytes = {
		0xFF, 0x35, 0x01, 0x00, 0x00, 0x00,							// PUSH [RIP+1]
		0xC3,														// RET
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };			// HOOK POINTER
	std::memcpy(hook_bytes.data() + 0x7, &hook_pointer, sizeof(hook_pointer));

	return hook_bytes;
}

void hideme::hook_function(uintptr_t function_address, uintptr_t hook_address)
{
	const auto shellcode = hideme::generate_shellcode(hook_address);

	DWORD old_protect;
	VirtualProtect(reinterpret_cast<void*>(function_address), 0x1000, 0x40, &old_protect);
	memcpy(reinterpret_cast<void*>(function_address), shellcode.data(), shellcode.size());
	VirtualProtect(reinterpret_cast<void*>(function_address), 0x1000, old_protect, &old_protect);
}

void hideme::remove_detour(uintptr_t function_address, char* original_bytes, size_t length)
{
	DWORD old_protect;
	VirtualProtect(reinterpret_cast<void*>(function_address), 0x1000, 0x40, &old_protect);
	memcpy(reinterpret_cast<void*>(function_address), original_bytes, length);
	VirtualProtect(reinterpret_cast<void*>(function_address), 0x1000, old_protect, &old_protect);
}