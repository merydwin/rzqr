//============== IV: Multiplayer - http://code.iv-multiplayer.com ==============
//
// File: CLocalPlayer.cpp
// Project: Client.Core
// Author(s): jenksta
//            Einstein
//            Leopard
//            MaVe
//            mabako
// License: See LICENSE in root directory
//
//==============================================================================

#include "CLocalPlayer.h"
#include "CPlayerManager.h"
#include "CNetworkManager.h"
#include "CVehicleManager.h"
#include "CStreamer.h"
#include "CChatWindow.h"
#include "CInputWindow.h"
#include "KeySync.h"
#include "Patcher/CPatcher.h"
#include "CCutsceneInteriors.h"
#include "COffsets.h"
#include "CCamera.h"
#include "CClientScriptManager.h"
#include "CFireManager.h"
#include "CFileTransfer.h"
#include "CCamera.h"

extern CNetworkManager		* g_pNetworkManager;
extern CPlayerManager		* g_pPlayerManager;
extern CVehicleManager		* g_pVehicleManager;
extern CStreamer			* g_pStreamer;
extern CChatWindow			* g_pChatWindow;
extern CInputWindow			* g_pInputWindow;
extern CCamera				* g_pCamera;
extern bool					m_bControlsDisabled;
extern CClientScriptManager * g_pClientScriptManager;
extern CFireManager			* g_pFireManager;
extern CFileTransfer		* g_pFileTransfer;

void * pAddress = NULL;
void * pReturnAddress = NULL;

extern CLocalPlayer * g_pLocalPlayer;
extern CCamera * g_pCamera;

void GetLocalPlayerSpawnPosition(int, CVector3 * vecSpawnPosition, float * fAngle)
{
	_asm
	{
		pushad
	}

	// Get the saved spawn position
	g_pLocalPlayer->GetSpawnPosition(vecSpawnPosition);

	// Get the desired angle
	*fAngle = g_pLocalPlayer->GetSpawnRotation();

	_asm
	{
		popad
	}
}

// Params seem to be IVPed * ped, Vector3 position, float rotation
// (could prolly skip the GetLocalPlayerSpawnPosition but I didn't get it to work)
void __declspec(naked) HandleLocalPlayerSpawn()
{
	_asm
	{
		pushad
	}

	g_pLocalPlayer->HandleSpawn();
	_asm
	{
		popad
		jmp COffsets::FUNC_SpawnPlayer
	}
}

CLocalPlayer::CLocalPlayer() : CNetworkPlayer(true),
	m_bIsDead(false),
	m_bToggleControl(true),
	m_fSpawnAngle(0),
	m_ulLastPureSyncTime(0),
	m_uiLastInterior(0),
	m_bDisableVehicleInfo(false),
	m_bFirstSpawn(false),
	m_bFinishedInitialize(false),
	m_bSpawnMarked(false)
{
	memset(&m_lastControlStateSent, 0, sizeof(CControlState));
	SetCanBeStreamedIn(false);

	// Patch to override spawn position and let the game call HandleSpawn
	CPatcher::InstallCallPatch(COffsets::FUNC_GetLocalPlayerSpawnPosition, (DWORD)GetLocalPlayerSpawnPosition, 5);
	CPatcher::InstallCallPatch(COffsets::CALL_SpawnLocalPlayer, (DWORD)HandleLocalPlayerSpawn, 5);
}

CLocalPlayer::~CLocalPlayer()
{

}

void CLocalPlayer::Respawn()
{
	// Set the local player state to respawning
	*(DWORD *)COffsets::VAR_LocalPlayerState = 5;
}

