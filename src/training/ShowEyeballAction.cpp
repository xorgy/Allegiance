/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	ShowEyeballAction.cpp
**
**  Author:  
**
**  Description:
**      Implementation of the training library "ShowEyeballAction" interface.
**
**  History:
*/
#include    "pch.h"
#include    "ShowEyeballAction.h"

namespace Training
{
    //------------------------------------------------------------------------------
    // class methods
    //------------------------------------------------------------------------------
    /* void */  ShowEyeballAction::ShowEyeballAction (bool showEyeball) :
    m_showEyeball (showEyeball)
    {
    }

    //------------------------------------------------------------------------------
    /* void */  ShowEyeballAction::~ShowEyeballAction (void)
    {
    }

    //------------------------------------------------------------------------------
    void        ShowEyeballAction::Execute (void)
    {
#ifdef ALLEGIANCE_NATIVE
        trekClient.ShowEyeball(m_showEyeball);
#else
        PlayerInfo* pPlayerInfo = trekClient.GetPlayerInfo ();
        ShipStatus  shipStatus = pPlayerInfo->GetShipStatus ();
        shipStatus.SetDetected (m_showEyeball);
        pPlayerInfo->SetShipStatus (shipStatus);
#endif
    }

    //------------------------------------------------------------------------------
}
