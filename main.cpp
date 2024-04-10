// Midnight-Guns-Launcher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <iostream>
#include <cstdint>
#include "fte_steam.h"
#include "st_common.h"
#include "md5.h"
#include "toojpeg.h"


/*
#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#include <stdio.h>
#include <tchar.h>
#include <shellapi.h>

HANDLE hPipeServer;


#endif
*/

#define GAME_LAUNCH_NAME "fteqw64"
#ifndef GAME_LAUNCH_NAME
#error Please define your game exe name.
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
typedef PROCESS_INFORMATION ProcessType;
typedef HANDLE PipeType;
#define NULLPIPE NULL
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
typedef pid_t ProcessType;
typedef int PipeType;
#define NULLPIPE -1
#endif

#pragma comment(lib, "steam_api")
#include "steam/steam_api.h"

#define DEBUGPIPE 1
#if DEBUGPIPE
#define dbgpipe printf
#else
static inline void dbgpipe(const char *fmt, ...) {}
#endif

PipeType pipeParentRead;
PipeType pipeParentWrite;
PipeType pipeChildRead;
PipeType pipeChildWrite;
ProcessType childPid;

bool	is_server;
char	EXECUTABLE_NAME[MAX_STRING];

/* platform-specific mainline calls this. */
static int mainline(int argc, char **argv);

/* Windows and Unix implementations of this stuff below. */
static void fail(const char *err);
static bool writePipe(PipeType fd, const void *buf, const unsigned int _len);
static int readPipe(PipeType fd, void *buf, const unsigned int _len);
static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
	PipeType *pPipeChildRead, PipeType *pPipeChildWrite);
static void closePipe(PipeType fd);
static bool setEnvVar(const char *key, const char *val);
static bool launchChild(ProcessType *pid, std::string argStr);
static int closeProcess(ProcessType *pid);

unsigned char PIPE_ReadByte(void);

#ifdef _WIN32
static void fail(const char *err)
{
	MessageBoxA(NULL, err, "ERROR", MB_ICONERROR | MB_OK);
	ExitProcess(1);
} // fail

static int pipeReady(PipeType fd)
{
	DWORD avail = 0;
	return (PeekNamedPipe(fd, NULL, 0, NULL, &avail, NULL) && (avail > 0));
} /* pipeReady */

static bool writePipe(PipeType fd, const void *buf, const unsigned int _len)
{
	const DWORD len = (DWORD)_len;
	DWORD bw = 0;
	return ((WriteFile(fd, buf, len, &bw, NULL) != 0) && (bw == len));
} // writePipe

static int readPipe(PipeType fd, void *buf, const unsigned int _len)
{
	DWORD avail = 0;
	PeekNamedPipe(fd, NULL, 0, NULL, &avail, NULL);
	if (avail < _len)
		return 0;
	
	const DWORD len = (DWORD)_len;
	DWORD br = 0;

	return ReadFile(fd, buf, len, &br, NULL) ? (int)br : 0;
} // readPipe

static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
	PipeType *pPipeChildRead, PipeType *pPipeChildWrite)
{
	SECURITY_ATTRIBUTES pipeAttr;

	pipeAttr.nLength = sizeof(pipeAttr);
	pipeAttr.lpSecurityDescriptor = NULL;
	pipeAttr.bInheritHandle = TRUE;
	if (!CreatePipe(pPipeParentRead, pPipeChildWrite, &pipeAttr, 0))
		return 0;

	pipeAttr.nLength = sizeof(pipeAttr);
	pipeAttr.lpSecurityDescriptor = NULL;
	pipeAttr.bInheritHandle = TRUE;
	if (!CreatePipe(pPipeChildRead, pPipeParentWrite, &pipeAttr, 0))
	{
		CloseHandle(*pPipeParentRead);
		CloseHandle(*pPipeChildWrite);
		return 0;
	} // if

	return 1;
} // createPipes

static void closePipe(PipeType fd)
{
	CloseHandle(fd);
} // closePipe

static bool setEnvVar(const char *key, const char *val)
{
	
	return (SetEnvironmentVariableA(key, val) != 0);
} // setEnvVar

static bool launchChild(ProcessType *pid, std::string argStr)
{
	STARTUPINFO info = { sizeof(info) };

	std::string buf(".\\");
	buf.append(EXECUTABLE_NAME);

	char argbuf[MAX_STRING];
	strcpy(argbuf, argStr.c_str());


	return (CreateProcessA(buf.c_str(),
		argbuf, NULL, NULL, TRUE, 0, NULL,
		NULL, &info, pid) != 0);
} // launchChild