void CLocalPlayer::HandleSpawn()
{
	// Hide the filedownload stuff
	//if(g_pFileTransfer)
		//g_pFileTransfer->SetCurrentFileText();

	// If we're already spawned(min. one time death), recreate fire(deleted after respawn)
	if(m_bFirstSpawn)
		g_pFireManager->ReCreateAllFire();

	CLogFile::Printf("HandleSpawn(LocalPlayer)");

	// Enable input if needed
	if(!g_pInputWindow->IsEnabled() && !m_bControlsDisabled)
		CGame::SetInputState(true);

	CLogFile::Printf("Reset vehicle entry/exit flags");
	// Reset vehicle entry/exit flags
	ResetVehicleEnterExit();

	CLogFile::Printf("Enable out controls");
	// Enable our controls
	SetPlayerControlAdvanced(true, true);

	CLogFile::Printf("Enable the HUD");
	// Enable the HUD
	CGame::SetHudVisible(true);

	CLogFile::Printf("Enable the radar");
	// Enable the radar
	CGame::SetRadarVisible(true);

	CLogFile::Printf("Enable the area names");
	// Enable the area names
	CGame::SetAreaNamesEnabled(true);

	CLogFile::Printf("Reset the camera");
	// Reset the camera
	g_pCamera->Reset();

	if(m_bFinishedInitialize && m_bFirstSpawn) {
		// Send the spawn notification to the server
		CBitStream bsSend;
		bsSend.Write(ModelHashToSkinId(GetModelInfo()->GetHash()));
		g_pNetworkManager->RPC(RPC_PlayerSpawn, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE);
		m_bSpawnMarked = true;
	}
	else
	{
		// Fade the screen out
		Scripting::ScreenFadeOut(1);
	}
	CLogFile::Printf("Flag us as alive");
	if(IsSpawned())
		Scripting::SetCharWillFlyThroughWindscreen(GetScriptingHandle(),true);

	// Flag us as alive
	m_bIsDead = false;
}

void CLocalPlayer::DoDeathCheck()
{
	// Have we not yet processed the death and is the local player dead?
	if(!m_bIsDead && IsDead())
	{
		// Get the kill info
		EntityId playerId = INVALID_ENTITY_ID;
		EntityId vehicleId = INVALID_ENTITY_ID;
		EntityId weaponId = INVALID_ENTITY_ID;
		GetKillInfo(&playerId, &vehicleId,&weaponId);

		CLogFile::Printf("HandleDeath(LocalPlayer, %d, %d, %d)", playerId, vehicleId, weaponId);

		// Send the death notification to the server
		CBitStream bsSend;
		bsSend.WriteCompressed(playerId);
		bsSend.WriteCompressed(vehicleId);
		bsSend.WriteCompressed(weaponId);
		g_pNetworkManager->RPC(RPC_Death, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED);

		// Mark ourselves as dead
		m_bIsDead = true;

		// Reset vehicle entry/exit flags
		ResetVehicleEnterExit();
	}
}

void CLocalPlayer::Pulse()
{
	CNetworkPlayer::Pulse();

	if(g_pFileTransfer && g_pNetworkManager) {
		if(g_pFileTransfer->DownloadFinished() && g_pFileTransfer->GetTransferListSize() == 0 && !m_bFinishedInitialize){
			m_bFinishedInitialize = true;

			g_pNetworkManager->RPC(RPC_PlayerJoinComplete, NULL, PRIORITY_HIGH, RELIABILITY_RELIABLE);
			CLogFile::Print("HandleLocalPlayerSpawn(#1, send join notification to the server)");
			if(!m_bSpawnMarked) {
				CLogFile::Print("HandleLocalPlayerSpawn(#2, loading scripts)");
				g_pClientScriptManager->LoadAll();
				m_bFirstSpawn = true;

				// Send the spawn notification to the server
				CBitStream bsSend;
				bsSend.Write(ModelHashToSkinId(GetModelInfo()->GetHash()));
				g_pNetworkManager->RPC(RPC_PlayerSpawn, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE);
					
				CLogFile::Print("HandleLocalPlayerSpawn(#3, spawn player)");

				// Set again spawn position..
				if(IsSpawned()) {
					CVector3 vecSpawnPos; GetSpawnPosition(&vecSpawnPos);
					SetPosition(vecSpawnPos);
				}
				Scripting::ScreenFadeIn(1);
				m_bSpawnMarked = true;
			}
		}
	}

	if(g_pNetworkManager->IsConnected())
	{
		if(IsSpawned())
		{
			// Do the death check
			DoDeathCheck();

			// Are we not dead?
			if(!m_bIsDead)
			{
				// Is a pure sync needed and are we not getting in/out of a vehicle?
				if(IsPureSyncNeeded() && !HasVehicleEnterExit())
				{
					// Are we not in a vehicle?
					if(!IsInVehicle())
					{
						// Send on foot sync
						SendOnFootSync();
					}
					else
					{
						// Are we not a passenger
						if(!IsAPassenger())
						{
							// Send in vehicle sync
							SendInVehicleSync();
						}
						else
						{
							// Send passenger sync
							SendPassengerSync();
						}
					}
					// Send our empty vehicle sync
					SendEmptyVehicleSync();
				}
				else
				{
					// Is a small sync needed?
					if(IsSmallSyncNeeded())
					{
						// Send small sync
						SendSmallSync();
					}
				}
				//Have we changed interiors?
				if(GetInterior() != m_uiLastInterior)
				{
					// Set the new interior
					m_uiLastInterior = GetInterior();

					// Check for cut scene interiors
					CCutsceneInteriors::Update(m_uiLastInterior);

					// Update all interiors
					g_pStreamer->UpdateInterior(m_uiLastInterior);
				}
			}
		}
	}
}

