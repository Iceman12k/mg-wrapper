

#include <iostream>
#include <cstdint>

#include "fte_steam.h"
#include "st_common.h"
#include "toojpeg.h"

namespace fs = std::filesystem;
using namespace std;



HSteamListenSocket	sockServer;
HSteamNetConnection	connClient;


typedef struct clientconn_s
{
	int					entnum;
	CSteamID			steamid;
	HSteamNetConnection sock;

	//struct clientconn_s *next;
} clientconn_t;
clientconn_t clientList[MAX_CLIENTS];

/*
clientconn_t *ClientList;
clientconn_t *ClientSetup()
{
	clientconn_t *new_client = new clientconn_t;
	//new_client->sock = netcon;
	new_client->next = ClientList;

	ClientList = new_client;
	return new_client;
}

void ClientCleanupList()
{
	clientconn_t *hold;
	clientconn_t *lst;
	for (lst = ClientList; lst != NULL; lst = hold)
	{
		//if (is_server)
		//	SteamGameServerNetworkingSockets()->CloseConnection(lst->sock, 0, NULL, false);
		//else
		//	SteamNetworkingSockets()->CloseConnection(lst->sock, 0, NULL, false);
		if (is_server)
			SteamGameServer()->EndAuthSession(lst->steamid);
		else
			SteamUser()->EndAuthSession(lst->steamid);

		hold = lst->next;
		free(lst);
	}

	ClientList = NULL;
}
*/

void ClientCleanup(clientconn_t *client)
{
	if (!client)
		return;

	if (is_server)
		SteamGameServer()->EndAuthSession(client->steamid);
	else
		SteamUser()->EndAuthSession(client->steamid);

	memset(client, 0, sizeof(clientconn_t));
}


void Steam_NewClient(char *dat, int numb)
{
	unsigned long long intID = stoull(dat);

	clientconn_t *conn = &clientList[numb];
	if (conn->entnum)
	{
		ClientCleanup(conn);
	}

	conn->steamid.SetFromUint64(intID);
	conn->entnum = numb;
}




void CNetworkingSockets::OnStatusChanged(SteamNetConnectionStatusChangedCallback_t *pCallback)
{
	/*
	cout << "Connection Status Change: ";
	cout << pCallback->m_info.m_eState << "\n";

	if (pCallback->m_eOldState == k_ESteamNetworkingConnectionState_None)
	{
		cout << "New connection!" << "\n";

		if (SteamNetworkingSockets()->AcceptConnection(pCallback->m_hConn))
		{
			clientconn_t *conn = ClientSetup(pCallback->m_hConn);
			conn->steamid = pCallback->m_info.m_identityRemote.GetSteamID();
		}
	}
	*/
}


void CNetworkingSockets::OnStatusChanged_Server(SteamNetConnectionStatusChangedCallback_t *pCallback)
{
	/*
	cout << "Server Connection Status Change: ";
	cout << pCallback->m_info.m_eState << "\n";

	if (pCallback->m_eOldState == k_ESteamNetworkingConnectionState_None)
	{
		cout << "New connection!" << "\n";
		if (SteamGameServerNetworkingSockets()->AcceptConnection(pCallback->m_hConn))
		{
			clientconn_t *conn = ClientSetup(pCallback->m_hConn);
			conn->steamid = pCallback->m_info.m_identityRemote.GetSteamID();
		}
	}
	*/
}


void CNetworkingSockets::OnSteamServerConnected(SteamServersConnected_t *pCallback)
{
	
	cout << "Steam server connected" << endl;
}


void CNetworkingSockets::ServerConnectFailure(SteamServerConnectFailure_t *pCallback)
{
	Con_Print("Steam: ServerConnectFailure - ");
	Con_Print(to_string(pCallback->m_eResult).c_str());
	Con_Print("\n");
}


void CNetworkingSockets::ServerConnected(SteamServersConnected_t *pCallback)
{
	Con_Print("Steam: ServerConnected\n");
	SteamGameServerInventory()->LoadItemDefinitions();
}


void CNetworkingSockets::ServerDisconnected(SteamServersDisconnected_t *pCallback)
{
	Con_Print("Steam: ServersDisconnected - ");
	Con_Print(to_string(pCallback->m_eResult).c_str());
	Con_Print("\n");
}


void CNetworkingSockets::GetAuthSessionTicketResponse(GetAuthSessionTicketResponse_t *pCallback)
{
	cout << "GetAuthSessionTicketResponse: " << pCallback->m_eResult << endl;

	//char authToken[1024];
	//uint32 authLength;
	myAuthTicket = pCallback->m_hAuthTicket;
}



