

#include    "pch.h"
#include    "AddRespawnAction.h"
#include    "trainingmission.h"

namespace Training
{
    //------------------------------------------------------------------------------
    // global variabes
    //------------------------------------------------------------------------------
    extern TrainingMission*    g_pMission;

    //------------------------------------------------------------------------------
    // class methods
    //------------------------------------------------------------------------------
    /* void */  AddRespawnAction::AddRespawnAction(CreateDroneAction* cda, float downtime, char spawnGroup, bool spawnOnAdd) :
        m_cda(cda),
        m_Downtime(downtime),
        m_SpawnGroup(spawnGroup),
        m_SpawnOnAdd(spawnOnAdd)
    {
    }

    //------------------------------------------------------------------------------
    /* void */  AddRespawnAction::~AddRespawnAction(void)
    {
        delete m_cda; // owned here until registered with the mission
    }

    //------------------------------------------------------------------------------
    void        AddRespawnAction::Execute(void)
    {
        if (m_cda) {
            g_pMission->AddRespawn(m_cda, m_Downtime, m_SpawnGroup, m_SpawnOnAdd);
            m_cda = NULL; // mission now owns the respawn action
        }
    }

    //------------------------------------------------------------------------------
}