static int closeProcess(ProcessType *pid)
{
	CloseHandle(pid->hProcess);
	CloseHandle(pid->hThread);
	return 0;
} // closeProcess

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	mainline(__argc, __argv);
	ExitProcess(0);
	return 0;  // just in case.
} // WinMain


#else  // everyone else that isn't Windows.

static void fail(const char *err)
{
	// !!! FIXME: zenity or something.
	fprintf(stderr, "%s\n", err);
	_exit(1);
} // fail

static int pipeReady(PipeType fd)
{
	int rc;
	struct pollfd pfd = { fd, POLLIN | POLLERR | POLLHUP, 0 };
	while (((rc = poll(&pfd, 1, 0)) == -1) && (errno == EINTR)) { /*spin*/ }
	return (rc == 1);
} /* pipeReady */

static bool writePipe(PipeType fd, const void *buf, const unsigned int _len)
{
	const ssize_t len = (ssize_t)_len;
	ssize_t bw;
	while (((bw = write(fd, buf, len)) == -1) && (errno == EINTR)) { /*spin*/ }
	return (bw == len);
} // writePipe

static int readPipe(PipeType fd, void *buf, const unsigned int _len)
{
	if (!pipeReady(fd))
		return 0;

	return read(fd, buf, (ssize_t)_len);
	//const ssize_t len = (ssize_t)_len;
	//ssize_t br;
	//while (((br = read(fd, buf, len)) == -1) && (errno == EINTR)) { /*spin*/ }
	//return (int)br == -1 ? (int)br : 0;
} // readPipe

static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
	PipeType *pPipeChildRead, PipeType *pPipeChildWrite)
{
	int fds[2];
	if (pipe(fds) == -1)
		return 0;
	*pPipeParentRead = fds[0];
	*pPipeChildWrite = fds[1];

	if (pipe(fds) == -1)
	{
		close(*pPipeParentRead);
		close(*pPipeChildWrite);
		return 0;
	} // if

	*pPipeChildRead = fds[0];
	*pPipeParentWrite = fds[1];

	return 1;
} // createPipes

static void closePipe(PipeType fd)
{
	close(fd);
} // closePipe

static bool setEnvVar(const char *key, const char *val)
{
	return (setenv(key, val, 1) != -1);
} // setEnvVar

static int GArgc = 0;
static char **GArgv = NULL;

static bool launchChild(ProcessType *pid, std::string argStr)
{
	*pid = fork();
	if (*pid == -1)   // failed
		return false;
	else if (*pid != 0)  // we're the parent
		return true;  // we'll let the pipe fail if this didn't work.

	std::string buf("./");
	buf.append(EXECUTABLE_NAME);

	GArgv[1] = (char*)argStr.c_str();

	// we're the child.
	GArgv[0] = strdup(buf.c_str());
	execvp(GArgv[0], GArgv);
	// still here? It failed! Terminate, closing child's ends of the pipes.
	_exit(1);
} // launchChild

static int closeProcess(ProcessType *pid)
{
	int rc = 0;
	while ((waitpid(*pid, &rc, 0) == -1) && (errno == EINTR)) { /*spin*/ }
	if (!WIFEXITED(rc))
		return 1;  // oh well.
	return WEXITSTATUS(rc);
} // closeProcess

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);
	GArgc = argc;
	GArgv = argv;
	return mainline(argc, argv);
} // main

#endif

void(*func_readarray[CL_MAX])();

namespace fs = std::filesystem;
using namespace std;

string dirGame;
string dirSteamTemp;
char steam_UserName[MAX_STRING];
CSteamID steam_LocalID;


string convertToString(char* a, int size)
{
	int i;
	string s = "";
	for (i = 0; i < size; i++) {
		s = s + a[i];
	}
	return s;
}


typedef struct
{
	unsigned char	data[MAX_BUFFSIZE];
	int				cursize;
} pipebuff_t;

pipebuff_t pipeSendBuffer;


int PIPE_SendData(void)
{
	unsigned long bytes_written = 0;
	int succ = 0;
	succ = writePipe(pipeParentWrite, pipeSendBuffer.data, pipeSendBuffer.cursize);
	pipeSendBuffer.cursize = 0;

	return succ;
}






