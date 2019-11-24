#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <Winternl.h>
#include <iostream>

int* originalWOW64 = nullptr;

__declspec(naked) int* GetWOW64Address()
{
	__asm
	{
		mov eax, fs: [0xC0]
		ret
	}
}

void WriteMemory(void* wow64Address, const void* buffer, const int size)
{
	DWORD dwOldProtect = 0;
	VirtualProtect(wow64Address, size, PAGE_EXECUTE_READWRITE, &dwOldProtect);
	memcpy(wow64Address, buffer, size);
	VirtualProtect(wow64Address, size, dwOldProtect, &dwOldProtect);
}

void WriteWOW64SyscallHookCode(int* wow64Address, const int* newJumpAddress)
{
	unsigned char bytes[] =
	{
		0x68, 0x00, 0x00, 0x00, 0x00,    // push x
		0xC3                             // ret
	};

	memcpy(&bytes[1], &newJumpAddress, sizeof(int*));
	WriteMemory(wow64Address, bytes, sizeof(bytes));
}

void saveOriginalWOW64(const void* wow64Address)
{
	// FF 25 00000000        - jmp dword ptr [00000000]
	auto size = 6;
	originalWOW64 = reinterpret_cast<int*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	if (originalWOW64 != nullptr)
	{
		memcpy(originalWOW64, wow64Address, size);
	}
}

DWORD BackupEax;
HANDLE processHandle;
PVOID baseAddress;
PVOID buffer;
ULONG numberOfBytesToWrite;
PULONG pNumberOfBytesToWrite;
PDWORD numberOfBytesWritten;
void __declspec(naked) HookedNtWriteVirtualMemory()
{
	__asm
	{
		mov BackupEax, eax

		mov eax, [esp + 0x14] // numberOfBytesWritten
		mov numberOfBytesWritten, eax

		mov eax, [esp + 0x14] // numberOfBytesToWrite
		mov numberOfBytesToWrite, eax

		lea eax, [esp + 0x14] // pointer numberOfBytesToWrite
		mov pNumberOfBytesToWrite, eax

		mov eax, [esp + 0x10] // buffer
		mov buffer, eax

		mov eax, [esp + 0x0C] // baseAddress
		mov baseAddress, eax

		mov eax, [esp + 0x08] // processHandle
		mov processHandle, eax

		mov eax, BackupEax

		pushad
		pushfd
	}

	// change buffer before syscall
	*reinterpret_cast<int*>(buffer) = 999;
	// *pNumberOfBytesToWrite = 0;

	// std::wprintf(L"buffer = %d\n", *reinterpret_cast<int*>(buffer));
	// std::wprintf(L"numberOfBytesToWrite = %d\n", *reinterpret_cast<int*>(pNumberOfBytesToWrite));

	__asm
	{
		popfd
		popad
		jmp originalWOW64
	}
}

#pragma warning(disable: 4414)
void __declspec(naked) HookedWOW64Syscall()
{
	__asm
	{
		cmp eax, 0x3A // syscall ID
		jz HookedNtWriteVirtualMemory

		// cmp eax, 0x26
		// jz HookedNtOpenProcess

		jmp originalWOW64
	}
}
#pragma warning(default: 4414)


int main()
{
	auto x = 100;
	auto y = 200;
	auto hookWOW64Syscall = true;

	std::wprintf(L"x = %d\n", x);

	if (hookWOW64Syscall)
	{
		auto wow64Address = GetWOW64Address();
		saveOriginalWOW64(wow64Address);
		WriteWOW64SyscallHookCode(wow64Address, reinterpret_cast<int*>(HookedWOW64Syscall));
	}

	while (true)
	{
		WriteProcessMemory(GetCurrentProcess(), &x, &y, sizeof(y), NULL);
		std::wprintf(L"x = %d\n", x);

		Sleep(1000);
	}
	return 0;
}