//
//  video-receiver.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_receiver__
#define __ndnrtc__video_receiver__

#include "ndnrtc-common.h"
#include "video-sender.h"
#include "frame-buffer.h"
#include "playout-buffer.h"
#include "ndnrtc-utils.h"

namespace ndnrtc
{
    class VideoReceiverParams : public VideoSenderParams {
    public:
        // static
        static VideoReceiverParams* defaultParams()
        {
            VideoReceiverParams *p = static_cast<VideoReceiverParams*>(VideoSenderParams::defaultParams());
            
            p->setIntParam(ParamNameProducerRate, 30);
            p->setIntParam(ParamNameInterestTimeout, 4);
            p->setIntParam(ParamNameFrameBufferSize, 120);
            p->setIntParam(ParamNameFrameSlotSize, 16000);
            return p;
        }
        
        // public methods go here
        int getProducerRate() const { return getParamAsInt(ParamNameProducerRate); }
        int getDefaultTimeout() const {return getParamAsInt(ParamNameInterestTimeout); }
        int getFrameBufferSize() const { return getParamAsInt(ParamNameFrameBufferSize); }
        int getFrameSlotSize() const { return getParamAsInt(ParamNameFrameSlotSize); }
    };
    
    class NdnVideoReceiver : public NdnRtcObject {
    public:
        NdnVideoReceiver(NdnParams *params);
        ~NdnVideoReceiver();
        
        int init(shared_ptr<Face> face);
        int startFetching();
        int stopFetching();
        void setFrameConsumer(IEncodedFrameConsumer *consumer) { frameConsumer_ = consumer; }
        
        unsigned int getPlaybackSkipped() { return playbackSkipped_; }
        unsigned int getNPipelined() { return pipelinerFrameNo_; }
        unsigned int getNPlayout() { return playoutFrameNo_; }
        unsigned int getNLateFrames() { return nLateFrames_; }
        unsigned int getBufferStat(FrameBuffer::Slot::State state) { return frameBuffer_.getStat(state); }
        double getPlayoutFreq () { return NdnRtcUtils::currentFrequencyMeterValue(playoutMeterId_); }
        double getIncomeFramesFreq() { return NdnRtcUtils::currentFrequencyMeterValue(incomeFramesMeterId_); }
        double getIncomeDataFreq() { return NdnRtcUtils::currentFrequencyMeterValue(assemblerMeterId_); }
        
    private:
        enum ReceiverMode {
            ReceiverModeCreated,
            ReceiverModeInit,
            ReceiverModeStarted,
            ReceiverModeWaitingFirstSegment,
            ReceiverModeFetch,
            ReceiverModeChase
        };
        
        ReceiverMode mode_;
        
        // statistics variables
        unsigned int playoutMeterId_, assemblerMeterId_, incomeFramesMeterId_;
        unsigned int playbackSkipped_ = 0;  // number of packets that were skipped due to late delivery,
                                        // i.e. playout thread requests frames at fixed rate, if a frame
                                        // has not arrived yet (not in playout buffer) - it is skipped
        unsigned int pipelinerOverhead_ = 0;   // number of outstanding frames pipeliner has requested already
        unsigned int nLateFrames_ = 0;      // number of late frames (arrived after their playback time)
        
        bool playout_;
        long playoutSleepIntervalUSec_; // 30 fps
        long playoutFrameNo_, pipelinerFrameNo_;
        int pipelinerEventsMask_, interestTimeoutMs_;
        unsigned int producerSegmentSize_;
        Name framesPrefix_;
        shared_ptr<Face> face_;
        
        FrameBuffer frameBuffer_;
        PlayoutBuffer playoutBuffer_;
        IEncodedFrameConsumer *frameConsumer_;
        
        webrtc::CriticalSectionWrapper &faceCs_; // needed to synmchrnous access to the NDN face object
        webrtc::ThreadWrapper &playoutThread_, &pipelineThread_, &assemblingThread_;
        
        // static routines for threads
        static bool playoutThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processPlayout(); }
        static bool pipelineThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processInterests(); }
        static bool assemblingThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processAssembling(); }
        
        // thread main functions (called iteratively by static routines)
        bool processPlayout();
        bool processInterests();
        bool processAssembling();
        
        // ndn-lib callbacks
        void onTimeout(const shared_ptr<const Interest>& interest);
        void onSegmentData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data);
        
        VideoReceiverParams *getParams() { return static_cast<VideoReceiverParams*>(params_); }
        
        void switchToMode(ReceiverMode mode);
        
        void requestInitialSegment();
        void pipelineInterests(FrameBuffer::Event &event);
        void requestSegment(unsigned int frameNo, unsigned int segmentNo);
        bool isStreamInterest(Name prefix);
        unsigned int getFrameNumber(Name prefix);
        unsigned int getSegmentNumber(Name prefix);
        void expressInterest(Name &prefix);
        void expressInterest(Interest &i);
        bool isLate(unsigned int frameNo);
    };
}

#endif /* defined(__ndnrtc__video_receiver__) */
