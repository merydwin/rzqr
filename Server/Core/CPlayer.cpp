//============== IV: Multiplayer - http://code.iv-multiplayer.com ==============
//
// File: CPlayer.cpp
// Project: Server.Core
// Author(s): jenksta
// License: See LICENSE in root directory
//
//==============================================================================
#include "CPlayer.h"
#include "CNetworkManager.h"
#include "CPlayerManager.h"
#include "CVehicleManager.h"
#include "CEvents.h"
#include <CSettings.h>
#include "CModuleManager.h"

extern CNetworkManager * g_pNetworkManager;
extern CPlayerManager * g_pPlayerManager;
extern CVehicleManager * g_pVehicleManager;
extern CEvents * g_pEvents;
extern CModuleManager * g_pModuleManager;

unsigned int playerColors[] =
{
	0xE59338FF, 0xEEDC2DFF, 0xD8C762FF, 0x3FE65CFF, 0xFF8C13FF, 0xC715FFFF, 0x20B2AAFF, 0xDC143CFF,	0x6495EDFF, 0xf0e68cFF, 0x778899FF, 0xFF1493FF,
	0xF4A460FF, 0xEE82EEFF, 0xFFD720FF, 0x8b4513FF,	0x4949A0FF, 0x148b8bFF, 0x14ff7fFF, 0x556b2fFF, 0x0FD9FAFF, 0x10DC29FF, 0x534081FF, 0x0495CDFF,
	0xF4A460FF, 0xEE82EEFF, 0xFFD720FF, 0x8b4513FF, 0x4949A0FF, 0x148b8bFF, 0x14ff7fFF, 0x556b2fFF,	0x0FD9FAFF, 0x10DC29FF, 0x534081FF, 0x0495CDFF,
	0xEF6CE8FF, 0xBD34DAFF, 0x247C1BFF, 0x0C8E5DFF,	0x635B03FF, 0xCB7ED3FF, 0x65ADEBFF, 0x5C1ACCFF, 0xF2F853FF, 0x11F891FF, 0x7B39AAFF, 0x53EB10FF,
	0x54137DFF, 0x275222FF, 0xF09F5BFF, 0x3D0A4FFF, 0x22F767FF, 0xD63034FF, 0x9A6980FF, 0xDFB935FF,	0x3793FAFF, 0x90239DFF, 0xE9AB2FFF, 0xAF2FF3FF,
	0x057F94FF, 0xB98519FF, 0x388EEAFF, 0x028151FF,	0xA55043FF, 0x0DE018FF, 0x93AB1CFF, 0x95BAF0FF, 0x369976FF, 0x18F71FFF, 0x4B8987FF, 0x491B9EFF,
	0x829DC7FF, 0xBCE635FF, 0xCEA6DFFF, 0x20D4ADFF, 0x2D74FDFF, 0x3C1C0DFF, 0x12D6D4FF, 0x48C000FF,	0x2A51E2FF, 0xE3AC12FF, 0xFC42A8FF, 0x2FC827FF,
	0x1A30BFFF, 0xB740C2FF, 0x42ACF5FF, 0x2FD9DEFF,	0xFAFB71FF, 0x05D1CDFF, 0xC471BDFF, 0x94436EFF, 0xC1F7ECFF, 0xCE79EEFF, 0xBD1EF2FF, 0x93B7E4FF,
	0x9F945CFF, 0xDCDE3DFF, 0x10C9C5FF, 0x70524DFF, 0x0BE472FF
};

CPlayer::CPlayer(EntityId playerId, String strName)
{	
	m_playerId = playerId;
	//m_pScriptingInstance = sq_createinstance()
	m_strName = strName;
	m_bSpawned = 0;
	m_iModelId = 1;
	m_pVehicle = NULL;
	m_byteVehicleSeatId = -1;
	memset(&m_previousControlState, 0, sizeof(CControlState));
	memset(&m_currentControlState, 0, sizeof(CControlState));
	m_fHeading = 0;
	m_iHealth = 200;
	m_uArmour = 0;
	m_state = STATE_TYPE_DISCONNECT;
	m_fSpawnHeading = 0;
	m_iMoney = 0;
	m_uWeapon = 0;
	m_uAmmo = 0;
	memset(&m_aimSyncData, 0, sizeof(AimSyncData));
	m_uiColor = playerColors[playerId];
	memset(&m_ucClothes, 0, sizeof(m_ucClothes));
	m_bHelmet = false;
	m_szAnimSpec = NULL;
	m_szAnimSpec = new char[256];
	m_szAnimGroup = NULL;
	m_szAnimGroup = new char[256];
	m_bMobilePhoneUse = false;
	m_ucDimension = 0;
	m_bWeaponDropEnabled = false;
	m_uiWantedLevel = 0;
	m_bJoined = false;
	m_bDrunk = false;
	m_bPhysicsEnabled = true;
}
CPlayer::~CPlayer() { }

