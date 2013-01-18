//============== IV: Multiplayer - http://code.iv-multiplayer.com ==============
//
// File: CGame.cpp
// Project: Client.Core
// Author(s): jenksta
//            aru
//            listener
// License: See LICENSE in root directory
//          Contains zlib licensed code from ScriptHook by aru
//
//==============================================================================

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include "CGame.h"
#include <CLogFile.h>
#include "Scripting.h"
#include "CGUI.h"
#include "CChatWindow.h"
#include "CInputWindow.h"
#include "KeySync.h"
#include "AimSync.h"
#include "Indicators.h"
#include "TrafficLights.h"
#include "ScriptHook.h"
#include "COffsets.h"
#include "CPools.h"
#include "CClientTaskManager.h"
#include "CCredits.h"
#include "CContextDataManager.h"
#include "CAudio.h"
#include "AimSync.h"
#include "CMainMenu.h"
#include "AnimGroups.h"
#include "CCamera.h"
#include "CHooks.h"
#include <SharedUtility.h>

// Enable one of them if we want/don't want trains
#ifdef IVMP_TRAINS
#undef IVMP_TRAINS
#endif

/*#ifndef IVMP_TRAINS
#define IVMP_TRAINS
#endif*/

extern CChatWindow        * g_pChatWindow;
extern CInputWindow       * g_pInputWindow;
extern CClientTaskManager * g_pClientTaskManager;
extern CCredits           * g_pCredits;
extern CMainMenu		  * g_pMainMenu;
extern CCamera			  * g_pCamera;

unsigned int				CGame::m_uiBaseAddress = 0;
bool						CGame::m_bInputState = false;
eState						CGame::m_State = GAME_STATE_NONE;
bool						CGame::m_bFocused = false;
CPools						* CGame::m_pPools = NULL;
CIVPad						* CGame::m_pPad = NULL;
CIVModelInfo				CGame::m_modelInfos[NUM_ModelInfos];
CIVWeaponInfo				CGame::m_weaponInfos[NUM_WeaponInfos];
CIVStreaming				* CGame::m_pStreaming = NULL;
CIVWeather					* CGame::m_pWeather = NULL;
CIVWorld					* CGame::m_pWorld = NULL;
bool						CGame::m_bNameTags = false;
bool						CGame::m_bGameLoaded = false;
bool						 CGame::m_bHeadMovement = true;
bool						CGame::m_bSpecialData[2] = {false, true};
bool						CGame::m_bKickedFromServer = false;



void CGame::SetFocused(bool bFocused)
{
	// Set the focused flag
	m_bFocused = bFocused;

	if(bFocused) // We have focus
	{
		// Enable input
		SetInputState(true);
		CAudioManager::RestoreAllVolume();

		if(g_pMainMenu)
			g_pMainMenu->HideLoadingScreen();
	}
	else // We do not have focus
	{
		// Disable input
		SetInputState(false);
		CAudioManager::SetAllVolume(0.0f);
	}
}

CIVModelInfo * CGame::GetModelInfo(int iModelIndex)
{
	if(iModelIndex < NUM_ModelInfos && iModelIndex >= 0 && m_modelInfos[iModelIndex].IsValid())
		return &m_modelInfos[iModelIndex];

	return NULL;
}

unsigned int CGame::GetLoadedModelCount()
{
	unsigned int uiCount = 0;

	for(int i = 0; i < NUM_ModelInfos; i++)
	{
		if(m_modelInfos[i].IsValid() && m_modelInfos[i].IsLoaded())
			uiCount++;
	}

	return uiCount;
}

unsigned int CGame::GetUnloadedModelCount()
{
	unsigned int uiCount = 0;

	for(int i = 0; i < NUM_ModelInfos; i++)
	{
		if(m_modelInfos[i].IsValid() && !m_modelInfos[i].IsLoaded())
			uiCount++;
	}

	return uiCount;
}

CIVWeaponInfo * CGame::GetWeaponInfo(eWeaponType weaponType)
{
	if(weaponType < NUM_WeaponInfos && weaponType >= 0)
		return &m_weaponInfos[weaponType];

	return NULL;
}

HWND CGame::GetHWnd()
{
	return *(HWND *)COffsets::VAR_HWnd;
}

DWORD CGame::GetTime()
{
	return *(DWORD *)COffsets::VAR_Time;
}

void CGame::Initialize()
{
	// Ensure all game structures are correctly aligned
	assert(sizeof(IVCamData) == 0x74);
	assert(sizeof(IVCam) == 0x140);
	assert(sizeof(IVCheckpoint) == 0x30);
	assert(sizeof(IVEntity) == 0x74);
	assert(sizeof(IVDynamicEntity) == 0x10C);
	//assert(sizeof(IVBaseModelInfo) == 0x0); // TODO
	assert(sizeof(IVPadConfig) == 0x7B8);
	assert(sizeof(IVPadDataHistoryItem) == 0x8);
	assert(sizeof(IVPadDataHistory) == 0x200);
	assert(sizeof(IVPadData) == 0x10);
	assert(sizeof(IVPad) == 0x3A84);
	//assert(sizeof(IVPedTaskManager) == 0x0); // TODO
	assert(sizeof(IVPedWeaponSlot) == 0xC);
	assert(sizeof(IVPedWeapons) == 0x11A);
	assert(sizeof(IVPhysical) == 0x210);
	assert(sizeof(IVPed) == 0xF00);
	assert(sizeof(IVPlayerInfo) == 0x5C0);
	assert(sizeof(IVPlayerPed) == 0xF00);
	assert(sizeof(IVPool) == 0x1C);
	//assert(sizeof(IVTask) == 0x0); // TODO
	assert(sizeof(IVVehicle) == 0x20D0);
	assert(sizeof(IVWeaponInfo) == 0x110);
	assert(sizeof(CSimpleCollection<void>) == 0x8);

	// Get the process base address
	m_uiBaseAddress = (unsigned int)GetModuleHandle(NULL);

	if(m_uiBaseAddress == 0)
	{
		CLogFile::Printf("Invalid Game Base Detected. Exiting...");
		ExitProcess(0);
	}

	CLogFile::Printf("Game Base: 0x%p (0x%p)", m_uiBaseAddress, (m_uiBaseAddress - 0x400000));
	m_uiBaseAddress -= 0x400000;
	COffsets::Init(m_uiBaseAddress);

	if(COffsets::GetVersion() == GAME_VERSION_UNKNOWN)
	{
		MessageBox(NULL, "Unknown game version detected. IV:MP currently only supports version 1.0.7.0", "Unknown game version", NULL);
		ExitProcess(0);
	}

	if(Patch())
	{
		// DOES this native have any function, ida says nope
		#ifdef DEBUG
			Scripting::DebugOn();
			CLogFile::Printf("Applied GTAIV DEBUG mode");
		#endif

		CLogFile::Printf("Applied patches");
		InstallKeySyncHooks();
		InstallAimSyncHooks();
		CLogFile::Printf("Applied key sync/aim sync hooks");

		InstallIndicatorHooks();
		InstallTrafficLightHooks();
		CLogFile::Printf("Applied misc hooks");

		InstallScriptHooks();
		CLogFile::Printf("Applied script hooks");

	}
	else
	{
		MessageBox(NULL, "Failed to apply patches. IV:MP needs to apply certain patches to run", "Failed to apply patches", NULL);
		ExitProcess(0);
	}

	// Replace "Loading .." with "IV:MP is loading...", some user say that the label sometimes is shown
	//Scripting::SetMsgForLoadingScreen("IV:MP is loading....");

	SetInputState(true);
	SetState(GAME_STATE_NONE);

	// Testcode
	//char *cVar4 = new char[10];
	//for (int I = 0; I <= 1; I++ )
 //   {
	//	if(I == 0)
	//		strcpy_s(cVar4, 32, "MPCellphone");
	//	else if(I == 1)
	//		strcpy_s(cVar4, 32, "speechControl_net");

	//	Scripting::RequestScript(cVar4);
	//	if(Scripting::HasScriptLoaded(cVar4)) {
	//		Scripting::StartNewScript(cVar4, 2014);
	//		Scripting::MarkScriptAsNoLongerNeeded(cVar4);
	//	}
	//}

	CLogFile::Print("Creating directories to download files to..");
	String strFolderName = SharedUtility::GetAbsolutePath("clientfiles");
	if(!SharedUtility::Exists(strFolderName))
		SharedUtility::CreateDirectoryA(strFolderName);
	if(!SharedUtility::Exists(strFolderName + "\\clientscripts"))
		SharedUtility::CreateDirectoryA(strFolderName + "\\clientscripts");
	if(!SharedUtility::Exists(strFolderName + "\\resources"))
		SharedUtility::CreateDirectoryA(strFolderName + "\\resources");	

	// Create our pools class
	m_pPools = new CPools();

	// Create the pad instance
	m_pPad = new CIVPad((IVPad *)COffsets::VAR_Pads);

	// Initialize our game classes
	m_pStreaming = new CIVStreaming();
	m_pWeather = new CIVWeather();
	m_pWorld = new CIVWorld();

	// Initialize the model infos
	for(int i = 0; i < NUM_ModelInfos; i++)
		m_modelInfos[i].SetIndex(i);

	// Disable invalid models
	m_modelInfos[125].SetIndex(-1); // Ingot (FIX) //41
	m_modelInfos[180].SetIndex(-1); // Uranus (FIX) //96
	m_modelInfos[191].SetIndex(-1); // Hellfury (FIX) // 107
	m_modelInfos[195].SetIndex(-1); // Zombieb (FIX) // 111

	// Initialize the weapon infos
	for(int i = 0; i < NUM_WeaponInfos; i++)
	{
		// TODO: Make SetType update the weapon info pointer?
		m_weaponInfos[i].SetType((eWeaponType)i);
		m_weaponInfos[i].SetWeaponInfo((IVWeaponInfo *)((GetBase() + ARRAY_WeaponInfos) + (i * sizeof(IVWeaponInfo))));
	}
}

void CGame::Shutdown()
{
	// Shutdown our game class instances
	SAFE_DELETE(m_pWorld);
	SAFE_DELETE(m_pWeather);
	SAFE_DELETE(m_pStreaming);
	SAFE_DELETE(m_pPad);

	// Delete our pools instance
	SAFE_DELETE(m_pPools);

	// Uninstall the script hooks
	UninstallScriptHooks();
}

// thx listener
void UnprotectMemory()
{
	BYTE * pImageBase = (BYTE *)(CGame::GetBase() + 0x400000);
	PIMAGE_DOS_HEADER   pDosHeader = (PIMAGE_DOS_HEADER)pImageBase;
	PIMAGE_NT_HEADERS   pNtHeader  = (PIMAGE_NT_HEADERS)(pImageBase + pDosHeader->e_lfanew);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeader);

	for(int iSection = 0; iSection < pNtHeader->FileHeader.NumberOfSections; iSection++, pSection++)
	{
		char * pszSectionName = (char *)pSection->Name;

		if(!strcmp(pszSectionName, ".text") || !strcmp(pszSectionName, ".rdata"))
			CPatcher::Unprotect((DWORD)(pImageBase + pSection->VirtualAddress), ((pSection->Misc.VirtualSize + 4095) & ~4095));
	}
}

DWORD dwJmp;
DWORD dwTestHandle;

