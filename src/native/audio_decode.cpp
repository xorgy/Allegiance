#include "audio_decode.hpp"
#include <stdexcept>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
namespace Native {
DecodedAudio decodeAudio(const std::filesystem::path& path,bool mono) {
    AVFormatContext* format=nullptr;
    if(avformat_open_input(&format,path.string().c_str(),nullptr,nullptr)<0)throw std::runtime_error("Cannot open audio "+path.string());
    struct Cleanup {AVFormatContext*& format;AVCodecContext* codec{};AVPacket* packet{};AVFrame* frame{};SwrContext* resample{};
        ~Cleanup(){swr_free(&resample);av_frame_free(&frame);av_packet_free(&packet);avcodec_free_context(&codec);avformat_close_input(&format);}} c{format};
    if(avformat_find_stream_info(format,nullptr)<0)throw std::runtime_error("Cannot read audio stream");
    const AVCodec* decoder=nullptr;int stream=av_find_best_stream(format,AVMEDIA_TYPE_AUDIO,-1,-1,&decoder,0);
    if(stream<0)throw std::runtime_error("No audio stream");
    c.codec=avcodec_alloc_context3(decoder);
    if(!c.codec || avcodec_parameters_to_context(c.codec,format->streams[stream]->codecpar)<0 || avcodec_open2(c.codec,decoder,nullptr)<0)throw std::runtime_error("Cannot initialize audio decoder");
    c.codec->log_level_offset=16; // Vorbis discard-timestamp diagnostics are not playback failures.
    int channels=mono || c.codec->ch_layout.nb_channels==1?1:2;
    AVChannelLayout layout;av_channel_layout_default(&layout,channels);
    int setup=swr_alloc_set_opts2(&c.resample,&layout,AV_SAMPLE_FMT_S16,48000,&c.codec->ch_layout,c.codec->sample_fmt,c.codec->sample_rate,0,nullptr);
    av_channel_layout_uninit(&layout);
    if(setup<0 || swr_init(c.resample)<0)throw std::runtime_error("Cannot initialize audio resampler");
    c.packet=av_packet_alloc();c.frame=av_frame_alloc();std::vector<int16_t> samples;
    if(!c.packet || !c.frame)throw std::bad_alloc();
    bool firstFrame=true;
    auto receive=[&](){
        int result;
        while((result=avcodec_receive_frame(c.codec,c.frame))==0) {
            if(firstFrame) {
                firstFrame=false;auto* input=format->streams[stream];
                auto start=input->start_time==AV_NOPTS_VALUE?0:input->start_time;
                // Vorbis' first decoded frame can start after granule zero.
                // Preserve that timeline: ASR offsets are authored against the
                // complete wave, not a version with its initial samples removed.
                if(c.frame->best_effort_timestamp!=AV_NOPTS_VALUE) {
                    auto leading=av_rescale_q(c.frame->best_effort_timestamp-start,input->time_base,AVRational{1,48000});
                    if(leading>0 && leading<=48000)samples.resize(size_t(leading)*channels,0);
                }
            }
            int count=swr_get_out_samples(c.resample,c.frame->nb_samples);auto begin=samples.size();samples.resize(begin+count*channels);
            uint8_t* output=reinterpret_cast<uint8_t*>(samples.data()+begin);
            int converted=swr_convert(c.resample,&output,count,const_cast<const uint8_t**>(c.frame->extended_data),c.frame->nb_samples);
            if(converted<0)throw std::runtime_error("Audio conversion failed");samples.resize(begin+converted*channels);av_frame_unref(c.frame);
        }
        if(result!=AVERROR(EAGAIN) && result!=AVERROR_EOF)throw std::runtime_error("Audio decoding failed");
    };
    while(av_read_frame(format,c.packet)>=0) {
        if(c.packet->stream_index==stream) {if(avcodec_send_packet(c.codec,c.packet)<0)throw std::runtime_error("Invalid audio packet");receive();}
        av_packet_unref(c.packet);
    }
    avcodec_send_packet(c.codec,nullptr);receive();
    int count=swr_get_out_samples(c.resample,0);auto begin=samples.size();samples.resize(begin+count*channels);
    uint8_t* output=reinterpret_cast<uint8_t*>(samples.data()+begin);int converted=swr_convert(c.resample,&output,count,nullptr,0);
    if(converted>=0)samples.resize(begin+converted*channels);else samples.resize(begin);
    return {std::move(samples),channels,48000};
}
}
