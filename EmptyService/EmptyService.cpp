#include <Windows.h>
#include <stdio.h>
#include <WtsApi32.h>
#include <tchar.h>
#include <userenv.h>
#include <tlhelp32.h>

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

#define SERVICE_NAME  _T("My Sample Service")
#define no_init_all deprecated
int _tmain(int argc, TCHAR *argv[])
{ 
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{LPWSTR(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{NULL, NULL}
	};

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		return GetLastError();
	}

	return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	DWORD Status = E_FAIL;
	HANDLE hThread;
	// Register our service control handler with the SCM
	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

	if (g_StatusHandle == NULL)
	{
		goto EXIT;
	}

	// Tell the service controller we are starting
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
	}

	/*
	 * Perform tasks necessary to start the service here
	 */

	 // Create a service stop event to wait on later
	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		// Error creating event
		// Tell service controller we are stopped and exit
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
		}
		goto EXIT;
	}

	// Tell the service controller we are started
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
	}

	// Start a thread that will perform the main task of the service
	hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

	// Wait until our worker thread exits signaling that the service needs to stop
	WaitForSingleObject(hThread, INFINITE);


	/*
	 * Perform any cleanup tasks
	 */

	CloseHandle(g_ServiceStopEvent);

	// Tell the service controller we are stopped
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
	}

EXIT:
	return;
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		/*
		 * Perform tasks necessary to stop the service here
		 */

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T("My Sample Service: ServiceCtrlHandler: SetServiceStatus returned error"));
		}

		// This will signal the worker thread to start shutting down
		SetEvent(g_ServiceStopEvent);

		break;

	default:
		break;
	}
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

void _wprintf(const wchar_t* format, ...)
{
	wchar_t msg[100];
	va_list args;
	va_start(args, format);
	vswprintf(msg, sizeof(msg) / sizeof(wchar_t), format, args); // do check return value
	va_end(args);
	OutputDebugStringW(msg);
}

BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
{
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))      
	{
		_printf("LookupPrivilegeValue error: %u\n", GetLastError());
		return FALSE;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if (bEnablePrivilege)
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes = 0;

	// Enable the privilege or disable all privileges.

	if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL))
	{
		_printf("AdjustTokenPrivileges error: %u\n", GetLastError());
		return FALSE;
	}

	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
	{
		_printf("The token does not have the specified privilege. \n");
		return FALSE;
	}
	return TRUE;
}

DWORD findWinlogonProcess()
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	PROCESSENTRY32 processEntry;
	processEntry.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(snapshot, &processEntry))
	{
		CloseHandle(snapshot);
		return 0;
	}

	do
	{
		if (wcscmp(processEntry.szExeFile, L"winlogon.exe") == 0)
		{
			CloseHandle(snapshot);
			return processEntry.th32ProcessID;
		}
	} while (Process32Next(snapshot, &processEntry));

	CloseHandle(snapshot);
	return 0;
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	LPWSTR injector_path = (LPWSTR)L"C:\\Users\\ISE\\source\\repos\\Injector\\x64\\Release\\Injector.exe";
	int do_once= 0;
	//  Periodically check if the service has been requested to stop
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		if (!do_once)
		{
			do_once = 1;
			HANDLE service_token;
			HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
			if (pHandle)
			{
				_printf("Get Current Process Finished\n");

				// get the token of the service
				if (OpenProcessToken(pHandle, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &service_token))
				{
					_printf("OpenProcessToken Finished\n");
					DWORD logon_pid = findWinlogonProcess();
					HANDLE winlogon_pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, logon_pid);
					HANDLE winlogonToken;
					if (OpenProcessToken(winlogon_pHandle, TOKEN_ALL_ACCESS, &winlogonToken))
					{
						_printf("OpenProcessToken Finished\n");
						STARTUPINFO si = { 0 };
						PROCESS_INFORMATION pi = { 0 };

						si.cb = sizeof(si);
						si.lpDesktop = LPWSTR(L"winsta0\\winlogon");

						si.dwFlags = STARTF_USESHOWWINDOW;
						si.wShowWindow = SW_HIDE;

						BOOL create_process_ret = CreateProcessAsUser(winlogonToken, injector_path, NULL, NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi);
						if (create_process_ret)
						{
							_printf("CreateProcessAsUser Finished\n");
							CloseHandle(pi.hProcess);
							CloseHandle(pi.hThread);
						}
						else
						{
							_printf("CreateProcessAsUser failed:%d\n", GetLastError());
						}
						CloseHandle(winlogonToken);
					}
					else
					{
						_printf("Open winlogon token failed:%d\n", GetLastError());
					}
					CloseHandle(winlogon_pHandle);
					CloseHandle(service_token);
				}
				else
				{
					_printf("Open Process Token Failed%d\n", GetLastError());
				}
				CloseHandle(pHandle);
			}
			else
			{
				_printf("GetCurrentProcess Failed:%d\n", GetLastError());
			}
		}
		else
		{
			Sleep(1000);
		} 
		
		
	}

	return ERROR_SUCCESS;
}