void _declspec(naked) IVTrain__Constructor()
{
	_asm 
	{		
		mov dwTestHandle, ecx
		pushad
	}

	CLogFile::Printf("Train-Handle(0x%x)", dwTestHandle);
	dwJmp = (CGame::GetBase() + 0x949BA6);

	_asm
	{
		popad
		push ebp
		mov ebp, esp
		and esp, 0FFFFFFF0h
		jmp dwJmp
	}
}

void _declspec(naked) IVTrain__Create()
{
	// TODO
	_asm
	{
	}
}

IVTask * ___pTask = NULL;

void _declspec(naked) CTask__Destructor_Hook()
{
	_asm
	{
		mov ___pTask, ecx
		pushad
	}


	// Do we have a client task manager?
	if(g_pClientTaskManager)
	{
		// Let the client task manager handle this task deletion
		g_pClientTaskManager->HandleTaskDelete(___pTask);
	}

	_asm
	{
		popad
		push esi
		mov esi, ecx
		push esi
		mov dword ptr [esi], offset COffsets::VAR_CTask__VFTable
		jmp COffsets::RETURN_CTask__Destructor
	}
}

void _declspec(naked) CEpisodes__IsEpisodeAvaliable_Hook()
{
	_asm
	{
		mov eax, [esp+4]
		test eax, eax
		jnz it_not
		mov al, 1
		retn 4
it_not:
		xor al, al
		retn 4
	}
}

DWORD dwJmp2;
DWORD dwAddress2;
DWORD dwModel;
DWORD dwModelArray = 0x15F73B0;
int iVehType = 0;

void _declspec(naked) CVehicleFactory__CreateVehicle_Part1()
{
    _asm
    {
            mov eax, [esp]         
            mov dwAddress2, eax
            mov edx, [esp + 4] // get the model
            mov edx, dwModelArray[edx * 4]
            mov dwModel, edx
            pushad
    }
    CLogFile::Printf("model (0x%x) - (0x%x)", dwModel, dwAddress2);
    //iVehType = *(int *)(dwModel + 0x6C);
    //CLogFile::Printf("type %d", iVehType);
    dwJmp2 = (CGame::GetBase() + 0x443D66);
    _asm
    {
            popad
            push ebp
            push edi
            mov edi, [esp + 4]
            jmp dwJmp2
    }
}
unsigned int          uiPlayerInfoIndex = 0;
IVPlayerInfo        * pReturnedPlayerInfo = NULL;
unsigned int          uiReturnedIndex = 0;
IVPlayerPed         * _pPlayerPed = NULL;

IVPlayerInfo * GetPlayerInfoFromIndex(unsigned int uiIndex)
{
	// Default to the local player info just incase the index is invalid
	pReturnedPlayerInfo = CGame::GetPools()->GetPlayerInfoFromIndex(0);

	// Is this not the local player info?
	if(uiIndex != 0)
	{
		CContextData * pContextInfo = CContextDataManager::GetContextData(uiIndex);

		if(pContextInfo)
			pReturnedPlayerInfo = pContextInfo->GetPlayerInfo()->GetPlayerInfo();
	}
	
	return pReturnedPlayerInfo;
}

void _declspec(naked) GetPlayerInfoFromIndex_Hook()
{
	_asm
	{
		mov eax, [esp+4]
		mov uiPlayerInfoIndex, eax
		pushad
	}

	GetPlayerInfoFromIndex(uiPlayerInfoIndex);

	_asm
	{
		popad
		mov eax, pReturnedPlayerInfo
		retn
	}
}

unsigned int GetIndexFromPlayerInfo(IVPlayerInfo * pPlayerInfo)
{
	// Default to the local player info just incase the player info is invalid
	uiReturnedIndex = 0;

	// Is this not the local player info?
	if(pPlayerInfo != CGame::GetPools()->GetPlayerInfoFromIndex(0))
	{
		CContextData * pContextInfo = CContextDataManager::GetContextData(pPlayerInfo);

		if(pContextInfo)
			uiReturnedIndex = pContextInfo->GetPlayerInfo()->GetPlayerNumber();
	}

	return uiReturnedIndex;
}

void _declspec(naked) GetIndexFromPlayerInfo_Hook()
{
	_asm
	{
		mov eax, [esp+4]
		mov pReturnedPlayerInfo, eax
		pushad
	}

	GetIndexFromPlayerInfo(pReturnedPlayerInfo);

	_asm
	{
		popad
		mov eax, uiReturnedIndex
		retn
	}
}

bool bInvalidIndex = false;

IVPlayerPed * GetLocalPlayerPed()
{
	// Default to the local player ped (If available)
	IVPlayerInfo * pPlayerInfo = CGame::GetPools()->GetPlayerInfoFromIndex(0);

	if(pPlayerInfo)
		_pPlayerPed = pPlayerInfo->m_pPlayerPed;
	else
		_pPlayerPed = NULL;

	// Is the local player id valid?
	if(CGame::GetPools()->GetLocalPlayerIndex() != -1)
	{
		// Is the player index not the local player?
		if(CGame::GetPools()->GetLocalPlayerIndex() != 0)
		{
			// Get the context info for the player index
			CContextData * pContextInfo = CContextDataManager::GetContextData((BYTE)CGame::GetPools()->GetLocalPlayerIndex());

			// Is the context info valid?
			if(pContextInfo)
			{
				// Set the player ped to the remote player
				_pPlayerPed = pContextInfo->GetPlayerPed()->GetPlayerPed();
			}
		}
	}

	// Some code to test a theory
	if(_pPlayerPed == NULL)
	{
		if(!bInvalidIndex)
		{
			CLogFile::Printf("GetLocalPlayerPed Return Is Invalid (Index is %d)", CGame::GetPools()->GetLocalPlayerIndex());
			bInvalidIndex = true;
		}
	}
	else
	{
		if(bInvalidIndex)
		{
			CLogFile::Printf("GetLocalPlayerPed Return Is Now Valid");
			bInvalidIndex = false;
		}
	}

	return _pPlayerPed;
}

void _declspec(naked) GetLocalPlayerPed_Hook()
{
	_asm
	{
		pushad
	}

	GetLocalPlayerPed();

	_asm
	{
		popad
		mov eax, _pPlayerPed
		retn
	}
}

IVPlayerPed * GetPlayerPedFromPlayerInfo(IVPlayerInfo * pPlayerInfo)
{
	// Is the player info pointer valid?
	if(pPlayerInfo)
		_pPlayerPed = pPlayerInfo->m_pPlayerPed;
	else
	{
		// Player info pointer is invalid, use the local player ped
		_pPlayerPed = GetLocalPlayerPed();
	}

	return _pPlayerPed;
}

void _declspec(naked) GetPlayerPedFromPlayerInfo_Hook()
{
	_asm
	{
		mov eax, [esp+4]
		mov pReturnedPlayerInfo, eax
		pushad
	}

	GetPlayerPedFromPlayerInfo(pReturnedPlayerInfo);

	_asm
	{
		popad
		mov eax, _pPlayerPed
		retn
	}
}

DWORD dwAddJump;
void _declspec(naked) CameraPointAtCoord(CVector3 * vecLookAt)
{
	_asm 
	{
		push ebp
		mov ebp, esp
		mov eax, [ebp+0Ch]
		mov vecLookAt, eax
		pushad
	}

	dwAddJump = ((CGame::GetBase() +  0xAFE840) + 0x6 );

	_asm
	{
		popad
		push ebp
		mov ebp, esp
		and esp, 0FFFFFFF0h
		jmp dwAddJump
	}
}

void _declspec(naked) CTaskSimpleStartVehicle__Process()
{
	_asm
	{
		xor al, al
		retn 4
	}
}

HANDLE hFile;
LPVOID lpBuffer;
DWORD dwRFile;
 
void _declspec(naked) OpenFile_Hook()
{
    _asm
    {
            mov eax, [esp+4]
            mov hFile, eax
            mov eax, [esp+8]
            mov lpBuffer, eax  
            pushad
    }
 
    strcpy((char *)hFile, "common/data/loadingscreens_ivmp.dat");
 
    dwRFile = (CGame::GetBase() + 0x7B2740);
    _asm
    {
            popad
            push lpBuffer
            push hFile
            call dwRFile
            add esp, 8
            retn
    }
}
char *szFile;
String strFile;
DWORD dwAddress4;
void _declspec(naked) LoadTexture_Hook()
{
	_asm
	{
		mov eax, [esp+4]
		mov szFile, eax
		pushad
	}
	strFile = szFile;
	CLogFile::Printf("LoadTexture: %s", strFile.Get());
	dwAddress4 = (CGame::GetBase() + 0xA712B0);
	_asm
	{
		popad
		push szFile
		call dwAddress4
		add esp, 8 ; should be 8
		retn
	}

}

