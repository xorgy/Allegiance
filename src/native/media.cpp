#include "assets.hpp"
#include <stdexcept>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace Native {
std::shared_ptr<ImageData> decodeImage(const std::filesystem::path& path) {
    AVFormatContext* format=nullptr;
    if(avformat_open_input(&format,path.string().c_str(),nullptr,nullptr)<0) throw std::runtime_error("FFmpeg cannot open "+path.string());
    struct Cleanup {
        AVFormatContext*& format; AVCodecContext* codec{};AVPacket* packet{};AVFrame* frame{};SwsContext* scale{};
        ~Cleanup(){sws_freeContext(scale);av_frame_free(&frame);av_packet_free(&packet);avcodec_free_context(&codec);avformat_close_input(&format);}
    } c{format};
    if(avformat_find_stream_info(format,nullptr)<0) throw std::runtime_error("FFmpeg cannot read "+path.string());
    const AVCodec* decoder=nullptr;
    int stream=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,&decoder,0);
    if(stream<0) throw std::runtime_error("No video/image stream: "+path.string());
    c.codec=avcodec_alloc_context3(decoder);
    if(!c.codec || avcodec_parameters_to_context(c.codec,format->streams[stream]->codecpar)<0 || avcodec_open2(c.codec,decoder,nullptr)<0)
        throw std::runtime_error("Cannot initialize FFmpeg decoder");
    c.packet=av_packet_alloc();c.frame=av_frame_alloc();
    bool ready=false;
    while(!ready && av_read_frame(format,c.packet)>=0) {
        if(c.packet->stream_index==stream) {
            if(avcodec_send_packet(c.codec,c.packet)<0) throw std::runtime_error("Invalid video packet");
            ready=avcodec_receive_frame(c.codec,c.frame)==0;
        }
        av_packet_unref(c.packet);
    }
    if(!ready) {avcodec_send_packet(c.codec,nullptr);ready=avcodec_receive_frame(c.codec,c.frame)==0;}
    if(!ready) throw std::runtime_error("No decoded image: "+path.string());
    auto im=std::make_shared<ImageData>();im->name=path.string();im->width=c.frame->width;im->height=c.frame->height;
    if(!im->width || !im->height || im->width>16384 || im->height>16384) throw std::runtime_error("Invalid image dimensions");
    im->pixels.resize(size_t(im->width)*im->height*4);
    c.scale=sws_getContext(im->width,im->height,AVPixelFormat(c.frame->format),im->width,im->height,AV_PIX_FMT_RGBA,SWS_BILINEAR,nullptr,nullptr,nullptr);
    if(!c.scale) throw std::runtime_error("Cannot convert image to RGBA");
    uint8_t* dest[]={im->pixels.data()};int strides[]={int(im->width*4)};
    sws_scale(c.scale,c.frame->data,c.frame->linesize,0,im->height,dest,strides);
    return im;
}
}