float PIPE_ReadFloat(void)
{
	float dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 4, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 4);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


uint32_t PIPE_ReadLong(void)
{
	int32_t dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 4, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 4);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


uint64_t PIPE_ReadLongLong(void)
{
	int64_t dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 8, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 8);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


int16_t PIPE_ReadShort(void)
{
	int16_t dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 2, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 2);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


uint8_t PIPE_ReadByte(void)
{
	uint8_t dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 1, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 1);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


int PIPE_ReadString(char *buff)
{
	unsigned long amount_written;

	int i;
	for (i = 0; i < MAX_STRING; i++)
	{
#if 0
		ReadFile(hPipeServer, buff + i, 1, NULL, NULL);
#endif
		readPipe(pipeParentRead, buff + i, 1);
		if (buff[i] == 0)
			break;
	}
	amount_written = i;

	/*
	cout << "Pipe: " << to_string(succ) << "\n";
	if (!succ)
	{
		cout << "PIPE CLOGGED: " << to_string(GetLastError()) << "\n";
		return 0;
	}
	*/

	return amount_written;
}


void PIPE_ReadCharArray(char *into, uint32_t *size)
{
	*size = (unsigned long)PIPE_ReadShort();
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, into, *size, size, NULL);
#endif
	succ = readPipe(pipeParentRead, into, *size);
	*size = (unsigned long)succ;
	if (!succ)
	{
		*size = 0;
	}
}






int PIPE_WriteFloat(float dat_float)
{
	uint32_t dat;
	memcpy(&dat, &dat_float, sizeof(uint32_t));

	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat & 0xFF;
	pipeSendBuffer.data[seek + 1] = (dat >> 8) & 0xFF;
	pipeSendBuffer.data[seek + 2] = (dat >> 16) & 0xFF;
	pipeSendBuffer.data[seek + 3] = (dat >> 24) & 0xFF;

	pipeSendBuffer.cursize += 4;

	return true;
}


int PIPE_WriteLong(uint32_t dat)
{
	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat & 0xFF;
	pipeSendBuffer.data[seek + 1] = (dat >> 8) & 0xFF;
	pipeSendBuffer.data[seek + 2] = (dat >> 16) & 0xFF;
	pipeSendBuffer.data[seek + 3] = (dat >> 24) & 0xFF;

	pipeSendBuffer.cursize += 4;

	return true;
}


int PIPE_WriteLongLong(uint64_t dat)
{
	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat & 0xFF;
	pipeSendBuffer.data[seek + 1] = (dat >> 8) & 0xFF;
	pipeSendBuffer.data[seek + 2] = (dat >> 16) & 0xFF;
	pipeSendBuffer.data[seek + 3] = (dat >> 24) & 0xFF;
	pipeSendBuffer.data[seek + 4] = (dat >> 32) & 0xFF;
	pipeSendBuffer.data[seek + 5] = (dat >> 40) & 0xFF;
	pipeSendBuffer.data[seek + 6] = (dat >> 48) & 0xFF;
	pipeSendBuffer.data[seek + 7] = (dat >> 56) & 0xFF;

	pipeSendBuffer.cursize += 8;

	return true;
}


int PIPE_WriteShort(int16_t dat)
{
	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat & 0xFF;
	pipeSendBuffer.data[seek + 1] = (dat >> 8) & 0xFF;

	pipeSendBuffer.cursize += 2;

	return true;
}


int PIPE_WriteByte(uint8_t dat)
{
	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat;
	pipeSendBuffer.cursize += 1;

	return true;
}


int PIPE_WriteString(char* str)
{
	int str_length = strlen(str);
	memcpy(&(pipeSendBuffer.data[pipeSendBuffer.cursize]), str, str_length);

	if (str[str_length - 1] != 0)
		pipeSendBuffer.data[pipeSendBuffer.cursize + str_length] = 0; str_length++;

	pipeSendBuffer.cursize += str_length;

	return true;
}


int PIPE_WriteCharArray(char *dat, uint32_t size)
{
	PIPE_WriteShort((int16_t)size);

	int seek = pipeSendBuffer.cursize;
	memcpy(&(pipeSendBuffer.data[seek]), dat, size);
	pipeSendBuffer.cursize += size;

	return true;
}


void Con_Print(const char *dat)
{
#ifdef WIN32
	PIPE_WriteByte(SV_PRINT);
	PIPE_WriteString((char*)dat);
#else
	cout << dat;
#endif
}