void Steam_AuthAccepted(CSteamID steamid)
{
	cout << "auth passed: " << to_string(steamid.ConvertToUint64()) << endl;
	unsigned long long steamid_uint64 = steamid.ConvertToUint64();

	int entnum = 0;
	clientconn_t *lst;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		lst = &clientList[i];
		if (steamid == lst->steamid)
		{
			cout << "auth passed for real" << endl;
			entnum = lst->entnum;
			break;
		}
	}

	string steamid_str = to_string(steamid.ConvertToUint64());
	PIPE_WriteByte(SV_AUTH_VALIDATED);
	PIPE_WriteByte(entnum);
	PIPE_WriteString(steamid_str.c_str(), strlen(steamid_str.c_str()));
}


void CNetworkingSockets::ValidateAuthTicketResponse(ValidateAuthTicketResponse_t *pCallback)
{
	cout << "ValidateAuthTicketResponse: " << pCallback->m_eAuthSessionResponse << endl;

	if (pCallback->m_eAuthSessionResponse == k_EAuthSessionResponseOK)
	{
		Steam_AuthAccepted(pCallback->m_SteamID);
	}
}


void CNetworkingSockets::ValidateAuthTicketResponse_Listen(ValidateAuthTicketResponse_t *pCallback)
{
	cout << "ValidateAuthTicketResponse: " << pCallback->m_eAuthSessionResponse << endl;

	if (pCallback->m_eAuthSessionResponse == k_EAuthSessionResponseOK)
	{
		Steam_AuthAccepted(pCallback->m_SteamID);
	}
}




///*
ofstream jpegFile;
void jpeg_writebyte(unsigned char oneByte)
{
	jpegFile << oneByte;
}


void transferToEngine(int imageHandle, string path, string filename)
{
	if (!fs::is_directory(path))
		fs::create_directories(path);

	if (fs::is_regular_file(path + filename))
		fs::remove(path + filename);


	uint32 avatarWidth, avatarHeight;
	bool success = SteamUtils()->GetImageSize(imageHandle, &avatarWidth, &avatarHeight);
	if (!success)
	{
		cout << "Steam avatar system made a fucky wucky getting image size" << endl;
		return;
	}

	// 4 bytes per pixel please sir
	int dataSize = avatarWidth * avatarHeight * 4;

	uint8 *avatarRGB = new uint8[dataSize];
	success = SteamUtils()->GetImageRGBA(imageHandle, avatarRGB, dataSize);

	cout << "saving avatar: " << path << filename << endl;


	jpegFile.open(path + filename, std::ios_base::out | std::ios_base::binary);
	TooJpeg::writeJpeg(jpeg_writebyte, avatarRGB, avatarWidth, avatarHeight, true, 90, false);
	jpegFile.close();
}
//*/


void Steam_SaveAvatar(int imageIndex, CSteamID avatar_steamID)
{
	string steamid_str = (to_string(avatar_steamID.ConvertToUint64()));
	PIPE_WriteByte(SV_AVATAR_FETCHED);
	PIPE_WriteString(steamid_str.c_str(), strlen(steamid_str.c_str()));

	string path = dirSteamTemp + ((to_string(avatar_steamID.ConvertToUint64())).c_str()) + "/";
	string filename;

	uint32 width;
	uint32 height;
	SteamUtils()->GetImageSize(imageIndex, &width, &height);

	if (width >= 128)
	{
		filename = "avatarLarge.jpg";
		PIPE_WriteByte(2);
	}
	else if (width >= 64)
	{
		filename = "avatarMedium.jpg";
		PIPE_WriteByte(1);
	}
	else
	{
		filename = "avatarSmall.jpg";
		PIPE_WriteByte(0);
	}

	transferToEngine(imageIndex, path, filename);
}





void CNetworkingSockets::getAvatar(CSteamID avatar_steamID)
{
	int avstatus;

	avstatus = SteamFriends()->GetLargeFriendAvatar(avatar_steamID);
	if (avstatus)
		Steam_SaveAvatar(avstatus, avatar_steamID);

	avstatus = SteamFriends()->GetMediumFriendAvatar(avatar_steamID);
	if (avstatus)
		Steam_SaveAvatar(avstatus, avatar_steamID);
	
	avstatus = SteamFriends()->GetSmallFriendAvatar(avatar_steamID);
	if (avstatus)
		Steam_SaveAvatar(avstatus, avatar_steamID);
}


void CNetworkingSockets::OnAvatarImageLoaded(AvatarImageLoaded_t* pCallback)
{
	Steam_SaveAvatar(pCallback->m_iImage, pCallback->m_steamID);
}






