﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TRANSCODE_H
#define ZLMEDIAKIT_TRANSCODE_H

#if defined(ENABLE_FFMPEG)

#include "Util/TimeTicker.h"
#include "Common/MediaSink.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/imgutils.h"
#include "libavutil/frame.h"
#ifdef __cplusplus
}
#endif

#define FF_CODEC_VER_7_1 AV_VERSION_INT(61, 0, 0)

namespace mediakit {

class FFmpegFrame {
public:
    using Ptr = std::shared_ptr<FFmpegFrame>;

    FFmpegFrame(std::shared_ptr<AVFrame> frame = nullptr);
    ~FFmpegFrame();

    AVFrame *get() const;
    void fillPicture(AVPixelFormat target_format, int target_width, int target_height);

private:
    char *_data = nullptr;
    std::shared_ptr<AVFrame> _frame;
};

class FFmpegSwr {
public:
    using Ptr = std::shared_ptr<FFmpegSwr>;

# if LIBAVCODEC_VERSION_INT >= FF_CODEC_VER_7_1
    FFmpegSwr(AVSampleFormat output, AVChannelLayout *ch_layout, int samplerate);
#else
    FFmpegSwr(AVSampleFormat output, int channel, int channel_layout, int samplerate);
#endif

    ~FFmpegSwr();
    FFmpegFrame::Ptr inputFrame(const FFmpegFrame::Ptr &frame);

private:

# if LIBAVCODEC_VERSION_INT >= FF_CODEC_VER_7_1
    AVChannelLayout _target_ch_layout;
#else
    int _target_channels;
    int _target_channel_layout;
#endif

    int _target_samplerate;
    AVSampleFormat _target_format;
    SwrContext *_ctx = nullptr;

    toolkit::ResourcePool<FFmpegFrame> _swr_frame_pool;
};

class TaskManager {
public:
    virtual ~TaskManager();

    void setMaxTaskSize(size_t size);
    void stopThread(bool drop_task);

protected:
    void startThread(const std::string &name);
    bool addEncodeTask(std::function<void()> task);
    bool addDecodeTask(bool key_frame, std::function<void()> task);
    bool isEnabled() const;

private:
    void onThreadRun(const std::string &name);

private:
    class ThreadExitException : public std::runtime_error {
    public:
        ThreadExitException() : std::runtime_error("exit") {}
    };

private:
    bool _decode_drop_start = false;
    bool _exit = false;
    size_t _max_task = 30;
    std::mutex _task_mtx;
    toolkit::semaphore _sem;
    toolkit::List<std::function<void()> > _task;
    std::shared_ptr<std::thread> _thread;
};

class FFmpegDecoder : public TaskManager {
public:
    using Ptr = std::shared_ptr<FFmpegDecoder>;
    using onDec = std::function<void(const FFmpegFrame::Ptr &)>;

    FFmpegDecoder(const Track::Ptr &track, int thread_num = 2, const std::vector<std::string> &codec_name = {});
    ~FFmpegDecoder() override;

    bool inputFrame(const Frame::Ptr &frame, bool live, bool async, bool enable_merge = true);
    void setOnDecode(onDec cb);
    void flush();
    const AVCodecContext *getContext() const;

private:
    void onDecode(const FFmpegFrame::Ptr &frame);
    bool inputFrame_l(const Frame::Ptr &frame, bool live, bool enable_merge);
    bool decodeFrame(const char *data, size_t size, uint64_t dts, uint64_t pts, bool live, bool key_frame);

private:
    // default merge frame
    bool _do_merger = true;
    toolkit::Ticker _ticker;
    onDec _cb;
    std::shared_ptr<AVCodecContext> _context;
    FrameMerger _merger{FrameMerger::h264_prefix};
    toolkit::ResourcePool<FFmpegFrame> _frame_pool;
};

class FFmpegSws {
public:
    using Ptr = std::shared_ptr<FFmpegSws>;

