// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <iostream>
#include <ShlObj.h>
#include <string>
#include <TlHelp32.h>
#include <vector>
#include <fstream>
#include <algorithm>

constexpr auto APP_NAME = L"rainmeter.exe";
constexpr auto APP_LOC = L"C:\\Program Files\\Rainmeter\\Rainmeter.exe";
constexpr auto BLACKLIST_FILE = L"GAMESLIST.txt";
constexpr auto SLEEP_TIMER_S = 5;

volatile bool have_closed_app = false;
volatile HANDLE process_snapshot{};
std::vector<std::wstring> blacklisted_apps{};

std::wstring ToLower(std::wstring str)
{
	std::transform(str.begin(), str.end(), str.begin(), (int(*)(int))std::tolower);
	return str;
}

void CreateAppProcess(const wchar_t* command)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	
	if (CreateProcess(command, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

void CloseApp()
{
	ShellExecute(NULL, L"open", L"C:\\Program Files\\Rainmeter\\Rainmeter.exe", L"!quit", NULL, SW_HIDE);
	have_closed_app = true;
}

void OpenApp()
{
	CreateAppProcess(APP_LOC);
	have_closed_app = false;
}

DWORD GetProcessIdFromName(const wchar_t* szProcessName)
{
	PROCESSENTRY32 entry{};
	entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(process_snapshot, &entry) != FALSE)
		while (Process32Next(process_snapshot, &entry) != FALSE)
			if (!ToLower(entry.szExeFile).compare(szProcessName))
				return entry.th32ProcessID;
	return 0;
}

bool CheckIfProcessRunning()
{
	return !!GetProcessIdFromName(APP_NAME);
}

void LoadBlacklist()
{
	std::wifstream infile;

	wchar_t ownPth[MAX_PATH];
	GetModuleFileName(GetModuleHandle(NULL), ownPth, MAX_PATH);
	std::wstring file = ownPth;
	file = file.substr(0, file.find_last_of(L'\\') + 1) + BLACKLIST_FILE;
	infile.open(file.c_str());

	std::wstring temp;
	while (infile >> temp)
		blacklisted_apps.push_back(ToLower(temp));

	infile.close();
}

//check for open processes written from non-steam games
bool LoopThroughBlacklist()
{
	PROCESSENTRY32 entry{};
	entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(process_snapshot, &entry) != FALSE)
		while (Process32Next(process_snapshot, &entry) != FALSE)
		{
			auto entry_lower = ToLower(entry.szExeFile);
			for (const auto& blacklist_item : blacklisted_apps)
				if (!blacklist_item.compare(entry_lower))
					return true;
		}

	return false;
}

//read a key from registry
DWORD RegGetDword(
	HKEY hKey,
	const std::wstring& subKey,
	const std::wstring& value
)
{
	DWORD data{};
	DWORD dataSize = sizeof(data);

	LONG retCode = ::RegGetValue(
		hKey,
		subKey.c_str(),
		value.c_str(),
		RRF_RT_REG_DWORD,
		nullptr,
		&data,
		&dataSize
	);

	if (retCode != ERROR_SUCCESS)
		return 0;
	return data;
}

bool SteanGaneRunning()
{
	//if steam isnt running, no point in checking for game status
	if (!GetProcessIdFromName(L"steam.exe"))
		return false;

	//HKEY_CURRENT_USER\Software\Valve\Steam\RunningAppID
	const wchar_t* REG_SW_GROUP_I_WANT = L"Software\\Valve\\Steam";
	const wchar_t* REG_KEY_I_WANT = L"RunningAppID";

	//read the id of currently running game
	//0 if nothing open, positive for game id
	DWORD data = RegGetDword(HKEY_CURRENT_USER, REG_SW_GROUP_I_WANT, REG_KEY_I_WANT);
	if (data > 0)
		return true;

	//default case
	return false;
}

bool CheckForOpenGame()
{
	//check for steam game
	if (SteanGaneRunning())
		return true;

	//check for blacklisted titles
	if (LoopThroughBlacklist())
		return true;

	//default case
	return false;
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	//set priority to low
	SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
	//load the gamelist if playing outside of steam
	LoadBlacklist();

	while (true)
	{
		Sleep(SLEEP_TIMER_S * 1000);

		process_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		//check for rainmeter status
		bool app_running = CheckIfProcessRunning();
		//check for gaming status
		bool app_should_be_running = !CheckForOpenGame();

		//close rainmeter if it is open and a game is running
		if (app_running && !app_should_be_running)
			CloseApp();
		//if the app closed rainmeter, open it if not gaming anymore
		else if (have_closed_app && !app_running && app_should_be_running)
			OpenApp();
		CloseHandle(process_snapshot);
	} //-V1020
}
