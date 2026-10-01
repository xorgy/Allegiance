// Original radarimage.cpp visibility of icons, labels, statistics and ranges.
// Detail methods are lazy: Windows does not calculate treasure/relay information
// for object kinds or radar modes that do not consume it.
#pragma once
namespace Allegiance {
enum RadarDetailMode { c_rlNone=-1,c_rlAll,c_rlDefault,c_rlTarget,c_rlMinimal };
inline int CycleRadarDetail(int mode){return (mode+1)%(int(c_rlMinimal)+1);}
struct RadarAppearance {bool icon,label,stats,tech;int range;};
template<class Detail>
inline RadarAppearance RadarPresentation(int mode,int type,bool important,bool onScreen,float separation,int range,const Detail& info) {
    if(mode==c_rlNone)return {false,false,false,false,0};
    constexpr int OT_ship=0,OT_station=1,OT_missile=2,OT_mine=3,OT_probe=4,OT_asteroid=5,OT_warp=7,OT_treasure=8;
    constexpr int c_iNotUseful=0,c_iUsefulTech=2;
    constexpr float rangeClipLabels=1000.f;
    bool tech=false;
bool    bIcon = true;
bool bLabel = important;
bool bStats = bLabel;
if (bLabel)
{
    if ((type == OT_treasure) &&
        (info.usefulTreasure() == c_iUsefulTech))
    {
        tech = true;
    }
}
else
{
    if (mode == c_rlMinimal)
    {
        if (!info.self())
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
                switch (mode)
                {
                    case c_rlAll:
                    {
                        bLabel = bStats = true;
                        if ((!onScreen) && (info.ownSide()))
                            range = 0;
                    }
                    break;
                    case c_rlDefault:
                    {
                        if ((onScreen) || (separation < rangeClipLabels))
                            bLabel = true;

                        if (info.ownSide())
                            range = 0;
                        else
                            bStats = true;
                    }
                    break;
                    case c_rlTarget:
                    {
                        if ( (info.ownSide()) || info.allied() ) //ALLY - Imago 7/3/09
                        {
                            if (!info.self())
                            {
                                bIcon = false;
                                range = 0;
                            }
                        }
                        else
                        {
                            if (onScreen)
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
                if (info.pedestal())
                {
                    if (separation >= rangeClipLabels)
                    {
                        bIcon = bLabel = bStats = false;
                        range = 0;
                    }
                }
                else
                {
                    switch (mode)
                    {
                        case c_rlAll:
                        {
                            bLabel = bStats = true;
                        }
                        break;
                        case c_rlDefault:
                        {
                            if ((separation >= rangeClipLabels) || (!onScreen))
                                range = 0;
                        }
                        break;
                        case c_rlTarget:
                        {
                            range = 0;
                            if ( ((info.ownSide()) || info.allied()) && (!onScreen) ) //ALLY imag0 7/3/09
                                bIcon = false;
                        }
                        break;
                    }
                }
            }
            break;

            case    OT_missile:
            {
                if (mode != c_rlAll)
                {
    

                    if (!info.missileTargetsOwnSide()) //ALLYTD?
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
                switch (mode)
                {
                    case c_rlAll:
                    {
                        if ((info.ownSide()) || info.allied() ) //ALLY imago 7/3/09
                        {
                            if ((!onScreen) && (separation >= rangeClipLabels))
                                range = 0;
                        }
                        else
                        {
                            if (onScreen)
                                bLabel = true;
                        }

                        bStats = true;
                    }
                    break;
                    case c_rlDefault:
                    {
                        if ((info.ownSide()) || info.allied() ) //ALLY imago 7/3/09
                        {
                            if (separation >= rangeClipLabels)
                                range = 0;
                        }
                    }
                    break;
                    case c_rlTarget:
                    {
                        if ((info.ownSide()) || (separation >= rangeClipLabels) || (info.allied())) //ALLY imago 7/3/09
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
                int iUseful = info.usefulTreasure();
                if (iUseful != c_iNotUseful)
                {
                    bLabel = (mode == c_rlAll);
                    if ((mode == c_rlTarget) && (separation >= rangeClipLabels))
                    {
                        bIcon = false;
                        range = 0;
                    }
                    else if (iUseful == c_iUsefulTech)
                    {
                        tech = true;
                    }
                }
                else if (mode != c_rlAll)
                {
                    bIcon = false;
                    range = 0;
                }
            }
            break;

            case    OT_asteroid:
            {
                switch (mode)
                {
                    case c_rlAll:
                    {
                        if (info.plainAsteroid())
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
                        if (info.plainAsteroid())
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
                bLabel = (mode <= c_rlDefault);
                if ((mode == c_rlTarget) && (separation >= rangeClipLabels))
                    range = 0;
            }
            break;
        }
    }
}

    return {bIcon,bLabel,bStats,tech,range};
}
}