void CLocalPlayer::SetSpawnLocation(CVector3 vecPosition, float fHeading)
{
	m_vecSpawnPosition = vecPosition;
	m_fSpawnAngle = fHeading;
}

void CLocalPlayer::SetPlayerControlAdvanced(bool bControl, bool bCamera)
{
	Scripting::SetPlayerControlAdvanced(GetGamePlayerNumber(), bControl, bControl, bControl);
	Scripting::SetCameraControlsDisabledWithPlayerControls(bCamera);
}

void CLocalPlayer::SendOnFootSync()
{
	CBitStream bsSend;
	OnFootSyncData syncPacket;

	/*GetPosition(m_oldOnFootSync.vecPos);
	GetMoveSpeed(m_oldOnFootSync.vecMoveSpeed);
	GetTurnSpeed(m_oldOnFootSync.vecTurnSpeed);
	if(m_oldOnFootSync.bDuckState == IsDucking())
		if(m_oldOnFootSync.fHeading == GetCurrentHeading())
			if(m_oldOnFootSync.uHealthArmour == ((GetHealth() << 16) | GetArmour()))*/
	// Get our control state
	GetControlState(&syncPacket.controlState);

	// Update the last sent control state
	//memcpy(&m_lastControlStateSent, &syncPacket.controlState, sizeof(CControlState));

	// Get their position
	GetPosition(syncPacket.vecPos);

	// Get their heading
	syncPacket.fHeading = GetCurrentHeading();

	// Get their move speed
	GetMoveSpeed(syncPacket.vecMoveSpeed);

	// Get their turn speed
	GetTurnSpeed(syncPacket.vecTurnSpeed);

	// Get their ducking state
	syncPacket.bDuckState = IsDucking();

	// Get their health and armour
	syncPacket.uHealthArmour = ((GetHealth() << 16) | GetArmour());

	// Set default animation stuff
	/*syncPacket.bAnim = false;

	// Check for anims
	// TODO Fix animation system
	if(m_bAnimating)
	{
		if(!Scripting::HasCharAnimFinished(GetScriptingHandle(),m_strAnimSpec))
		{
			float fTime;
			syncPacket.bAnim = true;
			strcpy(syncPacket.szAnimGroup, m_strAnimGroup);
			strcpy(syncPacket.szAnimSpecific, m_strAnimSpec);
			Scripting::GetCharAnimCurrentTime(g_pLocalPlayer->GetScriptingHandle(),m_strAnimGroup,m_strAnimSpec,&fTime);
			syncPacket.fAnimTime = fTime;
		}
		else
			m_bAnimating = false;
	}*/

	// Get their current weapon and ammo
	unsigned int uiCurrentWeapon = GetCurrentWeapon();
	syncPacket.uWeaponInfo = ((uiCurrentWeapon << 20) | GetAmmo(uiCurrentWeapon));

	// Write the on foot sync data to the bit stream
	bsSend.Write((char *)&syncPacket, sizeof(OnFootSyncData));

	// Check if they are aiming or firing
	// NOTE: Do i need to sync aim for combat too?
	if(syncPacket.controlState.IsAiming() || syncPacket.controlState.IsFiring())
	{
		// Write a 1 bit to say we have aim sync
		bsSend.Write1();

		// Get their aim sync data
		AimSyncData aimSyncPacket;
		GetAimSyncData(&aimSyncPacket);

		aimSyncPacket.bAiming = false;
		aimSyncPacket.bShooting = false;

		if(syncPacket.controlState.IsAiming())
			aimSyncPacket.bAiming = true;
		else if(syncPacket.controlState.IsFiring())
			aimSyncPacket.bShooting = true;

		// Write the aim sync data to the bit stream
		bsSend.Write((char *)&aimSyncPacket, sizeof(AimSyncData));
	}
	else
	{
		// Write a 0 bit to say we don't have aim sync
		bsSend.Write0();
	}

	g_pNetworkManager->RPC(RPC_OnFootSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED);

	// Send our latest head movement if it's enabled
	if(CGame::GetHeadMovement() && IsSpawned()) 
	{
		CBitStream bsHead;
		CVector3 vecLookAt; 

		g_pCamera->GetLookAt(vecLookAt);
		bsHead.Write(vecLookAt.fX);
		bsHead.Write(vecLookAt.fY);
		bsHead.Write(vecLookAt.fZ);

		g_pNetworkManager->RPC(RPC_HeadMovement, &bsHead, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED);
	}
}

