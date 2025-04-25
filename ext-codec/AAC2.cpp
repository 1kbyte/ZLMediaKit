/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AAC2.h"
#include "Common/Parser.h"
#include "Extension/Factory.h"
#ifdef ENABLE_MP4
#include "mpeg4-aac.h"
#endif
#include <Common/config.h>
#include <cctype> // 用于std::isdigit

using namespace std;
using namespace toolkit;

namespace mediakit{

// 引用AAC.cpp中的函数
extern int getAacFrameLength(const uint8_t *data, size_t bytes);
extern string makeAacConfig(const uint8_t *hex, size_t length);
extern int dumpAacConfig(const string &config, size_t length, uint8_t *out, size_t out_size);
extern bool parseAacConfig(const string &config, int &samplerate, int &channels);

/**
 * AAC2类型SDP
 */
class AAC2Sdp : public Sdp {
public:
    /**
     * 构造函数
     * @param aac_cfg aac两个字节的配置描述
     * @param payload_type rtp payload type
     * @param sample_rate 音频采样率
     * @param channels 通道数
     * @param bitrate 比特率
     */
    AAC2Sdp(const string &aac_cfg, int payload_type, int sample_rate, int channels, int bitrate)
        : Sdp(sample_rate, payload_type) {
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecAAC) << "/" << sample_rate << "/" << channels << "\r\n";

        string configStr;
        char buf[4] = { 0 };
        for (auto &ch : aac_cfg) {
            snprintf(buf, sizeof(buf), "%02X", (uint8_t)ch);
            configStr.append(buf);
        }
        _printer << "a=fmtp:" << payload_type << " streamtype=5;profile-level-id=1;mode=AAC-hbr;"
                 << "sizelength=13;indexlength=3;indexdeltalength=3;config=" << configStr << "\r\n";
    }

