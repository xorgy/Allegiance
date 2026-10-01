#include "public_directory.hpp"
#include <csignal>
#include <cstdio>
#include <thread>
#include <chrono>
#include <charconv>

namespace {volatile std::sig_atomic_t running=1;void stop(int){running=0;}}
int main(int argc,char** argv){
    try{
        uint16_t port=Native::Net::PublicDirectory::defaultPort;
        for(int i=1;i<argc;++i){
            std::string_view arg=argv[i];
            if(arg=="--help"){puts("allegiance-directory [--port 27599]\nRegister authorities with allegiance-server --directory HOST:PORT.\nAdd this directory endpoint to the client's existing servers.txt.");return 0;}
            if(arg!="--port" || ++i>=argc)throw std::runtime_error("Expected --port PORT");
            std::string_view value=argv[i];unsigned parsed{};auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),parsed);
            if(error!=std::errc{} || end!=value.data()+value.size() || !parsed || parsed>65535)throw std::runtime_error("Invalid directory port");
            port=uint16_t(parsed);
        }
        Native::Net::PublicDirectory directory(port);
        std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
        printf("Native Allegiance public directory listening on UDP %u\n",port);fflush(stdout);
        while(running){directory.poll();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        return 0;
    }catch(const std::exception& e){fprintf(stderr,"Directory: %s\n",e.what());return 1;}
}
