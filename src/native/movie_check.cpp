#include "movie.hpp"
#include "audio_backend.hpp"
#include <AL/al.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <stdexcept>
using namespace Native;
namespace {void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}}
int main(int argc,char** argv) {
    try {
        for(bool credits:{false,true})for(bool intro:{false,true}) {
            StartupMovies sequence(credits,intro);
            if(credits)require(sequence.next()=="intro_microsoft.avi","startup credits precede intro");
            if(intro)require(sequence.next()=="intro_movie.avi","startup intro follows credits or plays alone");
            require(sequence.next().empty() && sequence.next().empty(),"startup playlist finishes once after completion or skips");
        }
        Assets assets(argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork());
        TRef<OpenALBufferSource> mixer=new OpenALBufferSource(true);
        std::array<float,960> pcm;bool heard=false,paused=false;int frames=0;std::shared_ptr<ImageData> previous;
        {
            Movie movie(assets.find("intro_microsoft.avi"));
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
            while(!movie.finished() && std::chrono::steady_clock::now()<deadline) {
                auto frame=movie.advance();if(frame && frame!=previous){++frames;previous=frame;}
                if(frame && !paused) {
                    movie.paused(true);
                    for(int i=0;i<20;++i){mixer->render(pcm);require(movie.advance()==frame && !movie.finished(),"paused movie consumes video/audio time");}
                    movie.paused(false);paused=true;
                }
                mixer->render(pcm);for(float sample:pcm)heard|=std::abs(sample)>.001f;
                std::this_thread::yield();
            }
            require(movie.finished() && frames>1 && heard && paused,"original movie must decode, resume and finish both video and PCM");
        }
        // Skip while decode queues are full; destruction must wake the worker
        // and retire its source/buffers without needing presentation to resume.
        for(int i=0;i<3;++i){Movie movie(assets.find("intro_movie.avi"));movie.paused(true);movie.advance();}
        require(alGetError()==AL_NO_ERROR,"movie lifecycle leaves an OpenAL error");
        printf("Original FFmpeg FMV: %d presented frames, PCM, pause/resume, natural completion and repeated skip/teardown passed.\n",frames);return 0;
    }catch(const std::exception& e){fprintf(stderr,"movie: %s\n",e.what());return 1;}
}