    string getSdp() const override { return _printer; }

private:
    _StrPrinter _printer;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

AAC2Track::AAC2Track(int sample_rate, int channels, int sample_bit) {
    // 设置转码参数
    _sampleRate = sample_rate;
    _channel = channels;
    _sampleBit = sample_bit;
    
    // 创建有效的AAC配置
    // AAC-LC配置，根据采样率和通道数生成
    uint8_t profile = 2; // AAC LC
    uint8_t sample_rate_idx = 0;
    
    // 查找采样率索引
    if (sample_rate == 96000) sample_rate_idx = 0;
    else if (sample_rate == 88200) sample_rate_idx = 1;
    else if (sample_rate == 64000) sample_rate_idx = 2;
    else if (sample_rate == 48000) sample_rate_idx = 3;
    else if (sample_rate == 44100) sample_rate_idx = 4;
    else if (sample_rate == 32000) sample_rate_idx = 5;
    else if (sample_rate == 24000) sample_rate_idx = 6;
    else if (sample_rate == 22050) sample_rate_idx = 7;
    else if (sample_rate == 16000) sample_rate_idx = 8;
    else if (sample_rate == 12000) sample_rate_idx = 9;
    else if (sample_rate == 11025) sample_rate_idx = 10;
    else if (sample_rate == 8000) sample_rate_idx = 11;
    else if (sample_rate == 7350) sample_rate_idx = 12;
    else sample_rate_idx = 4; // 默认使用44100Hz
    
    uint8_t aac_cfg[2];
    aac_cfg[0] = (profile << 3) | (sample_rate_idx >> 1);
    aac_cfg[1] = ((sample_rate_idx & 0x01) << 7) | (channels << 3);
    
    _cfg.assign((char*)aac_cfg, 2);
    InfoL << "AAC2Track创建，采样率:" << sample_rate << "Hz, 通道数:" << channels;
}

AAC2Track::AAC2Track(const string &aac_cfg) {
    // 检查是否为数字字符串（如"1210"）
    if (aac_cfg.size() >= 4 && std::isdigit(aac_cfg[0]) && std::isdigit(aac_cfg[1]) && 
        std::isdigit(aac_cfg[2]) && std::isdigit(aac_cfg[3])) {
        // 数字字符串，创建有效的AAC配置
        WarnL << "检测到无效的AAC配置(数字字符串): " << hexdump(aac_cfg.data(), aac_cfg.size()) 
              << ", 使用默认配置替代";
        
        // AAC-LC, 44.1kHz, 立体声的标准配置
        uint8_t default_cfg[2] = {0x12, 0x10};
        _cfg.assign((char*)default_cfg, 2);
    } else if (aac_cfg.size() < 2) {
        throw std::invalid_argument("adts配置必须最少2个字节");
    } else {
        _cfg = aac_cfg;
    }
    update();
}

CodecId AAC2Track::getCodecId() const {
    return CodecAAC;
}

bool AAC2Track::ready() const {
    // return !_cfg.empty();
    return _channel != 0;
}

int AAC2Track::getAudioSampleRate() const {
    return _sampleRate;
}

int AAC2Track::getAudioSampleBit() const {
    return _sampleBit;
}

int AAC2Track::getAudioChannel() const {
    return _channel;
}

bool AAC2Track::initFFmpeg() {
    if (_ffmpeg_initialized) {
        return true;
    }
    
    try {
        // 创建解码器
        _audio_dec = std::make_shared<FFmpegDecoder>(shared_from_this());
        
        // 创建编码器
        // 确保通道布局正确设置
        auto track = Factory::getTrackByCodecId(CodecAAC, _sampleRate, _channel, _sampleBit);
        
        // 设置比特率（可选）
        GET_CONFIG(int, bitrate, General::kAacBitrate);
        if (_bitrate > 0) {
            track->setBitRate(_bitrate);
        } else if (bitrate > 0) {
            track->setBitRate(bitrate);
        } else {
            // 默认使用采样率作为比特率基准
            track->setBitRate(_sampleRate * _channel);
        }
        
        // 创建编码器并确保通道布局正确设置
        _audio_enc = std::make_shared<FFmpegEncoder>(track);
        
        // 设置解码回调
        _audio_dec->setOnDecode([this](const FFmpegFrame::Ptr & frame) {
            _audio_enc->inputFrame(frame, false);
        });
        
        // 设置编码回调
        _audio_enc->setOnEncode([this](const Frame::Ptr& frame) {
            AudioTrack::inputFrame(frame);
        });
        
        _ffmpeg_initialized = true;
        InfoL << "AAC2Track FFmpeg initialized, sample rate:" << _sampleRate << ", channels:" << _channel;
        return true;
    } catch (std::exception &ex) {
        ErrorL << "AAC2Track FFmpeg init failed: " << ex.what();
        _audio_dec = nullptr;
        _audio_enc = nullptr;
        _ffmpeg_initialized = false;
        return false;
    }
}

void AAC2Track::setTranscodeParams(int sample_rate, int channels, int sample_bit) {
    _sampleRate = sample_rate;
    _channel = channels;
    _sampleBit = sample_bit;
}

static Frame::Ptr addADTSHeader(const Frame::Ptr &frame_in, const std::string &aac_config) {
    auto frame = FrameImp::create();
    frame->_codec_id = CodecAAC;
    // 生成adts头
    char adts_header[32] = { 0 };
    auto size = dumpAacConfig(aac_config, frame_in->size(), (uint8_t *)adts_header, sizeof(adts_header));
    CHECK(size > 0, "Invalid adts config");
    frame->_prefix_size = size;
    frame->_dts = frame_in->dts();
    frame->_buffer.assign(adts_header, size);
    frame->_buffer.append(frame_in->data(), frame_in->size());
    frame->setIndex(frame_in->getIndex());
    return frame;
}

bool AAC2Track::inputFrame(const Frame::Ptr &frame) {
    if (!ready()) {
        return false;
    }
    
    if (!_ffmpeg_initialized && !initFFmpeg()) {
        WarnL << "AAC2Track FFmpeg not initialized";
        return false;
    }
    
    if (!frame->prefixSize()) {
        // 没有ADTS头，添加ADTS头
        return inputFrame_l(addADTSHeader(frame, _cfg));
    }

    bool ret = false;
    // 有adts头，尝试分帧
    int64_t dts = frame->dts();
    int64_t pts = frame->pts();

    auto ptr = frame->data();
    auto end = frame->data() + frame->size();
    while (ptr < end) {
        auto frame_len = getAacFrameLength((uint8_t *)ptr, end - ptr);
        if (frame_len < ADTS_HEADER_LEN) {
            break;
        }
        if (frame_len == (int)frame->size()) {
            return inputFrame_l(frame);
        }
        auto sub_frame = std::make_shared<FrameInternalBase<FrameFromPtr>>(frame, (char *)ptr, frame_len, dts, pts, ADTS_HEADER_LEN);
        ptr += frame_len;
        if (ptr > end) {
            WarnL << "invalid aac length in adts header: " << frame_len
                  << ", remain data size: " << end - (ptr - frame_len);
            break;
        }
        if (inputFrame_l(sub_frame)) {
            ret = true;
        }
        dts += 1024 * 1000 / getAudioSampleRate();
        pts += 1024 * 1000 / getAudioSampleRate();
    }
    return ret;
}

bool AAC2Track::inputFrame_l(const Frame::Ptr &frame) {
    if (_cfg.empty() && frame->prefixSize()) {
        // 未获取到aac_cfg信息，根据7个字节的adts头生成aac config
        _cfg = makeAacConfig((uint8_t *)(frame->data()), frame->prefixSize());
        update();
    }

    if (frame->size() > frame->prefixSize()) {
        // 除adts头外，有实际负载
        _count++;
        if (_count % 100 == 0) {
            InfoL << "AAC2Track processed " << _count << " frames";
        }
        
        // 使用FFmpeg解码
        _audio_dec->inputFrame(frame, true, false);
        return true;
    }
    return false;
}

toolkit::Buffer::Ptr AAC2Track::getExtraData() const {
    CHECK(ready());
    return std::make_shared<BufferString>(_cfg);
}

void AAC2Track::setExtraData(const uint8_t *data, size_t size) {
    CHECK(size >= 2);
    _cfg.assign((char *)data, size);
    update();
}

bool AAC2Track::update() {
    // 尝试解析AAC配置
    if (!parseAacConfig(_cfg, _sampleRate, _channel)) {
        // 解析失败，检查配置是否为数字字符串（如"1210"）
        if (_cfg.size() >= 4 && std::isdigit(_cfg[0]) && std::isdigit(_cfg[1]) && 
            std::isdigit(_cfg[2]) && std::isdigit(_cfg[3])) {
            // 可能是误传入的数字字符串，尝试使用默认配置
            WarnL << "AAC配置格式错误(可能是数字字符串): " << hexdump(_cfg.data(), _cfg.size()) 
                  << ", 使用默认配置";
            
            // 设置默认值
            _sampleRate = 44100; // 默认采样率
            _channel = 2;        // 默认双声道
            
            // 创建有效的AAC配置
            // AAC-LC, 44.1kHz, 立体声的标准配置
            uint8_t aac_cfg[2] = {0x12, 0x10}; // 对应于AAC-LC, 44.1kHz, 立体声
            _cfg.assign((char*)aac_cfg, 2);
            
            return true;
        }
        return false;
    }
    return true;
}

Track::Ptr AAC2Track::clone() const {
    return std::make_shared<AAC2Track>(*this);
}

Sdp::Ptr AAC2Track::getSdp(uint8_t payload_type) const {
    return std::make_shared<AAC2Sdp>(getExtraData()->toString(), payload_type, getAudioSampleRate(), getAudioChannel(), getBitRate() >> 10);
}

} // namespace mediakit