/*
==STEAM INVENTORY========================
All code pertaining to Steam Inventory Service
=========================================
*/
void Inventory_UpdateClientLoadout(int clientNum);
void CInventoryService::Inventory_FullUpdate(SteamInventoryFullUpdate_t* pCallback)
{
	//Con_Print("^xf00^binventory updato\n");
	fullupdateincoming = true;
}

void CInventoryService::Inventory_ResultReady(SteamInventoryResultReady_t* pCallback)
{	
	Con_Print("SteamInventoryResultReady_t ");
	Con_Print(to_string(pCallback->m_handle).c_str());
	Con_Print("\n");
	if (fullupdateincoming)
	{
		uint32 old_arraySize = itemArraySize;
		if (SteamInventory()->GetResultItems(pCallback->m_handle, NULL, &itemArraySize) && itemArraySize > 0)
		{
			if (itemArray != NULL)
			{
				free(itemArray);
				itemArray = NULL;
			}

			SteamItemDetails_t *item;
			itemArray = (SteamItemDetails_t *)malloc(sizeof(SteamItemDetails_t) * itemArraySize);
			SteamInventory()->GetResultItems(pCallback->m_handle, itemArray, &itemArraySize);
			Con_Print("GetResultStatus: ");
			Con_Print(to_string(SteamInventory()->GetResultStatus(pCallback->m_handle)).c_str());
			Con_Print("\n");

			for (uint32 j = 0; j < itemArraySize; j++)
			{
				item = &itemArray[j];

				Con_Print("^xf0fItem #");
				Con_Print(to_string(j).c_str());
				Con_Print(": ");
				Con_Print(to_string(item->m_itemId).c_str());
				Con_Print("  ID");
				Con_Print(to_string(item->m_iDefinition).c_str());
				Con_Print("\n");
			}

			Steam_SendInventory();
		}

		SteamInventory()->DestroyResult(pCallback->m_handle);
	}
	else if (pCallback->m_handle == waitingtoSerialize)
	{
		char data[4096];
		uint32 datasize = sizeof(data);
		if (SteamInventory()->SerializeResult(waitingtoSerialize, data, &datasize))
		{
			PIPE_WriteByte(SV_INV_LOCALSERIAL);
			PIPE_WriteCharArray(data, datasize);
		}
		waitingtoSerialize = k_SteamInventoryResultInvalid;
		SteamInventory()->DestroyResult(pCallback->m_handle);
	}


	// check if it's one we're waiting on trigger drop for
	CInventoryService::triggerdropwaiting_t *tdhold = InventoryService.triggerdropQueueList;
	CInventoryService::triggerdropwaiting_t *tdlst = InventoryService.triggerdropQueueList;
	while (tdlst)
	{
		if (pCallback->m_handle == tdlst->resultHandle)
		{
			SteamItemDetails_t *item;
			SteamItemDetails_t newitemArray[MAX_LOADOUT];
			uint32 newitemArraySize = MAX_LOADOUT;
			SteamInventory()->GetResultItems(pCallback->m_handle, newitemArray, &newitemArraySize);

			if (newitemArraySize)
			{
				for (int i = 0; i < newitemArraySize; i++)
				{
					char itemName[MAX_STRING];
					uint32 size = MAX_STRING;
					item = &newitemArray[i];

					SteamInventory()->GetItemDefinitionProperty(item->m_iDefinition, "name", itemName, &size);
					itemName[size] = 0;

					PIPE_WriteByte(SV_INV_NEWITEM);
					PIPE_WriteLong(item->m_iDefinition);
					PIPE_WriteString(itemName, sizeof(itemName));
				}

				SteamItemDetails_t *itArray = (SteamItemDetails_t*)malloc(sizeof(SteamItemDetails_t) * (itemArraySize + newitemArraySize));
				memcpy(itArray, itemArray, sizeof(itemArray));
				memcpy(itArray + sizeof(itemArray), newitemArray, sizeof(SteamItemDetails_t) * newitemArraySize);
				itemArraySize = (itemArraySize + newitemArraySize);
				Steam_SendInventory();
			}
			else
			{
				Con_Print("Empty trigger drop\n");
			}

			SteamInventory()->DestroyResult(pCallback->m_handle);
			if (tdlst == InventoryService.triggerdropQueueList)
				InventoryService.triggerdropQueueList = tdlst->next;
			else
				tdhold->next = tdlst->next;
			free(tdlst);
			return;
		}

		tdhold = tdlst;
		tdlst = tdlst->next;
	}


	// check if it's one we're waiting on deserialize for.
	CInventoryService::loadoutwaiting_t *hold = InventoryService.loadoutQueueList;
	CInventoryService::loadoutwaiting_t *lst = InventoryService.loadoutQueueList;
	while (lst)
	{
		if (pCallback->m_handle == InventoryService.clientInventory[lst->clientNum])
		{
			Inventory_UpdateClientLoadout(lst->clientNum);

			SteamInventory()->DestroyResult(pCallback->m_handle);
			if (lst == InventoryService.loadoutQueueList)
				InventoryService.loadoutQueueList = lst->next;
			else
				hold->next = lst->next;
			free(lst);
			return;
		}

		hold = lst;
		lst = lst->next;
	}

	fullupdateincoming = false;
}

