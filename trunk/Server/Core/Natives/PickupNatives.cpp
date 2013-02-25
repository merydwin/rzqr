//============== IV: Multiplayer - http://code.iv-multiplayer.com ==============
//
// File: PickupNatives.cpp
// Project: Server.Core
// Author(s): mabako
// License: See LICENSE in root directory
//
//==============================================================================

#include "../Natives.h"
#include "Scripting/CScriptingManager.h"
#include "../CPickupManager.h"

extern CPickupManager * g_pPickupManager;

// Pickup functions

void CPickupNatives::Register(CScriptingManager * pScriptingManager)
{
	pScriptingManager->RegisterFunction("createPickup", Create, 9, "iiiffffff");
	pScriptingManager->RegisterFunction("deletePickup", Delete, 1, "i");
	pScriptingManager->RegisterFunction("getPickupModel", GetModel, 1, "i");
	pScriptingManager->RegisterFunction("getPickupType", GetType, 1, "i");
	pScriptingManager->RegisterFunction("setPickupValue", SetValue, 2, "ii");
	pScriptingManager->RegisterFunction("getPickupValue", GetValue, 1, "i");
	pScriptingManager->RegisterFunction("setPickupCoordinates", SetCoordinates, 4, "ifff");
	pScriptingManager->RegisterFunction("getPickupCoordinates", GetCoordinates, 1, "i");
	pScriptingManager->RegisterFunction("setPickupPosition", SetCoordinates, 4, "ifff");
	pScriptingManager->RegisterFunction("getPickupPosition", GetCoordinates, 1, "i");
	pScriptingManager->RegisterFunction("setPickupRotation", SetRotation, 4, "ifff");
	pScriptingManager->RegisterFunction("getPickupRotation", GetRotation, 1, "i");
}

// createPickup(modelhash, type, value?, x, y, z, rx, ry, rz)
SQInteger CPickupNatives::Create(SQVM * pVM)
{
	SQInteger modelhash, type, value;
	float x, y, z, rx, ry, rz;
	sq_getinteger(pVM, 2, &modelhash);
	sq_getinteger(pVM, 3, &type);
	sq_getinteger(pVM, 4, &value);
	sq_getfloat(pVM, 5, &x);
	sq_getfloat(pVM, 6, &y);
	sq_getfloat(pVM, 7, &z);
	sq_getfloat(pVM, 8, &rx);
	sq_getfloat(pVM, 9, &ry);
	sq_getfloat(pVM, 10, &rz);
	sq_pushinteger(pVM, g_pPickupManager->Create(modelhash, (unsigned char)type, (unsigned int)value, x, y, z, rx, ry, rz));
	return 1;
}

// deletePickup(pickupid)
SQInteger CPickupNatives::Delete(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, -1, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		g_pPickupManager->Delete(pickupid);
		sq_pushbool(pVM, true);
		return 1;
	}
	sq_pushbool(pVM, false);
	return 1;
}

// getPickupModel(pickupid)
SQInteger CPickupNatives::GetModel(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, 2, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		sq_pushinteger(pVM, g_pPickupManager->GetModel(pickupid));
		return 1;
	}
	sq_pushbool(pVM, false);
	return 1;
}

// getPickupType(pickupid)
SQInteger CPickupNatives::GetType(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, 2, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		sq_pushinteger(pVM, g_pPickupManager->GetType(pickupid));
		return 1;
	}
	sq_pushbool(pVM, false);
	return 1;
}

// setPickupValue(pickupid, value)
SQInteger CPickupNatives::SetValue(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, 2, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		SQInteger pvalue;
		sq_getinteger(pVM, 3, &pvalue);
		sq_pushbool(pVM, g_pPickupManager->SetValue(pickupid, pvalue));
		return 1;
	}
	sq_pushbool(pVM, false);
	return 1;
}

// getPickupValue(pickupid)
SQInteger CPickupNatives::GetValue(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, 2, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		sq_pushinteger(pVM, g_pPickupManager->GetValue(pickupid));
		return 1;
	}
	sq_pushbool(pVM, false);
	return 1;
}

// setPickupCoordinates(Pickupid, x, y, z)
SQInteger CPickupNatives::SetCoordinates(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, 2, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		CVector3 vecPosition;
		sq_getvector3(pVM, 3, &vecPosition);
		sq_pushbool(pVM, g_pPickupManager->SetPosition(pickupid, vecPosition));
		return 1;
	}
	sq_pushbool(pVM, false);
	return 1;
}

// getPickupCoordinates(Pickupid)
SQInteger CPickupNatives::GetCoordinates(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, 2, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		CVector3 vecPosition;
		if(g_pPickupManager->GetPosition(pickupid, &vecPosition))
		{
			sq_newarray(pVM, 0);
			sq_pushfloat(pVM, vecPosition.fX);
			sq_arrayappend(pVM, -2);
			sq_pushfloat(pVM, vecPosition.fY);
			sq_arrayappend(pVM, -2);
			sq_pushfloat(pVM, vecPosition.fZ);
			sq_arrayappend(pVM, -2);
			return 1;
		}
	}
	sq_pushbool(pVM, false);
	return 1;
}


// setPickupRotation(Pickupid, x, y, z)
SQInteger CPickupNatives::SetRotation(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, 2, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		CVector3 vecPosition;
		sq_getvector3(pVM, 3, &vecPosition);
		sq_pushbool(pVM, g_pPickupManager->SetRotation(pickupid, vecPosition));
		return 1;
	}
	sq_pushbool(pVM, false);
	return 1;
}

// getPickupRotation(Pickupid)
SQInteger CPickupNatives::GetRotation(SQVM * pVM)
{
	SQInteger pickupid;
	sq_getinteger(pVM, 2, &pickupid);
	if(g_pPickupManager->DoesExist(pickupid))
	{
		CVector3 vecPosition;
		if(g_pPickupManager->GetRotation(pickupid, &vecPosition))
		{
			sq_newarray(pVM, 0);
			sq_pushfloat(pVM, vecPosition.fX);
			sq_arrayappend(pVM, -2);
			sq_pushfloat(pVM, vecPosition.fY);
			sq_arrayappend(pVM, -2);
			sq_pushfloat(pVM, vecPosition.fZ);
			sq_arrayappend(pVM, -2);
			return 1;
		}
	}
	sq_pushbool(pVM, false);
	return 1;
}
