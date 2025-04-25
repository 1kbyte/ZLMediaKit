#include "RtmpMediaSourceMuxer.h"
#include "Extension/Factory.h"
#include "Codec/Transcode.h"

namespace mediakit {

bool needTransToOpus(CodecId codec) {
    // GET_CONFIG(int, transG711, Rtc::kTranscodeG711);
    // switch (codec)
    // {
    // case CodecG711U:
    // case CodecG711A:
    //     return transG711;
    // case CodecAAC:
    //     return true;
    // default:
    //     return false;
    // }
    return true;
}

bool RtmpMediaSourceMuxer::addTrack(const Track::Ptr &track) {
    Track::Ptr newTrack = track;
    if (track->getTrackType() == TrackAudio && needTransToOpus(track->getCodecId())) {
        newTrack = Factory::getTrackByCodecId(CodecOpus);
        newTrack->setBitRate(64000);
        _audio_dec.reset(new FFmpegDecoder(track));
        _audio_enc.reset(new FFmpegEncoder(newTrack));
        // aac to opus
        _audio_dec->setOnDecode([this](const FFmpegFrame::Ptr &frame) { _audio_enc->inputFrame(frame, false); });
        _audio_enc->setOnEncode([this](const Frame::Ptr &frame) { RtmpMuxer::inputFrame(frame); });
    }
    return RtmpMuxer::addTrack(newTrack);
}

void RtmpMediaSourceMuxer::resetTracks()
{
    RtmpMuxer::resetTracks();
  _audio_dec = nullptr;
  _audio_enc = nullptr;
  if (_count) {
    InfoL << "stop transcode with " << _count << " items";
    _count = 0;
  }
}

void RtmpMediaSourceMuxer::onRegist(MediaSource &sender, bool regist)
{
  MediaSourceEventInterceptor::onRegist(sender, regist);
  _regist = regist;
}

bool RtmpMediaSourceMuxer::inputFrame(const Frame::Ptr &frame)
{
  if (_clear_cache) {
    _clear_cache = false;
    _media_src->clearCache();
  }
  if (_enabled) {
#if defined(ENABLE_FFMPEG)
    if (needTransToOpus(frame->getCodecId())) {
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
      }
      else if (_count) {
        InfoL << "stop transcode with " << _count << " items";
        _count = 0;
      }
      return true;
    }
#endif
    return RtmpMuxer::inputFrame(frame);
  }
  return false;
}

}//namespace mediakit