bool CGame::Patch()
{
	// Unprotect .text and .rdata memory and leave it unprotected
	UnprotectMemory();

	if(COffsets::GetVersion() == GAME_VERSION_7)
	{
		Scripting::SetPedDensityMultiplier(0);
		Scripting::SetParkedCarDensityMultiplier(0);
		Scripting::SetRandomCarDensityMultiplier(0);

		// Return at start of CTaskSimplePlayRandomAmbients::ProcessPed (Disable random amient animations)
		// NOTE: This also disables ambient head movements and maybe some other stuff we actually want
		*(DWORD *)(GetBase() + 0x9849F0) = 0x900004C2;

		// Hook GetPlayerInfoFromIndex to use our own function
		CPatcher::InstallJmpPatch((GetBase() + 0x817F20), (DWORD)GetPlayerInfoFromIndex_Hook);

		// Hook GetIndexFromPlayerInfo to use our own function
		CPatcher::InstallJmpPatch((GetBase() + 0x817DC0), (DWORD)GetIndexFromPlayerInfo_Hook);

		// Hook GetLocalPlayerPed to use our own function
		CPatcher::InstallJmpPatch((GetBase() + 0x817F40), (DWORD)GetLocalPlayerPed_Hook);

		// Hook CTask::~CTask to use our own function
		CPatcher::InstallJmpPatch((GetBase() + 0xA288D0), (DWORD)CTask__Destructor_Hook);

		// Hook CEpisodes::IsEpisodeAvaliable to use our own function
		CPatcher::InstallJmpPatch((GetBase() + 0x814810), (DWORD)CEpisodes__IsEpisodeAvaliable_Hook);
		
		// Hook loading screen
		CPatcher::InstallHookCall((GetBase() + 0x424301), (DWORD)OpenFile_Hook);
		char *szTxt = "platform:/textures/loadingscreens_ivmp_textures";
		CPatcher::InstallPushPatch((GetBase() + 0x423F04), (DWORD)szTxt);
		CPatcher::InstallCallPatch((GetBase() + 0x424B26), (DWORD)RemoveInitialLoadingScreens);
		CPatcher::InstallJmpPatch((GetBase() + 0xD549DC), (GetBase() + 0xD549C0));// Disables loading music, reduces gta loading time & fix crash
		CPatcher::InstallJmpPatch((GetBase() + 0xD549EC), (GetBase() + 0xD549D0));// Disables loading music, reduces gta loading time & fix crash
		//CPatcher::InstallJmpPatch((GetBase() + 0xA714F0), (DWORD)LoadTexture_Hook);
		
		// IMG/.DAT loading function(later useful for importing .dat or .wtd or .wft files(vehicles etc.))
		//CPatcher::InstallJmpPatch((GetBase() + 0x8D79A0), (GetBase() + 0x8D7BB2));

		// Make the game think we are not connected to the internet
		*(BYTE *)(GetBase() + 0x10F1390) = 0; // byteInternetConnectionState
		*(DWORD *)(GetBase() + 0x7AF1A0) = 0x90C3C032; // xor al, al; retn; nop

		// Don't load startup.sco
		*(BYTE *)(GetBase() + 0x809A8A) = 0x75;

		// Always start a new game
		CPatcher::InstallJmpPatch((GetBase() + 0x5B0311), (GetBase() + 0x5B03BF));

#ifdef IVMP_TRAINS
		// Hook for CVehicleFactory__CreateVehicle
		// CPatcher::InstallJmpPatch((GetBase() + 0x443D60), (DWORD)CVehicleFactory__CreateVehicle_Part1);
		CPatcher::InstallJmpPatch((GetBase() + 0x949BA0), (DWORD)IVTrain__Constructor);
		//CPatcher::InstallJmpPatch((GetBase() + 0x973AF0), (DWORD)IVTrain__Create);

		// not sure..
		CPatcher::InstallNopPatch((GetBase() + 0x8ACD64), 5);

		// Install trains
		//DWORD dwAddress = (CGame::GetBase() + 0x94A700);
		//_asm call dwAddress;
#else
		// Disable parked cars
		CPatcher::InstallRetnPatch(GetBase() + 0xB3EDF0);

		// Disable emergency services and garbage trucks
		CPatcher::InstallNopPatch((GetBase() + 0x4215CF/*4215D4*/), 5);

		// Disable vehicle entries
		*(DWORD *)(GetBase() + 0x9B1ED0) = 0x0CC2C033;
		*(BYTE *)(GetBase() + 0x9B1ED4) = 0x00;

		// Disable vehicle exits
		*(BYTE *)(GetBase() + 0x9BBBFC) = 0xEB;

		// Disable random peds and vehicles
		CPatcher::InstallNopPatch((GetBase() + 0x8ACD64), 5);
		CPatcher::InstallNopPatch((GetBase() + 0x421610), 5);
		CPatcher::InstallNopPatch((GetBase() + 0x81B22E), 5);

		// Disable vehicle generation
		//CPatcher::InstallJmpPatch((GetBase() + 0x438D00),(GetBase() + 0x438D62));

		// Disable train generation
		//CPatcher::InstallJmpPatch((GetBase() + 0x94A700),(GetBase() + 0x94A9F4));
#endif
		// Disable scenario peds
		*(BYTE *)(GetBase() + 0x9F72C0) = 0xB8; // mov eax,
		*(DWORD *)(GetBase() + 0x9F72C1) = 0x0; // 0
		*(BYTE *)(GetBase() + 0x9F72C5) = 0xC3; // retn

		// Disable fake cars
		CPatcher::InstallRetnPatch(GetBase() + 0x9055D0);

		// Disable sleep
		*(DWORD *)(GetBase() + 0x401835) = 1;

		// Hook for pointcamatcoord
		//CPatcher::InstallJmpPatch((GetBase() + 0xAFE840), (DWORD)CameraPointAtCoord);

		// Disable auto vehicle start when player enter to it
		CPatcher::InstallJmpPatch((GetBase() + 0xA9F300), (DWORD)CTaskSimpleStartVehicle__Process);

		// Adds a lot of world stuff which was disabled since Alpha >.<
		PatchWorldAndTrain();

		// Don't initialize error reporting
		CPatcher::InstallRetnPatch(GetBase() + 0xD356D0);

		// Certificates check (RETN 8)
		*(DWORD *)(GetBase() + 0x403F10) = 0x900008C2;

		// xor eax, eax - address of the RGSC object
		*(DWORD *)(GetBase() + 0x40262D) = 0x4AE9C033;

		// Skip RGSC connect and EFC checks (jmp 40289E)
		*(DWORD *)(GetBase() + 0x402631) = 0x90000002;

		// NOP; MOV [g_rgsc], eax
		*(WORD *)(GetBase() + 0x402883) = 0xA390;

		// Disable VDS100/VDS110/VDS120 and other error 
		//CPatcher::InstallJmpPatch((GetBase() + 0x40262D), (GetBase() + 0x40265C));
		//CPatcher::InstallJmpPatch((GetBase() + 0x402605), (GetBase() + 0x402613));
		// -> uses other hook

		// Disable VDS102 error
		CPatcher::InstallNopPatch((GetBase() + 0x4028ED), 42);

		// Last RGSC init check (NOP*6)
		CPatcher::InstallNopPatch((GetBase() + 0x40291D), 6);

		// Skip missing tests
		CPatcher::InstallNopPatch((GetBase() + 0x402B12), 14);
		CPatcher::InstallNopPatch((GetBase() + 0x402D17), 14);
		*(DWORD *)(GetBase() + 0x403870) = 0x90CC033; // xor eax, eax; retn
		*(DWORD *)(GetBase() + 0x404250) = 0x90CC033; // xor eax, eax; retn

		// Disable securom spot checks (mov al, 1; retn)
		*(DWORD *)(GetBase() + 0xBAC160) = 0x90C301B0;
		*(DWORD *)(GetBase() + 0xBAC180) = 0x90C301B0;
		*(DWORD *)(GetBase() + 0xBAC190) = 0x90C301B0;
		*(DWORD *)(GetBase() + 0xBAC1C0) = 0x90C301B0;

		// Install crash fixes
		CHooks::Install();
		return true;
	}

	return false;
}

bool CGame::IsMenuActive()
{
	DWORD dwMenuActive1 = (COffsets::VAR_MenuActive_1);

	if(dwMenuActive1 == 2 || dwMenuActive1 == 6)
		return 0;

	return *(bool *)(COffsets::VAR_MenuActive_2);
}

void CGame::SetTime(int uHour, int uMinute)
{
	DWORD dwFunc = COffsets::FUNC_SetTimeOfDay;
	_asm
	{
		push -1
		push 0
		push uMinute
		push uHour
		call dwFunc
		add esp, 10h
	}
	*(DWORD *)(*(DWORD *)(COffsets::VAR_TimeOfDay) + 0x260) = 2;
}

void CGame::SetDayOfWeek(int uDay)
{
	*(DWORD *)(COffsets::VAR_CClock__DayOfWeek) = uDay;
}

void CGame::LockTime(int uHour, int uMinute)
{
	*(DWORD *)(COffsets::VAR_CClock__LockedHour) = uHour;
	*(DWORD *)(COffsets::VAR_CClock__LockedMinute) = uMinute;
}

void CGame::GetTime(int * uHour, int * uMinute)
{
	*uHour = *(DWORD *)(COffsets::VAR_CClock__LockedHour);

	if(*uHour == -1)
		*uHour = *(DWORD *)(COffsets::VAR_CClock__Hour);

	*uMinute = *(DWORD *)(COffsets::VAR_CClock__LockedMinute);

	if(*uMinute == -1)
		*uMinute = *(DWORD *)(COffsets::VAR_CClock__Minute);
}

void CGame::SetWantedLevel(DWORD dwLevel)
{
	*(DWORD *)(COffsets::VAR_CurrentWantedLevel) = dwLevel;
}

DWORD CGame::GetWantedLevel()
{
	return *(DWORD *)(COffsets::VAR_CurrentWantedLevel);
}

void CGame::SetHudVisible(bool bVisible)
{
	*(bool *)(COffsets::VAR_HudEnabled) = bVisible;
}

bool CGame::IsHudVisible()
{
	return *(bool *)COffsets::VAR_HudEnabled;
}

void CGame::SetRadarVisible(bool bVisible)
{
	*(bool *)(COffsets::VAR_RadarVisible) = bVisible;
}

bool CGame::IsRadarVisible()
{
	return *(bool *)COffsets::VAR_RadarVisible;
}

void CGame::SetPlayerNamesVisible(BYTE byteVisible)
{
	if(byteVisible < 2)
		*(BYTE *)(COffsets::VAR_PlayerNamesVisible) = byteVisible;
	else
		*(BYTE *)(COffsets::VAR_PlayerNamesVisible) = 0;
}

BYTE CGame::ArePlayerNamesVisible()
{
	return *(BYTE *)(COffsets::VAR_PlayerNamesVisible);
}

void CGame::SetAreaNamesEnabled(bool bEnabled)
{
	*(bool *)(COffsets::VAR_AreaNamesDisabled) = !bEnabled;
}

bool CGame::AreAreaNamesEnabled()
{
	return !(*(bool *)COffsets::VAR_AreaNamesDisabled);
}

/**@brief Get the matrix represented as euler angles around ZYX
   * @param yaw Yaw around X axis
   * @param pitch Pitch around Y axis
   * @param roll around X axis 
   * @param solution_number Which solution of two possible solutions ( 1 or 2) are possible values*/	
  /*void getEulerZYX(btScalar& yaw, btScalar& pitch, btScalar& roll, unsigned int solution_number = 1) const
  {
    struct Euler{btScalar yaw, pitch, roll;};
    Euler euler_out;
    Euler euler_out2; //second solution
    //get the pointer to the raw data
    
    // Check that pitch is not at a singularity
    if (btFabs(m_el[2].x()) >= 1)
    {
      euler_out.yaw = 0;
      euler_out2.yaw = 0;
	
      // From difference of angles formula
      btScalar delta = btAtan2(m_el[0].x(),m_el[0].z());
      if (m_el[2].x() > 0)  //gimbal locked up
      {
        euler_out.pitch = SIMD_PI / btScalar(2.0);
        euler_out2.pitch = SIMD_PI / btScalar(2.0);
        euler_out.roll = euler_out.pitch + delta;
        euler_out2.roll = euler_out.pitch + delta;
      }
      else // gimbal locked down
      {
        euler_out.pitch = -SIMD_PI / btScalar(2.0);
        euler_out2.pitch = -SIMD_PI / btScalar(2.0);
        euler_out.roll = -euler_out.pitch + delta;
        euler_out2.roll = -euler_out.pitch + delta;
      }
    }
    else
    {
      euler_out.pitch = - btAsin(m_el[2].x());
      euler_out2.pitch = SIMD_PI - euler_out.pitch;
	
      euler_out.roll = btAtan2(m_el[2].y()/btCos(euler_out.pitch), 
			       m_el[2].z()/btCos(euler_out.pitch));
      euler_out2.roll = btAtan2(m_el[2].y()/btCos(euler_out2.pitch), 
				m_el[2].z()/btCos(euler_out2.pitch));
	
      euler_out.yaw = btAtan2(m_el[1].x()/btCos(euler_out.pitch), 
			      m_el[0].x()/btCos(euler_out.pitch));
      euler_out2.yaw = btAtan2(m_el[1].x()/btCos(euler_out2.pitch), 
                               m_el[0].x()/btCos(euler_out2.pitch));
    }
    
    if (solution_number == 1)
    { 
		yaw = euler_out.yaw; 
		pitch = euler_out.pitch;
		roll = euler_out.roll;
    }
    else
    { 
		yaw = euler_out2.yaw; 
		pitch = euler_out2.pitch;
		roll = euler_out2.roll;
    }
  }*/