void Steam_Handshake(void)
{
	PIPE_WriteByte(SV_STEAMID);
	PIPE_WriteString((char*)(to_string(steam_LocalID.ConvertToUint64())).c_str());

	if (is_server)
		return;

	PIPE_WriteByte(SV_SETNAME);
	PIPE_WriteString(steam_UserName);
}


HAuthTicket myAuthTicket;
void Steam_Auth_Fetch(void)
{
	Con_Print("auth fetch request!");
	Con_Print("\n");

	char authToken[1024];
	uint32 authLength;
	myAuthTicket = SteamUser()->GetAuthSessionTicket(authToken, sizeof(authToken), &authLength, NULL);


	Con_Print("auth generated");
	Con_Print(to_string(myAuthTicket).c_str());
	Con_Print("\n");


	cout << "auth generated: " << to_string(myAuthTicket) << endl;
	cout << "MD5: " << md5(convertToString(authToken, authLength)) << endl;

	Con_Print("auth generated length: ");
	Con_Print(to_string(authLength).c_str());
	Con_Print("\n");

	Con_Print("auth generated MD5: ");
	Con_Print(md5(convertToString(authToken, authLength)).c_str());
	Con_Print("\n");


	PIPE_WriteByte(SV_AUTH_RETRIEVED);
	PIPE_WriteCharArray(authToken, authLength);

	/*
	if (fs::is_directory(dirSteamTemp) == FALSE)
		fs::create_directories(dirSteamTemp);

	if (fs::is_regular_file(dirSteamTemp + "authtoken"))
		fs::remove(dirSteamTemp + "authtoken");

	ofstream authfile;
	authfile.open(dirSteamTemp + "authtoken");
	authfile.write(authToken, authLength);
	authfile.close();
	*/

	cout << "sending auth ticket of length " << to_string(authLength) << endl;
}


void Steam_Auth_Validate(void)
{
	int entnum = PIPE_ReadByte();

	char steamIDChar[MAX_STRING];
	int steamIDLength = PIPE_ReadString(steamIDChar);

	Steam_NewClient(steamIDChar, entnum);
	cout << "steam id length: " << to_string(steamIDLength) << endl;

	char authToken[1024];
	uint32_t authSize = 0;
	PIPE_ReadCharArray(authToken, &authSize);



	cout << "auth token length: " << to_string(authSize) << endl;
	cout << "MD5: " << md5(convertToString(authToken, authSize)) << endl;

	Con_Print("auth validate length: ");
	Con_Print(to_string(authSize).c_str());
	Con_Print("\n");

	Con_Print("auth validate MD5: ");
	Con_Print(md5(convertToString(authToken, authSize)).c_str());
	Con_Print("\n");


	CSteamID steamID;
	uint64_t intID = stoull(convertToString(steamIDChar, steamIDLength));
	steamID.SetFromUint64(intID);

	int result;
	if (is_server)
		result = SteamGameServer()->BeginAuthSession(authToken, authSize, steamID);
	else
		result = SteamUser()->BeginAuthSession(authToken, authSize, steamID);
	cout << "Auth session for " << convertToString(steamIDChar, steamIDLength) << ": " << to_string(result) << endl;

	Con_Print("Auth session for ");
	Con_Print(convertToString(steamIDChar, steamIDLength).c_str());
	Con_Print(": ");
	Con_Print(to_string(result).c_str());
	Con_Print("\n");


	/*
	if (result == k_EBeginAuthSessionResultOK)
	{
		//PIPE_WriteByte(SV_AUTH_VALIDATED);
		//PIPE_WriteString(steamIDChar);
		//Steam_AuthAccepted(steamID);
	}
	*/
}




void Steam_Cleanup(void)
{
	//if (fs::is_regular_file(dirSteamTemp + "authtoken"))
	//	fs::remove(dirSteamTemp + "authtoken");

	if (fs::is_directory(dirSteamTemp) && !fs::is_empty(dirSteamTemp))
	{
		fs::remove_all(dirSteamTemp);
	}
}


