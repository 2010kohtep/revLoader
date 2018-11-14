#define _CRT_SECURE_NO_WARNINGS	

#include <Windows.h>
#include <iostream>
#include <io.h>

char g_LauncherDir[MAX_PATH] = { 0 };
char g_RevIniName[MAX_PATH]  = { 0 };
char g_ProcName[MAX_PATH]    = { 0 };
char g_LibraryName[MAX_PATH] = { 0 };

char g_GameAppId[256] = { 0 };

wchar_t **g_Argv = nullptr;
char g_AdditionalProcName[MAX_PATH] = { 0 };
int g_NumArgs = 0;

bool GetSteamAppID(char *pszOut)
{
	FILE* f = fopen("steam_appid.txt", "r");
	if (!f)
	{
		*pszOut = '\0';
		return false;
	}

	int fno = _fileno(f);
	int flen = _filelength(fno);
	fread(pszOut, 1, flen, f);
	fclose(f);

	char *psz = strchr(pszOut, ' ');
	if (psz)
	{
		*psz = '\0';
	}

	return true;
}

void CreateSharedMemFile(HANDLE *hMapView, HANDLE *hFileMap, HANDLE *hEvent)
{
	char Dest[260];

	*hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, 4u, 0, 0x400u, "Local\\SteamStart_SharedMemFile");

	if (!*hFileMap)
	{
		sprintf(Dest, "Unable to CreateFileMapping: %i", GetLastError());
		MessageBoxA(HWND_DESKTOP, Dest, "a", MB_OK);
	}

	*hMapView = MapViewOfFile(*hFileMap, 0xF001Fu, 0, 0, 0);

	if (!*hMapView)
	{
		sprintf(Dest, "Unable to MapViewOfFile: %i", GetLastError());
		MessageBoxA(HWND_DESKTOP, Dest, "a", MB_OK);
		CloseHandle(*hFileMap);
	}

	*hEvent = CreateEventA(NULL, FALSE, FALSE, "Local\\SteamStart_SharedMemLock");

	if (!*hEvent)
	{
		sprintf(Dest, "Unable to CreateEvent: %i", GetLastError());
		MessageBoxA(HWND_DESKTOP, Dest, "a", MB_OK);
		CloseHandle(*hFileMap);
		CloseHandle(*hMapView);
	}

	SetEvent(*hEvent);
}

void SetActiveProcess(int pid)
{
	DWORD dwD;
	HKEY phkResult;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, 0x20006, &phkResult))
		RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, NULL, 0, 0x20006, NULL, &phkResult, &dwD);

	RegSetValueExA(phkResult, "pid", 0, 4, (unsigned char *)&pid, 4);
	RegCloseKey(phkResult);
}

void SetSteamClientDll(char *pszLib)
{
	DWORD dwD;
	HKEY phkResult;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, 0x20006, &phkResult))
		RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, NULL, 0, 0x20006, NULL, &phkResult, &dwD);

	RegSetValueExA(phkResult, "SteamClientDll", 0, 1, (unsigned char *)pszLib, strlen(pszLib) + 1);
	RegCloseKey(phkResult);
}