void CGame::ConvertRotationMatrixToEulerAngles(Matrix& matRotation, CVector3& vecRotation)
{
	// This function uses RADIANS not DEGREES
	// Matrix->XYZEuler
	// From http://www.geometrictools.com/LibFoundation/Mathematics/Wm4Matrix3.inl.html
	if(matRotation.vecRight.fZ < 1.0f)
	{
		if(matRotation.vecRight.fZ > -1.0f)
		{
			vecRotation = CVector3(atan2(-matRotation.vecForward.fZ, matRotation.vecUp.fZ), asin(matRotation.vecRight.fZ), 
				atan2(-matRotation.vecRight.fY, matRotation.vecRight.fX));
		}
		else
		{
			// WARNING.  Not unique.  XA - ZA = -atan2(r10,r11)
			vecRotation = CVector3(-atan2(matRotation.vecForward.fX, matRotation.vecForward.fY), -HALF_PI, 0.0f);
		}
	}
	else
	{
		// WARNING.  Not unique.  XAngle + ZAngle = atan2(r10,r11)
		vecRotation = CVector3(atan2(matRotation.vecForward.fX, matRotation.vecForward.fY), HALF_PI, 0.0f);
	}
}

void CGame::ConvertEulerAnglesToRotationMatrix(CVector3 &vecRotation, Matrix &matRotation)
{
#if 0
	// This function uses RADIANS not DEGREES
	// XYZEuler(Pitch, Roll, Yaw)->Matrix // WAT?
	// From Ogre
	float fYAngle = 
	//
	float fCos, fSin;

	fCos = (float)cos(fYAngle);
	fSin = (float)sin(fYAngle);
	Matrix kXMat(CVector3(1.0,0.0,0.0),CVector3(0.0,fCos,-fSin),CVector3(0.0,fSin,fCos));

	fCos = (float)cos(fPAngle);
	fSin = (float)sin(fPAngle);
	Matrix kYMat(CVector3(fCos,0.0,fSin),CVector3(0.0,1.0,0.0),CVector3(-fSin,0.0,fCos));

	fCos = (float)cos(fRAngle);
	fSin = (float)sin(fRAngle);
	Matrix kZMat(CVector3(fCos,-fSin,0.0),CVector3(fSin,fCos,0.0),CVector3(0.0,0.0,1.0));

	/**this*/matRotation = kXMat*(kYMat*kZMat);
#endif
	// This function uses RADIANS not DEGREES
	// XYZEuler(Pitch, Roll, Yaw)->Matrix
	// Taken from the Quake3 source code
	// test, swap around if needed
	/*float yaw = vecRotation.fZ;
	float pitch = vecRotation.fX;
	float roll = vecRotation.fY;

	float sy = sin(yaw);
	float cy = cos(yaw);
	float sp = sin(pitch);
	float cp = cos(pitch);
	float sr = sin(roll);
	float cr = cos(roll);

	// for d3d
	matRotation.vecRight = CVector3(cp * cy, sr * sp * cy + cr * -sy, cr * sp * cy + -sr * -sy);
	// for gta swap the forward and up
	matRotation.vecForward = CVector3(cp * sy, sr * sp * sy + cr * cy, cr * sp * sy + -sr * cy);
	matRotation.vecUp = CVector3(-sp, sr * cp, cr * cp);*/
	Matrix34 matGameRotation;
	matGameRotation.FromMatrix(&matRotation);
	Matrix34 * pMatGameRotation = &matGameRotation;
	CVector3 * pVecGameRotation = &vecRotation;
	DWORD dwFunc = COffsets::FUNC_CMatrix__ConvertFromEulerAngles;
	_asm
	{
		push pVecGameRotation
		mov ecx, pMatGameRotation
		call dwFunc
	}
	matGameRotation.ToMatrix(&matRotation);
}

/*void CGame::ConvertQuaternionToRotationMatrix(Quaternion * quatRotation, GameMatrix34 * matRotation)
{
	DWORD dwFunc = COffsets::FUNC_CMatrix__ConvertFromQuaternion;
	_asm
	{
		push quatRotation
		mov ecx, matRotation
		call dwFunc
	}
}

void CGame::ConvertRotationMatrixToQuaternion(GameMatrix34 * matRotation, Quaternion * quatRotation)
{
	DWORD dwFunc = COffsets::FUNC_CQuaternion__ConvertFromMatrix;
	_asm
	{
		push matRotation
		mov ecx, quatRotation
		call dwFunc
	}
}*/

bool CGame::SetScrollBarText(unsigned char ucScrollbar, const char* szText)
{
	if(ucScrollbar > 7)
		return false;

	if(strlen(szText) >= 1299)
		return false;

	if(strlen(szText) == 0)
	{
		// Will throw a division by zero error if its length is zero, so use a pretty much empty string instead
		return SetScrollBarText(ucScrollbar, " ");
	}

	strncpy((char *)(COffsets::VAR_ScrollBarText + ucScrollbar * 1300), szText, strlen(szText) + 1);
	return true;
}

void CGame::ResetScrollBars(unsigned char ucScrollbar)
{
	// *financial
	if(ucScrollbar == 0 || ucScrollbar == 0xFF)
		SetScrollBarText(0, "BSHT+3%  CUBEL+2%  SPUK+1%  CABOX-2%  OOT+3%  ECLA-7%  BEAN-5%  LOGR+3%  RWDC-2%  CHKO+6%  BIGL-2%       Sprunk overcomes aggressive Logger.       TIKL+59%  FRUC-12%  WHIZ+69%  BADR+22%  GNIC-32%       Whiz has liquid assets worth over 1 Billion.       NXJN+36%  XGAS+26%  SYNC+8%  RONO+16%  GLBOIL+26%       Economic growth eases slightly.       LSNW+3%  METV-2%  WZNT+2%  CANE-3%  LIBT+10%  CNTU-2%  EYEF+3%       Corporate America braces for impact.       BOLS-10%  JLZA+1%  LOMB+17%  FLEE-18%  SHRK+20%  BAWS-21%       Only one in five SA:MP players have left their house this year.       BNCO+3%  BAWSAQ-2%  PLAP+2%  ERIS-3%       Shares unruffled by surprise activist assault.       BETA+14%  UGEN-1%  MOLIS+8%  MAXR-4%  MTA+30%       Energy bills cut for millions of people.       ~~~~~       ");

	// *theatre
	if(ucScrollbar == 1 || ucScrollbar == 0xFF)
		SetScrollBarText(1, "A new musical 'RAINCOAT EXPOSE' A real eye-opener at The Seagull Theater.       Tickets available for all shows at the eyefind.info box office.       Do not miss 'Banging trash can lids for an hour' at the Liberteen Theater  What a racket!       Magic Curtain Theater presents 'Swinging Puppets' and a fantastic new play 'Shakespeare on Drugs' The bard trip.       'The Flying Whore' at the Amdram Theatre  Enormous talents!  Needs to be seen to be believed!       'WTF' A tale of online love showing at The Maginot Theater.       You'll be LOL       ROFL       Returns after a sell out UK tour 'ABANDON SHIT' back at the Seagull Theater.       Performance Nonsense 'Fairy Body Paint' at the Liberteen Theater  What a big bunch of fairies.       ~~~~~       ");

	// *advertising
	if(ucScrollbar == 2 || ucScrollbar == 0xFF)
	{
		// Get the credits string
		String strCredits = g_pCredits->GetCreditsString();

		// Append the clock to the credits string
		strCredits.Append("  ~~~~       ");

		// Set the scroll bar text
		SetScrollBarText(2, strCredits.Get());
	}
	
	// *clock (doesn't scroll)
	if(ucScrollbar == 3 || ucScrollbar == 0xFF)
		SetScrollBarText(3, "~~~~~       ");

	// *urls
	if(ucScrollbar == 4 || ucScrollbar == 0xFF)
		SetScrollBarText(4, "autoeroticar.com       chiropracticovernight.com       ilovetoblow.org       a-thousand-words.net       libertytreeonline.com       babiesovernight.com       love-meet.net       loveyourmeat.com       myonlineme.com       outsourceforamerica.com       pointclickshipabitch.com       weazelnews.com       piswasser.com       beanmachinecoffee.com       burgershot.net       electrictit.com       krapea.com       lipurgex.com       littlelacysurprisepageant.com       publiclibertyonline.com       vipluxuryringtones.com       americantravelguide.net       fruitcomputers.com       designerslave.com       erisfootware.com       eugenicsincorporated.com       flyhighpizzapie.com       gloryholethemepark.com       craplist.net       blogsnobs.org       libertycitypolice.com       sprunksoda.com       rustybrownsringdonuts.com       peepthatshit.com       lootandwank.com       myroomonline.net       dragonbrainthemovie.com       eatbiglogs.com       friendswithoutfaces.net       hornyhighschoolreunions.com       money2makemoney.com       poker-in-the-rear.com       redwoodcigarettes.com       roidsforlittleboys.com       ");
	
	// *comedyclub
	if(ucScrollbar == 5 || ucScrollbar == 0xFF)
		SetScrollBarText(5, "split sides proudly presents special guest -  ricky gervais - with resident comedian - katt williams -       ");

	// *traffic
	if(ucScrollbar == 6 || ucScrollbar == 0xFF)
		SetScrollBarText(6, "stay in sane       min speed 20 miles       max speed 30 miles       check your fuel       tiredness killed her       car thefts on the up. lock your car       be a courteous driver       watch your speed       ");

	// *news
	if(ucScrollbar == 7 || ucScrollbar == 0xFF)
		SetScrollBarText(7, "crime on an all time low.       ~~~~~       sudden influx illegal immigrants from balkan says mayor.       liberty city attracts more tourist than any other city in the world       ");
}

void CGame::GetScrollBarText(unsigned char ucScrollbar, char* szReturn)
{
	if(ucScrollbar > 7)
	{
		szReturn[0] = '\0';
		return;
	}

	strcpy(szReturn, (char *)(COffsets::VAR_ScrollBarText + ucScrollbar * 1300));
}

void CGame::SetScrollBarColor(unsigned int uiColor)
{
	*(DWORD *)(COffsets::VAR_ScrollBarColor) = ((uiColor >> 8) & 0xFFFFFF) + ((uiColor << 24) & 0xFF000000);
}

unsigned int CGame::GetScrollBarColor()
{
	unsigned int uiColor = *(DWORD *)(COffsets::VAR_ScrollBarColor);
	return ((uiColor << 8) & 0xFFFFFF00) + ((uiColor >> 24) & 0xFF);
}

String CGame::GetCurrentAreaName()
{
	String strAreaName;
	char * szAreaName = (char *)(COffsets::VAR_CurrentAreaName);

	for(int i = 0; i < 128; i += 2)
	{
		char c = szAreaName[i];

		if(c == 0)
			break;

		strAreaName += c;
	}

	return strAreaName;
}

