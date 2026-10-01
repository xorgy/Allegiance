#include "audio_decode.hpp"
#import <AVFoundation/AVFoundation.h>
#include <AudioToolbox/AudioToolbox.h>
#include <cmath>
#include <stdexcept>

namespace Native {
DecodedAudio decodeAudio(const std::filesystem::path& path,bool mono) {
    @autoreleasepool {
        auto name=path.string();
        NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:name.c_str()]];
        AVURLAsset* asset=[AVURLAsset URLAssetWithURL:url options:nil];
        __block NSArray<AVAssetTrack*>* tracks;
        __block NSError* loadError;
        auto ready=dispatch_semaphore_create(0);
        [asset loadTracksWithMediaType:AVMediaTypeAudio completionHandler:^(NSArray<AVAssetTrack*>* value,NSError* error) {
            tracks=value;loadError=error;dispatch_semaphore_signal(ready);
        }];
        if(dispatch_semaphore_wait(ready,dispatch_time(DISPATCH_TIME_NOW,30*NSEC_PER_SEC)))
            throw std::runtime_error("Native audio opening timed out: "+name);
        auto track=tracks.firstObject;
        if(!track)throw std::runtime_error("Cannot open native audio "+name+": "+(loadError?loadError.localizedDescription.UTF8String:"no audio track"));

        // AudioToolbox exposes the Vorbis granule duration. AVFoundation's track
        // duration includes codec padding, so it is unsuitable for authored ASR
        // loop bounds. Its reader correctly handles priming, unlike the compound
        // ExtAudioFile Vorbis/resampler path on macOS 26.
        AudioStreamBasicDescription input{};ExtAudioFileRef file{};size_t expected{};
        if(ExtAudioFileOpenURL((__bridge CFURLRef)url,&file)==noErr) {
            UInt32 size=sizeof(input);ExtAudioFileGetProperty(file,kExtAudioFileProperty_FileDataFormat,&size,&input);
            SInt64 count{};size=sizeof(count);
            if(ExtAudioFileGetProperty(file,kExtAudioFileProperty_FileLengthFrames,&size,&count)==noErr &&
               count>0 && input.mSampleRate>0) {
                double frames=double(count)*48000/input.mSampleRate;
                // Round outwards so a sub-sample conversion cannot invalidate
                // an ASR endpoint authored at the end of the original wave.
                if(frames<48000.*60*60)expected=size_t(std::ceil(frames));
            }
            ExtAudioFileDispose(file);
        }
        int channels=mono || input.mChannelsPerFrame==1?1:2;
        NSError* error;
        AVAssetReader* reader=[[AVAssetReader alloc] initWithAsset:asset error:&error];
        NSDictionary* format=@{AVFormatIDKey:@(kAudioFormatLinearPCM),AVSampleRateKey:@48000,
            AVNumberOfChannelsKey:@(channels),AVLinearPCMBitDepthKey:@16,
            AVLinearPCMIsFloatKey:@NO,AVLinearPCMIsBigEndianKey:@NO,AVLinearPCMIsNonInterleaved:@NO};
        AVAssetReaderTrackOutput* output=[[AVAssetReaderTrackOutput alloc] initWithTrack:track outputSettings:format];
        output.alwaysCopiesSampleData=NO;
        if(!reader || ![reader canAddOutput:output])throw std::runtime_error("Cannot configure native audio reader: "+name);
        [reader addOutput:output];
        if(![reader startReading])throw std::runtime_error("Cannot start native audio reader: "+name);
        DecodedAudio decoded{{},channels,48000};decoded.samples.reserve(expected*channels);
        while(auto sample=[output copyNextSampleBuffer]) {
            struct Cleanup {CMSampleBufferRef sample;~Cleanup(){CFRelease(sample);}} cleanup{sample};
            auto buffer=CMSampleBufferGetDataBuffer(sample);
            if(!buffer)throw std::runtime_error("Native audio reader produced no PCM buffer");
            auto bytes=CMBlockBufferGetDataLength(buffer);
            if(bytes%(channels*sizeof(int16_t)))throw std::runtime_error("Native audio reader produced partial PCM frames");
            auto first=decoded.samples.size();decoded.samples.resize(first+bytes/sizeof(int16_t));
            if(CMBlockBufferCopyDataBytes(buffer,0,bytes,decoded.samples.data()+first)!=noErr)
                throw std::runtime_error("Cannot copy native decoded audio");
        }
        if(reader.status!=AVAssetReaderStatusCompleted)
            throw std::runtime_error("Native audio decoding failed: "+name);
        // Native resampling may omit its final sub-millisecond filter tail.
        // Keep the container's sample timeline and original ASR endpoints.
        if(expected)decoded.samples.resize(expected*channels,0);
        return decoded;
    }
}
}