void Steam_Init(void)
{
	func_readarray[CL_HANDSHAKE] = Steam_Handshake;
	func_readarray[CL_STARTSERVER] = Steam_StartServer;
	func_readarray[CL_CONNECTSERVER] = Steam_ConnectToServer;
	func_readarray[CL_DISCONNECTSERVER] = Steam_Disconnect;
	func_readarray[CL_AUTH_FETCH] = Steam_Auth_Fetch;
	func_readarray[CL_AUTH_VALIDATE] = Steam_Auth_Validate;
	func_readarray[CL_PLAYINGWITH] = Steam_PlayingWith;

	func_readarray[CL_INV_SENDITEMS] = Steam_SendInventory;
	func_readarray[CL_INV_GETPROPERTY] = Steam_GetProperty;
	func_readarray[CL_INV_GETINSTANCEPROPERTY] = Steam_GetInstanceProperty;
	func_readarray[CL_INV_UPDATECLIENT] = Steam_UpdateClient;
	func_readarray[CL_INV_BUILDSERIAL] = Steam_RequestedSerial;
	func_readarray[CL_INV_CLIENTLOADOUT] = Steam_GetClientLoadout;
	func_readarray[CL_INV_GRANTITEM] = Steam_GrantItem;
	func_readarray[CL_INV_TIMEDROP] = Steam_PlaytimeDrop;
}


void processCommands(void)
{
	while (true)
	{
#ifndef _WIN32
		/*
		if (gameProcess > 0)
		{
			char nChar;
			while (read(hPipeServer[HPIPE_SERVER_READ], &nChar, 1) == 1) {
				write(STDOUT_FILENO, &nChar, 1);
			}
		}
		*/
#endif

		if (is_server)
			SteamGameServer_RunCallbacks();

		SteamAPI_RunCallbacks();
		//Con_Print("SteamAPI_RunCallbacks\n");
		//cout << "processCommands tick" << endl;

		unsigned char index = PIPE_ReadByte();
		while (index != 255)
		{
			cout << "reading " << to_string(index) << endl;

			if (index < CL_MAX)
			{
				//Con_Print("reading client command ");
				//Con_Print(to_string(index).c_str());
				//Con_Print("\n");
				//cout << "reading client command " << to_string(index) << endl;
				func_readarray[index]();
			}

			index = PIPE_ReadByte();
		}

		if (pipeSendBuffer.cursize)
			PIPE_SendData();

		//cout << "tick" << endl;

#if 0
		if (!is_server)
		{
			InventoryService.inventoryUpdateTimer--;
			if (InventoryService.inventoryUpdateTimer <= 0)
			{
				InventoryService.inventoryUpdateTimer = 3000;

				SteamInventoryResult_t resultHandle;
				SteamGetInventory()->GetAllItems(&resultHandle);
			}
		}
#endif


#ifdef _WIN32
		if (!WaitForSingleObject(childPid.hProcess, 0))
		{
			Steam_Cleanup();
			exit(0);
		}

		Sleep(100);
#else
		int status;
		waitpid(childPid, &status, WNOHANG);

		if (!status)
		{
			Steam_Cleanup();
			exit(0);
		}


		sleep(1);
#endif
	}
}


static bool setEnvironmentVars(PipeType readpipe, PipeType writepipe)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) readpipe);
	if (!setEnvVar("STEAMSHIM_READHANDLE", buf))
		return false;

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) writepipe);
	if (!setEnvVar("STEAMSHIM_WRITEHANDLE", buf))
		return false;

	return true;
} // setEnvironmentVars


CNetworkingSockets NetworkingSockets;
CInventoryService InventoryService;

ISteamInventory *SteamGetInventory(void)
{
	return is_server ? SteamGameServerInventory() : SteamInventory();
}

