#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <iostream>
#include <string>
#include "libs/Memory Manager/ProcessManager.hpp"
using namespace std;

void WriteAbsoluteJump64(void* absJumpMemory, void* addrToJumpTo)
{
	uint8_t absJumpInstructions[] =
	{
	  0x49, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //mov r10, addr
	  0x41, 0xFF, 0xE2 //jmp r10
	};

	uint64_t addrToJumpTo64 = (uint64_t)addrToJumpTo;
	memcpy(&absJumpInstructions[2], &addrToJumpTo64, sizeof(addrToJumpTo64));
	memcpy(absJumpMemory, absJumpInstructions, sizeof(absJumpInstructions));
}
void* AllocatePageNearAddress(void* targetAddr)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	const uint64_t PAGE_SIZE = sysInfo.dwPageSize;

	uint64_t startAddr = (uint64_t(targetAddr) & ~(PAGE_SIZE - 1)); //round down to nearest page boundary
	uint64_t minAddr = min(startAddr - 0x7FFFFF00, (uint64_t)sysInfo.lpMinimumApplicationAddress);
	uint64_t maxAddr = max(startAddr + 0x7FFFFF00, (uint64_t)sysInfo.lpMaximumApplicationAddress);

	uint64_t startPage = (startAddr - (startAddr % PAGE_SIZE));

	uint64_t pageOffset = 1;
	while (1)
	{
		uint64_t byteOffset = pageOffset * PAGE_SIZE;
		uint64_t highAddr = startPage + byteOffset;
		uint64_t lowAddr = (startPage > byteOffset) ? startPage - byteOffset : 0;

		bool needsExit = highAddr > maxAddr && lowAddr < minAddr;

		if (highAddr < maxAddr)
		{
			void* outAddr = VirtualAlloc((void*)highAddr, PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (outAddr)
				return outAddr;
		}

		if (lowAddr > minAddr)
		{
			void* outAddr = VirtualAlloc((void*)lowAddr, PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (outAddr != nullptr)
				return outAddr;
		}

		pageOffset++;

		if (needsExit)
		{
			break;
		}
	}

	return nullptr;
}
void InstallHook(void* func2hook, void* payloadFunction)
{
	void* relayFuncMemory = AllocatePageNearAddress(func2hook);
	WriteAbsoluteJump64(relayFuncMemory, payloadFunction); //write relay func instructions

	//now that the relay function is built, we need to install the E9 jump into the target func,
	//this will jump to the relay function
	DWORD oldProtect;
	VirtualProtect(func2hook, 1024, PAGE_EXECUTE_READWRITE, &oldProtect);

	//32 bit relative jump opcode is E9, takes 1 32 bit operand for jump offset
	uint8_t jmpInstruction[5] = { 0xE9, 0x0, 0x0, 0x0, 0x0 };

	//to fill out the last 4 bytes of jmpInstruction, we need the offset between 
	//the relay function and the instruction immediately AFTER the jmp instruction
	const uint64_t relAddr = (uint64_t)relayFuncMemory - ((uint64_t)func2hook + sizeof(jmpInstruction));
	memcpy(jmpInstruction + 1, &relAddr, 4);

	//install the hook
	memcpy(func2hook, jmpInstruction, sizeof(jmpInstruction));
}

extern "C" int get_money_hook();
extern "C" int get_money();
extern "C" int set_money(DWORD64 money);

void Thread()
{
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONIN$", "r", stdin);

	ProcessMgr.Attach("Stardew Valley.exe");
	auto found_addresses = ProcessMgr.SearchMemory("00 48 83 EC 28 48 89 4C 24 30 48 8B 91 C0 04 00 00 48 8B 4A 48 48 8B 54 24 30 39 09",
		0x0000000000000000, 0xffffffffffffffff);

	DWORD old_protect;
	unsigned char* hook_location = (unsigned char*)(found_addresses[0] + 33);

	unsigned char original_opcodes[8];
	for (int i = 0; i < 8; i++) {
		original_opcodes[i] = *(hook_location + i);
	}

	InstallHook(hook_location, get_money_hook);

	Sleep(100);
	int input = 0;

	while (true)
	{
		system("cls");

		switch (input)
		{
		case 0:
		{
			string str_input = "";
			cout << "[1] Para Arttir\n[2] Para Azalt\n[3] Exit\n\n>>> ";
			cin >> input;
			break;
		}
		case 1:
		{
			cout << "ne kadar arttiram : ";
			int arttir_input = 0;
			cin >> arttir_input;

			set_money(get_money() + arttir_input);
			input = 0;
			break;
		}
		case 2:
		{
			cout << "ne kadar azaltam : ";
			int azalt_input = 0;
			cin >> azalt_input;

			set_money(get_money() - azalt_input);
			input = 0;
			break;
		}
		case 3:
		{
			for (int i = 0; i < 8; i++) {
				*(hook_location + i) = original_opcodes[i];
			}

			FreeConsole();
			FreeLibraryAndExitThread(NULL, 0);
		}
		default:
			break;
		}
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Thread, 0, 0, 0);
	}

	return true;
}