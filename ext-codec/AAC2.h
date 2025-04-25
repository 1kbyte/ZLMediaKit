/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AAC2_H
#define ZLMEDIAKIT_AAC2_H

#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "Codec/Transcode.h"
#define ADTS_HEADER_LEN 7

namespace mediakit{

/**
 * AAC2音频通道，使用FFmpeg库处理
 * AAC2 audio channel, using FFmpeg library for processing
 */
class AAC2Track : public std::enable_shared_from_this<AAC2Track>, public AudioTrack {
public:
    using Ptr = std::shared_ptr<AAC2Track>;

    AAC2Track() = default;

    /**
     * 通过aac extra data 构造对象
     * Construct object through AAC extra data
     */
    AAC2Track(const std::string &aac_cfg);
    AAC2Track(int sample_rate, int channels, int sample_bit);

    bool ready() const override;
    CodecId getCodecId() const override;
    int getAudioChannel() const override;
    int getAudioSampleRate() const override;
    int getAudioSampleBit() const override;
    bool inputFrame(const Frame::Ptr &frame) override;
    toolkit::Buffer::Ptr getExtraData() const override;
    void setExtraData(const uint8_t *data, size_t size) override;
    bool update() override;

    /**
     * 初始化FFmpeg解码器和编码器
     * Initialize FFmpeg decoder and encoder
     */
    bool initFFmpeg();

    /**
     * 设置转码参数
     * Set transcoding parameters
     * @param sample_rate 采样率
     * @param channels 通道数
     * @param sample_bit 采样位数
     */
    void setTranscodeParams(int sample_rate, int channels, int sample_bit = 16);

private:
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    Track::Ptr clone() const override;
    bool inputFrame_l(const Frame::Ptr &frame);

private:
    std::string _cfg;
    int _channel = 0;
    int _sampleRate = 0;
    int _sampleBit = 16;
    int _bitrate = 0;
    
    // FFmpeg相关成员
    std::shared_ptr<FFmpegDecoder> _audio_dec;
    std::shared_ptr<FFmpegEncoder> _audio_enc;
    bool _ffmpeg_initialized = false;
    int _count = 0; // 用于统计处理的帧数
};

}//namespace mediakit
#endif //ZLMEDIAKIT_AAC2_H