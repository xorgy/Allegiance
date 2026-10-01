// Frozen radarimage.cpp at 39ef2c0. Rendering outputs reduced to five scalars.
#pragma once
namespace OriginalRadar {
constexpr int c_rlAll=0,c_rlDefault=1,c_rlTarget=2,c_rlMinimal=3,c_ucRadarOffScreen=0;
constexpr int OT_ship=0,OT_station=1,OT_missile=2,OT_mine=3,OT_probe=4,OT_asteroid=5,OT_warp=7,OT_treasure=8;
constexpr int c_iNotUseful=0,c_iUsefulTech=2,c_sabmPedestal=1;constexpr float rangeClipLabels=1000;
struct Side {bool ally;static bool AlliedSides(Side* a,Side* b){return a&&b&&a->ally&&b->ally;}};
struct Model {Side* side;Model* target;bool pedestal;int useful;Side* GetSide(){return side;}Model* GetTarget(){return target;}Model* GetBaseStationType(){return this;}bool HasCapability(int){return pedestal;}};
using IstationIGC=Model;using ImissileIGC=Model;using ItreasureIGC=Model;using ImodelIGC=Model;
inline int UsefulTreasure(Model*,Model* m){return m->useful;}
struct Appearance {bool icon,label,stats,tech;int range;};
inline Appearance check(int m_radarLOD,int type,bool important,int ucRadarState,float separation,int range,Model* pmodel,Model* pshipSource,Side* psideMine,const char* pszName) {
    auto* pside=pmodel->GetSide();bool tech=false;
                    bool    bIcon = true;
                    bool bLabel=important,bStats=bLabel;
                    if (bLabel)
                    {
                        if ((type == OT_treasure) &&
                            (UsefulTreasure(pshipSource, (ItreasureIGC*)pmodel) == c_iUsefulTech))
                        {
                            tech=true;
                        }
                    }
                    else
                    {
                        if (m_radarLOD == c_rlMinimal)
                        {
                            if (pmodel != pshipSource)
                            {
                                bIcon = false;
                                range = 0;
                            }
                        }
                        else
                        {
                            switch (type)
                            {
                                case    OT_ship:
                                {
                                    switch (m_radarLOD)
                                    {
                                        case c_rlAll:
                                        {
                                            bLabel = bStats = true;
                                            if ((ucRadarState == c_ucRadarOffScreen) && (pside == psideMine))
                                                range = 0;
                                        }
                                        break;
                                        case c_rlDefault:
                                        {
                                            if ((ucRadarState != c_ucRadarOffScreen) || (separation < rangeClipLabels))
                                                bLabel = true;

                                            if (pside == psideMine)
                                                range = 0;
                                            else
                                                bStats = true;
                                        }
                                        break;
                                        case c_rlTarget:
                                        {
                                            if ( (pside == psideMine) || pside->AlliedSides(pside,psideMine) ) //ALLY - Imago 7/3/09
                                            {
                                                if (pmodel != pshipSource)
                                                {
                                                    bIcon = false;
                                                    range = 0;
                                                }
                                            }
                                            else
                                            {
                                                if (ucRadarState != c_ucRadarOffScreen)
                                                    bStats = true;

                                                if (separation < rangeClipLabels)
                                                    bLabel = true;
                                            }
                                        }
                                        break;
                                    }
                                }
                                break;

                                case    OT_station:
                                {
                                    if (((IstationIGC*)pmodel)->GetBaseStationType()->HasCapability(c_sabmPedestal))
                                    {
                                        if (separation >= rangeClipLabels)
                                        {
                                            bIcon = bLabel = bStats = false;
                                            range = 0;
                                        }
                                    }
                                    else
                                    {
                                        switch (m_radarLOD)
                                        {
                                            case c_rlAll:
                                            {
                                                bLabel = bStats = true;
                                            }
                                            break;
                                            case c_rlDefault:
                                            {
                                                if ((separation >= rangeClipLabels) || (ucRadarState == c_ucRadarOffScreen))
                                                    range = 0;
                                            }
                                            break;
                                            case c_rlTarget:
                                            {
                                                range = 0;
                                                if ( ((pside == psideMine) || pside->AlliedSides(pside,psideMine)) && (ucRadarState == c_ucRadarOffScreen) ) //ALLY imag0 7/3/09
                                                    bIcon = false;
                                            }
                                            break;
                                        }
                                    }
                                }
                                break;

                                case    OT_missile:
                                {
                                    if (m_radarLOD != c_rlAll)
                                    {
                                        ImodelIGC*  pmodelTarget = ((ImissileIGC*)pmodel)->GetTarget();

                                        if ((pmodelTarget == NULL) || (pmodelTarget->GetSide() != psideMine)) //ALLYTD?
                                        {
                                            bIcon = false;
                                            range = 0;
                                        }
                                    }
                                }
                                break;

                                case    OT_probe:
                                case    OT_mine:
                                {
                                    switch (m_radarLOD)
                                    {
                                        case c_rlAll:
                                        {
                                            if ((pside == psideMine) || pside->AlliedSides(pside,psideMine) ) //ALLY imago 7/3/09
                                            {
                                                if ((ucRadarState == c_ucRadarOffScreen) && (separation >= rangeClipLabels))
                                                    range = 0;
                                            }
                                            else
                                            {
                                                if (ucRadarState != c_ucRadarOffScreen)
                                                    bLabel = true;
                                            }

                                            bStats = true;
                                        }
                                        break;
                                        case c_rlDefault:
                                        {
                                            if ((pside == psideMine) || pside->AlliedSides(pside,psideMine) ) //ALLY imago 7/3/09
                                            {
                                                if (separation >= rangeClipLabels)
                                                    range = 0;
                                            }
                                        }
                                        break;
                                        case c_rlTarget:
                                        {
                                            if ((pside == psideMine) || (separation >= rangeClipLabels) || (pside->AlliedSides(pside,psideMine))) //ALLY imago 7/3/09
                                            {
                                                bIcon = false;
                                                range = 0;
                                            }
                                        }
                                        break;
                                    }
                                }
                                break;

                                case    OT_treasure:
                                {
                                    int iUseful = UsefulTreasure(pshipSource, (ItreasureIGC*)pmodel);
                                    if (iUseful != c_iNotUseful)
                                    {
                                        bLabel = (m_radarLOD == c_rlAll);
                                        if ((m_radarLOD == c_rlTarget) && (separation >= rangeClipLabels))
                                        {
                                            bIcon = false;
                                            range = 0;
                                        }
                                        else if (iUseful == c_iUsefulTech)
                                        {
                                            tech=true;
                                        }
                                    }
                                    else if (m_radarLOD != c_rlAll)
                                    {
                                        bIcon = false;
                                        range = 0;
                                    }
                                }
                                break;

                                case    OT_asteroid:
                                {
                                    switch (m_radarLOD)
                                    {
                                        case c_rlAll:
                                        {
                                            if ((pszName[0] == '\0') || (pszName[0] == 'A'))
                                            {
                                                if (separation >= rangeClipLabels)
                                                {
                                                    bIcon = false;
                                                    range = 0;
                                                }
                                                else if (range == 0)
                                                    bIcon = false;
                                            }
                                            else
                                                bLabel = bStats = true;
                                        }
                                        break;

                                        case c_rlDefault:
                                        {
                                            if ((pszName[0] == '\0') || (pszName[0] == 'A'))
                                            {
                                                if (separation >= rangeClipLabels)
                                                {
                                                    bIcon = false;
                                                    range = 0;
                                                }
                                                else if (range == 0)
                                                    bIcon = false;
                                            }
                                        }
                                        break;

                                        case c_rlTarget:
                                        {
                                            if (separation >= rangeClipLabels)
                                            {
                                                bIcon = false;
                                                range = 0;
                                            }
                                            else if (range == 0)
                                                bIcon = false;
                                        }
                                        break;
                                    }
                                }
                                break;

                                case    OT_warp:
                                {
                                    bLabel = (m_radarLOD <= c_rlDefault);
                                    if ((m_radarLOD == c_rlTarget) && (separation >= rangeClipLabels))
                                        range = 0;
                                }
                                break;
                            }
                        }
                    }


    return {bIcon,bLabel,bStats,tech,range};
}
}