static int mainline(int argc, char **argv)
{
	char servertoken[MAX_STRING];
	servertoken[0] = 0;
	pipeParentRead = NULLPIPE;
	pipeParentWrite = NULLPIPE;
	pipeChildRead = NULLPIPE;
	pipeChildWrite = NULLPIPE;

	dbgpipe("Parent starting mainline.\n");

	if (!createPipes(&pipeParentRead, &pipeParentWrite, &pipeChildRead, &pipeChildWrite))
		fail("Failed to create application pipes");
	else if (!setEnvironmentVars(pipeChildRead, pipeChildWrite))
		fail("Failed to set environment variables");
#if 0
	else if (!initSteamworks(pipeParentWrite))
		fail("Failed to initialize Steamworks");
#endif

	cout << "Launching Midnight Guns..." << "\n";
	dirGame = fs::current_path().string();
	dirSteamTemp = dirGame + "/mguns/data/_STEAMTEMP/";

	stringstream argStr;
#ifdef _WIN32
	strcpy(EXECUTABLE_NAME, "fteqw64.exe");
#else
	strcpy(EXECUTABLE_NAME, "fteqw64");
#endif


	int i;
#ifdef _WIN32
	i = 0;
#else
	i = 1;
#endif
	for (; i < argc; i++)
	{
		if (!strcmp(argv[i], "-server"))
		{
			is_server = true;
#ifdef _WIN32
			strcpy(EXECUTABLE_NAME, "fteqwsv64.exe");
#else
			strcpy(EXECUTABLE_NAME, "fteqw-sv64");
#endif
			char str[MAX_STRING];
			strcpy(str, argv[i + 1]);
			
			if (str[0] != '+' && str[0] != '-')
			{
				strcpy(servertoken, str);
				Con_Print("Server detected: ");
				Con_Print(servertoken);
				Con_Print("\n");
				i++;
			}
			else
			{
				Con_Print("Server detected: anonymous\n");
			}
		}
		else
		{
#ifdef _WIN32
			if (i == 0)
#else
			if (i == 1)
#endif
				argStr << " \"" << argv[i] << "\"";
			else
				argStr << " " << argv[i];
		}
	}

	argStr << " +plug_load steam ";


	int should_close = true;
	if (!is_server)
	{
		should_close = !SteamAPI_Init();
		cout << "SteamAPI_Init()" << endl;
	}
	else
	{
		should_close = !SteamGameServer_Init(0, 0, 0, eServerModeAuthentication, "midnightguns");
		cout << "SteamGameServer_Init()" << endl;

		SteamGameServer()->SetModDir("MidnightGuns");
		SteamGameServer()->SetProduct("1877600");
		SteamGameServer()->SetGameDescription("Midnight Guns");

		SteamGameServer()->SetDedicatedServer(true);
		if (servertoken[0] == 0)
		{
			SteamGameServer()->LogOnAnonymous();
		}
		else
		{
			Con_Print("logged in\n");
			SteamGameServer()->LogOn(servertoken);
		}
	}

	if (!launchChild(&childPid, argStr.str()))
	{
		char errmsg[128];
#ifdef _WIN32
		sprintf(errmsg, "Failed to launch application: %i", GetLastError());
#endif

		fail(errmsg);
	}
	//fail("Failed to launch application %i");
	
#if 0
	if (should_close)
	{
		Con_Print("^1ERROR: SteamAPI_Init failed!\n");
		return -1;
	}
#endif

// Close the ends of the pipes that the child will use; we don't need them.
	closePipe(pipeChildRead);
	closePipe(pipeChildWrite);
	pipeChildRead = pipeChildWrite = NULLPIPE;


	Steam_Init();

	if (is_server)
	{
		steam_LocalID = SteamGameServer()->GetSteamID();
	}
	else
	{
		strcpy(steam_UserName, SteamFriends()->GetPersonaName());
		steam_LocalID = SteamUser()->GetSteamID();
	}


	PIPE_WriteByte(SV_STEAMID);
	PIPE_WriteString((char*)(to_string(steam_LocalID.ConvertToUint64())).c_str());

	//Con_Print("SteamGetInventory()->LoadItemDefinitions(): ");
	//Con_Print(to_string(SteamGetInventory()->LoadItemDefinitions()).c_str());
	//Con_Print("\n");
	if (!is_server) // fetch client inventory
	{
		// fetch initial inventory
		InventoryService.itemArray = NULL;
		InventoryService.itemArraySize = 0;
		SteamInventoryResult_t resultHandle;
		SteamInventory()->GetAllItems(&resultHandle);
		memset(InventoryService.clientInventory, k_SteamInventoryResultInvalid, sizeof(InventoryService.clientInventory));
		InventoryService.inventoryUpdateTimer = 600;
		//
	}


	dbgpipe("Parent in command processing loop.\n");
	// Now, we block for instructions until the pipe fails (child closed it or
	//  terminated/crashed).
	//processCommands(pipeParentRead, pipeParentWrite);
	processCommands();

	dbgpipe("Parent shutting down.\n");

	// Close our ends of the pipes.
	//writeBye(pipeParentWrite);
	closePipe(pipeParentRead);
	closePipe(pipeParentWrite);

	//deinitSteamworks();

	dbgpipe("Parent waiting on child process.\n");

	// Wait for the child to terminate, close the child process handles.
	const int retval = closeProcess(&childPid);

	dbgpipe("Parent exiting mainline (child exit code %d).\n", retval);

	return retval;
} // mainline