#if 1
void CInventoryService::Inventory_ResultReady_Server(SteamInventoryResultReady_t* pCallback)
{
	Con_Print("SteamInventoryResultReady_t ");
	Con_Print(to_string(pCallback->m_handle).c_str());
	Con_Print("\n");

	if (pCallback->m_handle == waitingtoSerialize)
	{
		char data[4096];
		uint32 datasize = sizeof(data);
		if (SteamGetInventory()->SerializeResult(waitingtoSerialize, data, &datasize))
		{
			PIPE_WriteByte(SV_INV_LOCALSERIAL);
			PIPE_WriteCharArray(data, datasize);
		}
		waitingtoSerialize = k_SteamInventoryResultInvalid;
	}
	
	// check if it's one we're waiting on deserialize for.
	CInventoryService::loadoutwaiting_t *hold = InventoryService.loadoutQueueList;
	CInventoryService::loadoutwaiting_t *lst = InventoryService.loadoutQueueList;
	while (lst)
	{
		if (pCallback->m_handle == InventoryService.clientInventory[lst->clientNum])
		{
			Inventory_UpdateClientLoadout(lst->clientNum);

			if (lst == InventoryService.loadoutQueueList)
				InventoryService.loadoutQueueList = lst->next;
			else
				hold->next = lst->next;
			free(lst);
			return;
		}

		hold = lst;
		lst = lst->next;
	}

	uint32 itemArraySize = MAX_LOADOUT;
	SteamGetInventory()->GetResultItems(pCallback->m_handle, NULL, &itemArraySize);
	if (itemArraySize == 0)
		SteamGetInventory()->DestroyResult(pCallback->m_handle);
}
#endif

void Steam_SendInventory(void) //CL_INV_SENDITEMS
{
	SteamItemDetails_t *item;
	PIPE_WriteByte(SV_INV_ITEMLIST);
	PIPE_WriteShort(InventoryService.itemArraySize);
	if (InventoryService.itemArraySize > 0)
	{
		for (uint32_t i = 0; i < InventoryService.itemArraySize; i++)
		{
			item = &InventoryService.itemArray[i];
			PIPE_WriteLongLong(item->m_itemId);
			PIPE_WriteLong(item->m_iDefinition);
		}
	}
}


void Steam_SendProperty(long int itemid, char *name, char *value)
{
	PIPE_WriteByte(SV_INV_PROPERTY);
	PIPE_WriteLong(itemid);
	PIPE_WriteString(name, strlen(name));
	PIPE_WriteString(value, strlen(value));
}


void CInventoryService::Inventory_DefinitionUpdate(SteamInventoryDefinitionUpdate_t* pCallback)
{
	char propertyvalue[MAX_STRING];
	uint32 propertyvaluesize = sizeof(propertyvalue);

	Con_Print("SteamInventoryDefinitionUpdate_t\n");
	CInventoryService::propertywaiting_t *lst = InventoryService.propertyQueueList;
	while (lst != NULL)
	{
		SteamInventory()->GetItemDefinitionProperty(lst->itemid, lst->name, propertyvalue, &propertyvaluesize);
		propertyvalue[propertyvaluesize] = 0;

		Steam_SendProperty(lst->itemid, lst->name, propertyvalue);

		InventoryService.propertyQueueList = lst->next;
		free(lst);
		lst = InventoryService.propertyQueueList;
	}
}

#if 1
void CInventoryService::Inventory_DefinitionUpdate_Server(SteamInventoryDefinitionUpdate_t* pCallback)
{
	char propertyvalue[MAX_STRING];
	uint32 propertyvaluesize = sizeof(propertyvalue);

	Con_Print("SteamInventoryDefinitionUpdate_t\n");
	CInventoryService::propertywaiting_t *lst = InventoryService.propertyQueueList;
	while (lst != NULL)
	{
		SteamGetInventory()->GetItemDefinitionProperty(lst->itemid, lst->name, propertyvalue, &propertyvaluesize);
		propertyvalue[propertyvaluesize] = 0;

		Steam_SendProperty(lst->itemid, lst->name, propertyvalue);

		InventoryService.propertyQueueList = lst->next;
		free(lst);
		lst = InventoryService.propertyQueueList;
	}
}
#endif

