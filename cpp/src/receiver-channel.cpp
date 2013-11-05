//
//  receiver-channel.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "receiver-channel.h"
#include "sender-channel.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//********************************************************************************
#pragma mark - construction/destruction
NdnReceiverChannel::NdnReceiverChannel(const ParamsStruct &params,
                                       const ParamsStruct &audioParams) :
NdnMediaChannel(params, audioParams),
localRender_(new NdnRenderer(1,params)),
decoder_(new NdnVideoDecoder(params)),
receiver_(new NdnVideoReceiver(params)),
audioReceiveChannel_(new NdnAudioReceiveChannel(NdnRtcUtils::sharedVoiceEngine()))
{
    localRender_->setObserver(this);
    decoder_->setObserver(this);
    receiver_->setObserver(this);
    audioReceiveChannel_->setObserver(this);
    
    receiver_->setFrameConsumer(decoder_.get());
    decoder_->setFrameConsumer(localRender_.get());
}

int NdnReceiverChannel::init()
{
    int res = NdnMediaChannel::init();
    
    if (RESULT_FAIL(res))
        return res;
    
    { // initialize video
        videoInitialized_ = RESULT_NOT_FAIL(localRender_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't initialize local renderer for "
                        "incoming video");
        
        videoInitialized_ &= RESULT_NOT_FAIL(decoder_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't initialize decoder for incoming "
                        "video");
        
        videoInitialized_ &= RESULT_NOT_FAIL(receiver_->init(ndnFace_));
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't initialize ndn fetching for "
                        "incoming video");
    }
    
    { // initialize audio
        audioInitialized_ = RESULT_NOT_FAIL(audioReceiveChannel_->init(audioParams_,
                                                                       ndnAudioFace_));
        if (!audioInitialized_)
            notifyError(RESULT_WARN, "can't initialize audio receive channel");
        
    }
    
    isInitialized_ = audioInitialized_||videoInitialized_;
    
    if (!isInitialized_)
        return notifyError(RESULT_ERR, "both audio and video can not be initialized. aborting.");
    
    INFO("fetching initialized with video: %s, audio: %s",
         (videoInitialized_)?"yes":"no",
         (audioInitialized_)?"yes":"no");
    
    return (videoInitialized_&&audioInitialized_)?RESULT_OK:RESULT_WARN;
}
int NdnReceiverChannel::startTransmission()
{
    int res = NdnMediaChannel::startTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    unsigned int tid = 1;
    
    if (videoInitialized_)
    {
        videoTransmitting_ = RESULT_NOT_FAIL(localRender_->startRendering(string(params_.producerId)));
        if (!videoTransmitting_)
            notifyError(RESULT_WARN, "can't start render");
        
        videoTransmitting_ &= RESULT_NOT_FAIL(receiver_->startFetching());
        if (!videoTransmitting_)
            notifyError(RESULT_WARN, "can't start fetching frames from ndn");
    }
    
    if (audioInitialized_)
    {
        audioTransmitting_ = RESULT_NOT_FAIL(audioReceiveChannel_->start());
        if (!audioTransmitting_)
            notifyError(RESULT_WARN, "can't start audio receive channel");
    }
    
    isTransmitting_ = audioTransmitting_ || videoTransmitting_;
    
    if (!isTransmitting_)
        return notifyError(RESULT_ERR, "both audio and video fetching can not "
                           "be started. aborting");
    
    INFO("fetching started with video: %s, audio: %s",
         (videoInitialized_)?"yes":"no",
         (audioInitialized_)?"yes":"no");
    
    return (audioTransmitting_&&videoTransmitting_)?RESULT_OK:RESULT_WARN;
}
int NdnReceiverChannel::stopTransmission()
{
    int res = NdnMediaChannel::stopTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    if (videoTransmitting_)
    {
        videoTransmitting_ = false;
        
        localRender_->stopRendering();
        receiver_->stopFetching();
    }
    
    if (audioTransmitting_)
    {
        audioTransmitting_ = false;
        audioReceiveChannel_->stop();
    }
    
    INFO("fetching stopped");
    
    isTransmitting_ = false;
    return RESULT_OK;
}
void NdnReceiverChannel::getStat(ReceiverChannelStatistics &stat) const
{
    memset(&stat,0,sizeof(stat));
    
    stat.nPipeline_ = receiver_->getNPipelined();
    stat.nPlayback_ = receiver_->getNPlayout();
//    stat.nFetched_ = receiver_->getLatest(FrameBuffer::Slot::StateReady);
    stat.nLate_ = receiver_->getNLateFrames();
    
    stat.nSkipped_ = receiver_->getPlaybackSkipped();
//    stat.nTotalTimeouts_ = receiver_
//    stat.nTimeouts_ = receiver_
    
    stat.nFree_ = receiver_->getBufferStat(FrameBuffer::Slot::StateFree);
    stat.nLocked_ = receiver_->getBufferStat(FrameBuffer::Slot::StateLocked);
    stat.nAssembling_ = receiver_->getBufferStat(FrameBuffer::Slot::StateAssembling);
    stat.nNew_ = receiver_->getBufferStat(FrameBuffer::Slot::StateAssembling);
    
    stat.playoutFreq_  = receiver_->getPlayoutFreq();
    stat.inDataFreq_ = receiver_->getIncomeDataFreq();
    stat.inFramesFreq_ = receiver_->getIncomeFramesFreq();
}
