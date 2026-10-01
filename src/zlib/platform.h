#pragma once

#ifdef _WIN32
#include <windows.h>
#ifdef ALLEGIANCE_NATIVE
#include <mmsystem.h>
#include <oleauto.h>
// Obsolete segmented-pointer/RPC macros must not pollute portable C++ names.
#undef near
#undef far
#undef small
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <mutex>
#endif
#else
// Fixed-width definitions for the existing core's Windows-era interfaces.
// Native rendering, input, audio and file I/O do not use Win32 APIs.
#include <cstdint>
#include <cctype>
#include <cwctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <cfloat>
#include <climits>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
using CHAR = char;
using BYTE = uint8_t;
using byte = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using LONG = int32_t;
using ULONG = uint32_t;
using UINT = uint32_t;
using INT = int32_t;
using INT64 = int64_t;
using COLORREF = uint32_t;
using BOOL = int;
using HRESULT = int32_t;
using VOID = void;
using HANDLE = void*;
using HWND = void*;
using HINSTANCE = void*;
using LPVOID = void*;
using PVOID = void*;
using LPCSTR = const char*;
using LPSTR = char*;
using BSTR = wchar_t*;
using CRITICAL_SECTION = std::recursive_mutex;
struct FILETIME { uint32_t dwLowDateTime, dwHighDateTime; };
struct SYSTEMTIME { uint16_t wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds; };
struct POINT { int32_t x,y; };
struct RECT { int32_t left,top,right,bottom; };
constexpr HRESULT E_POINTER = int32_t(0x80004003), STG_E_FILENOTFOUND = int32_t(0x80030002);
constexpr HRESULT E_ABORT = int32_t(0x80004004);
constexpr HRESULT S_OK = 0, S_FALSE = 1;
constexpr HRESULT E_FAIL = int32_t(0x80004005), E_INVALIDARG = int32_t(0x80070057);
constexpr HRESULT E_OUTOFMEMORY = int32_t(0x8007000e), E_NOTIMPL = int32_t(0x80004001);
constexpr int MAX_PATH = 4096;
constexpr bool TRUE = true, FALSE = false;
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr) ((HRESULT)(hr) < 0)
#define __forceinline inline __attribute__((always_inline))
#define lstrcpyA strcpy
#define lstrcmpA strcmp
#define lstrcatA strcat
#define __stdcall
#define __cdecl
#define WINAPI
#define CALLBACK
#define OUT
#define __int64 long long
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _alloca alloca
#define lstrcmpi strcasecmp
#define lstrlen strlen
#define lstrcpy strcpy
#define ZeroMemory(p,n) memset(p,0,n)
#define CopyMemory(p,q,n) memcpy(p,q,n)
inline void InitializeCriticalSection(CRITICAL_SECTION*) {}
inline void DeleteCriticalSection(CRITICAL_SECTION*) {}
inline void EnterCriticalSection(CRITICAL_SECTION* m) { m->lock(); }
inline void LeaveCriticalSection(CRITICAL_SECTION* m) { m->unlock(); }
inline uint32_t timeGetTime() {
    return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline void DebugBreak() { std::abort(); }
inline char* _strupr(char* p) { for (char* s=p; *s; ++s) *s=std::toupper((unsigned char)*s); return p; }
inline char* _strlwr(char* p) { for (char* s=p; *s; ++s) *s=std::tolower((unsigned char)*s); return p; }
inline char* _itoa(int n,char* s,int base) {
    if (base == 10) sprintf(s,"%d",n);
    else std::abort();
    return s;
}
inline int _itoa_s(int n,char* s,size_t size,int base) {
    if (base != 10) return -1;
    return snprintf(s,size,"%d",n) < int(size) ? 0 : -1;
}
#endif

#ifndef _WIN32
void GetLocalTime(SYSTEMTIME* out);
bool SystemTimeToFileTime(const SYSTEMTIME*,FILETIME*);
bool FileTimeToSystemTime(const FILETIME*,SYSTEMTIME*);
#endif