void Steam_GetProperty(void) //CL_INV_GETPROPERTY
{
	SteamItemDef_t itemdefid;
	char propertyname[MAX_STRING];
	char propertyvalue[MAX_STRING];
	uint32 propertyvaluesize = sizeof(propertyvalue);

	itemdefid = PIPE_ReadLong();
	PIPE_ReadString(propertyname);
	
	if (SteamGetInventory()->GetItemDefinitionProperty(itemdefid, propertyname, propertyvalue, &propertyvaluesize))
	{
		//Con_Print("got property\n");

		propertyvalue[propertyvaluesize] = 0;
		Steam_SendProperty(itemdefid, propertyname, propertyvalue);
	}
	else
	{
		if (!SteamGetInventory()->LoadItemDefinitions())
			return;

		//Con_Print("failed to get property\n");

		CInventoryService::propertywaiting_t *lst;
		for (lst = InventoryService.propertyQueueList; lst; lst = lst->next)
		{
			if (lst->itemid == itemdefid && !strcmp(lst->name, propertyname))
				return;
		}

		lst = (CInventoryService::propertywaiting_t*)malloc(sizeof(CInventoryService::propertywaiting_t));
		lst->itemid = itemdefid;
		strcpy(lst->name, propertyname);

		// relink into the list
		lst->next = InventoryService.propertyQueueList;
		InventoryService.propertyQueueList = lst;
	}
}

void Steam_GetInstanceProperty(void) //CL_INV_GETINSTANCEPROPERTY
{
	///*
	int clientnum;
	SteamItemInstanceID_t iteminst;
	char propertyname[MAX_STRING];
	char propertyvalue[MAX_STRING];
	uint32 propertyvaluesize = sizeof(propertyvalue);
	SteamInventoryResult_t invResult = k_SteamInventoryResultInvalid;
	uint32 invIndex = -1;

	clientnum = PIPE_ReadByte();
	iteminst = PIPE_ReadLongLong();
	PIPE_ReadString(propertyname);

	if (clientnum == 255) // search all players for this instance, very slow
	{
		Con_Print("scanning all clients for instance\n");
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (clientList[i].entnum == 0) // client slot is empty
				continue;

			invResult = InventoryService.clientInventory[i];
			if (invResult == k_SteamInventoryResultInvalid)
				continue;

			SteamItemDetails_t *item;
			SteamItemDetails_t itemArray[MAX_LOADOUT];
			uint32 itemArraySize = MAX_LOADOUT;
			SteamGetInventory()->GetResultItems(invResult, itemArray, &itemArraySize);
			
			/*
			Con_Print("GetResultStatus: ");
			Con_Print(to_string(SteamInventory()->GetResultStatus(invResult)).c_str());
			Con_Print("\n");
			*/
			Con_Print(to_string(itemArraySize).c_str());
			Con_Print(" items found in inventory ");
			Con_Print(to_string(invResult).c_str());
			Con_Print("\n");
			

			for (uint32 j = 0; j < itemArraySize; j++)
			{
				item = &itemArray[j];
				if (item->m_itemId == iteminst)
				{
					Con_Print("instance found\n");
					i = MAX_CLIENTS;
					invIndex = j;
					break;
				}
				else
				{
					Con_Print(to_string(item->m_itemId).c_str());
					Con_Print(" vs ");
					Con_Print(to_string(iteminst).c_str());
					Con_Print("\n");
				}
			}
		}
	}
	else
	{
		if (clientList[clientnum].entnum == 0) // client slot is empty
			return;

		invResult = InventoryService.clientInventory[clientnum];
		if (invResult == k_SteamInventoryResultInvalid)
			return;

		SteamItemDetails_t *item;
		SteamItemDetails_t itemArray[MAX_LOADOUT];
		uint32 itemArraySize = MAX_LOADOUT;
		SteamGetInventory()->GetResultItems(invResult, itemArray, &itemArraySize);

		for (uint32 j = 0; j < itemArraySize; j++)
		{
			item = &itemArray[j];
			if (item->m_itemId == iteminst)
			{
				invIndex = j;
				break;
			}
		}
	}

	if (invIndex == -1)
		return;

	Con_Print("Trying to fetch property \"");
	Con_Print((const char *)propertyname);
	Con_Print("\" for item ");
	Con_Print(to_string(iteminst).c_str());
	Con_Print("\n");

	if (SteamGetInventory()->GetResultItemProperty(invResult, invIndex, propertyname, propertyvalue, &propertyvaluesize))
	{
		propertyvalue[propertyvaluesize] = 0;
		//Steam_SendProperty(iteminst, propertyname, propertyvalue);
		Con_Print((const char *)propertyname);
		Con_Print(" = ");
		Con_Print((const char *)propertyvalue);
		Con_Print("\n");

		PIPE_WriteByte(SV_INV_PROPERTY);
		PIPE_WriteLongLong(iteminst);
		PIPE_WriteString(propertyname, sizeof(propertyname));
		PIPE_WriteString(propertyvalue, sizeof(propertyvalue));
	}
	//*/
}

