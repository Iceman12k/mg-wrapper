#pragma once

#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"


#include <future>
#include <fstream> 
#include <sstream>
#include <filesystem>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_CLIENTS 256


extern CSteamID steam_LocalID;
extern bool is_server;
extern HAuthTicket myAuthTicket;
extern std::string dirSteamTemp;

class CNetworkingSockets
{
	private:
		STEAM_GAMESERVER_CALLBACK(CNetworkingSockets, OnStatusChanged_Server, SteamNetConnectionStatusChangedCallback_t);
		STEAM_GAMESERVER_CALLBACK(CNetworkingSockets, OnSteamServerConnected, SteamServersConnected_t);
		STEAM_GAMESERVER_CALLBACK(CNetworkingSockets, ValidateAuthTicketResponse, ValidateAuthTicketResponse_t);

		STEAM_GAMESERVER_CALLBACK(CNetworkingSockets, ServerConnectFailure, SteamServerConnectFailure_t);
		STEAM_GAMESERVER_CALLBACK(CNetworkingSockets, ServerConnected, SteamServersConnected_t);
		STEAM_GAMESERVER_CALLBACK(CNetworkingSockets, ServerDisconnected, SteamServersDisconnected_t);

		STEAM_CALLBACK(CNetworkingSockets, ValidateAuthTicketResponse_Listen, ValidateAuthTicketResponse_t);
		STEAM_CALLBACK(CNetworkingSockets, OnStatusChanged, SteamNetConnectionStatusChangedCallback_t);
		STEAM_CALLBACK(CNetworkingSockets, GetAuthSessionTicketResponse, GetAuthSessionTicketResponse_t);
		STEAM_CALLBACK(CNetworkingSockets, OnAvatarImageLoaded, AvatarImageLoaded_t);

	public:
		static void getAvatar(CSteamID avatar_steamID);
};


class CInventoryService
{
	private:
		STEAM_GAMESERVER_CALLBACK(CInventoryService, Inventory_ResultReady_Server, SteamInventoryResultReady_t);
		STEAM_GAMESERVER_CALLBACK(CInventoryService, Inventory_DefinitionUpdate_Server, SteamInventoryDefinitionUpdate_t);

		STEAM_CALLBACK(CInventoryService, Inventory_FullUpdate, SteamInventoryFullUpdate_t);
		STEAM_CALLBACK(CInventoryService, Inventory_ResultReady, SteamInventoryResultReady_t);
		STEAM_CALLBACK(CInventoryService, Inventory_DefinitionUpdate, SteamInventoryDefinitionUpdate_t);

	public:
		bool fullupdateincoming = false;
		typedef struct propertywaiting_s
		{
			uint32 itemid;
			char name[MAX_STRING];
			struct propertywaiting_s *next;
		} propertywaiting_t;
		propertywaiting_t	*propertyQueueList;
		typedef struct loadoutwaiting_s
		{
			int clientNum;
			struct loadoutwaiting_s *next;
		} loadoutwaiting_t;
		loadoutwaiting_t	*loadoutQueueList;
		typedef struct triggerdropwaiting_s
		{
			SteamInventoryResult_t resultHandle;
			struct triggerdropwaiting_s *next;
		} triggerdropwaiting_t;
		triggerdropwaiting_t	*triggerdropQueueList;

		SteamItemDetails_t	*itemArray;
		uint32				itemArraySize;

		int					inventoryUpdateTimer;
		SteamInventoryResult_t clientInventory[MAX_CLIENTS];
		SteamInventoryResult_t waitingtoSerialize;
		SteamItemInstanceID_t myLoadout[16];
		uint32 myLoadoutSize;
};

extern CNetworkingSockets NetworkingSockets;
extern CInventoryService InventoryService;


void Steam_AuthAccepted(CSteamID steamid);
void Steam_Handshake(void);
void Steam_StartServer(void);
void Steam_ConnectToServer(void);
void Steam_Disconnect(void);
void Steam_NewClient(char *dat, int numb);
void Steam_PlayingWith(void);
void Steam_SendInventory(void);
void Steam_GetProperty(void);
void Steam_GetInstanceProperty(void);
void Steam_UpdateClient(void);
void Steam_RequestedSerial(void);
void Steam_GetClientLoadout(void);
void Steam_GrantItem(void);
void Steam_PlaytimeDrop(void);
void Steam_RichPresence(void);
void Steam_GrantPromoItems(void);
void Con_Print(const char *msg);
ISteamInventory *SteamGetInventory(void);

