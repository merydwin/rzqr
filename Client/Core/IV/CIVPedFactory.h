//============== IV: Multiplayer - http://code.iv-multiplayer.com ==============
//
// File: CIVPedFactory.cpp
// Project: Client.Core
// Author(s): XForce
// License: See LICENSE in root directory
//
//==============================================================================

#include "CIVPed.h"
#include "../CGame.h"
#include "../COffsets.h"
#include <CLogFile.h>

class IVPedFactory
{
public:
	virtual 			~IVPedFactory();
	virtual void 		Function1(); // pure virtual
	virtual IVPed* 		CreateDummyPed(int iModelIndex, Matrix34* pMatrix, bool createNetwork);
	virtual int 		RemovePed(IVPed*);
	virtual IVPed* 		CreatePed(int a1, int unk = 0);
	virtual IVPed* 		CreatePedIntelligent(WORD* wPlayerData, int iModelIndex, Matrix34* pMatrix, int remotePed, char couldNetworkPed);
	virtual int 		RemoveIntelligentPed(IVPed*);
	virtual IVPed* 		CreatePlayerPed(WORD* playerNumber, int iModelIndex, int playerNum, Matrix34 *pMatrix, bool createNetworkPlayer);
	virtual void		RemovePlayer(IVPed*);
	virtual void 		AssignDefaultTask(IVPed *a1, int a2, int a3, char a4);
};

class IVPedFactoryNY : public IVPedFactory
{

};