String CGame::GetCurrentStreetName()
{
	String strStreetName;
	char * szStreetName = (char *)(COffsets::VAR_CurrentStreetName);

	for(int i = 0; i < 128; i += 2)
	{
		char c = szStreetName[i];

		if(c == 0)
			break;

		strStreetName += c;
	}

	return strStreetName;
}
void CGame::RemoveInitialLoadingScreens()
{
	// Legal, Legal 2, R*, R*N, GTA:IV, ...
	/*for( int i = 0; i < *(int *)(COffsets::VAR_NumLoadingScreens); ++i ) // For loading screen the make the black background
	{
		if( i < 2 )
		{
			*(DWORD *)(COffsets::VAR_FirstLoadingScreenType + i * 400) = ((i < 2) ? 0 : 0);
			*(DWORD *)(COffsets::VAR_FirstLoadingScreenDuration + i * 400) = 3600000;
		}

		if( i >= 2 && i <= 4 )
		{
			*(DWORD *)(COffsets::VAR_FirstLoadingScreenDuration + i * 400) = 0;
		}
	}*/

	/*
	// Legal, Legal 2, R*, R*N, GTA:IV, ...
	for(int i = 0; i < *(int *)(COffsets::VAR_NumLoadingScreens); ++i)
	{
		*(DWORD *)(COffsets::VAR_FirstLoadingScreenType + i * 400) = ((i <= 4) ? 4 : i);

		if(i <= 4)
			*(DWORD *)(COffsets::VAR_FirstLoadingScreenDuration + i * 400) = 0;
	}
	*/
	for( int i = 0; i < *(int *)(COffsets::VAR_NumLoadingScreens); ++i ) {
		// Disable legal notice
		if( i < 2 ) {
			*(DWORD *)(COffsets::VAR_FirstLoadingScreenType + i * 400) = ((i < 2) ? 0 : 0);
			*(DWORD *)(COffsets::VAR_FirstLoadingScreenDuration + i * 400) = 0;
		}

		// Disable GTA IV logo & other loading screens
		if( i == 4 )
			*(DWORD *)(COffsets::VAR_FirstLoadingScreenDuration + i * 400) = 0;

		/*if(i > 4) {
			//CLogFile::Printf("%d(%x/%p/%d)",i,*(DWORD *)(COffsets::VAR_FirstLoadingScreenType + i * 400),*(DWORD *)(COffsets::VAR_FirstLoadingScreenType + i * 400),*(DWORD *)(COffsets::VAR_FirstLoadingScreenType + i * 400));
			//*(DWORD *)(COffsets::VAR_FirstLoadingScreenDuration + i * 400) = 0;
			//*(DWORD *)(COffsets::VAR_FirstLoadingScreenType + i * 400) = 6;
		}*/
	}
}

bool CGame::GetScreenPositionFromWorldPosition(CVector3 vecWorldPosition, Vector2 &vecScreenPosition)
{
	// AFEA30 // AFF290 // B002E0
	DWORD dwAddress = (CGame::GetBase() + 0xAFF3A0);
    CVector3 * pWorldPosition = &vecWorldPosition;
    Vector2 * pScreenPosition = &vecScreenPosition;
	int iOnScreen;
    _asm
    {
            push pScreenPosition
            push 2					;game viewport id ; 1= mapicon
            push pWorldPosition
            call dwAddress
			mov eax, [esp+4]
			mov iOnScreen, eax
	}
	CVector3 vecScreenPos;
	bool onScreen = false;
	if(g_pCamera)
		onScreen = g_pCamera->IsOnScreen(vecWorldPosition);
	
	/*vecScreenPosition.X = vecScreenPos.fX;
	vecScreenPosition.Y = vecScreenPos.fY;*/

	return onScreen;
	//CLogFile::Printf("[W2S]WORLD(%f,%f,%f),SCREEN(%f,%f), BOOLOnSCREEN(%d,%d)",vecWorldPosition.fX,vecWorldPosition.fY,vecWorldPosition.fZ,fX,fY,iOnScreen,iResult);
}

void CGame::CreateExplosion(CVector3& vecPosition, unsigned int uiExplosionType, float fRadius, bool bSound, bool bInvisible, float fCameraShake)
{
	DWORD dwFunc = (CGame::GetBase() + 0x9609F0);
	CVector3 * pPosition = &vecPosition;
	_asm
	{
		push -1
		push 0
		push 0
		push 0
		push 0
		push 0
		push 0
		push 0
		push bInvisible
		push 0
		push fCameraShake
		push bSound
		push 0
		push 0
		push pPosition
		push fRadius
		push uiExplosionType
		push 0
		push 0
		call dwFunc
		add esp, 4Ch
	}
}

unsigned int CGame::CreateFire(CVector3 vecPosition, unsigned int uiStrength, unsigned char ucAllowedGenerations)
{
	return Scripting::StartScriptFire(vecPosition.fX, vecPosition.fY, vecPosition.fZ, ucAllowedGenerations, uiStrength);
}

bool CGame::DeleteFire(unsigned int uiFire)
{
	Scripting::RemoveScriptFire(uiFire);
	return true;
}

void * CGame::Alloc(DWORD dwSize)
{
	DWORD dwFunc = (CGame::GetBase() + 0x4011D0); // alloc
	void * pMemory = NULL;
	_asm
	{
		push dwSize
		call dwFunc
		mov pMemory, eax
		add esp, 4
	}
	return pMemory;
}

void CGame::Free(void * pMemory)
{
	DWORD dwFunc = (CGame::GetBase() + 0x5B1C10); // gta_free
	_asm
	{
		push pMemory
		call dwFunc
		add esp, 4
	}
}

void CGame::ToggleLazlowStation(bool bToggle)
{
	*(BYTE *)(GetBase() + 0xF38728) = (bToggle == false);
	*(BYTE *)(GetBase() + 0x166805C) = (bToggle == true);
}

void CGame::InitCutscene(char * szCutsceneName, bool bUnknown1, bool bUnknown2)
{
	DWORD dwFunc = (CGame::GetBase() + 0x866980);
	_asm
	{
		push bUnknown2
		push bUnknown1
		push szCutsceneName
		call dwFunc
		add esp, 0Ch
	}
}

void CGame::StopCutscene(char * szCutsceneName)
{
	DWORD dwFunc = (CGame::GetBase() + 0x865E10);
	_asm
	{
		push szCutsceneName
		call dwFunc
		add esp, 4
	}
}

void CGame::RequestAnimGroup(const char *szAnimGroup)
{
	/*for(int i = 0; i < 760; i++)
	{
		if(animationGroups[i].Get() == szAnimGroup)
		{
			if(!Scripting::HaveAnimsLoaded(animationGroups[i].Get()))
				Scripting::RequestAnims(animationGroups[i].Get());

			return;
		}
	}*/
	if(!Scripting::HaveAnimsLoaded(szAnimGroup))
		Scripting::RequestAnims(szAnimGroup);
}

void CGame::ReleaseAnimGroup(const char *szAnimGroup)
{
	/*for(int i = 0; i < 760; i++)
	{
		if(animationGroups[i].Get() == szAnimGroup)
		{
			if(Scripting::HaveAnimsLoaded(animationGroups[i].Get()))
				Scripting::RemoveAnims(animationGroups[i].Get());

			return;
		}
	}*/

	if(Scripting::HaveAnimsLoaded(szAnimGroup))
		Scripting::RemoveAnims(szAnimGroup);
}

DWORD CGame::GetNativeAddress(DWORD dwNative)
{
	DWORD dwFunc = COffsets::FUNC_ScrVM__FindNativeAddress;
	DWORD dwNativeFunc = NULL;
	_asm
	{
		push esi
		mov esi, dwNative
		call dwFunc
		pop esi
		mov dwNativeFunc, eax
	}

	if(dwNativeFunc != NULL)
		return dwNativeFunc;

	return -1;
}

