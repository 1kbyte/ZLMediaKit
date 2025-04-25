/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "G711ToPcmTranscoder.h"
#include "Util/logger.h"

using namespace std;

namespace mediakit {

G711ToPcmTranscoder::G711ToPcmTranscoder(const Track::Ptr &g711_track, const Track::Ptr &pcm_track) {
    // 判断输入轨道是否为G711
    if (g711_track->getCodecId() != CodecG711A && g711_track->getCodecId() != CodecG711U) {
        throw std::invalid_argument("不支持转码的编码类型:" + std::to_string(g711_track->getCodecId()));
    }

    // 获取音频参数
    auto audio_track = dynamic_pointer_cast<AudioTrack>(g711_track);
    CHECK(audio_track, "不是音频轨道");
    _codec_id = g711_track->getCodecId();
    _sample_rate = audio_track->getAudioSampleRate();
    _channels = audio_track->getAudioChannel();
    _sample_bit = audio_track->getAudioSampleBit();

    // 创建PCM轨道
    if (!pcm_track) {
        _pcm_track = std::make_shared<PCMTrack>(_sample_rate, _channels, _sample_bit);
    } else {
        _pcm_track = pcm_track;
    }

    InfoL << "创建G711到PCM转码器，编码类型:" << _codec_id << ", 采样率:" << _sample_rate 
          << ", 通道数:" << _channels << ", 采样位数:" << _sample_bit;
}

bool G711ToPcmTranscoder::inputFrame(const Frame::Ptr &frame) {
    if (frame->getCodecId() != _codec_id) {
        WarnL << "输入帧编码类型与G711转码器不匹配:" << frame->getCodecId() << " != " << _codec_id;
        return false;
    }

    // 转换G711为PCM
    auto pcm_frame = convertG711ToPcm(frame);
    if (!pcm_frame) {
        WarnL << "G711转PCM失败";
        return false;
    }

    // 将PCM帧分发给监听者
    return FrameDispatcher::inputFrame(pcm_frame);
}

Frame::Ptr G711ToPcmTranscoder::convertG711ToPcm(const Frame::Ptr &g711_frame) {
    // 获取G711数据
    auto data = g711_frame->data();
    auto size = g711_frame->size();
    if (size == 0) {
        return nullptr;
    }

    // PCM数据长度是G711的2倍（每个G711样本解码为16位PCM样本）
    _pcm_buffer.resize(size * 2);

    // 根据编码类型选择解码方法
    if (_codec_id == CodecG711A) {
        // G711A (alaw) 转 PCM
        for (size_t i = 0; i < size; ++i) {
            int16_t pcm_sample = alaw_to_linear((uint8_t)data[i]);
            _pcm_buffer[i * 2] = pcm_sample & 0xFF;
            _pcm_buffer[i * 2 + 1] = (pcm_sample >> 8) & 0xFF;
        }
    } else {
        // G711U (ulaw) 转 PCM
        for (size_t i = 0; i < size; ++i) {
            int16_t pcm_sample = ulaw_to_linear((uint8_t)data[i]);
            _pcm_buffer[i * 2] = pcm_sample & 0xFF;
            _pcm_buffer[i * 2 + 1] = (pcm_sample >> 8) & 0xFF;
        }
    }

    // 创建PCM帧
    auto pcm_frame = FrameImp::create();
    pcm_frame->_codec_id = CodecL16;
    pcm_frame->_dts = g711_frame->dts();
    pcm_frame->_pts = g711_frame->pts();
    pcm_frame->_buffer.assign(_pcm_buffer.data(), _pcm_buffer.size());
    pcm_frame->_prefix_size = 0;
    return pcm_frame;
}

int16_t G711ToPcmTranscoder::alaw_to_linear(uint8_t alaw) {
    alaw ^= 0x55;
    int t = (alaw & 0x0F) << 4;
    int seg = (alaw & 0x70) >> 4;
    switch (seg) {
        case 0:
            t += 8;
            break;
        case 1:
            t += 0x108;
            break;
        default:
            t += 0x108;
            t <<= seg - 1;
    }
    return (alaw & 0x80) ? t : -t;
}

int16_t G711ToPcmTranscoder::ulaw_to_linear(uint8_t ulaw) {
    ulaw = ~ulaw;
    int t = ((ulaw & 0x0F) << 3) + 0x84;
    t <<= (ulaw & 0x70) >> 4;
    return (ulaw & 0x80) ? (0x84 - t) : (t - 0x84);
}

Track::Ptr G711ToPcmTranscoder::getPcmTrack() const {
    return _pcm_track;
}

} // namespace mediakit