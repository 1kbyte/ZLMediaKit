/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PCM_H
#define ZLMEDIAKIT_PCM_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit{

/**
 * PCM音频通道
 */
class PCMTrack : public AudioTrackImp{
public:
    using Ptr = std::shared_ptr<PCMTrack>;
    
    /**
     * 构造PCM音频Track对象
     * @param sample_rate 音频采样率
     * @param channels 通道数
     * @param sample_bit 采样位数，通常为16
     */
    PCMTrack(int sample_rate = 8000, int channels = 1, int sample_bit = 16) : 
        AudioTrackImp(CodecL16, sample_rate, channels, sample_bit) {}

    /**
     * 获取编码器类型
     */
    CodecId getCodecId() const override {
        return CodecL16;
    }

    /**
     * 获取音频通道数
     */
    int getAudioChannel() const override {
        return AudioTrackImp::getAudioChannel();
    }

    /**
     * 获取音频采样位数，一般为16或8
     */
    int getAudioSampleBit() const override {
        return AudioTrackImp::getAudioSampleBit();
    }

    /**
     * 获取音频采样率
     */
    int getAudioSampleRate() const override {
        return AudioTrackImp::getAudioSampleRate();
    }

private:
    Track::Ptr clone() const override {
        return std::make_shared<PCMTrack>(*this);
    }
};

} // namespace mediakit
#endif // ZLMEDIAKIT_PCM_H