void CLocalPlayer::SendInVehicleSync()
{
	CNetworkVehicle * pVehicle = GetVehicle();

	if(pVehicle && pVehicle->IsStreamedIn())
	{
		CBitStream bsSend;
		InVehicleSyncData syncPacket;

		// Write the vehicle id
		bsSend.WriteCompressed(pVehicle->GetVehicleId());

		// Get our control state
		GetControlState(&syncPacket.controlState);

		// Update the last sent control state
		//memcpy(&m_lastControlStateSent, &syncPacket.controlState, sizeof(CControlState)); // Don't activate, will destroy the control state sync ;)

		// Get their vehicles position
		pVehicle->GetPosition(syncPacket.vecPos);

		// Get their vehicles rotation
		pVehicle->GetRotation(syncPacket.vecRotation);

		// Get their vehicles colors
		pVehicle->GetColors(syncPacket.byteColors[0], syncPacket.byteColors[1], syncPacket.byteColors[2], syncPacket.byteColors[3]);

		// Get their vehicles siren state
		syncPacket.bSirenState = pVehicle->GetSirenState();

		// Get their vehicle engine state
		syncPacket.bEngineStatus = pVehicle->GetEngineState();

		// Get their vehicles turn speed
		pVehicle->GetTurnSpeed(syncPacket.vecTurnSpeed);

		// Get their vehicles move speed
		pVehicle->GetMoveSpeed(syncPacket.vecMoveSpeed);

		// Get their vehicle quaternion
		//pVehicle->GetQuaternion(syncPacket.fQuaternion);

		// Get their vehicles engine health & petroltank health
		syncPacket.uiHealth = pVehicle->GetHealth();
		syncPacket.fPetrolHealth = (float)pVehicle->GetPetrolTankHealth();

		// Get their vehicles dirt level
		syncPacket.fDirtLevel = pVehicle->GetDirtLevel();

		// Get their lights
		syncPacket.bLights = pVehicle->GetLightsState();

		// Get vehicle deformation
		//CVector3 vecPos;
		//pVehicle->GetDeformation(vecPos);

		// Get the door stuff
		syncPacket.fDoor[0] = pVehicle->GetCarDoorAngle(0);
		syncPacket.fDoor[1] = pVehicle->GetCarDoorAngle(1);
		syncPacket.fDoor[2] = pVehicle->GetCarDoorAngle(2);
		syncPacket.fDoor[3] = pVehicle->GetCarDoorAngle(3);
		syncPacket.fDoor[4] = pVehicle->GetCarDoorAngle(4);
		syncPacket.fDoor[5] = pVehicle->GetCarDoorAngle(5);

		// Get their health and armour
		syncPacket.uPlayerHealthArmour = ((GetHealth() << 16) | GetArmour());

		// Get their current weapon and ammo
		unsigned int uCurrentWeapon = GetCurrentWeapon();
		syncPacket.uPlayerWeaponInfo = ((uCurrentWeapon << 20) | GetAmmo(uCurrentWeapon));
		
		// Set default window and typres values & check them
		for(int i = 0; i <= 5; i++)
		{
			if(i < 4)
			{
				syncPacket.bWindow[i] = false;
				syncPacket.bTyre[i] = false;
			}
			else if(i > 3)
				syncPacket.bTyre[i] = false;
		}

		
		for(int i = 0; i <= 3; i++)
		{
			if(!Scripting::IsVehWindowIntact(pVehicle->GetScriptingHandle(),(Scripting::eVehicleWindow)i))
				syncPacket.bWindow[i] = true;
		}

		for(int i = 0; i <= 5; i++)
		{
			if(Scripting::IsCarTyreBurst(pVehicle->GetScriptingHandle(),(Scripting::eVehicleTyre)i))
				syncPacket.bTyre[i] = true;
		}

		// Write the in vehicle sync data to the bit stream
		bsSend.Write((char *)&syncPacket, sizeof(InVehicleSyncData));

		// Check if they are doing a drive by
		if(syncPacket.controlState.IsDoingDriveBy())
		{
			// Write a 1 bit to say we have aim sync
			bsSend.Write1();

			// Get their aim sync data
			AimSyncData aimSyncPacket;
			GetAimSyncData(&aimSyncPacket);

			// Write the aim sync data to the bit stream
			bsSend.Write((char *)&aimSyncPacket, sizeof(AimSyncData));
		}
		else
		{
			// Write a 0 bit to say we don't have aim sync
			bsSend.Write0();
		}

		g_pNetworkManager->RPC(RPC_InVehicleSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED);

		// Check if our car is dead(exploded or in water)
		if(Scripting::IsCarDead(pVehicle->GetScriptingHandle()) || (Scripting::IsCarInWater(pVehicle->GetScriptingHandle()) && CGame::GetSpecialData(1)))
		{
			CBitStream bsDeath;
			bsDeath.Write(pVehicle->GetVehicleId());
			g_pNetworkManager->RPC(RPC_ScriptingVehicleDeath, &bsDeath, PRIORITY_HIGH, RELIABILITY_UNRELIABLE_SEQUENCED);
		}
	}
}