void Inventory_UpdateClientLoadout(int clientNum)
{
	clientconn_t *lst;
	//for (int i = 0; i < MAX_CLIENTS; i++)
	//{
	lst = &clientList[clientNum];
	if (lst->entnum == clientNum)
	{
		if (!SteamGetInventory()->CheckResultSteamID(InventoryService.clientInventory[clientNum], lst->steamid))
		{
			SteamGetInventory()->DestroyResult(InventoryService.clientInventory[clientNum]);
			InventoryService.clientInventory[clientNum] = k_SteamInventoryResultInvalid;
			Con_Print("^xff0Client is spoofing inventory steamid ");
			Con_Print(to_string(lst->steamid.ConvertToUint64()).c_str());
			Con_Print("\n");
			return;
		}
	}
	//}


	SteamItemDetails_t *item;
	SteamItemDetails_t itemArray[MAX_LOADOUT];
	uint32 itemArraySize = MAX_LOADOUT;
	SteamGetInventory()->GetResultItems(InventoryService.clientInventory[clientNum], itemArray, &itemArraySize);

	PIPE_WriteByte(SV_INV_SERIALPASSED);
	PIPE_WriteByte(clientNum);

#if _DEBUG
	Con_Print("~~~Inventory #");
	Con_Print(to_string(InventoryService.clientInventory[clientNum]).c_str());
	Con_Print("~~~\n");

	for (uint32 j = 0; j < itemArraySize; j++)
	{
		item = &itemArray[j];

		Con_Print("^xf0fItem ^7#");
		Con_Print(to_string(j).c_str());
		Con_Print(": ");
		Con_Print(to_string(item->m_itemId).c_str());
		Con_Print("  ID");
		Con_Print(to_string(item->m_iDefinition).c_str());
		Con_Print("\n");
	}
#endif
}

void Steam_UpdateClient(void) //CL_INV_UPDATECLIENT
{
	char data[0xFFFF];
	uint32_t datasize;
	int clientNum = PIPE_ReadByte();
	PIPE_ReadCharArray(data, &datasize);

	if (InventoryService.clientInventory[clientNum] != k_SteamInventoryResultInvalid) // de-allocate any old inventory in that client's slot
	{
		Con_Print("Deallocating inventory for reuse\n");
		SteamGetInventory()->DestroyResult(InventoryService.clientInventory[clientNum]);
		InventoryService.clientInventory[clientNum] = k_SteamInventoryResultInvalid;
	}

	SteamGetInventory()->DeserializeResult(&InventoryService.clientInventory[clientNum], data, datasize, false);
	
	if (SteamGetInventory()->GetResultStatus(InventoryService.clientInventory[clientNum]) == k_EResultOK)
	{
		Inventory_UpdateClientLoadout(clientNum);
		return;
	}

	Con_Print("Allocating new loadout request in the queue\n");
	CInventoryService::loadoutwaiting_t *newloadout = (CInventoryService::loadoutwaiting_t *)malloc(sizeof(CInventoryService::loadoutwaiting_t));
	newloadout->next = InventoryService.loadoutQueueList;
	newloadout->clientNum = clientNum;
	InventoryService.loadoutQueueList = newloadout;
}

void Steam_RequestedSerial(void) // CL_INV_BUILDSERIAL
{
	InventoryService.myLoadoutSize = PIPE_ReadByte();
	memset(InventoryService.myLoadout, 0, sizeof(InventoryService.myLoadout));

	for (int i = 0; i < InventoryService.myLoadoutSize; i++)
	{
		InventoryService.myLoadout[i] = PIPE_ReadLongLong();
	}

	SteamInventoryResult_t resultToSerialize;
	SteamGetInventory()->GetItemsByID(&resultToSerialize, InventoryService.myLoadout, InventoryService.myLoadoutSize);
	if (resultToSerialize == k_SteamInventoryResultInvalid) // check if we created a valid result
	{
		Con_Print("CL_INV_BUILDSERIAL: resultToSerialize == k_SteamInventoryResultInvalid");
		return;
	}

	char data[4096];
	uint32 datasize = sizeof(data);
	if (SteamGetInventory()->SerializeResult(resultToSerialize, data, &datasize))
	{
		PIPE_WriteByte(SV_INV_LOCALSERIAL);
		PIPE_WriteCharArray(data, datasize);
	}
	else
	{
		InventoryService.waitingtoSerialize = resultToSerialize;
	}
}

