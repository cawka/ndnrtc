//
//  audio-receiver.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#undef NDN_LOGGING

#include "audio-receiver.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
#pragma mark - construction/destruction
NdnAudioReceiver::NdnAudioReceiver(const ParamsStruct &params) :
NdnMediaReceiver(params)
{
    playoutBuffer_ = new PlayoutBuffer();
    frameLogger_ = new NdnLogger(NdnLoggerDetailLevelDefault,
                                 "fetch-astat-%s.log", params.producerId);
    
}

NdnAudioReceiver::~NdnAudioReceiver()
{
    stopFetching();
    delete frameLogger_;
}
//******************************************************************************
#pragma mark - public
int NdnAudioReceiver::init(shared_ptr<Face> face)
{
    return NdnMediaReceiver::init(face);
}

int NdnAudioReceiver::startFetching()
{
    return NdnMediaReceiver::startFetching();
}

int NdnAudioReceiver::stopFetching()
{
    return NdnMediaReceiver::stopFetching();
}

//******************************************************************************
#pragma mark - intefaces realization -

//******************************************************************************
#pragma mark - private
void NdnAudioReceiver::playbackPacket(int64_t packetTsLocal)
{
    if (packetConsumer_)
    {
        FrameBuffer::Slot *slot = playoutBuffer_->acquireNextSlot();
        
        if (slot)
        {
            NdnAudioData::AudioPacket packet = slot->getAudioFrame();
            
            if (packet.length_ && packet.data_)
            {
                jitterTiming_.startFramePlayout();
                
                if (packet.isRTCP_)
                    packetConsumer_->onRTCPPacketReceived(packet.length_,
                                                          packet.data_);
                else
                {
                    packetConsumer_->onRTPPacketReceived(packet.length_,
                                                         packet.data_);
                    frameLogger_->log(NdnLoggerLevelInfo,
                                      "PLAYOUT: \t%d \t%d \t%d \t%d \t%d \t%ld \t%ld \t%.2f",
                                      slot->getFrameNumber(),
                                      slot->assembledSegmentsNumber(),
                                      slot->totalSegmentsNumber(),
                                      slot->isKeyFrame(),
                                      playoutBuffer_->getJitterSize(),
                                      slot->getAssemblingTimeUsec(),
                                      packet.timestamp_,
                                      currentProducerRate_);
                }
                
                // get playout time
                int framePlayoutTime = playoutBuffer_->releaseAcquiredSlot();
                int adjustedPlayoutTime = 0;
                
                // if av sync has been set up                
                if (slot && avSync_.get())
                    adjustedPlayoutTime= avSync_->synchronizePacket(slot,
                                                                    packetTsLocal);
                
                framePlayoutTime += adjustedPlayoutTime;
                jitterTiming_.updatePlayoutTime(packet.isRTCP_?0:framePlayoutTime);
                
                // setup and run playout timer for calculated playout interval
                jitterTiming_.runPlayoutTimer();
            }
            else
                WARN("got bad audio packet");
        } // no slot - missed packet
        else
        {
            DBG("can't obtain next audio slot");
            // should release slot event if it was not received
            playoutBuffer_->releaseAcquiredSlot();
        }
    }
}

bool NdnAudioReceiver::isLate(unsigned int frameNo)
{
    if (mode_ == ReceiverModeFetch &&
        frameNo < playoutBuffer_->framePointer())
        return true;
    
    return false;
}