    FFmpegSws(AVPixelFormat output, int width, int height);
    ~FFmpegSws();
    FFmpegFrame::Ptr inputFrame(const FFmpegFrame::Ptr &frame);
    int inputFrame(const FFmpegFrame::Ptr &frame, uint8_t *data);

private:
    FFmpegFrame::Ptr inputFrame(const FFmpegFrame::Ptr &frame, int &ret, uint8_t *data);

private:
    int _target_width = 0;
    int _target_height = 0;
    int _src_width = 0;
    int _src_height = 0;
    SwsContext *_ctx = nullptr;
    AVPixelFormat _src_format = AV_PIX_FMT_NONE;
    AVPixelFormat _target_format = AV_PIX_FMT_NONE;
    toolkit::ResourcePool<FFmpegFrame> _sws_frame_pool;
};

class FFmpegUtils {
public:
    /**
     * 保持图片为jpeg或png
     * @param frame 解码后的帧
     * @param filename 保存文件路径
     * @param fmt jpg:AV_PIX_FMT_YUVJ420P，PNG:AV_PIX_FMT_RGB24
     * @return
     */
    static std::tuple<bool, std::string> saveFrame(const FFmpegFrame::Ptr &frame, const char *filename, AVPixelFormat fmt = AV_PIX_FMT_YUVJ420P);
};

class FFmpegAudioFifo {
public:
    FFmpegAudioFifo() = default;
    ~FFmpegAudioFifo();

    bool Write(const AVFrame *frame);
    bool Read(AVFrame *frame, int sample_size);
    int size() const;

private:
    int _channels = 0;
    int _samplerate = 0;
    double _tsp = 0;
    double _timebase = 0;
    AVAudioFifo *_fifo = nullptr;
    AVSampleFormat _format = AV_SAMPLE_FMT_NONE;
};

class FFmpegEncoder : public TaskManager, public CodecInfo {
public:
    using Ptr = std::shared_ptr<FFmpegEncoder>;
    using onEnc = std::function<void(const Frame::Ptr &)>;

    FFmpegEncoder(const Track::Ptr &track, int thread_num = 2);
    ~FFmpegEncoder() override;

    void flush();
    CodecId getCodecId() const override { return _codecId; }
    const AVCodecContext *getContext() const { return _context.get(); }

    void setOnEncode(onEnc cb) { _cb = std::move(cb); }
    bool inputFrame(const FFmpegFrame::Ptr &frame, bool async);

private:
    bool inputFrame_l(FFmpegFrame::Ptr frame);
    bool encodeFrame(AVFrame *frame);
    void onEncode(AVPacket *packet);
    bool openVideoCodec(int width, int height, int bitrate, const AVCodec *codec);
    bool openAudioCodec(int samplerate, int channel, int bitrate, const AVCodec *codec);

private:
    onEnc _cb;
    CodecId _codecId;
    const AVCodec *_codec = nullptr;
    AVDictionary *_dict = nullptr;
    std::shared_ptr<AVCodecContext> _context;

    std::unique_ptr<FFmpegSws> _sws;
    std::unique_ptr<FFmpegSwr> _swr;
    std::unique_ptr<FFmpegAudioFifo> _fifo;
    bool var_frame_size = false;
};


class G711Transcoder : public FrameDispatcher {
    public:
        using Ptr = std::shared_ptr<G711Transcoder>;
        G711Transcoder(const Track::Ptr &g711_track, const Track::Ptr &opus_track) {
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
                _encoder->inputFrame(frame, true);
                return true;
            });
            InfoL << "G711ToOpusTranscoder created: " << g711_track->getCodecName() << " -> Opus";
        #else
            WarnL << "G711ToOpusTranscoder requires ENABLE_FFMPEG";
        #endif
        }
        ~G711Transcoder() = default;

        bool inputFrame(const Frame::Ptr &frame) override {
    #if defined(ENABLE_FFMPEG)
            if (_decoder && _encoder) {
                // 将G711帧送入解码器
                // Send G711 frame to decoder
                _decoder->inputFrame(frame, true, true, true);
                return true;
            }
    #endif
            WarnL << "G711ToOpusTranscoder not initialized or FFMPEG not enabled";
            return false;
        }
    private:
        Track::Ptr _g711_track;
        Track::Ptr _opus_track;

    #if defined(ENABLE_FFMPEG)
        std::shared_ptr<FFmpegDecoder> _decoder;
        std::shared_ptr<FFmpegEncoder> _encoder;
    #endif
    };

}//namespace mediakit
#endif// ENABLE_FFMPEG
#endif //ZLMEDIAKIT_TRANSCODE_H
