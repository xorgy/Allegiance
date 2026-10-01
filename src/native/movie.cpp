#include "movie.hpp"
#include "thread_affinity.hpp"
#include "presentation.hpp"
#include <AL/al.h>
#include <array>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
namespace Native {
struct Movie::Impl {
    AVFormatContext* format{};AVCodecContext* video{},*audio{};SwsContext* scale{};SwrContext* resample{};
    int videoStream=-1,audioStream=-1,width{},height{};
    struct VideoFrame {double pts;std::shared_ptr<ImageData> image;};
    struct AudioFrame {double pts;std::vector<int16_t> samples;};
    std::deque<VideoFrame> images;std::deque<AudioFrame> samples;
    std::mutex mutex;std::condition_variable changed;std::thread worker;
    bool stop{},eof{},done{},paused{};std::string error;
    ALuint source{};std::array<ALuint,8> buffers{};std::vector<ALuint> freeBuffers;
    std::unordered_map<ALuint,uint64_t> lengths;
    uint64_t played{},startTime{},audioEndTime{};double audioStart{},decodedAudioEnd{},videoEnd{};uint64_t pauseTime{};bool started{};
    std::shared_ptr<ImageData> current;
    ~Impl() {
        {std::lock_guard lock(mutex);stop=true;}changed.notify_all();if(worker.joinable())worker.join();
        if(source){alSourceStop(source);alDeleteSources(1,&source);alDeleteBuffers(buffers.size(),buffers.data());}
        swr_free(&resample);sws_freeContext(scale);avcodec_free_context(&video);avcodec_free_context(&audio);avformat_close_input(&format);
    }
    void initialize(const std::filesystem::path& file) {
        const Affinity::ScopedUnpin workerMask(true);
        if(avformat_open_input(&format,file.string().c_str(),nullptr,nullptr)<0 || avformat_find_stream_info(format,nullptr)<0)throw std::runtime_error("Cannot open movie: "+file.string());
        auto decoder=[&](AVMediaType type,int& stream,AVCodecContext*& context,bool required) {
            const AVCodec* codec=nullptr;stream=av_find_best_stream(format,type,-1,-1,&codec,0);
            if(stream<0){if(required)throw std::runtime_error("Movie has no video stream");return;}
            context=avcodec_alloc_context3(codec);
            if(!context || avcodec_parameters_to_context(context,format->streams[stream]->codecpar)<0 || avcodec_open2(context,codec,nullptr)<0)throw std::runtime_error("Cannot initialize movie decoder");
        };
        decoder(AVMEDIA_TYPE_VIDEO,videoStream,video,true);decoder(AVMEDIA_TYPE_AUDIO,audioStream,audio,false);
        width=video->width;height=video->height;
        if(width<=0 || height<=0 || width>8192 || height>8192)throw std::runtime_error("Invalid movie dimensions");
        scale=sws_getContext(width,height,video->pix_fmt,width,height,AV_PIX_FMT_RGBA,SWS_BILINEAR,nullptr,nullptr,nullptr);
        if(!scale)throw std::runtime_error("Cannot create movie pixel converter");
        if(audio) {
            AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;
            if(swr_alloc_set_opts2(&resample,&stereo,AV_SAMPLE_FMT_S16,48000,&audio->ch_layout,audio->sample_fmt,audio->sample_rate,0,nullptr)<0 || swr_init(resample)<0)throw std::runtime_error("Cannot create movie audio converter");
            alGenSources(1,&source);alGenBuffers(buffers.size(),buffers.data());freeBuffers.assign(buffers.begin(),buffers.end());
            alSourcei(source,AL_SOURCE_RELATIVE,AL_TRUE);alSourcef(source,AL_ROLLOFF_FACTOR,0);alSourcef(source,AL_GAIN,0.8f);
        }
        worker=std::thread([this]{decode();});
    }
    double timestamp(AVFrame* frame,int stream) {
        int64_t ts=frame->best_effort_timestamp;auto* s=format->streams[stream];
        if(ts==AV_NOPTS_VALUE)ts=0;
        return double(ts-(s->start_time==AV_NOPTS_VALUE?0:s->start_time))*av_q2d(s->time_base);
    }
    void decoded(AVFrame* frame,bool isVideo) {
        if(isVideo) {
            auto image=std::make_shared<ImageData>();image->width=width;image->height=height;image->mipmaps=false;image->name="FMV";image->pixels.resize(size_t(width)*height*4);
            uint8_t* dest[]={image->pixels.data()};int stride[]={width*4};sws_scale(scale,frame->data,frame->linesize,0,height,dest,stride);
            std::unique_lock lock(mutex);changed.wait(lock,[&]{return stop || images.size()<4;});if(stop)return;
            double pts=timestamp(frame,videoStream);auto rate=av_guess_frame_rate(format,format->streams[videoStream],frame);
            double duration=frame->duration>0?frame->duration*av_q2d(format->streams[videoStream]->time_base):rate.num>0?double(rate.den)/rate.num:0;
            videoEnd=std::max(videoEnd,pts+duration);images.push_back({pts,std::move(image)});
        } else {
            int count=swr_get_out_samples(resample,frame->nb_samples);AudioFrame chunk;chunk.pts=timestamp(frame,audioStream);chunk.samples.resize(size_t(count)*2);
            uint8_t* dest=reinterpret_cast<uint8_t*>(chunk.samples.data());int n=swr_convert(resample,&dest,count,const_cast<const uint8_t**>(frame->extended_data),frame->nb_samples);
            if(n<0)throw std::runtime_error("Movie audio conversion failed");chunk.samples.resize(size_t(n)*2);
            decodedAudioEnd=chunk.pts+double(n)/48000;
            std::unique_lock lock(mutex);changed.wait(lock,[&]{return stop || samples.size()<32;});if(stop)return;samples.push_back(std::move(chunk));
        }
        changed.notify_all();
    }
    void decode() {
        AVPacket* packet=av_packet_alloc();AVFrame* frame=av_frame_alloc();
        try {
            auto receive=[&](AVCodecContext* decoder,bool isVideo) {
                int result;while((result=avcodec_receive_frame(decoder,frame))==0) {decoded(frame,isVideo);av_frame_unref(frame);std::lock_guard lock(mutex);if(stop)return;}
                if(result!=AVERROR(EAGAIN) && result!=AVERROR_EOF)throw std::runtime_error("Movie frame decoding failed");
            };
            while(av_read_frame(format,packet)>=0) {
                {std::lock_guard lock(mutex);if(stop)break;}
                auto* decoder=packet->stream_index==videoStream?video:packet->stream_index==audioStream?audio:nullptr;
                if(decoder){if(avcodec_send_packet(decoder,packet)<0)throw std::runtime_error("Invalid movie packet");receive(decoder,decoder==video);}
                av_packet_unref(packet);
            }
            avcodec_send_packet(video,nullptr);receive(video,true);if(audio){
                avcodec_send_packet(audio,nullptr);receive(audio,false);
                int count=swr_get_out_samples(resample,0);AudioFrame tail;tail.pts=decodedAudioEnd;tail.samples.resize(size_t(count)*2);
                uint8_t* dest=reinterpret_cast<uint8_t*>(tail.samples.data());int n=swr_convert(resample,&dest,count,nullptr,0);
                if(n>0) {
                    tail.samples.resize(size_t(n)*2);std::unique_lock lock(mutex);
                    changed.wait(lock,[&]{return stop || samples.size()<32;});if(!stop)samples.push_back(std::move(tail));
                }
            }
        }catch(const std::exception& e){std::lock_guard lock(mutex);error=e.what();}
        av_frame_free(&frame);av_packet_free(&packet);{std::lock_guard lock(mutex);eof=true;}changed.notify_all();
    }
    void setPaused(bool value) {
        if(paused==value)return;uint64_t now=monotonicNs();
        if(value){pauseTime=now;if(source && started)alSourcePause(source);}
        else {if(started)startTime+=now-pauseTime;if(audioEndTime)audioEndTime+=now-pauseTime;}
        paused=value;
    }
    std::shared_ptr<ImageData> advance() {
        std::lock_guard lock(mutex);if(!error.empty())throw std::runtime_error(error);
        if(audio) {
            ALint processed{};alGetSourcei(source,AL_BUFFERS_PROCESSED,&processed);
            while(processed-->0){ALuint buffer;alSourceUnqueueBuffers(source,1,&buffer);played+=lengths[buffer];freeBuffers.push_back(buffer);}
            while(!samples.empty() && !freeBuffers.empty()) {
                auto buffer=freeBuffers.back();freeBuffers.pop_back();auto& chunk=samples.front();
                if(!started && played==0 && freeBuffers.size()==buffers.size()-1)audioStart=chunk.pts;
                alBufferData(buffer,AL_FORMAT_STEREO16,chunk.samples.data(),chunk.samples.size()*2,48000);lengths[buffer]=chunk.samples.size()/2;alSourceQueueBuffers(source,1,&buffer);samples.pop_front();
            }
        }
        if(!started && !images.empty() && (!audio || freeBuffers.size()<buffers.size())) {
            started=true;startTime=paused?pauseTime:monotonicNs();if(audio && !paused)alSourcePlay(source);
        }
        uint64_t clock=paused?pauseTime:monotonicNs();
        double time=started?double(clock-startTime)/1e9:0;
        if(audio && started) {
            ALint offset{},state;alGetSourcei(source,AL_SAMPLE_OFFSET,&offset);alGetSourcei(source,AL_SOURCE_STATE,&state);
            time=audioStart+double(played+offset)/48000;
            if(!paused && state!=AL_PLAYING && freeBuffers.size()<buffers.size())alSourcePlay(source);
            // A container may end the audio before its last video frame. Once
            // audio is drained, advance that tail on the monotonic clock.
            if(eof && samples.empty() && freeBuffers.size()==buffers.size()) {
                if(!audioEndTime)audioEndTime=clock;
                time+=double(clock-audioEndTime)/1e9;
            }
        }
        while(!images.empty() && images.front().pts<=time) {current=images.front().image;images.pop_front();}
        done=eof && images.empty() && time>=videoEnd && samples.empty() && (!audio || freeBuffers.size()==buffers.size());changed.notify_all();return current;
    }
};
Movie::Movie(const std::filesystem::path& file):impl_(std::make_unique<Impl>()){impl_->initialize(file);}
Movie::~Movie()=default;
glm::ivec2 Movie::size()const{return {impl_->width,impl_->height};}
std::shared_ptr<ImageData> Movie::advance(){return impl_->advance();}
bool Movie::finished()const{return impl_->done;}
void Movie::paused(bool value){impl_->setPaused(value);}
}