void Steam_GetClientLoadout(void)
{
	int clientNum = PIPE_ReadByte();

	SteamInventoryResult_t invResult = k_SteamInventoryResultInvalid;
	invResult = InventoryService.clientInventory[clientNum];
	if (invResult == k_SteamInventoryResultInvalid)
		return;

	SteamItemDetails_t *item;
	SteamItemDetails_t itemArray[MAX_LOADOUT];
	uint32 itemArraySize = MAX_LOADOUT;
	SteamGetInventory()->GetResultItems(invResult, itemArray, &itemArraySize);

	if (!itemArraySize)
		return;

	Con_Print("sending loadout with ");
	Con_Print(to_string(itemArraySize).c_str());
	Con_Print(" items\n");

	PIPE_WriteByte(SV_INV_CLIENTLOADOUT);
	PIPE_WriteByte(clientNum);
	PIPE_WriteByte(itemArraySize);

	for (uint32 j = 0; j < itemArraySize; j++)
	{
		item = &itemArray[j];
		PIPE_WriteLong(item->m_iDefinition);
		PIPE_WriteLongLong(item->m_itemId);
	}
}

void Steam_GrantItem(void)
{
	if (is_server)
		return;

	int itemID[1];
	SteamInventoryResult_t handle;
	itemID[0] = PIPE_ReadLong();

	SteamInventory()->GenerateItems(&handle, itemID, NULL, (uint32)1);
	SteamInventory()->DestroyResult(handle);

	InventoryService.inventoryUpdateTimer = min(InventoryService.inventoryUpdateTimer, 10);
}

void Steam_PlaytimeDrop(void)
{
	if (is_server)
		return;

	SteamInventoryResult_t resultHandle;
	SteamInventory()->TriggerItemDrop(&resultHandle, 1100); // hardcoded 1100 playtime generator for now, we can do more with this later.


	Con_Print("Allocating new trigger drop in the queue ");
	Con_Print(to_string(resultHandle).c_str());
	Con_Print("\n");
	CInventoryService::triggerdropwaiting_t *newdrop = (CInventoryService::triggerdropwaiting_t *)malloc(sizeof(CInventoryService::triggerdropwaiting_t));
	newdrop->next = InventoryService.triggerdropQueueList;
	newdrop->resultHandle = resultHandle;
	InventoryService.triggerdropQueueList = newdrop;

	/*
	SteamItemDetails_t *item;
	SteamItemDetails_t itemArray[MAX_LOADOUT];
	uint32 itemArraySize = MAX_LOADOUT;
	SteamInventory()->GetResultItems(resultHandle, itemArray, &itemArraySize);

	if (itemArraySize)
	{
		for (int i = 0; i < itemArraySize; i++)
		{
			char itemName[MAX_STRING];
			uint32 size = MAX_STRING;
			item = &itemArray[i];

			SteamInventory()->GetItemDefinitionProperty(item->m_iDefinition, "name", itemName, &size);
			itemName[size] = 0;

			PIPE_WriteByte(SV_INV_NEWITEM);
			PIPE_WriteLong(item->m_iDefinition);
			PIPE_WriteString(itemName);
		}
	}
	*/
}





void Steam_StartServer(void)
{
	bool isListen = PIPE_ReadByte();
	
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		ClientCleanup(&clientList[i]);
	}

	if (is_server)
	{
		if (sockServer)
		{
			//SteamGameServerNetworkingSockets()->CloseListenSocket(sockServer);
		}

		if (!isListen && connClient)
		{
			//SteamGameServerNetworkingSockets()->CloseConnection(connClient, 0, NULL, false);
			connClient = 0;
		}

		sockServer = 1;//SteamGameServerNetworkingSockets()->CreateListenSocketP2P(0, 0, NULL);
	}
	else
	{
		if (sockServer)
		{
			//SteamNetworkingSockets()->CloseListenSocket(sockServer);
		}

		if (!isListen && connClient)
		{
			//SteamNetworkingSockets()->CloseConnection(connClient, 0, NULL, false);
			connClient = 0;
		}


		sockServer = 1;//SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, NULL);
	}

	Con_Print("Starting server: ");
	Con_Print(to_string(sockServer).c_str());
	Con_Print("\n");
}


