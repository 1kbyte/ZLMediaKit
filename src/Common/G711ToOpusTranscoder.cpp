/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "G711ToOpusTranscoder.h"
#include "Common/config.h"

#if defined(ENABLE_FFMPEG)
#include "Codec/Transcode.h"
#endif

using namespace std;
using namespace toolkit;

namespace mediakit {

G711ToOpusTranscoder::G711ToOpusTranscoder(const Track::Ptr &g711_track, const Track::Ptr &opus_track) {
    _g711_track = g711_track;
    _opus_track = opus_track;
    
#if defined(ENABLE_FFMPEG)
    // 创建G711解码器
    // Create G711 decoder
    _decoder = std::make_shared<FFmpegDecoder>(g711_track);
    
    // 创建Opus编码器
    // Create Opus encoder
    _encoder = std::make_shared<FFmpegEncoder>(opus_track);
    
    // 设置解码回调
    // Set decode callback
    _decoder->setOnDecode([this](const FFmpegFrame::Ptr &frame) {
        // 将解码后的PCM数据送入Opus编码器
        // Send decoded PCM data to Opus encoder
        _encoder->inputFrame(frame, [this](const Frame::Ptr &frame) {
            // 将编码后的Opus数据分发给下游
            // Dispatch encoded Opus data to downstream
            return FrameDispatcher::inputFrame(frame);
        });
        return true;
    });
    
    InfoL << "G711ToOpusTranscoder created: " << g711_track->getCodecName() << " -> Opus";
#else
    WarnL << "G711ToOpusTranscoder requires ENABLE_FFMPEG";
#endif
}

bool G711ToOpusTranscoder::inputFrame(const Frame::Ptr &frame) {
#if defined(ENABLE_FFMPEG)
    if (_decoder && _encoder) {
        // 将G711帧送入解码器
        // Send G711 frame to decoder
        _decoder->inputFrame(frame);
        return true;
    }
#endif
    WarnL << "G711ToOpusTranscoder not initialized or FFMPEG not enabled";
    return false;
}

} // namespace mediakit