// Internal (for sync & etc)
void CPlayer::AddForPlayer(EntityId playerId)
{
	CBitStream bsSend;
	bsSend.WriteCompressed(m_playerId);
	bsSend.Write(m_uiColor);
	bsSend.Write(m_strName);
	g_pNetworkManager->RPC(RPC_NewPlayer, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, playerId, false);
}
void CPlayer::DeleteForPlayer(EntityId playerId)
{
	CBitStream bsSend;
	bsSend.WriteCompressed(m_playerId);
	g_pNetworkManager->RPC(RPC_DeletePlayer, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, playerId, false);
}
void CPlayer::AddForWorld()
{
	for(EntityId i = 0; i < MAX_PLAYERS; i++)
	{		
		if(g_pPlayerManager->DoesExist(i) && i != m_playerId)
			AddForPlayer(i);
	}
}
void CPlayer::DeleteForWorld()
{	
	for(EntityId i = 0; i < MAX_PLAYERS; i++)
	{		
		if(g_pPlayerManager->DoesExist(i) && i != m_playerId)
			DeleteForPlayer(i);
	}
}
void CPlayer::SpawnForPlayer(EntityId playerId)
{
	CBitStream bsSend;
	bsSend.WriteCompressed(m_playerId);
	bsSend.Write(m_iModelId);
	bsSend.Write(m_bHelmet);
	bsSend.Write(m_vecPosition);
	bsSend.Write(m_fHeading);
	bsSend.Write(m_ucDimension);

	if(m_pVehicle)
	{
		bsSend.WriteCompressed(m_pVehicle->GetVehicleId());
		bsSend.Write(m_byteVehicleSeatId);
	}
	else		
		bsSend.WriteCompressed(INVALID_ENTITY_ID);

	bool bModifiedClothing = false;
	for(unsigned char uc = 0; uc < 11; ++ uc)
	{		
		if(m_ucClothes[uc] != 0)	
		{			
			bModifiedClothing = true;
			break;
		}
	}

	bsSend.WriteBit(bModifiedClothing);
	if(bModifiedClothing)
	{	
		for(unsigned char uc = 0; uc < 11; ++ uc)
			bsSend.Write(m_ucClothes[uc]);
	}

	g_pNetworkManager->RPC(RPC_PlayerSpawn, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, playerId, false);
}
void CPlayer::KillForPlayer(EntityId playerId)
{
	CBitStream bsSend;
	bsSend.WriteCompressed(m_playerId);
	g_pNetworkManager->RPC(RPC_PlayerDeath, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, playerId, false);
}
void CPlayer::SpawnForWorld()
{
	for(EntityId i = 0; i < MAX_PLAYERS; i++)
	{		
		if(g_pPlayerManager->DoesExist(i) && i != m_playerId)	
			SpawnForPlayer(i);
	}
	m_bSpawned = true;
	SetState(STATE_TYPE_SPAWN);
}
void CPlayer::KillForWorld()
{	
	for(EntityId i = 0; i < MAX_PLAYERS; i++)
	{
		if(g_pPlayerManager->DoesExist(i) && i != m_playerId)
			KillForPlayer(i);
	}
	m_bSpawned = false;
	SetState(STATE_TYPE_DEATH);
}
void CPlayer::StoreOnFootSync(OnFootSyncData * syncPacket, bool bHasAimSyncData, AimSyncData * aimSyncData)
{
	// If we have warped out of a vehicle update its occupant	
	if(m_pVehicle && m_pVehicle->GetOccupant(m_byteVehicleSeatId) == this)		
		m_pVehicle->SetOccupant(m_byteVehicleSeatId, NULL);

	// Invalidate the vehicle pointer	
	m_pVehicle = NULL;

	// Invalidate the vehicle seat id
	m_byteVehicleSeatId = -1;

	// Set the control state	
	SetControlState(&syncPacket->controlState);

	// Set the position	
	m_vecPosition = syncPacket->vecPos;

	// Set the heading	
	m_fHeading = syncPacket->fHeading;
	
	// Set the Dimension
	m_ucDimension = syncPacket->ucDimension;

	// Set the move speed
	m_vecMoveSpeed = syncPacket->vecMoveSpeed;

	// Set the duck state
	m_bDuckState = syncPacket->bDuckState;

	// Set the health and armour	
	m_iHealth = (syncPacket->uHealthArmour >> 16);
	m_uArmour = ((syncPacket->uHealthArmour << 16) >> 16);

	/*
	m_bAnimating = syncPacket->bAnim;
	m_szAnimGroup = syncPacket->szAnimGroup;
	m_szAnimSpec = syncPacket->szAnimSpecific;
	m_fAnimTime = syncPacket->fAnimTime;
	if(m_bAnimating)		
		CLogFile::Printf("%s/%s",m_szAnimGroup,m_szAnimSpec);
	*/

	// Set the weapon and ammo	m_uWeapon = (syncPacket->uWeaponInfo >> 20);
	m_uAmmo = ((syncPacket->uWeaponInfo << 12) >> 12);

	// Do we have aim sync data?	
	if(bHasAimSyncData)
	{		
		// Set the aim sync data
		memcpy(&m_aimSyncData, aimSyncData, sizeof(AimSyncData));
		UpdateWeaponSync(aimSyncData->vecAimTarget,aimSyncData->vecShotSource, aimSyncData->vecLookAt);

		/*		
		// Shitcode, we can detect it clientside, only for tests
		aimSyncData->bShooting = false;
		aimSyncData->bAiming = false;

		if()
			aimSyncData->bShooting = true;
		else
			aimSyncData->bAiming = true;
		*/	
	}

	// Set the state to on foot or dead (if dead)
	SetState(STATE_TYPE_ONFOOT);
	
	// Send the sync to all other players
	CBitStream bsSend;
	bsSend.WriteCompressed(m_playerId);
	bsSend.WriteCompressed(GetPing());
	bsSend.WriteCompressed(m_bHelmet);
	bsSend.Write((char *)syncPacket, sizeof(OnFootSyncData));

	// Do we have aim sync data?
	if(bHasAimSyncData)
	{
		// Write a 1 bit to say we have aim sync
		bsSend.Write1();
		bsSend.Write((char *)aimSyncData, sizeof(AimSyncData));
	}
	else
	{	
		// Write a 0 bit to say we don't have aim sync
		bsSend.Write0();
	}
	g_pNetworkManager->RPC(RPC_OnFootSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED, m_playerId, true);
}
void CPlayer::StoreInVehicleSync(CVehicle * pVehicle, InVehicleSyncData * syncPacket, bool bHasAimSyncData, AimSyncData * aimSyncData)
{	
	// If we have warped into a vehicle update its driver
	if(!m_pVehicle && pVehicle->GetDriver() != this)
		pVehicle->SetDriver(this);
	// Store the vehicle pointer
	m_pVehicle = pVehicle;

	// Store the vehicle seat id
	m_byteVehicleSeatId = 0;

	// Set the control state
	SetControlState(&syncPacket->controlState);

	//CLogFile::PrintDebugf("Controlstates(%d): %d,%d,%d,%d",m_playerId, syncPacket->controlState.ucInVehicleMove[0],syncPacket->controlState.ucInVehicleMove[1],syncPacket->controlState.ucInVehicleMove[2],syncPacket->controlState.ucInVehicleMove[3]);

	// Set the position to the vehicle position
	m_vecPosition = syncPacket->vecPos;

	// Set the rotation to the vehicle rotation
	// TODO: Player has full rotation vector too
	m_fHeading = syncPacket->vecRotation.fZ;

	// Set the move speed to the vehicle move speed
	m_vecMoveSpeed = syncPacket->vecMoveSpeed;

	// Set the health and armour
	m_iHealth = (syncPacket->uPlayerHealthArmour >> 16);
	m_uArmour = ((syncPacket->uPlayerHealthArmour << 16) >> 16);

	// Set the weapon and ammo	m_uWeapon = (syncPacket->uPlayerWeaponInfo >> 20);
	m_uAmmo = ((syncPacket->uPlayerWeaponInfo << 12) >> 12);

	// Do we have aim sync data?	
	if(bHasAimSyncData)	
	{
		// Set the aim sync data
		memcpy(&m_aimSyncData, aimSyncData, sizeof(AimSyncData));
		UpdateWeaponSync(aimSyncData->vecAimTarget,aimSyncData->vecShotSource,aimSyncData->vecLookAt);
	}

	// Set the state to in vehicle	SetState(STATE_TYPE_INVEHICLE);
	// Send the sync to all other players	
	CBitStream bsSend;
	bsSend.WriteCompressed(m_playerId);
	bsSend.WriteCompressed(pVehicle->GetVehicleId());
	bsSend.WriteCompressed(GetPing());
	bsSend.WriteCompressed(m_bHelmet);
	bsSend.Write((char *)syncPacket, sizeof(InVehicleSyncData));

	// Do we have aim sync data?	
	if(bHasAimSyncData)
	{		
		// Write a 1 bit to say we have aim sync
		bsSend.Write1();
		bsSend.Write((char *)aimSyncData, sizeof(AimSyncData));
	}
	else
	{	
		// Write a 0 bit to say we don't have aim sync
		bsSend.Write0();
	}
	g_pNetworkManager->RPC(RPC_InVehicleSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED, m_playerId, true);
}
void CPlayer::StorePassengerSync(CVehicle * pVehicle, PassengerSyncData * syncPacket, bool bHasAimSyncData, AimSyncData * aimSyncData)
{
	// If we have warped into a vehicle update its passenger
	if(!m_pVehicle && pVehicle->GetPassenger(syncPacket->byteSeatId) != this)	
		pVehicle->SetPassenger(syncPacket->byteSeatId, this);

	// Store the vehicle pointer
	m_pVehicle = pVehicle;

	// Store the vehicle seat id
	m_byteVehicleSeatId = syncPacket->byteSeatId;

	// Set the control state
	SetControlState(&syncPacket->controlState);

	// Set the health and armour
	m_iHealth = (syncPacket->uPlayerHealthArmour >> 16);
	m_uArmour = ((syncPacket->uPlayerHealthArmour << 16) >> 16);

	// Set the weapon and ammo
	m_uWeapon = (syncPacket->uPlayerWeaponInfo >> 20);
	m_uAmmo = ((syncPacket->uPlayerWeaponInfo << 12) >> 12);

	// Set the position to the vehicle position
	pVehicle->GetPosition(m_vecPosition);

	// Set the rotation to the vehicle rotation
	// TODO: Player has full rotation vector too
	CVector3 vecRotation;
	pVehicle->GetRotation(vecRotation);
	m_fHeading = vecRotation.fZ;

	// Set the move speed to the vehicle move speed
	pVehicle->GetMoveSpeed(m_vecMoveSpeed);

	// Do we have aim sync data?
	if(bHasAimSyncData)
	{
		// Set the aim sync data
		memcpy(&m_aimSyncData, aimSyncData, sizeof(AimSyncData));
		UpdateWeaponSync(aimSyncData->vecAimTarget,aimSyncData->vecShotSource,aimSyncData->vecLookAt);
	}

	// Set the state to passenger
	SetState(STATE_TYPE_PASSENGER);

	// Send the sync to all other players
	CBitStream bsSend;
	bsSend.WriteCompressed(m_playerId);
	bsSend.WriteCompressed(pVehicle->GetVehicleId());
	bsSend.WriteCompressed(GetPing());
	bsSend.WriteCompressed(m_bHelmet);
	bsSend.Write((char *)syncPacket, sizeof(PassengerSyncData));

	// Do we have aim sync data?
	if(bHasAimSyncData)
	{	
		// Write a 1 bit to say we have aim sync
		bsSend.Write1();
		bsSend.Write((char *)aimSyncData, sizeof(AimSyncData));
	}
	else
	{	
		// Write a 0 bit to say we don't have aim sync
		bsSend.Write0();
	}
	g_pNetworkManager->RPC(RPC_PassengerSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED, m_playerId, true);
}
void CPlayer::StoreSmallSync(SmallSyncData * syncPacket, bool bHasAimSyncData, AimSyncData * aimSyncData)
{
	// Set the control state
	SetControlState(&syncPacket->controlState);

	// Set the duck state
	m_bDuckState = syncPacket->bDuckState;

	// Set the weapon and ammo
	m_uWeapon = (syncPacket->uWeaponInfo >> 20);
	m_uAmmo = ((syncPacket->uWeaponInfo << 12) >> 12);

	// Do we have aim sync data?
	if(bHasAimSyncData)
	{
		// Set the aim sync data
		memcpy(&m_aimSyncData, aimSyncData, sizeof(AimSyncData));
		UpdateWeaponSync(aimSyncData->vecAimTarget,aimSyncData->vecShotSource,aimSyncData->vecLookAt);
	}

	// Send the sync to all other players
	CBitStream bsSend;
	bsSend.WriteCompressed(m_playerId);
	bsSend.Write((char *)syncPacket, sizeof(SmallSyncData));

	// Do we have aim sync data?
	if(bHasAimSyncData)
	{
		// Write a 1 bit to say we have aim sync
		bsSend.Write1();
		bsSend.Write((char *)aimSyncData, sizeof(AimSyncData));
	}
	else
	{	
		// Write a 0 bit to say we don't have aim sync
		bsSend.Write0();
	}
	g_pNetworkManager->RPC(RPC_SmallSync, &bsSend, PRIORITY_LOW, RELIABILITY_UNRELIABLE_SEQUENCED, m_playerId, true);
}
bool CPlayer::UpdateWeaponSync(CVector3 vecAim, CVector3 vecShot, CVector3 vecLookAt)
{	
	CSquirrelArguments pArguments;
	pArguments.push(m_playerId);
	if((vecShot-m_vecLastShot).Length() != 0)	
	{		
		m_vecLastAim = vecAim;
		m_vecLastShot = vecShot;
		pArguments.push(vecShot.fX);
		pArguments.push(vecShot.fY);
		pArguments.push(vecShot.fZ);
		pArguments.push(vecLookAt.fX);
		pArguments.push(vecLookAt.fY);
		pArguments.push(vecLookAt.fZ);
		pArguments.push(true);
		return true;
	}
	else	
	{		
		m_vecLastAim = vecAim;
		pArguments.push(vecAim.fX);
		pArguments.push(vecAim.fY);
		pArguments.push(vecAim.fZ);
		pArguments.push(vecLookAt.fX);
		pArguments.push(vecLookAt.fY);
		pArguments.push(vecLookAt.fZ);
		pArguments.push(false);
		return false;
	}
	g_pEvents->Call("playerShot", &pArguments);
}
void CPlayer::UpdateHeadMoveSync(CVector3 vecHead)
{
	if((vecHead-m_vecLastHeadMove).Length() != 0)
	{		
		// push stuff			
		CSquirrelArguments pArguments;
		pArguments.push(m_playerId);
		pArguments.push(m_vecLastHeadMove.fX);
		pArguments.push(m_vecLastHeadMove.fY);
		pArguments.push(m_vecLastHeadMove.fZ);
		pArguments.push(vecHead.fX);
		pArguments.push(vecHead.fY);
		pArguments.push(vecHead.fZ);

		// Update coords
		m_vecLastHeadMove = vecHead;

		// Call the event
		g_pEvents->Call("headMove", &pArguments);
	}
}
void CPlayer::Process() { }
void CPlayer::SetState(eStateType state)
{
	if(m_state != state)
	{		
		CSquirrelArguments pArguments;
		pArguments.push(m_playerId);
		pArguments.push(m_state);
		pArguments.push(state);
		g_pEvents->Call("playerChangeState", &pArguments);

		m_state = state;
	}
}
void CPlayer::SetControlState(CControlState * controlState)
{	
	if(m_currentControlState != *controlState)	
	{		
		m_previousControlState = m_currentControlState;
		m_currentControlState = *controlState;
		if(CVAR_GET_BOOL("frequentevents"))		
		{			
			CSquirrelArguments pArguments;
			pArguments.push(m_playerId);
			g_pEvents->Call("playerChangePadState", &pArguments);
			g_pEvents->Call("playerChangeControlState", &pArguments);
		}
	}
}

