#include <Windows.h>
#include <stdio.h>
#include <WtsApi32.h>
#include <tchar.h>
#include <userenv.h>
#include <tlhelp32.h>

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;
HANDLE hPipe; // pipe to communicate with the injector

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

#define pipeName _T("\\\\.\\pipe\\pipeToInjector")
#define SERVICE_NAME  _T("My Sample Service")
#define no_init_all deprecated

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


void ServiceInit()
{
	LPWSTR injector_path = (LPWSTR)L"C:\\Users\\ISE\\source\\repos\\Injector\\x64\\Release\\Injector.exe";

	// Create the named pipe to signal the injector when to unhook
	hPipe = CreateNamedPipe(pipeName, PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1024, 0, 0, NULL);

	HANDLE service_token;

	// get handle of the service
	HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
	if (pHandle)
	{
		_printf("Get Current Process Finished\n");

		// get the token of the service
		if (OpenProcessToken(pHandle, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &service_token))
		{
			_printf("OpenProcessToken of the service Finished\n");
			// find pid of winlogon.exe
			DWORD logon_pid = findWinlogonProcess();
			HANDLE winlogon_pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, logon_pid);
			HANDLE winlogonToken;
			
			// get token of winlogon.exe
			if (OpenProcessToken(winlogon_pHandle, TOKEN_ALL_ACCESS, &winlogonToken))
			{
				_printf("OpenProcessToken of winlogon.exe Finished\n");
				STARTUPINFO si = { 0 };
				PROCESS_INFORMATION pi = { 0 };

				si.cb = sizeof(si);
				si.lpDesktop = LPWSTR(L"winsta0\\winlogon");

				si.dwFlags = STARTF_USESHOWWINDOW;
				si.wShowWindow = SW_HIDE;

				// create Injector Process in session1\winsta0\winlogon
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
		_printf("My Sample Service: ServiceMain: SetServiceStatus returned error");
	}

	ServiceInit();
	

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
			_printf("My Sample Service: ServiceMain: SetServiceStatus returned error");
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
		_printf("My Sample Service: ServiceMain: SetServiceStatus returned error");
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
		_printf("My Sample Service: ServiceMain: SetServiceStatus returned error");
	}

EXIT:
	return;
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
		_printf("Service is Stopping\n");
		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		if (hPipe != INVALID_HANDLE_VALUE)
		{

			DWORD dwWritten;
			char msg[] = "STOP"; 
			WriteFile(hPipe, msg, sizeof(msg), &dwWritten, NULL); // tell the injector to release the hook
			CloseHandle(hPipe);
		}
	
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			_printf("My Sample Service: ServiceCtrlHandler: SetServiceStatus returned error");
		}

		// This will signal the worker thread to start shutting down
		SetEvent(g_ServiceStopEvent);

		break;

	default:
		break;
	}
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	
	//  Periodically check if the service has been requested to stop
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{

		// simulate work of the service
		Sleep(1000);
	}

	return ERROR_SUCCESS;
}