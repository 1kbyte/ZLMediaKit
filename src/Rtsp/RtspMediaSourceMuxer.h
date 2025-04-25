/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H

#include "RtspMuxer.h"
#include "Rtsp/RtspMediaSource.h"
#include "Codec/Transcode.h"
#include "Extension/Factory.h"

namespace mediakit {
class FFmpegDecoder;
class FFmpegEncoder;

class RtspMediaSourceMuxer final : public RtspMuxer, public MediaSourceEventInterceptor,
                                   public std::enable_shared_from_this<RtspMediaSourceMuxer> {
public:
    using Ptr = std::shared_ptr<RtspMediaSourceMuxer>;

    RtspMediaSourceMuxer(const MediaTuple& tuple,
                         const ProtocolOption &option,
                         const TitleSdp::Ptr &title = nullptr) : RtspMuxer(title) {
        _option = option;
        if (_option.audio_transcode) {
            #ifndef ENABLE_FFMPEG
                WarnL << "without ffmpeg, skip transcode setting";
                _option.audio_transcode = false;
            #endif
        }
        _media_src = std::make_shared<RtspMediaSource>(tuple);
        getRtpRing()->setDelegate(_media_src);
    }

    // ~RtspMediaSourceMuxer() override {
    //     try {
    //         RtspMuxer::flush();
    //     } catch (std::exception &ex) {
    //         WarnL << ex.what();
    //     }
    // }

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        setDelegate(listener);
        _media_src->setListener(shared_from_this());
    }

    int readerCount() const{
        return _media_src->readerCount();
    }

    void setTimeStamp(uint32_t stamp){
        _media_src->setTimeStamp(stamp);
    }

    void addTrackCompleted() override {
        RtspMuxer::addTrackCompleted();
        _media_src->setSdp(getSdp());
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        _enabled = _option.rtsp_demand ? size : true;
        if (!size && _option.rtsp_demand) {
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool needTransToOpus(CodecId codec) {
        switch (codec)
        {
        case CodecG711U:
        case CodecG711A:
            return true;
        case CodecAAC:
            return true;
        default:
            return false;
        }
    }

    bool needTransToAac(CodecId codec) {
        switch (codec)
        {
        case CodecG711U:
        case CodecG711A:
            return true;
        case CodecOpus:
            return true;
        default:
            return false;
        }
    }


    bool inputFrame(const Frame::Ptr &frame) override {
        if (_clear_cache && _option.rtsp_demand) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled || !_option.rtsp_demand) {
            #if defined(ENABLE_FFMPEG)
            if (_option.audio_transcode && needTransToOpus(frame->getCodecId())) {
                if (!_audio_dec) { // addTrack可能没调, 这边根据情况再调一次
                    Track::Ptr track;
                    switch (frame->getCodecId())
                    {
                    case CodecAAC:
                    track = Factory::getTrackByCodecId(CodecAAC, 44100, 2, 16);
                    break;
                    case CodecG711A:
                    case CodecG711U:
                    track = Factory::getTrackByCodecId(frame->getCodecId());
                    break;
                    default:
                    break;
                    }
                    if (track)
                    addTrack(track);
                    if (!_audio_dec) return false;
                }
                if (readerCount() || !_regist) {
                    _audio_dec->inputFrame(frame, true, false);
                    if (!_count)
                    InfoL << "start transcode " << frame->getCodecName() << "," << frame->pts() << "->Opus";
                    _count++;
                } else if (_count) {
                    InfoL << "stop transcode with " << _count << " items";
                    _count = 0;
                }
                return true;
            }
            #endif
            return RtspMuxer::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        // 缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存  [AUTO-TRANSLATED:7cfd4d49]
        // The inputFrame function is still allowed to be triggered when the cache has not been cleared, so that the cache can be cleared in time.
        return _option.rtsp_demand ? (_clear_cache ? true : _enabled) : true;
    }
#if defined(ENABLE_FFMPEG)
    ~RtspMediaSourceMuxer() override{resetTracks();}

    void onRegist(MediaSource &sender, bool regist) override {
        MediaSourceEventInterceptor::onRegist(sender, regist);
        _regist = regist;
    }
    bool addTrack(const Track::Ptr & track) override {
        Track::Ptr newTrack = track;
        if (_option.audio_transcode && needTransToOpus(track->getCodecId())) {
          newTrack = Factory::getTrackByCodecId(CodecOpus);
          GET_CONFIG(int, bitrate, General::kOpusBitrate);
          newTrack->setBitRate(bitrate);
          _audio_dec.reset(new FFmpegDecoder(track));
          _audio_enc.reset(new FFmpegEncoder(newTrack));
          // aac to opus
          _audio_dec->setOnDecode([this](const FFmpegFrame::Ptr & frame) {
            _audio_enc->inputFrame(frame, false);
          });
          _audio_enc->setOnEncode([this](const Frame::Ptr& frame) {
              RtspMuxer::inputFrame(frame);
          });
        }
        return RtspMuxer::addTrack(newTrack);
    }
    void resetTracks() override {
        RtspMuxer::resetTracks();
        _audio_dec = nullptr;
        _audio_enc = nullptr;
        if (_count) {
          InfoL << "stop transcode with " << _count << " items";
          _count = 0;
        }
    }

private:
    int _count = 0;
    bool _regist = false;
    std::shared_ptr<FFmpegDecoder> _audio_dec;
    std::shared_ptr<FFmpegEncoder> _audio_enc;
#endif
private:
    bool _enabled = true;
    bool _clear_cache = false;
    ProtocolOption _option;
    RtspMediaSource::Ptr _media_src;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
