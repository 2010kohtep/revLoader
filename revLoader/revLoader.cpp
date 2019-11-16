#define _CRT_SECURE_NO_WARNINGS	

#include <Windows.h>
#include <iostream>
#include <io.h>

char g_LauncherDir[MAX_PATH] = {};
char g_RevIniName[MAX_PATH] = {};
char g_ProcName[MAX_PATH] = {};
char g_LibraryName[MAX_PATH] = {};

char g_GameAppId[256] = {};

wchar_t **g_Argv = nullptr;
char g_AdditionalProcName[MAX_PATH] = {};
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
	fread(pszOut, sizeof(pszOut[0]), flen, f);
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
	char szDest[260];

	*hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, "Local\\SteamStart_SharedMemFile");

	if (!*hFileMap)
	{
		sprintf(szDest, "Unable to CreateFileMapping: %i", GetLastError());
		MessageBoxA(HWND_DESKTOP, szDest, "a", MB_OK);
	}
	
	*hMapView = MapViewOfFile(*hFileMap, SECTION_ALL_ACCESS, 0, 0, 0);

	if (!*hMapView)
	{
		sprintf(szDest, "Unable to MapViewOfFile: %i", GetLastError());
		MessageBoxA(HWND_DESKTOP, szDest, "a", MB_OK);
		CloseHandle(*hFileMap);
	}

	*hEvent = CreateEventA(NULL, FALSE, FALSE, "Local\\SteamStart_SharedMemLock");

	if (!*hEvent)
	{
		sprintf(szDest, "Unable to CreateEvent: %i", GetLastError());
		MessageBoxA(HWND_DESKTOP, szDest, "a", MB_OK);
		CloseHandle(*hFileMap);
		CloseHandle(*hMapView);
	}

	SetEvent(*hEvent);
}

void SetActiveProcess(int pid)
{
	DWORD dwD;
	HKEY phkResult;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, KEY_WRITE, &phkResult))
		RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, NULL, 0, KEY_WRITE, NULL, &phkResult, &dwD);

	RegSetValueExA(phkResult, "pid", 0, REG_DWORD, (BYTE *)&pid, sizeof(pid));
	RegCloseKey(phkResult);
}

void SetSteamClientDll(char *pszLib)
{
	DWORD dwD;
	HKEY phkResult;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, KEY_WRITE, &phkResult))
		RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", 0, NULL, 0, KEY_WRITE, NULL, &phkResult, &dwD);

	RegSetValueExA(phkResult, "SteamClientDll", 0, REG_SZ, (BYTE *)pszLib, strlen(pszLib) + 1);
	RegCloseKey(phkResult);
}

void StartGameApp()
{
	HANDLE hFileMap = 0;
	HANDLE hMapView = 0;
	HANDLE hSteamMem = 0;
	CreateSharedMemFile(&hMapView, &hFileMap, &hSteamMem);

	STARTUPINFOA StartupInformation = {};
	PROCESS_INFORMATION ProcessInformation = {};

	StartupInformation.cb = sizeof(StartupInformation);

	if (CreateProcessA(NULL, g_ProcName, NULL, NULL, FALSE, 0, NULL, NULL, &StartupInformation, &ProcessInformation))
	{
		SetActiveProcess(ProcessInformation.dwProcessId);

		WaitForSingleObject(ProcessInformation.hThread, INFINITE);
		if (hSteamMem)
			CloseHandle(hSteamMem);
		if (hMapView)
			CloseHandle(hMapView);
		if (hFileMap)
			CloseHandle(hFileMap);
	}
	else
	{
		char szDest[512];
		sprintf(szDest, "Unable to execute command %s (%d)", g_ProcName, GetLastError());
		MessageBoxA(HWND_DESKTOP, szDest, "Error", MB_ICONWARNING | MB_SYSTEMMODAL);
	}
}

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
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

	g_Argv = CommandLineToArgvW(GetCommandLineW(), &g_NumArgs);

	for (int i = 0; i < g_NumArgs; i++)
	{
		if (_wcsicmp(g_Argv[i], L"-launch") == 0)
		{
			wcstombs(g_ProcName, g_Argv[i++ + 1], sizeof(g_ProcName) - 1);
		}
		else if (_wcsicmp(g_Argv[i], L"-appid") == 0)
		{
			wcstombs(g_GameAppId, g_Argv[i++ + 1], sizeof(g_GameAppId) - 1);
		}
		else
		{
			if (i != 0)
			{
				char szArg[128];
				wcstombs(szArg, g_Argv[i], sizeof(szArg) - 1);
				strcat(g_AdditionalProcName, szArg);
				strcat(g_AdditionalProcName, " ");
			}
		}
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

	char szSteamClientDll[MAX_PATH];
	if (GetPrivateProfileStringA("Loader", "SteamClientDll", "", szSteamClientDll, sizeof(szSteamClientDll), g_RevIniName))
	{
		if (szSteamClientDll[0] != '\0')
		{
			strcpy(g_LibraryName, g_LauncherDir);
			strcat(g_LibraryName, szSteamClientDll);

			if (!LoadLibraryA(g_LibraryName))
			{
				char szDest[512];
				sprintf(szDest, "Can't find steamclient.dll relative to executable path %s", g_LauncherDir);
				MessageBoxA(HWND_DESKTOP, szDest, "Warning", MB_ICONWARNING | MB_SYSTEMMODAL);
				return -1;
			}

			SetSteamClientDll(g_LibraryName);
		}
	}

	strcpy(g_LibraryName, g_LauncherDir);
	strcat(g_LibraryName, "steam.dll");

	if (!LoadLibraryA(g_LibraryName))
	{
		char szDest[512];
		sprintf(szDest, "Can't find steam.dll relative to executable path %s", g_LauncherDir);
		MessageBoxA(HWND_DESKTOP, szDest, "Warning", MB_ICONWARNING | MB_SYSTEMMODAL);
		return -1;
	}

	StartGameApp();

	return 0;
}