void Steam_ConnectToServer(void)
{
	bool isListen = PIPE_ReadByte();
	char steamid_str[MAX_STRING];
	PIPE_ReadString(steamid_str);

	return;

	if (is_server)
		return;

	if (!isListen && sockServer)
	{
		//SteamNetworkingSockets()->CloseListenSocket(sockServer);
		sockServer = 0;
	}

	CSteamID steamid;
	steamid.SetFromUint64(stoull(steamid_str));
	cout << "trying to connect to Steam ID - " << steamid_str << endl;

	SteamNetworkingIdentity identityRemote;
	identityRemote.SetSteamID(steamid);

	if (myAuthTicket)
	{
		SteamUser()->CancelAuthTicket(myAuthTicket);
		myAuthTicket = 0;
	}


	connClient = 1;//SteamNetworkingSockets()->ConnectP2P(identityRemote, 0, 0, NULL);

	cout << "Connecting to server: " << connClient << endl;
}



void Steam_PlayingWith(void)
{
	char steamid_str[MAX_STRING];
	PIPE_ReadString(steamid_str);

	if (*steamid_str == 0) // uh.. g-g-g-ghooost
		return;

	CSteamID playingID;
	playingID.SetFromUint64(stoull(steamid_str));

	SteamFriends()->SetPlayedWith(playingID);
	SteamFriends()->RequestUserInformation(playingID, false);
	CNetworkingSockets::getAvatar(playingID);
}


void Steam_RichPresence(void)
{
	richpresencetypes type = (richpresencetypes)PIPE_ReadByte();
	static richpresencestates rich_state;
	static int rich_score[2];
	static char rich_serverip[MAX_STRING];
	static char rich_servername[MAX_STRING];
	static char rich_mapname[MAX_STRING];
	static int rich_partysize;

	if (type == RP_STATE)
	{
		rich_state = (richpresencestates)PIPE_ReadByte();
		const char *status;

		switch (rich_state) {
			default:
			case RPSTATE_MENU:
				status = "#Status_MenuDisconnected"; break;
			case RPSTATE_MULTIPLAYER:
				status = "#Status_Multiplayer"; break;
			case RPSTATE_MULTIPLAYER_WARMUP:
				status = "#Status_MultiplayerWarmup"; break;
			case RPSTATE_MULTIPLAYER_RANKED:
				status = "#Status_MultiplayerRanked"; break;
			case RPSTATE_MULTIPLAYER_SPECTATE:
				status = "#Status_MultiplayerSpectate"; break;
			case RPSTATE_SINGLEPLAYER:
				status = "#Status_Singleplayer"; break;
			case RPSTATE_COOP:
				status = "#Status_Coop"; break;
		}

		SteamFriends()->SetRichPresence("steam_display", status);
	}
	else if (type == RP_SCORE)
	{
		char score_string[MAX_STRING];
		const char *score_type;
		
		rich_score[0] = PIPE_ReadShort();
		rich_score[1] = PIPE_ReadShort();
		snprintf(score_string, MAX_STRING, "[ %i : %i ]", rich_score[0], rich_score[1]);
		if (rich_score[0] > rich_score[1])
			score_type = "#Winning";
		else if (rich_score[0] < rich_score[1])
			score_type = "#Losing";
		else
			score_type = "#Tied";

		SteamFriends()->SetRichPresence("score", score_string);
		SteamFriends()->SetRichPresence("scoretype", score_type);
	}
	else if (type == RP_SERVER)
	{
		char status_string[512];
		char connect_string[512];
		char partysize_string[8];
		PIPE_ReadString(rich_mapname);
		rich_partysize = PIPE_ReadByte();
		PIPE_ReadString(rich_serverip);
		PIPE_ReadString(rich_servername);

		snprintf(status_string, 512, "Playing on %s", rich_servername);
		SteamFriends()->SetRichPresence("status", status_string);

		snprintf(connect_string, 512, "+connect %s", rich_serverip);
		SteamFriends()->SetRichPresence("connect", connect_string);
		
		snprintf(partysize_string, 8, "%i", rich_partysize);
		SteamFriends()->SetRichPresence("steam_player_group", rich_serverip);
		SteamFriends()->SetRichPresence("steam_player_group_size", partysize_string);

		SteamFriends()->SetRichPresence("map", rich_mapname);
	}
}


void Steam_Disconnect(void)
{
	if (sockServer)
	{
		/*
		if (is_server)
			SteamGameServerNetworkingSockets()->CloseListenSocket(sockServer);
		else
			SteamNetworkingSockets()->CloseListenSocket(sockServer);
		*/

		sockServer = 0;
	}

	if (connClient)
	{
		/*
		if (is_server)
			SteamGameServerNetworkingSockets()->CloseConnection(connClient, 0, NULL, false);
		else
			SteamNetworkingSockets()->CloseConnection(connClient, 0, NULL, false);
		*/

		connClient = 0;
	}
	
	if (myAuthTicket)
	{
		SteamUser()->CancelAuthTicket(myAuthTicket);
		myAuthTicket = 0;
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		ClientCleanup(&clientList[i]);
	}

	cout << "Disconnected from server\n";
}