// "" Interface ""
bool CPlayer::SetName(String strName)
{
	if(strName.GetLength() < 2 || strName.GetLength() > MAX_NAME_LENGTH)
		return false;
	if(strName == m_strName || g_pPlayerManager->IsNameInUse(strName))
		return false;

	m_strName = strName;
	CBitStream bsSend;
	bsSend.Write(m_playerId);
	bsSend.Write(strName);
	g_pNetworkManager->RPC(RPC_NameChange, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
	return true;
}
bool CPlayer::SetModel(int iModelId)
{
	// if it's the same clothes, there's no tell the clients.
	// If you remove this, make sure clothes are not reset when using the same skin the player has been using before
	// (or modify the SetModel function client-side to remove the clothes anyway)
	if(iModelId == m_iModelId)	
		return true;

	// TODO: unsigned short instead of int	
	if(iModelId == 0 || (iModelId >= 4 && iModelId <= 345))
	{		
		memset(&m_ucClothes, 0, sizeof(m_ucClothes));

		// TODO: Why checking if spawned? If not so just ignore(and desync) ?!	
		if(m_bSpawned)	
		{			
			CBitStream bsSend;
			bsSend.Write(m_playerId);
			bsSend.Write(iModelId);
			g_pNetworkManager->RPC(RPC_ScriptingSetModel, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
			m_iModelId = iModelId;
		}
		return true;
	}
	return false;
}
void CPlayer::SetCameraPos(const CVector3& vecPosition)
{	
	m_vecPosition = vecPosition;
	CBitStream bsSend;
	bsSend.Write(vecPosition);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerCameraPos, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::SetCameraLookAt(const CVector3& vecPosition)
{
	CBitStream bsSend;
	bsSend.Write(vecPosition);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerCameraLookAt, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	// CrackHD: WHAT ?
	// m_vecPosition = vecPosition;
}
void CPlayer::SetCoordinates(const CVector3& vecPosition)
{	
	CBitStream bsSend;
	bsSend.Write(vecPosition);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerCoordinates, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_vecPosition = vecPosition;
}
void CPlayer::SetHeading(float fHeading)
{
	CBitStream bsSend;
	bsSend.Write(fHeading);
	g_pNetworkManager->RPC(RPC_ScriptingSetHeading, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_fHeading = fHeading;
}
void CPlayer::SetVelocity(const CVector3& vecMoveSpeed)
{
	CBitStream bsSend;
	bsSend.Write(vecMoveSpeed);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerMoveSpeed, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_vecMoveSpeed = vecMoveSpeed;
}
void CPlayer::SetDucking(bool bDuckState)
{	
	// TODO: RPC
	m_bDuckState = bDuckState;
}
void CPlayer::SetHealth(int iHealth)
{	
	CBitStream bsSend;
	bsSend.Write(iHealth);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerHealth, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_iHealth = iHealth;
}
int CPlayer::GetHealth()
{	
	// CrackHD: Health range is 0...100, -1 is for "dead"	
	if(m_iHealth >= 100)
		return m_iHealth - 100;
	else
		return -1;
}
void CPlayer::GiveWeapon(unsigned int uiWeaponModelId, unsigned int uiAmmo)
{	
	CBitStream bsSend;
	bsSend.Write(uiWeaponModelId);
	bsSend.Write(uiAmmo);
	g_pNetworkManager->RPC(RPC_ScriptingGivePlayerWeapon, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::SetGravity(float fGravity)
{
	// TODO: Scripting native doesn't work.
	CBitStream bsSend;
	bsSend.Write(fGravity);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerGravity, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_fGravity = fGravity;
}
void CPlayer::SetArmour(unsigned int uArmour)
{
	CBitStream bsSend;
	bsSend.Write(uArmour);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerArmour, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_uArmour = uArmour;
}
void CPlayer::SetTime(unsigned int ucHour, unsigned int ucMinute, unsigned int ucMinuteDuration)
{	
	CBitStream bsSend;
	bsSend.Write(ucHour);
	bsSend.Write(ucMinute);
	bsSend.Write(ucMinuteDuration);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerTime, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::SetWeather(BYTE byteWeatherId)
{
	CBitStream bsSend;
	bsSend.Write(byteWeatherId);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerWeather, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::SetSpawnLocation(const CVector3& vecPosition, float fHeading)
{	
	if(!m_bSpawned)
	{	
		// CrackHD: do we really need this?
		m_vecPosition = vecPosition;
		m_fHeading = fHeading;
	}
	m_vecSpawnPosition = vecPosition;
	m_fSpawnHeading = fHeading;

	CBitStream bsSend;
	bsSend.Write(vecPosition);
	bsSend.Write(fHeading);
	g_pNetworkManager->RPC(RPC_ScriptingSetSpawnLocation, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::GetSpawnLocation(CVector3& vecPosition, float * fHeading)
{
	vecPosition = m_vecSpawnPosition;
	*fHeading = m_fSpawnHeading;
}

// Toggles
void CPlayer::TogglePayAndSpray(bool bToggle)
{	
	CBitStream bsSend;
	bsSend.Write(bToggle);
	g_pNetworkManager->RPC(RPC_ScriptingTogglePayAndSpray, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE, m_playerId, false);
}
void CPlayer::ToggleAutoAim(bool bToggle)
{
	CBitStream bsSend;
	bsSend.Write(bToggle);
	g_pNetworkManager->RPC(RPC_ScriptingToggleAutoAim, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE, m_playerId, false);
}
void CPlayer::ToggleDrunk(bool bToggle)
{
	CBitStream bsSend;
	bsSend.Write(bToggle);
	g_pNetworkManager->RPC(RPC_ScriptingToggleDrunk, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE, m_playerId, false);

	m_bDrunk = bToggle;
}
void CPlayer::ToggleControls(bool bControls)
{	
	CBitStream bsSend;
	bsSend.Write(bControls);
	g_pNetworkManager->RPC(RPC_ScriptingToggleControls, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_bControls = bControls;
}
void CPlayer::ToggleFrozen(bool bPlayerFrozen, bool bCameraFrozen)
{
	CBitStream bsSend;
	bsSend.Write(bPlayerFrozen);
	bsSend.Write(bCameraFrozen);
	g_pNetworkManager->RPC(RPC_ScriptingToggleFrozen, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::ToggleHUD(bool bVisible)
{
	CBitStream bsSend;
	bsSend.Write(bVisible);
	g_pNetworkManager->RPC(RPC_ScriptingToggleHUD, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::ToggleRadar(bool bVisible)
{	
	CBitStream bsSend;
	bsSend.Write(bVisible);
	g_pNetworkManager->RPC(RPC_ScriptingToggleRadar, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::ToggleNameTags(bool bToggle)
{
	CBitStream bsSend;
	bsSend.Write(bToggle);
	g_pNetworkManager->RPC(RPC_ScriptingToggleNames, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::ToggleAreaNames(bool bToggle)
{
	CBitStream bsSend;
	bsSend.Write(bToggle);
	g_pNetworkManager->RPC(RPC_ScriptingToggleAreaNames, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}

// Money
bool CPlayer::GiveMoney(int iMoney)
{
	if(iMoney == 0)		
		return true;
	// bounds checking	
	if(iMoney > 0 && m_iMoney + iMoney < m_iMoney)		
		return false;
	if(iMoney < 0 && m_iMoney + iMoney < 0)		
		return false;
	m_iMoney += iMoney;

	CBitStream bsSend;
	bsSend.Write(iMoney);
	g_pNetworkManager->RPC(RPC_ScriptingGivePlayerMoney, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
	return true;
}
bool CPlayer::SetMoney(int iMoney)
{
	if(m_iMoney == iMoney)
		return true;
	if(iMoney < 0)
		return false;
	m_iMoney = iMoney;

	CBitStream bsSend;
	bsSend.Write(iMoney);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerMoney, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
	return true;
}

// Clothes
void CPlayer::ResetClothes()
{	
	CBitStream bsSend;
	bsSend.Write(m_playerId);
	g_pNetworkManager->RPC(RPC_ScriptingResetPlayerClothes, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);

	memset(&m_ucClothes,0,sizeof(m_ucClothes));
}
void CPlayer::SetClothes(unsigned char ucBodyPart, unsigned char ucClothes)
{	
	if(ucBodyPart < 0 || ucBodyPart >= 11)		
		return;

	CBitStream bsSend;
	bsSend.Write(m_playerId);
	bsSend.Write(ucBodyPart);
	bsSend.Write(ucClothes);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerClothes, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);

	m_ucClothes[ucBodyPart] = ucClothes;
}
unsigned char CPlayer::GetClothes(unsigned char ucBodyPart)
{	
	if(ucBodyPart >= 11)
		return 0;
	return m_ucClothes[ucBodyPart];
}

// Camera
// TODO: remove one of 2 following?
void CPlayer::SetCameraBehind()
{	
	CBitStream bsSend;
	g_pNetworkManager->RPC(RPC_ScriptingSetCameraBehindPlayer, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::ResetCamera()
{
	g_pNetworkManager->RPC(RPC_ScriptingResetPlayerCamera, NULL, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}

// Weapons
void CPlayer::SetWeapon(unsigned int uWeapon)
{
	// TODO: RPC
	m_uWeapon = uWeapon;
}
void CPlayer::SetAmmo(unsigned int uAmmo)
{
	// TODO: RPC
	m_uAmmo = uAmmo;
}

// Networking
String CPlayer::GetIp()
{
	// Return his networked IP
	return g_pNetworkManager->GetPlayerIp(m_playerId);
}
unsigned short CPlayer::GetPing()
{	
	return (unsigned short)g_pNetworkManager->GetNetServer()->GetPlayerLastPing(m_playerId);
}
void CPlayer::Kick(bool bSendNotification)
{
	// Kick the player
	g_pNetworkManager->GetNetServer()->KickPlayer(m_playerId, bSendNotification);
}
void CPlayer::Ban(unsigned int uiSeconds)
{
	// Ban the player
	g_pNetworkManager->AddBan(GetIp(), uiSeconds);

	// Kick the player
	Kick(true);
}
String CPlayer::GetSerial()
{
	return g_pNetworkManager->GetPlayerSerial(m_playerId);
}

// Other functions
void CPlayer::UseMobilePhone(bool bUse)
{	
	// TODO: RPC
	m_bMobilePhoneUse = bUse;
}
void CPlayer::SetColor(unsigned int color)
{	
	if(m_uiColor != color)	
	{		
		m_uiColor = color;
		CBitStream bsSend;
		bsSend.Write(m_playerId);
		bsSend.Write(color);
		g_pNetworkManager->RPC(RPC_ScriptingSetPlayerColor, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
	}
}
void CPlayer::GiveHelmet()
{	
	if(IsSpawned())
	{		
		CBitStream bsSend;
		bsSend.Write(m_playerId);
		g_pNetworkManager->RPC(RPC_ScriptingGiveHelmet, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
		m_bHelmet = true;
	}
}
void CPlayer::RemoveHelmet()
{
	if(IsSpawned())
	{
		CBitStream bsSend;
		bsSend.Write(m_playerId);
		g_pNetworkManager->RPC(RPC_ScriptingRemoveHelmet, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
		m_bHelmet = false;
	}
}
void CPlayer::SetDimension(DimensionId ucDimension)
{	
	CBitStream bsSend;
	bsSend.Write(m_playerId);
	bsSend.Write(ucDimension);
	// TODO WTF ? Client shoud handle 1 rpc and detect if player in vehicle, set player's vehicle to same dimension	
	// Done ^^ ViruZz*
	//g_pNetworkManager->RPC(RPC_ScriptingSetVehicleDimension, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerDimension, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);

	m_ucDimension = ucDimension;
}
unsigned int CPlayer::GetFileChecksum(int iFile)
{	
	if(iFile >= 0 && iFile < 2)	
	{		
		// handling.dat		
		if(iFile == 0)			
			return m_FileCheck.uiHandleFileChecksum;
		// gta.dat
		if(iFile == 1) 
			return m_FileCheck.uiGTAFileChecksum;
	}
	return -1;
}
void CPlayer::SetWantedLevel(unsigned int uiWantedLevel)
{	
	CBitStream bsSend;
	bsSend.Write(uiWantedLevel);
	g_pNetworkManager->RPC(RPC_ScriptingSetWantedLevel, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_uiWantedLevel = uiWantedLevel;
}
void CPlayer::SetBlockWeaponDrop(bool drop)
{	
	// TODO: rpc
	m_bWeaponDropEnabled = drop;
}
void CPlayer::TogglePhysics(bool bEnabled)
{	
	CBitStream bsSend;
	bsSend.Write(bEnabled);
	g_pNetworkManager->RPC(RPC_ScriptingToggleRagdoll, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	m_bPhysicsEnabled = bEnabled;
}
void CPlayer::DisplayText(float fX, float fY, String strText, unsigned int uiTime)
{	
	CBitStream bsSend;
	bsSend.Write(fX);
	bsSend.Write(fY);
	bsSend.Write(strText);
	bsSend.Write(uiTime);
	g_pNetworkManager->RPC(RPC_ScriptingDisplayText, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::DisplayInfoText(String strText, unsigned int uiTime)
{	
	CBitStream bsSend;
	bsSend.Write(strText);
	bsSend.Write(uiTime);
	g_pNetworkManager->RPC(RPC_ScriptingDisplayInfoText, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::SetInvincible(bool bInvincible)
{	
	CBitStream bsSend;
	bsSend.Write(bInvincible);
	g_pNetworkManager->RPC(RPC_ScriptingSetPlayerInvincible, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);

	//m_bIsInvicible = bInvincible;
}
bool CPlayer::WarpIntoVehicle(EntityId vehicleId, BYTE byteSeatId)
{	
	if(g_pVehicleManager->DoesExist(vehicleId))
	{
		CBitStream bsSend;
		bsSend.Write(vehicleId);
		bsSend.Write(byteSeatId);
		g_pNetworkManager->RPC(RPC_ScriptingWarpPlayerIntoVehicle, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
	}
	return false;
}
void CPlayer::RemoveFromVehicle(bool bAnimated)
{
	CBitStream bsSend;
	bsSend.WriteBit(bAnimated);
	g_pNetworkManager->RPC(RPC_ScriptingRemovePlayerFromVehicle, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
void CPlayer::TriggerClientEvent(CSquirrelArguments sqArgs)
{	
	// info: Event name must be stored in first string argument.	
	CBitStream bsSend;
	sqArgs.serialize(&bsSend);
	g_pNetworkManager->RPC(RPC_ScriptingEventCall, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, m_playerId, false);
}
