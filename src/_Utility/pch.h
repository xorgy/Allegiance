//////////////////////////////////////////////////////////////////////////////
//
// PCH.H
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

#ifndef __MODULE__
#define __MODULE__ "Utility"
#endif

//
// C headers
//

#include "stdio.h"
#include "string.h"
#include <stdarg.h>
#include <time.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
// mdvalley: Another include to shut the ATL libraries up.
#include <stddef.h>

//
// Windows headers
//

#if defined(_WIN32) && !defined(ALLEGIANCE_NATIVE)
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#include <urlmon.h>

#include <algorithm>

// BT - STEAM
#include "steam_gameserver.h"

//
// Wintrek headers
//
#include "guids.h"
#endif
#include "zlib.h"
#include "Utility.h"
//#include "zauth.h" // KG removed


#endif
