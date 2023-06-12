#include <Windows.h>
#include <stdio.h>
#include <WtsApi32.h>
#include <tchar.h>
#include <userenv.h>

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

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	int do_once= 0;
	//  Periodically check if the service has been requested to stop
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		
		if (!do_once)
		{
			_printf("\nDo once\n");
			do_once = 1;
			HANDLE self_token;
			HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
			if (pHandle)
			{
				_printf("Get Current Process Finished\n");
				// get the token of the service
				if (OpenProcessToken(pHandle, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &self_token))
				{
					_printf("OpenProcessToken Finished\n");
					// Add SE_TCB_NAME privilege to the service token 
					if (SetPrivilege(self_token, SE_TCB_NAME, TRUE))
					{
						_printf("SetPrivilege Finished\n");
						PWTS_SESSION_INFO pSessionInfo = NULL;
						DWORD sessionCount = 0;
						// Enumerate all sessions
						if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &sessionCount))
						{
							_printf("WTSEnumerateSessions Finished\n");
							for (DWORD i = 0; i < sessionCount; ++i)
							{
								DWORD sessionId = pSessionInfo[i].SessionId;
								if (sessionId != 0)
								{
									HANDLE userToken; 

									if (WTSQueryUserToken(sessionId, &userToken))
									{
										HANDLE userTokenDup;
										_printf("new cmd WTSQueryUserToken Finished\n");
										if (DuplicateTokenEx(userToken, MAXIMUM_ALLOWED, NULL, SecurityDelegation, TokenPrimary, &userTokenDup))
										{
											_printf("DuplicateTokenEx Finished\n");

											//LPWSTR cmd = (LPWSTR)L"C:\\Users\\ISE\\source\\repos\\Injector\\x64\\Debug\\Injector.exe";
											LPWSTR cmd = (LPWSTR)L"C:\\Users\\ISE\\source\\repos\\Injector\\x64\\Release\\Injector.exe";
											STARTUPINFO si = { 0 };
											PROCESS_INFORMATION pi = { 0 };

											si.cb = sizeof(si);
											si.lpDesktop = LPWSTR(L"winsta0\\winlogon");
											//si.lpDesktop = LPWSTR(L"winsta0\\default");
											si.dwFlags = STARTF_USESHOWWINDOW;
											si.wShowWindow = SW_HIDE;

											LPVOID lpEnvironment = NULL;
											CreateEnvironmentBlock(&lpEnvironment, userTokenDup, FALSE);

											_printf("CreateProcessAsUser Started\n");
											BOOL create_process_ret = CreateProcessAsUser(userTokenDup, cmd, NULL, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT, lpEnvironment, NULL, &si, &pi);
											_printf("CreateProcessAsUser Finished\n");
											if (create_process_ret)
											{
												_printf("CreateProcessAsUser Finished\n");
												CloseHandle(pi.hProcess);
												CloseHandle(pi.hThread);
												DestroyEnvironmentBlock(lpEnvironment);
											}
											else
											{
												_printf("CreateProcessAsUser failed:%d\n", GetLastError());
											}
										}
										else
										{
											_printf("DuplicateTokenEx failed:%d\n", GetLastError());
										}
									}
									else
									{
										_printf("WTSQueryUserToken Failed:%d\n", GetLastError());
									}
									CloseHandle(userToken);
								}
							}

							WTSFreeMemory(pSessionInfo);
						}
						else
						{
							_printf("Enumerate Sessions failed%d\n", GetLastError());
						}
					}
					else
					{
						_printf("SetPrivilege Failed%d\n", GetLastError());
					}
				}
				else
				{
					_printf("Open Process Token Failed%d\n", GetLastError());
				}
				CloseHandle(self_token);
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