void CLocalPlayer::SendPassengerSync()
{
	CNetworkVehicle * pVehicle = GetVehicle();

	if(pVehicle && pVehicle->IsStreamedIn())
	{
		// TODO: Add position/rotation/move speed to passenger sync data? (only if we have no driver?)
		CBitStream bsSend;
		PassengerSyncData syncPacket;

		// Write the vehicle id
		bsSend.WriteCompressed(pVehicle->GetVehicleId());

		// Get our control state
		GetControlState(&syncPacket.controlState);

		// Update the last sent control state
		memcpy(&m_lastControlStateSent, &syncPacket.controlState, sizeof(CControlState));

		// Get their seat id
		syncPacket.byteSeatId = GetVehicleSeatId();

		// Get their health and armour
		syncPacket.uPlayerHealthArmour = ((GetHealth() << 16) | GetArmour());

		// Get their current weapon and ammo
		unsigned int uCurrentWeapon = GetCurrentWeapon();
		syncPacket.uPlayerWeaponInfo = ((uCurrentWeapon << 20) | GetAmmo(uCurrentWeapon));

		// Write the passenger sync data to the bit stream
		bsSend.Write((char *)&syncPacket, sizeof(PassengerSyncData));

		// Check if they are doing a drive by
		// NOTE: I think certain vehicles (e.g. helicoptors) allow 3rd person
		// shooting from them which involves the on foot aim/fire keys so add
		// them to this check if needed
		if(syncPacket.controlState.IsDoingDriveBy())
		{
			// Write a 1 bit to say we have aim sync
			bsSend.Write1();

			// Get their aim sync data
			AimSyncData aimSyncPacket;
			GetAimSyncData(&aimSyncPacket);

			// Write the aim sync data to the bit stream
			bsSend.Write((char *)&aimSyncPacket, sizeof(AimSyncData));
		}
		else
		{
			// Write a 0 bit to say we don't have aim sync
			bsSend.Write0();
		}

		g_pNetworkManager->RPC(RPC_PassengerSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED);
	}
}

