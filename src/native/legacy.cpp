#include "legacy.hpp"
ILogger* g_pDebugLogger = nullptr;
void ZAssertImpl(bool ok,const char* expr,const char* file,int line,const char*) {
    if (!ok) { fprintf(stderr,"Assertion: %s (%s:%d)\n",expr,file,line); std::abort(); }
}
void retailf(const char* fmt,...) {
    va_list args; va_start(args,fmt); vfprintf(stderr,fmt,args); va_end(args);
}

#ifndef _WIN32
static SYSTEMTIME systemTime(const tm& t) {
    return {uint16_t(t.tm_year+1900),uint16_t(t.tm_mon+1),uint16_t(t.tm_wday),
        uint16_t(t.tm_mday),uint16_t(t.tm_hour),uint16_t(t.tm_min),uint16_t(t.tm_sec),0};
}
void GetLocalTime(SYSTEMTIME* out) {
    time_t now = time(nullptr); tm t{}; localtime_r(&now,&t); *out = systemTime(t);
}
bool SystemTimeToFileTime(const SYSTEMTIME* in,FILETIME* out) {
    tm t{}; t.tm_year=in->wYear-1900; t.tm_mon=in->wMonth-1; t.tm_mday=in->wDay;
    t.tm_hour=in->wHour; t.tm_min=in->wMinute; t.tm_sec=in->wSecond;
    uint64_t ticks = (int64_t(timegm(&t))+11644473600LL)*10000000ULL+in->wMilliseconds*10000ULL;
    *out = {uint32_t(ticks),uint32_t(ticks>>32)}; return true;
}
bool FileTimeToSystemTime(const FILETIME* in,SYSTEMTIME* out) {
    uint64_t ticks = uint64_t(in->dwLowDateTime)|(uint64_t(in->dwHighDateTime)<<32);
    time_t seconds = int64_t(ticks/10000000ULL)-11644473600LL;
    tm t{}; if (!gmtime_r(&seconds,&t)) return false;
    *out=systemTime(t); out->wMilliseconds=(ticks/10000)%1000; return true;
}
#endif