void StartGameApp()
{
	HANDLE hFileMap = 0;
	HANDLE hMapView = 0;
	HANDLE hSteamMem = 0;
	CreateSharedMemFile(&hMapView, &hFileMap, &hSteamMem);

	STARTUPINFOA StartupInformation = { 0 };
	StartupInformation.cb = sizeof(StartupInformation);
	PROCESS_INFORMATION ProcessInformation = { 0 };

	if (CreateProcessA(NULL, g_ProcName, NULL, NULL, FALSE, 0, NULL, NULL, &StartupInformation, &ProcessInformation))
	{
		SetActiveProcess(ProcessInformation.dwProcessId);

		WaitForSingleObject(ProcessInformation.hThread, -1);
		if (hSteamMem)
			CloseHandle(hSteamMem);
		if (hMapView)
			CloseHandle(hMapView);
		if (hFileMap)
			CloseHandle(hFileMap);
	}
	else
	{
		char Dest[512];
		int iErrCode = GetLastError();
		sprintf(Dest, "Unable to execute command %s (%d)", g_ProcName, iErrCode);
		MessageBoxA(HWND_DESKTOP, Dest, "Error", MB_ICONWARNING | MB_SYSTEMMODAL);
	}
}

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR     lpCmdLine,
	_In_ int       nCmdShow
)
{
	if (!GetModuleFileNameA(NULL, g_LauncherDir, sizeof(g_LauncherDir)))
	{
		MessageBoxA(HWND_DESKTOP, "Unable to initialize the process", "Error", MB_ICONWARNING | MB_SYSTEMMODAL);
		return -1;
	}

	char *psz = strrchr(g_LauncherDir, '\\') + 1;
	*psz = '\0';

	strcpy(g_RevIniName, g_LauncherDir);
	strcat(g_RevIniName, "rev.ini");

	wchar_t *pszCmdLine = GetCommandLineW();
	int iCurArg = 0;
	g_Argv = CommandLineToArgvW(pszCmdLine, &g_NumArgs);

	if (g_NumArgs > 0)
	{
		do
		{
			if (_wcsicmp(g_Argv[iCurArg], L"-launch") == 0)
			{
				wcstombs(g_ProcName, g_Argv[iCurArg++ + 1], sizeof(g_ProcName) - 1);
			}
			else if (_wcsicmp(g_Argv[iCurArg], L"-appid") == 0)
			{
				wcstombs(g_GameAppId, g_Argv[iCurArg++ + 1], sizeof(g_GameAppId) - 1);
			}
			else
			{
				if (iCurArg != 0)
				{
					char sArg[128];
					wcstombs(sArg, g_Argv[iCurArg], sizeof(sArg) - 1);
					strcat(g_AdditionalProcName, sArg);
					strcat(g_AdditionalProcName, " ");
				}
			}

			++iCurArg;
		} while (iCurArg < g_NumArgs);
	}

	if (strlen(g_AdditionalProcName) != 0)
		strcat(g_ProcName, g_AdditionalProcName);

	if (!GetPrivateProfileStringA("Loader", "ProcName", "", g_ProcName, sizeof(g_ProcName), g_RevIniName))
	{
		MessageBoxA(HWND_DESKTOP, "ProcName value not found on command line or in rev.ini. Please edit the file.", 
			"Error", MB_ICONWARNING | MB_SYSTEMMODAL);
		return -1;
	}

	if (!GetSteamAppID(g_GameAppId))
	{
		MessageBoxA(HWND_DESKTOP, "No steam_appid.txt detected, the game might not launch correctly", 
			"Warning", MB_ICONWARNING | MB_SYSTEMMODAL);
	}

	if (g_GameAppId[0] != '\0')
	{
		SetEnvironmentVariableA("SteamGameId", g_GameAppId);
		SetEnvironmentVariableA("SteamAppId", g_GameAppId);
	}

	char SteamClientDll[MAX_PATH];
	if (GetPrivateProfileStringA("Loader", "SteamClientDll", "", SteamClientDll, sizeof(SteamClientDll), g_RevIniName))
	{
		if (SteamClientDll[0] != '\0')
		{
			strcpy(g_LibraryName, g_LauncherDir);
			strcat(g_LibraryName, SteamClientDll);

			if (!LoadLibraryA(g_LibraryName))
			{
				char Dest[512];
				sprintf(Dest, "Can't find steamclient.dll relative to executable path %s", g_LauncherDir);
				MessageBoxA(HWND_DESKTOP, Dest, "Warning", MB_ICONWARNING | MB_SYSTEMMODAL);
				return -1;
			}

			SetSteamClientDll(g_LibraryName);
		}
	}

	strcpy(g_LibraryName, g_LauncherDir);
	strcat(g_LibraryName, "steam.dll");

	if (!LoadLibraryA(g_LibraryName))
	{
		char Dest[512];
		sprintf(Dest, "Can't find steam.dll relative to executable path %s", g_LauncherDir);
		MessageBoxA(HWND_DESKTOP, Dest, "Warning", MB_ICONWARNING | MB_SYSTEMMODAL);
		return -1;
	}

	StartGameApp();

	return 0;
}