void CLocalPlayer::SendSmallSync()
{
	CBitStream bsSend;
	SmallSyncData syncPacket;

	// Get our control state
	GetControlState(&syncPacket.controlState);

	// Update the last sent control state
	memcpy(&m_lastControlStateSent, &syncPacket.controlState, sizeof(CControlState));

	// Get their ducking state
	syncPacket.bDuckState = IsDucking();

	// Get their current weapon and ammo
	unsigned int uCurrentWeapon = GetCurrentWeapon();
	syncPacket.uWeaponInfo = ((uCurrentWeapon << 20) | GetAmmo(uCurrentWeapon));

	// Write the small key sync data to the bit stream
	bsSend.Write((char *)&syncPacket, sizeof(SmallSyncData));

	// Check if they are aiming or firing
	if(syncPacket.controlState.IsAiming() || syncPacket.controlState.IsFiring())
	{
		// Write a 1 bit to say we have aim sync
		bsSend.Write1();

		// Get their aim sync data
		AimSyncData aimSyncPacket;
		GetAimSyncData(&aimSyncPacket);

		// Write the aim sync data to the bit stream
		bsSend.Write((char *)&aimSyncPacket, sizeof(AimSyncData));
	}
	else
	{
		// Write a 0 bit to say we don't have aim sync
		bsSend.Write0();
	}

	g_pNetworkManager->RPC(RPC_SmallSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED);
}

bool CLocalPlayer::IsPureSyncNeeded()
{
	// Get the current time
	unsigned long ulCurrentTime = SharedUtility::GetTime();

	// Has it been TICK_RATE or more ms since our last pure sync?
	if(ulCurrentTime >= (m_ulLastPureSyncTime + TICK_RATE))
	{
		// Update the last pure sync time
		m_ulLastPureSyncTime = ulCurrentTime;
		return true;
	}

	return false;
}

bool CLocalPlayer::IsSmallSyncNeeded()
{
	// Are we spawned?
	if(IsSpawned())
	{
		// Get the current control state
		CControlState controlState;
		GetControlState(&controlState);

		// Is the current control state different to the last sent control state?
		if(controlState != m_lastControlStateSent)
			return true;
	}

	return false;
}

unsigned short CLocalPlayer::GetPing()
{
	return (unsigned short)g_pNetworkManager->GetNetClient()->GetLastPing();
}

void CLocalPlayer::SetControl(bool control)
{
	m_bToggleControl = control;
}

bool CLocalPlayer::GetControl()
{
	return m_bToggleControl;
}
/*
void CLocalPlayer::SetAnimation(const char * strGroup, const char * strAnim)
{
	//TODO, reverse this function
	Scripting::TaskPlayAnim(GetScriptingHandle(),strAnim, strGroup,(float)8,0,0,0,0,-1);

	printf(m_strAnimGroup,strGroup);
	printf(m_strAnimSpec,strAnim);
	m_bAnimating = true;
}
*/

// TODO: or just sync nearest vehicle?
void CLocalPlayer::SendEmptyVehicleSync()
{
	EMPTYVEHICLESYNCPACKET syncPacket;
	CBitStream bsSend;
	int uiOccupants = 0;

	if(IsSpawned() && g_pVehicleManager && g_pNetworkManager->IsConnected())
	{
		for(EntityId iD = 0; iD < g_pVehicleManager->GetCount(); iD++)
		{
			if(g_pVehicleManager->Exists(iD))
			{
				if(g_pVehicleManager->Get(iD)->IsSpawned() && g_pVehicleManager->Get(iD)->IsStreamedIn())
				{
					// Reset stuff
					uiOccupants = 0;
					bsSend.Reset();

					for(BYTE i = 0; i < g_pVehicleManager->Get(iD)->GetMaxPassengers(); i++)
					{
						// Does this passenger seat contain a passenger?
						if(g_pVehicleManager->Get(iD)->GetPassenger(i))
						{
							uiOccupants = 1;
							break;
						}
					}
					if(uiOccupants == 0)
					{
						if(g_pVehicleManager->Get(iD)->StoreEmptySync(&syncPacket)) {
			
							bsSend.Write((char *)&syncPacket,sizeof(EMPTYVEHICLESYNCPACKET));
							g_pNetworkManager->RPC(RPC_EmptyVehicleSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED);
						}
					}
				}
			}
		}
	}
}
			
bool CLocalPlayer::IsCameraAttachedToEntity(unsigned int uiHandle)
{
	if(IsSpawned() && g_pCamera)
	{
		if(g_pCamera->IsCameraAttached() > 0) {
			if(g_pCamera->GetCameraAttachedHandle() == uiHandle)
				return true;
			else
				return false;
		}
		return false;
	}
	return false;
}

void CLocalPlayer::Reset()
{
	m_bFirstSpawn = false;
	m_bFinishedInitialize = false;
	m_bSpawnMarked = false;

	// Reset file transfer
	if(g_pFileTransfer)
		g_pFileTransfer->SetDownloadFinished(false);
}