void CGame::InitializeDefaultGameComponents()
{
	Scripting::SetZonePopulationType( "Zact1", 0 );
    Scripting::SetZonePopulationType( "Zact2", 0 );
    Scripting::SetZonePopulationType( "Zacti", 1 );
    Scripting::SetZonePopulationType( "Zacti1", 1 );
    Scripting::SetZonePopulationType( "ZAIRPT1", 2 );
    Scripting::SetZonePopulationType( "ZAIRPT2", 2 );
    Scripting::SetZonePopulationType( "ZAIRPT3", 2 );
    Scripting::SetZonePopulationType( "Zald1", 3 );
    Scripting::SetZonePopulationType( "Zald2", 3 );
    Scripting::SetZonePopulationType( "Zald3", 3 );
    Scripting::SetZonePopulationType( "Zald4", 3 );
    Scripting::SetZonePopulationType( "ZBEG1", 4 );
    Scripting::SetZonePopulationType( "ZBEG2", 4 );
    Scripting::SetZonePopulationType( "ZBECCT1", 5 );
    Scripting::SetZonePopulationType( "ZBECCT2", 5 );
    Scripting::SetZonePopulationType( "ZBECCT3", 5 );
    Scripting::SetZonePopulationType( "ZBECCT4", 5 );
    Scripting::SetZonePopulationType( "ZBECCT5", 5 );
    Scripting::SetZonePopulationType( "Zberc", 6 );
    Scripting::SetZonePopulationType( "Zberc1", 6 );
    Scripting::SetZonePopulationType( "ZBOAB1", 7 );
    Scripting::SetZonePopulationType( "ZBOAB2", 7 );
    Scripting::SetZonePopulationType( "ZBOAB3", 7 );
    Scripting::SetZonePopulationType( "ZBOAB4", 7 );
    Scripting::SetZonePopulationType( "ZBOAB5", 7 );
    Scripting::SetZonePopulationType( "ZBOAB6", 7 );
    Scripting::SetZonePopulationType( "ZBOULE1", 8 );
    Scripting::SetZonePopulationType( "ZBOULE2", 8 );
    Scripting::SetZonePopulationType( "ZBOULE3", 8 );
    Scripting::SetZonePopulationType( "ZBOULE4", 8 );
    Scripting::SetZonePopulationType( "ZCERV1", 9 );
    Scripting::SetZonePopulationType( "ZCERV2", 9 );
    Scripting::SetZonePopulationType( "ZCERV3", 9 );
    Scripting::SetZonePopulationType( "ZCERV4", 9 );
    Scripting::SetZonePopulationType( "Zcgar1", 10 );
    Scripting::SetZonePopulationType( "Zcgar2", 10 );
    Scripting::SetZonePopulationType( "Zcgar3", 10 );
    Scripting::SetZonePopulationType( "Zcgci1", 11 );
    Scripting::SetZonePopulationType( "Zcgci2", 11 );
    Scripting::SetZonePopulationType( "Zcgci3", 11 );
    Scripting::SetZonePopulationType( "Zcgci4", 11 );
    Scripting::SetZonePopulationType( "ZCHASE1", 12 );
    Scripting::SetZonePopulationType( "ZCHASE2", 12 );
    Scripting::SetZonePopulationType( "Zchin", 13 );
    Scripting::SetZonePopulationType( "Zchisl1", 14 );
    Scripting::SetZonePopulationType( "Zchisl2", 14 );
    Scripting::SetZonePopulationType( "ZCity1", 15 );
    Scripting::SetZonePopulationType( "ZCity2", 15 );
    Scripting::SetZonePopulationType( "Zcois1", 16 );
    Scripting::SetZonePopulationType( "Zcois3", 16 );
    Scripting::SetZonePopulationType( "Zcois4", 16 );
    Scripting::SetZonePopulationType( "Zcois5", 16 );
    Scripting::SetZonePopulationType( "Zcois8", 16 );
    Scripting::SetZonePopulationType( "ZDOWNT", 17 );
    Scripting::SetZonePopulationType( "Zeast", 18 );
    Scripting::SetZonePopulationType( "ZEHOK1", 19 );
    Scripting::SetZonePopulationType( "ZEHOK2", 19 );
    Scripting::SetZonePopulationType( "ZEHOK3", 19 );
    Scripting::SetZonePopulationType( "ZEHOK4", 19 );
    Scripting::SetZonePopulationType( "ZEHOK5", 19 );
    Scripting::SetZonePopulationType( "ZEHOK6", 19 );
    Scripting::SetZonePopulationType( "ZEHOK7", 19 );
    Scripting::SetZonePopulationType( "ZEHOK8", 19 );
    Scripting::SetZonePopulationType( "ZEHOK9", 19 );
    Scripting::SetZonePopulationType( "ZEHOK10", 19 );
    Scripting::SetZonePopulationType( "Zehol1", 20 );
    Scripting::SetZonePopulationType( "Zehol2", 20 );
    Scripting::SetZonePopulationType( "ZESTCT1", 21 );
    Scripting::SetZonePopulationType( "ZESTCT2", 21 );
    Scripting::SetZonePopulationType( "ZESTCT3", 21 );
    Scripting::SetZonePopulationType( "ZESTCT4", 21 );
    Scripting::SetZonePopulationType( "ZESTCT5", 21 );
    Scripting::SetZonePopulationType( "ZESTCT6", 21 );
    Scripting::SetZonePopulationType( "ZESTCT7", 21 );
    Scripting::SetZonePopulationType( "ZESTCT8", 21 );
    Scripting::SetZonePopulationType( "ZESTCT9", 21 );
    Scripting::SetZonePopulationType( "Zexc1", 22 );
    Scripting::SetZonePopulationType( "Zexc2", 22 );
    Scripting::SetZonePopulationType( "Zexc3", 22 );
    Scripting::SetZonePopulationType( "Zexc4", 22 );
    Scripting::SetZonePopulationType( "ZFIEPR1", 24 );
    Scripting::SetZonePopulationType( "ZFIEPR2", 24 );
    Scripting::SetZonePopulationType( "ZFIEPR3", 24 );
    Scripting::SetZonePopulationType( "ZFIEPR4", 24 );
    Scripting::SetZonePopulationType( "ZFIEPR5", 24 );
    Scripting::SetZonePopulationType( "ZFIEPR6", 24 );
    Scripting::SetZonePopulationType( "Zfisn", 25 );
    Scripting::SetZonePopulationType( "zfiss1", 26 );
    Scripting::SetZonePopulationType( "zfiss2", 26 );
    Scripting::SetZonePopulationType( "zfiss3", 26 );
    Scripting::SetZonePopulationType( "zfiss4", 26 );
    Scripting::SetZonePopulationType( "zfiss5", 26 );
    Scripting::SetZonePopulationType( "Zfort", 27 );
    Scripting::SetZonePopulationType( "ZFRIS1", 23 );
    Scripting::SetZonePopulationType( "ZFRIS2", 23 );
    Scripting::SetZonePopulationType( "ZGelf1", 28 );
    Scripting::SetZonePopulationType( "ZGelf2", 28 );
    Scripting::SetZonePopulationType( "ZGelf3", 28 );
    Scripting::SetZonePopulationType( "ZGelf4", 28 );
    Scripting::SetZonePopulationType( "ZGelf5", 28 );
    Scripting::SetZonePopulationType( "ZGelf6", 28 );
    Scripting::SetZonePopulationType( "ZGelf7", 28 );
    Scripting::SetZonePopulationType( "ZHap1", 29 );
    Scripting::SetZonePopulationType( "ZHap2", 29 );
    Scripting::SetZonePopulationType( "ZHap3", 29 );
    Scripting::SetZonePopulationType( "Zhat", 30 );
    Scripting::SetZonePopulationType( "ZHOVEB1", 31 );
    Scripting::SetZonePopulationType( "ZHOVEB2", 31 );
    Scripting::SetZonePopulationType( "ZHOVEB3", 31 );
    Scripting::SetZonePopulationType( "ZINDUS1", 32 );
    Scripting::SetZonePopulationType( "ZINDUS2", 32 );
    Scripting::SetZonePopulationType( "ZINDUS3", 32 );
    Scripting::SetZonePopulationType( "ZINDUS4", 32 );
    Scripting::SetZonePopulationType( "ZINDUS5", 32 );
    Scripting::SetZonePopulationType( "ZINDUS6", 32 );
    Scripting::SetZonePopulationType( "Zital", 33 );
    Scripting::SetZonePopulationType( "Zlanc1", 34 );
    Scripting::SetZonePopulationType( "Zlanc2", 34 );
    Scripting::SetZonePopulationType( "Zlance", 35 );
    Scripting::SetZonePopulationType( "ZLBAY1", 36 );
    Scripting::SetZonePopulationType( "ZLBAY2", 36 );
    Scripting::SetZonePopulationType( "ZLBAY3", 36 );
    Scripting::SetZonePopulationType( "ZLBAY4", 36 );
    Scripting::SetZonePopulationType( "ZLBAY5", 36 );
    Scripting::SetZonePopulationType( "Zleft1", 37 );
    Scripting::SetZonePopulationType( "Zleft2", 37 );
    Scripting::SetZonePopulationType( "Zlowe1", 38 );
    Scripting::SetZonePopulationType( "Zlowe2", 38 );
    Scripting::SetZonePopulationType( "Zmdw1", 39 );
    Scripting::SetZonePopulationType( "Zmdw2", 39 );
    Scripting::SetZonePopulationType( "Zmdw3", 39 );
    Scripting::SetZonePopulationType( "Zmeat", 42 );
    Scripting::SetZonePopulationType( "ZMHILLS", 41 );
    Scripting::SetZonePopulationType( "Zmide", 43 );
    Scripting::SetZonePopulationType( "Zmidpa", 44 );
    Scripting::SetZonePopulationType( "ZMPARK1", 40 );
    Scripting::SetZonePopulationType( "ZMPARK2", 40 );
    Scripting::SetZonePopulationType( "ZMPARK3", 40 );
    Scripting::SetZonePopulationType( "ZMPARK4", 40 );
    Scripting::SetZonePopulationType( "Znhol1", 45 );
    Scripting::SetZonePopulationType( "Znhol2", 45 );
    Scripting::SetZonePopulationType( "Znorm", 47 );
    Scripting::SetZonePopulationType( "Znort1", 48 );
    Scripting::SetZonePopulationType( "Znort2", 48 );
    Scripting::SetZonePopulationType( "Znort3", 48 );
    Scripting::SetZonePopulationType( "Znort4", 48 );
    Scripting::SetZonePopulationType( "Znort5", 48 );
    Scripting::SetZonePopulationType( "ZNRDNS1", 46 );
    Scripting::SetZonePopulationType( "ZNRDNS2", 46 );
    Scripting::SetZonePopulationType( "ZNRDNS3", 46 );
    Scripting::SetZonePopulationType( "ZNRDNS4", 46 );
    Scripting::SetZonePopulationType( "ZNRDNS5", 46 );
    Scripting::SetZonePopulationType( "ZNRDNS6", 46 );
    Scripting::SetZonePopulationType( "ZNRDNS7", 46 );
    Scripting::SetZonePopulationType( "ZOUTLO", 49 );
    Scripting::SetZonePopulationType( "Zport", 50 );
    Scripting::SetZonePopulationType( "Zport1", 50 );
    Scripting::SetZonePopulationType( "Zpres", 51 );
    Scripting::SetZonePopulationType( "Zpurg1", 52 );
    Scripting::SetZonePopulationType( "Zpurg2", 52 );
    Scripting::SetZonePopulationType( "Zpurg3", 52 );
    Scripting::SetZonePopulationType( "ZRHIL1", 53 );
    Scripting::SetZonePopulationType( "ZRHIL2", 53 );
    Scripting::SetZonePopulationType( "ZRHIL3", 53 );
    Scripting::SetZonePopulationType( "ZRHIL4", 53 );
    Scripting::SetZonePopulationType( "ZRHIL5", 53 );
    Scripting::SetZonePopulationType( "ZRHIL6", 53 );
    Scripting::SetZonePopulationType( "ZRHIL7", 53 );
    Scripting::SetZonePopulationType( "ZRHIL8", 53 );
    Scripting::SetZonePopulationType( "ZRHIL9", 53 );
    Scripting::SetZonePopulationType( "ZRHIL10", 53 );
    Scripting::SetZonePopulationType( "ZRHIL11", 53 );
    Scripting::SetZonePopulationType( "ZRHIL12", 53 );
    Scripting::SetZonePopulationType( "ZRHIL13", 53 );
    Scripting::SetZonePopulationType( "ZRHIL14", 53 );
    Scripting::SetZonePopulationType( "ZRHIL15", 53 );
    Scripting::SetZonePopulationType( "ZRHIL16", 53 );
    Scripting::SetZonePopulationType( "ZSHTLER", 55 );
    Scripting::SetZonePopulationType( "ZSLOPES", 56 );
    Scripting::SetZonePopulationType( "ZSOHAN1", 54 );
    Scripting::SetZonePopulationType( "ZSOHAN2", 54 );
    Scripting::SetZonePopulationType( "Zstar", 57 );
    Scripting::SetZonePopulationType( "ZSTEI1", 58 );
    Scripting::SetZonePopulationType( "ZSTEI2", 58 );
    Scripting::SetZonePopulationType( "ZSTEI3", 58 );
    Scripting::SetZonePopulationType( "ZSTEI4", 58 );
    Scripting::SetZonePopulationType( "ZSTEI5", 58 );
    Scripting::SetZonePopulationType( "ZSTEI6", 58 );
    Scripting::SetZonePopulationType( "ZSTEI7", 58 );
    Scripting::SetZonePopulationType( "ZSTEI8", 58 );
    Scripting::SetZonePopulationType( "ZSTEI9", 58 );
    Scripting::SetZonePopulationType( "ZSTEI10", 58 );
    Scripting::SetZonePopulationType( "ZSTEI11", 58 );
    Scripting::SetZonePopulationType( "Zsuff1", 59 );
    Scripting::SetZonePopulationType( "Zsuff2", 59 );
    Scripting::SetZonePopulationType( "Ztri", 60 );
    Scripting::SetZonePopulationType( "Ztudo1", 61 );
    Scripting::SetZonePopulationType( "Ztudo2", 61 );
    Scripting::SetZonePopulationType( "Zvarh", 62 );
    Scripting::SetZonePopulationType( "Zwest1", 63 );
    Scripting::SetZonePopulationType( "Zwest2", 63 );
    Scripting::SetZonePopulationType( "Zwestm", 64 );
    Scripting::SetZonePopulationType( "ZWILIS1", 65 );
    Scripting::SetZonePopulationType( "ZAIRU0", 66 );
    Scripting::SetZonePopulationType( "ZAIRU1", 66 );
    Scripting::SetZonePopulationType( "ZAIRU2", 66 );
    Scripting::SetZonePopulationType( "ZAIRU3", 66 );
    Scripting::SetZonePopulationType( "ZAIRU4", 66 );
    Scripting::SetZonePopulationType( "ZAIRU5", 66 );
    Scripting::SetZonePopulationType( "ZAIRU6", 66 );
    Scripting::SetZonePopulationType( "ZAIRU7", 66 );
    Scripting::SetZonePopulationType( "ZAIRU8", 66 );
    Scripting::SetZonePopulationType( "ZAIRU9", 66 );

	Scripting::SetZoneScumminess( "Zact1", 4 );
    Scripting::SetZoneScumminess( "Zact2", 4 );
    Scripting::SetZoneScumminess( "Zacti", 5 );
    Scripting::SetZoneScumminess( "Zacti1", 5 );
    Scripting::SetZoneScumminess( "ZAIRPT1", 3 );
    Scripting::SetZoneScumminess( "ZAIRPT2", 3 );
    Scripting::SetZoneScumminess( "ZAIRPT3", 3 );
    Scripting::SetZoneScumminess( "Zald1", 3 );
    Scripting::SetZoneScumminess( "Zald2", 3 );
    Scripting::SetZoneScumminess( "Zald3", 3 );
    Scripting::SetZoneScumminess( "Zald4", 3 );
    Scripting::SetZoneScumminess( "ZBEG1", 0 );
    Scripting::SetZoneScumminess( "ZBEG2", 0 );
    Scripting::SetZoneScumminess( "ZBECCT1", 3 );
    Scripting::SetZoneScumminess( "ZBECCT2", 3 );
    Scripting::SetZoneScumminess( "ZBECCT3", 3 );
    Scripting::SetZoneScumminess( "ZBECCT4", 3 );
    Scripting::SetZoneScumminess( "ZBECCT5", 3 );
    Scripting::SetZoneScumminess( "Zberc", 1 );
    Scripting::SetZoneScumminess( "Zberc1", 1 );
    Scripting::SetZoneScumminess( "ZBOAB1", 3 );
    Scripting::SetZoneScumminess( "ZBOAB2", 3 );
    Scripting::SetZoneScumminess( "ZBOAB3", 3 );
    Scripting::SetZoneScumminess( "ZBOAB4", 3 );
    Scripting::SetZoneScumminess( "ZBOAB5", 3 );
    Scripting::SetZoneScumminess( "ZBOAB6", 3 );
    Scripting::SetZoneScumminess( "ZBOULE1", 4 );
    Scripting::SetZoneScumminess( "ZBOULE2", 4 );
    Scripting::SetZoneScumminess( "ZBOULE3", 4 );
    Scripting::SetZoneScumminess( "ZBOULE4", 4 );
    Scripting::SetZoneScumminess( "ZCERV1", 3 );
    Scripting::SetZoneScumminess( "ZCERV2", 3 );
    Scripting::SetZoneScumminess( "ZCERV3", 3 );
    Scripting::SetZoneScumminess( "ZCERV4", 3 );
    Scripting::SetZoneScumminess( "Zcgar1", 0 );
    Scripting::SetZoneScumminess( "Zcgar2", 0 );
    Scripting::SetZoneScumminess( "Zcgar3", 0 );
    Scripting::SetZoneScumminess( "Zcgci1", 0 );
    Scripting::SetZoneScumminess( "Zcgci2", 0 );
    Scripting::SetZoneScumminess( "Zcgci3", 0 );
    Scripting::SetZoneScumminess( "Zcgci4", 0 );
    Scripting::SetZoneScumminess( "ZCHASE1", 5 );
    Scripting::SetZoneScumminess( "ZCHASE2", 5 );
    Scripting::SetZoneScumminess( "Zchin", 3 );
    Scripting::SetZoneScumminess( "Zchisl1", 4 );
    Scripting::SetZoneScumminess( "Zchisl2", 4 );
    Scripting::SetZoneScumminess( "ZCity1", 0 );
    Scripting::SetZoneScumminess( "ZCity2", 0 );
    Scripting::SetZoneScumminess( "Zcois1", 4 );
    Scripting::SetZoneScumminess( "Zcois3", 4 );
    Scripting::SetZoneScumminess( "Zcois4", 4 );
    Scripting::SetZoneScumminess( "Zcois5", 4 );
    Scripting::SetZoneScumminess( "Zcois8", 4 );
    Scripting::SetZoneScumminess( "ZDOWNT", 3 );
    Scripting::SetZoneScumminess( "Zeast", 2 );
    Scripting::SetZoneScumminess( "ZEHOK1", 2 );
    Scripting::SetZoneScumminess( "ZEHOK2", 2 );
    Scripting::SetZoneScumminess( "ZEHOK3", 2 );
    Scripting::SetZoneScumminess( "ZEHOK4", 2 );
    Scripting::SetZoneScumminess( "ZEHOK5", 2 );
    Scripting::SetZoneScumminess( "ZEHOK6", 2 );
    Scripting::SetZoneScumminess( "ZEHOK7", 2 );
    Scripting::SetZoneScumminess( "ZEHOK8", 2 );
    Scripting::SetZoneScumminess( "ZEHOK9", 2 );
    Scripting::SetZoneScumminess( "ZEHOK10", 2 );
    Scripting::SetZoneScumminess( "Zehol1", 5 );
    Scripting::SetZoneScumminess( "Zehol2", 5 );
    Scripting::SetZoneScumminess( "ZESTCT1", 3 );
    Scripting::SetZoneScumminess( "ZESTCT2", 3 );
    Scripting::SetZoneScumminess( "ZESTCT3", 3 );
    Scripting::SetZoneScumminess( "ZESTCT4", 3 );
    Scripting::SetZoneScumminess( "ZESTCT5", 3 );
    Scripting::SetZoneScumminess( "ZESTCT6", 3 );
    Scripting::SetZoneScumminess( "ZESTCT7", 3 );
    Scripting::SetZoneScumminess( "ZESTCT8", 3 );
    Scripting::SetZoneScumminess( "ZESTCT9", 3 );
    Scripting::SetZoneScumminess( "Zexc1", 0 );
    Scripting::SetZoneScumminess( "Zexc2", 0 );
    Scripting::SetZoneScumminess( "Zexc3", 0 );
    Scripting::SetZoneScumminess( "Zexc4", 0 );
    Scripting::SetZoneScumminess( "ZFIEPR1", 5 );
    Scripting::SetZoneScumminess( "ZFIEPR2", 5 );
    Scripting::SetZoneScumminess( "ZFIEPR3", 5 );
    Scripting::SetZoneScumminess( "ZFIEPR4", 5 );
    Scripting::SetZoneScumminess( "ZFIEPR5", 5 );
    Scripting::SetZoneScumminess( "ZFIEPR6", 5 );
    Scripting::SetZoneScumminess( "Zfisn", 2 );
    Scripting::SetZoneScumminess( "zfiss1", 2 );
    Scripting::SetZoneScumminess( "zfiss2", 2 );
    Scripting::SetZoneScumminess( "zfiss3", 2 );
    Scripting::SetZoneScumminess( "zfiss4", 2 );
    Scripting::SetZoneScumminess( "zfiss5", 2 );
    Scripting::SetZoneScumminess( "Zfort", 4 );
    Scripting::SetZoneScumminess( "ZFRIS1", 4 );
    Scripting::SetZoneScumminess( "ZFRIS2", 4 );
    Scripting::SetZoneScumminess( "ZGelf1", 0 );
    Scripting::SetZoneScumminess( "ZGelf2", 0 );
    Scripting::SetZoneScumminess( "ZGelf3", 0 );
    Scripting::SetZoneScumminess( "ZGelf4", 0 );
    Scripting::SetZoneScumminess( "ZGelf5", 0 );
    Scripting::SetZoneScumminess( "ZGelf6", 0 );
    Scripting::SetZoneScumminess( "ZGelf7", 0 );
    Scripting::SetZoneScumminess( "ZHap1", 1 );
    Scripting::SetZoneScumminess( "ZHap2", 1 );
    Scripting::SetZoneScumminess( "ZHap3", 1 );
    Scripting::SetZoneScumminess( "Zhat", 1 );
    Scripting::SetZoneScumminess( "ZHOVEB1", 3 );
    Scripting::SetZoneScumminess( "ZHOVEB2", 3 );
    Scripting::SetZoneScumminess( "ZHOVEB3", 3 );
    Scripting::SetZoneScumminess( "ZINDUS1", 5 );
    Scripting::SetZoneScumminess( "ZINDUS2", 5 );
    Scripting::SetZoneScumminess( "ZINDUS3", 5 );
    Scripting::SetZoneScumminess( "ZINDUS4", 5 );
    Scripting::SetZoneScumminess( "ZINDUS5", 5 );
    Scripting::SetZoneScumminess( "ZINDUS6", 5 );
    Scripting::SetZoneScumminess( "Zital", 3 );
    Scripting::SetZoneScumminess( "Zlanc1", 1 );
    Scripting::SetZoneScumminess( "Zlanc2", 1 );
    Scripting::SetZoneScumminess( "Zlance", 1 );
    Scripting::SetZoneScumminess( "ZLBAY1", 4 );
    Scripting::SetZoneScumminess( "ZLBAY2", 4 );
    Scripting::SetZoneScumminess( "ZLBAY3", 4 );
    Scripting::SetZoneScumminess( "ZLBAY4", 4 );
    Scripting::SetZoneScumminess( "ZLBAY5", 4 );
    Scripting::SetZoneScumminess( "Zleft1", 1 );
    Scripting::SetZoneScumminess( "Zleft2", 1 );
    Scripting::SetZoneScumminess( "Zlowe1", 3 );
    Scripting::SetZoneScumminess( "Zlowe2", 3 );
    Scripting::SetZoneScumminess( "Zmdw1", 1 );
    Scripting::SetZoneScumminess( "Zmdw2", 1 );
    Scripting::SetZoneScumminess( "Zmdw3", 1 );
    Scripting::SetZoneScumminess( "Zmeat", 2 );
    Scripting::SetZoneScumminess( "ZMHILLS", 2 );
    Scripting::SetZoneScumminess( "Zmide", 1 );
    Scripting::SetZoneScumminess( "Zmidpa", 1 );
    Scripting::SetZoneScumminess( "ZMPARK1", 2 );
    Scripting::SetZoneScumminess( "ZMPARK2", 2 );
    Scripting::SetZoneScumminess( "ZMPARK3", 2 );
    Scripting::SetZoneScumminess( "ZMPARK4", 2 );
    Scripting::SetZoneScumminess( "Znhol1", 4 );
    Scripting::SetZoneScumminess( "Znhol2", 4 );
    Scripting::SetZoneScumminess( "Znorm", 2 );
    Scripting::SetZoneScumminess( "Znort1", 5 );
    Scripting::SetZoneScumminess( "Znort2", 5 );
    Scripting::SetZoneScumminess( "Znort3", 5 );
    Scripting::SetZoneScumminess( "Znort4", 5 );
    Scripting::SetZoneScumminess( "Znort5", 5 );
    Scripting::SetZoneScumminess( "ZNRDNS1", 2 );
    Scripting::SetZoneScumminess( "ZNRDNS2", 2 );
    Scripting::SetZoneScumminess( "ZNRDNS3", 2 );
    Scripting::SetZoneScumminess( "ZNRDNS4", 2 );
    Scripting::SetZoneScumminess( "ZNRDNS5", 2 );
    Scripting::SetZoneScumminess( "ZNRDNS6", 2 );
    Scripting::SetZoneScumminess( "ZNRDNS7", 2 );
    Scripting::SetZoneScumminess( "ZOUTLO", 3 );
    Scripting::SetZoneScumminess( "Zport", 4 );
    Scripting::SetZoneScumminess( "Zport1", 4 );
    Scripting::SetZoneScumminess( "Zpres", 3 );
    Scripting::SetZoneScumminess( "Zpurg1", 2 );
    Scripting::SetZoneScumminess( "Zpurg2", 2 );
    Scripting::SetZoneScumminess( "Zpurg3", 2 );
    Scripting::SetZoneScumminess( "ZRHIL1", 1 );
    Scripting::SetZoneScumminess( "ZRHIL2", 1 );
    Scripting::SetZoneScumminess( "ZRHIL3", 1 );
    Scripting::SetZoneScumminess( "ZRHIL4", 1 );
    Scripting::SetZoneScumminess( "ZRHIL5", 1 );
    Scripting::SetZoneScumminess( "ZRHIL6", 1 );
    Scripting::SetZoneScumminess( "ZRHIL7", 1 );
    Scripting::SetZoneScumminess( "ZRHIL8", 1 );
    Scripting::SetZoneScumminess( "ZRHIL9", 1 );
    Scripting::SetZoneScumminess( "ZRHIL10", 1 );
    Scripting::SetZoneScumminess( "ZRHIL11", 1 );
    Scripting::SetZoneScumminess( "ZRHIL12", 1 );
    Scripting::SetZoneScumminess( "ZRHIL13", 1 );
    Scripting::SetZoneScumminess( "ZRHIL14", 1 );
    Scripting::SetZoneScumminess( "ZRHIL15", 1 );
    Scripting::SetZoneScumminess( "ZRHIL16", 1 );
    Scripting::SetZoneScumminess( "ZSHTLER", 4 );
    Scripting::SetZoneScumminess( "ZSLOPES", 1 );
    Scripting::SetZoneScumminess( "ZSOHAN1", 5 );
    Scripting::SetZoneScumminess( "ZSOHAN1", 5 );
    Scripting::SetZoneScumminess( "Zstar", 2 );
    Scripting::SetZoneScumminess( "ZSTEI1", 3 );
    Scripting::SetZoneScumminess( "ZSTEI2", 3 );
    Scripting::SetZoneScumminess( "ZSTEI3", 3 );
    Scripting::SetZoneScumminess( "ZSTEI4", 3 );
    Scripting::SetZoneScumminess( "ZSTEI5", 3 );
    Scripting::SetZoneScumminess( "ZSTEI6", 3 );
    Scripting::SetZoneScumminess( "ZSTEI7", 3 );
    Scripting::SetZoneScumminess( "ZSTEI8", 3 );
    Scripting::SetZoneScumminess( "ZSTEI9", 3 );
    Scripting::SetZoneScumminess( "ZSTEI10", 3 );
    Scripting::SetZoneScumminess( "ZSTEI11", 3 );
    Scripting::SetZoneScumminess( "Zsuff1", 2 );
    Scripting::SetZoneScumminess( "Zsuff1", 2 );
    Scripting::SetZoneScumminess( "Ztri", 1 );
    Scripting::SetZoneScumminess( "Ztudo1", 3 );
    Scripting::SetZoneScumminess( "Ztudo2", 3 );
    Scripting::SetZoneScumminess( "Zvarh", 4 );
    Scripting::SetZoneScumminess( "Zwest1", 0 );
    Scripting::SetZoneScumminess( "Zwest2", 0 );
    Scripting::SetZoneScumminess( "Zwestm", 1 );
    Scripting::SetZoneScumminess( "ZWILIS1", 2 );
    Scripting::SetZoneScumminess( "ZAIRU0", 3 );
    Scripting::SetZoneScumminess( "ZAIRU1", 3 );
    Scripting::SetZoneScumminess( "ZAIRU2", 3 );
    Scripting::SetZoneScumminess( "ZAIRU3", 3 );
    Scripting::SetZoneScumminess( "ZAIRU4", 3 );
    Scripting::SetZoneScumminess( "ZAIRU5", 3 );
    Scripting::SetZoneScumminess( "ZAIRU6", 3 );
    Scripting::SetZoneScumminess( "ZAIRU7", 3 );
    Scripting::SetZoneScumminess( "ZAIRU8", 3 );
    Scripting::SetZoneScumminess( "ZAIRU9", 3 );
}

