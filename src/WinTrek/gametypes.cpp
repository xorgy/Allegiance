#include "pch.h"
#include "gametypes.h"
#include "../ClientCore/GameTypes.h"
static_assert(c_cUnlimitedLives==32767 && c_mmHiLo==8 && c_mmBrawl==6 && c_mmPinWheel==2 && c_mmGrid==12, "Shared game presets must use the IGC identifiers");

class GameTypeImpl : public GameType
{
    ZString m_strName;
    bool m_bAllowDevelopments;
    bool m_bInvulnerableStations;
    bool m_bEjectPods;
    short m_nLives;
    float m_fGoalTeamMoney;
    short m_iGoalConquestPercentage;
    short m_nGoalFlagsCount;
    short m_nGoalArtifactsCount;
    short m_nGoalTeamKills;
    short m_nGoalTerritory;
    float m_fGameLength;
	bool m_bExperimental; // mmf 10/07 Experimental game type
    MapMakerID m_mmMapType;

public:
    GameTypeImpl(
        const ZString& strName, 
        bool bAllowDevelopments, 
        bool bInvulnerableStations,
        bool bEjectPods,
        short nLives, 
        float fGoalTeamMoney,
        short iGoalConquestPercentage,
        short nGoalFlagsCount,
        short nGoalArtifactsCount,
        short nGoalTeamKills,
        short iGoalTerritory,
        float fGameLength,
	    bool bExperimental, // mmf 10/07 Experimental game type
		MapMakerID mmMapType
        ) :

        m_strName(strName),
        m_bAllowDevelopments(bAllowDevelopments),
        m_bInvulnerableStations(bInvulnerableStations),
        m_bEjectPods(bEjectPods),
        m_nLives(nLives),
        m_fGoalTeamMoney(fGoalTeamMoney),
        m_iGoalConquestPercentage(iGoalConquestPercentage),
        m_nGoalFlagsCount(nGoalFlagsCount),
        m_nGoalArtifactsCount(nGoalArtifactsCount),
        m_nGoalTeamKills(nGoalTeamKills),
        m_nGoalTerritory(iGoalTerritory),
        m_fGameLength(fGameLength),
		m_bExperimental(bExperimental), // mmf 10/07 Experimental game type
        m_mmMapType(mmMapType)
    {
    };

    virtual ZString GetName()
    {
        return m_strName;
    };

    virtual bool IsGameType(const MissionParams& misparams)
    {
        return Allegiance::MatchesGameType(
            {misparams.bAllowDevelopments,misparams.bInvulnerableStations,misparams.bEjectPods,misparams.iLives,
             misparams.fGoalTeamMoney,misparams.iGoalConquestPercentage,misparams.nGoalFlagsCount,misparams.nGoalArtifactsCount,
             misparams.nGoalTeamKills,misparams.iGoalTerritoryPercentage,misparams.dtGameLength,misparams.bExperimental},
            {m_bAllowDevelopments,m_bInvulnerableStations,m_bEjectPods,m_nLives,m_fGoalTeamMoney,m_iGoalConquestPercentage,
             m_nGoalFlagsCount,m_nGoalArtifactsCount,m_nGoalTeamKills,m_nGoalTerritory,m_fGameLength,m_bExperimental});

        // note: allow any map type.
    };

    virtual void Apply(MissionParams& misparams)
    {
        misparams.bAllowDevelopments = m_bAllowDevelopments;
        misparams.bInvulnerableStations = m_bInvulnerableStations;
        misparams.bEjectPods = m_bEjectPods;
        misparams.iLives = m_nLives;
        misparams.fGoalTeamMoney = m_fGoalTeamMoney;
        misparams.iGoalConquestPercentage = m_iGoalConquestPercentage;
        misparams.nGoalFlagsCount = m_nGoalFlagsCount;
        misparams.nGoalArtifactsCount = m_nGoalArtifactsCount;
        misparams.nGoalTeamKills = m_nGoalTeamKills;
        misparams.iGoalTerritoryPercentage = m_nGoalTerritory;
        misparams.dtGameLength = m_fGameLength;
		misparams.bExperimental = m_bExperimental; // mmf 10/07 Experimental game type
		misparams.mmMapType = m_mmMapType;
        //misparams.bScoresCount = misparams.bClubGame;
		misparams.bScoresCount = true;
 }
};

namespace 
{
    bool bGameTypesInitialized = false;
    TList<TRef<GameType> > listGameTypes;

    void VerifyGameTypeInitialization()
    {
        TRef<GameType> pNewGameType;
        if (!bGameTypesInitialized)
        {
            for (const auto& type : Allegiance::GameTypes)
            {
                const auto& v = type.values;
                pNewGameType = new GameTypeImpl(type.name, v.developments, v.invulnerableStations, v.ejectPods,
                    v.lives, v.prosperity, v.conquest, v.flags, v.artifacts, v.kills, v.territory, v.seconds,
                    v.experimental, type.map);
                listGameTypes.PushEnd(pNewGameType);
            }

            bGameTypesInitialized = true;
        }
    }
};


TRef<GameType> GameType::FindType(const MissionParams& misparams)
{
    VerifyGameTypeInitialization();
    TList<TRef<GameType> >::Iterator gameTypesIter(listGameTypes);

    while (!gameTypesIter.End())
    {
        if (gameTypesIter.Value()->IsGameType(misparams))
        {
            return gameTypesIter.Value();
        }
        gameTypesIter.Next();
    }

    return NULL;
}

TList<TRef<GameType> >& GameType::GetGameTypes()
{
    VerifyGameTypeInitialization();
    return listGameTypes;
}
