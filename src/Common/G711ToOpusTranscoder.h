/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_G711TOPUSTRANSCODER_H
#define ZLMEDIAKIT_G711TOPUSTRANSCODER_H

#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "ext-codec/Opus.h"
#include "ext-codec/G711.h"

#if defined(ENABLE_FFMPEG)
#include "Codec/Transcode.h"
#endif

namespace mediakit {

/**
 * G711音频转Opus转码器
 * G711 audio to Opus transcoder
 */
class G711ToOpusTranscoder : public FrameDispatcher {
public:
    using Ptr = std::shared_ptr<G711ToOpusTranscoder>;
    
    G711ToOpusTranscoder(const Track::Ptr &g711_track, const Track::Ptr &opus_track);
    ~G711ToOpusTranscoder() = default;
    
    /**
     * 输入G711音频帧
     * Input G711 audio frame
     */
    bool inputFrame(const Frame::Ptr &frame) override;
    
private:
    Track::Ptr _g711_track;
    Track::Ptr _opus_track;
    
#if defined(ENABLE_FFMPEG)
    std::shared_ptr<FFmpegDecoder> _decoder;
    std::shared_ptr<FFmpegEncoder> _encoder;
#endif
};

} // namespace mediakit

#endif // ZLMEDIAKIT_G711TOPUSTRANSCODER_H