void CGame::PatchWorldAndTrain() 
{
	DWORD dwCallAddress;

	// Patch vehicles(our own vehicle generator(for trains etc)) and other world stuff 
	//CLogFile::PrintDebugf("Patching 0x438D00 Address 1");
	DWORD dwPatchAddress = (GetBase() + 0x841DD0);
	_asm call dwPatchAddress;
	//CLogFile::PrintDebugf("Patching 0x438D00 Address 2");
	dwPatchAddress = (GetBase() + 0x817F40);
	_asm call dwPatchAddress;
	//CLogFile::PrintDebugf("Patching 0x438D00 Address 3");
	dwPatchAddress = (GetBase() + 0x94C4F0);
	_asm call dwPatchAddress;
	//CLogFile::PrintDebugf("Patching 0x438D00 Address 4");
	//dwPatchAddress = (GetBase() + 0xA1C290); // Seems vehicle create (generator) function "AMBIENT_JET_TAXI" "AMBIENT_JET" etc etc...
	//_asm call dwPatchAddress;
	//CLogFile::PrintDebugf("Patching 0x438D00 Address 5");
	dwPatchAddress = (GetBase() + 0x8BE200);
	_asm call dwPatchAddress;
	//CLogFile::PrintDebugf("Patching 0x438D00 Address 6");
	//dwPatchAddress = (GetBase() + 0x439200); // Seems any important vehicle register function
	//_asm call dwPatchAddress;
	//DWORD dwTrainCreate = (CGame::GetBase() + 0x94A700);
	//_asm call dwTrainCreate;

	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  1");
	//dwCallAddress = (CGame::GetBase() + 0x9FF8E0); // Unkown
	//_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  2");
	dwCallAddress = (CGame::GetBase() + 0xA3C4D0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  3");
	dwCallAddress = (CGame::GetBase() + 0x9FFBE0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  4");
	//dwCallAddress = (CGame::GetBase() + 0x8FEEA0); // Seems to be a script
	//_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  5");
	//dwCallAddress = (CGame::GetBase() + 0x81FBD0); // Seems to be a script
	//_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  6");
	//dwCallAddress = (CGame::GetBase() + 0x8ACD50); // Seems to be a script
	//_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  7");
	dwCallAddress = (CGame::GetBase() + 0x85D730);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  8");
	dwCallAddress = (CGame::GetBase() + 0x870900);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  9");
	dwCallAddress = (CGame::GetBase() + 0x7B7F70);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  10");

	dwCallAddress = (CGame::GetBase() + 0x902200);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  11");
	//dwCallAddress = (CGame::GetBase() + 0x96C750); // Unkown
	//_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  12");
	dwCallAddress = (CGame::GetBase() + 0x444440);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  13");
	dwCallAddress = (CGame::GetBase() + 0x90A890);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  14");
	dwCallAddress = (CGame::GetBase() + 0x425940);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  15");

	dwCallAddress = (CGame::GetBase() + 0x8D9FC0);
	DWORD dwCallAddAddress = (CGame::GetBase() + 0x13501E8);
	_asm
	{
		mov ecx, dwCallAddAddress
		call dwCallAddress
	}
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  16");

	dwCallAddress = (CGame::GetBase() + 0x809F60);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  17");
	dwCallAddress = (CGame::GetBase() + 0x8184F0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  18");

	// NO VEHICLE CREATION YET!!
	
	dwCallAddress = (CGame::GetBase() + 0x7B4700);
	dwCallAddAddress = (CGame::GetBase() + 0x10F47E0);
	DWORD dwCallSecondAddAddress = (CGame::GetBase() + 0x10D0170);
	DWORD dwCallThirdAddAddress = (CGame::GetBase() + 0x10E1984);
	DWORD dwCallFourthAddAddress = (CGame::GetBase() + 0xF0E4A0);
	DWORD dwCallFinalAddress = (CGame::GetBase() + 0x427000);
	short shFirst = (CGame::GetBase() + 0x421672);
	//WORD dwTemp;

	//NOTE: Assembler VS 2010 doesn't recognize jnz and jn... 
	/*
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.1");
	_asm
	{
		mov ecx, dwCallAddAddress
		call dwCallAddress
		or esi, 0FFFFFFFFh
		cmp dwCallSecondAddAddress, 1
		mov edi, 12h
	}
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.2");
	_asm
	{
		pushf
		pop ax
		and ax, 0x100
		mov [dwTemp], ax
		push ax
		popf
	}
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.3");
	if(!dwTemp) 
	{
		CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.3-JUMP");
		_asm jmp shFirst; 
	}
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.4");
	_asm 
	{ 
		cmp dwCallThirdAddAddress, esi
	}
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.5");
	_asm
	{
		pushf
		pop ax
		and ax, 0x100
		mov [dwTemp], ax
		push ax
		popf
	}
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.6");
	if(!dwTemp) 
	{
		CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.6-JUMP");
		_asm jmp shFirst; 
	}
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.7");
	_asm 
	{ 
		cmp dwCallFourthAddAddress, edi 
	}
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.8");
	_asm
	{
		pushf
		pop ax
		and ax, 0x100
		mov [dwTemp], ax
		push ax
		popf
	}
	if(!dwTemp) 
	{
		CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.8-JUMP");
		_asm jmp shFirst; 
	}
	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.9");
	_asm call dwCallFinalAddress;

	CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  19.COMPLETE");
	*/
	dwCallAddress = (CGame::GetBase() + 0x7A8F20);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  20");
	/*dwCallAddress = (CGame::GetBase() + 0x88F3C0); // Seems to be a script
	dwCallAddAddress = (CGame::GetBase() + 0xF21A68);
	_asm 
	{
		mov ecx, dwCallAddAddress	
		call dwCallAddress
	}*/
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  21");
	//dwCallAddress = (CGame::GetBase() + 0x7CBFF0);
	//_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  22");
	dwCallAddress = (CGame::GetBase() + 0x828670);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  23");
	dwCallAddress = (CGame::GetBase() + 0x8240E0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  24");
	/*dwCallAddress = (CGame::GetBase() + 0x7B3BE0); // Seems to be a script
	dwCallAddAddress = (CGame::GetBase() + 0x10F47E0);
	_asm 
	{
		mov ecx, dwCallAddAddress	
		call dwCallAddress
	}*/
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  25");
	dwCallAddress = (CGame::GetBase() + 0x81B490);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  26");
	dwCallAddress = (CGame::GetBase() + 0x9209D0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  27");
	dwCallAddress = (CGame::GetBase() + 0x4431C0);
	dwCallAddAddress = (CGame::GetBase() + 0x10F47F8);
	_asm 
	{
		mov ecx, dwCallAddAddress
		push ecx
		call dwCallAddress
		add esp, 4
	}
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  28");
	dwCallAddress = (CGame::GetBase() + 0x865BA0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  29");
	dwCallAddress = (CGame::GetBase() + 0x8AE530);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  30");
	//dwCallAddress = (CGame::GetBase() + 0x8DF1A0); // Seems to be a script
	//_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  31");
	dwCallAddress = (CGame::GetBase() + 0x8C16E0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  32");
	dwCallAddress = (CGame::GetBase() + 0x945120);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  33");
	dwCallAddress = (CGame::GetBase() + 0x93F420);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  34");
	dwCallAddress = (CGame::GetBase() + 0x948BE0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  35");
	dwCallAddress = (CGame::GetBase() + 0x8691C0);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  36");
	dwCallAddress = (CGame::GetBase() + 0x889D60);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  37");
	/*dwCallAddress = (CGame::GetBase() + 0x884180); // Seems to be a script
	dwCallAddAddress = (CGame::GetBase() + 0x11E8F0C);
	_asm 
	{
		mov ecx, dwCallAddAddress
		call dwCallAddress
	}*/
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  38");
	/*dwCallAddress = (CGame::GetBase() + 0x91EDB0); // Unkown
	dwCallAddAddress = (CGame::GetBase() + 0x152F770);
	_asm 
	{
		mov ecx, dwCallAddAddress
		call dwCallAddress
	}*/
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  39");
	dwCallAddress = (CGame::GetBase() + 0x93F400);
	_asm call dwCallAddress;
	//CLogFile::PrintDebugf("PATCH TRAIN/WORLD ADDITION  40");

	// NO VEHICLE CREATION YET!!
}

unsigned int CGame::GetHashFromString(const char *szString)
{
	DWORD dwAddress = (CGame::GetBase() + 0x5A8290);
	unsigned int uiHash;
	_asm
	{
		mov eax, szString
		call dwAddress
		mov uiHash, eax
	}
	return uiHash;
}