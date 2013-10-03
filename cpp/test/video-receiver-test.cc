//
//  video-receiver-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#define DEBUG

#define NDN_LOGGING
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR
#define NDN_TRACE

#include "test-common.h"
#include "frame-buffer.h"
#include "playout-buffer.h"
#include "video-receiver.h"

using namespace ndnrtc;

//********************************************************************************
// VideoReceiver tests
class NdnReceiverTester : public NdnRtcObjectTestHelper, public IEncodedFrameConsumer
{
public :
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        shared_ptr<Transport::ConnectionInfo> connInfo(new TcpTransport::ConnectionInfo("localhost"));
        
        params_.reset(VideoReceiverParams::defaultParams());
        ndnTransport_.reset(new TcpTransport());
        ndnFace_.reset(new Face(ndnTransport_, connInfo));
        
        std::string streamAccessPrefix = params_.get()->getStreamKeyPrefix();
        ndnFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                 bind(&NdnReceiverTester::onInterest, this, _1, _2, _3),
                                 bind(&NdnReceiverTester::onRegisterFailed, this, _1));
        
        shared_ptr<PrivateKeyStorage> privateKeyStorage(new PrivateKeyStorage());
        shared_ptr<IdentityManager> identityManager(new IdentityManager(privateKeyStorage));
        
        ndnKeyChain_.reset(new KeyChain(identityManager));
        ndnKeyChain_->setFace(ndnFace_.get());
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        ndnFace_.reset();
        ndnTransport_.reset();
        params_.reset();
        ndnKeyChain_.reset();
    }
    
    void onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, Transport& transport)
    {
        INFO("got interest: %s", interest->getName().toUri().c_str());
    }
    
    void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
    {
        FAIL();
    }
    
    void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
    {
        receivedFrames_.push_back(encodedImage._timeStamp);
    }
    
protected:
    shared_ptr<VideoReceiverParams> params_;
    shared_ptr<Transport> ndnTransport_;
    shared_ptr<Face> ndnFace_;
    shared_ptr<KeyChain> ndnKeyChain_;
    
    std::vector<webrtc::EncodedImage*> fetchedFrames_;
    std::vector<uint32_t> sentFrames_;
    std::vector<uint32_t> receivedFrames_;
    
    void publishFrame(unsigned int frameNo, unsigned int segmentSize, int freshness = 3, bool mixedSendOrder = true)
    {
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        NdnFrameData payload(*frame);
        
        unsigned int fullSegmentsNum = payload.getLength()/segmentSize;
        unsigned int totalSegmentsNum = (payload.getLength() - fullSegmentsNum*segmentSize)?fullSegmentsNum+1:fullSegmentsNum;
        unsigned int lastSegmentSize = payload.getLength() - fullSegmentsNum*segmentSize;
        vector<int> segmentsSendOrder;
        
        Name prefix(params_->getStreamFramePrefix().c_str());
        shared_ptr<const vector<unsigned char>> frameNumberComponent = NdnRtcNamespace::getFrameNumberComponent(frameNo);
        
        prefix.addComponent(*frameNumberComponent);
        
        // setup send order for segments
        for (int i = 0; i < totalSegmentsNum; i++)
            segmentsSendOrder.push_back(i);
        
        if (mixedSendOrder)
            random_shuffle(segmentsSendOrder.begin(), segmentsSendOrder.end());
        
        for (int i = 0; i < totalSegmentsNum; i++)
        {
            unsigned int segmentIdx = segmentsSendOrder[i];
            unsigned char *segmentData = payload.getData()+segmentIdx*segmentSize;
            unsigned int segmentSize = (segmentIdx == totalSegmentsNum -1)?lastSegmentSize:segmentSize;
            shared_ptr<const vector<unsigned char>> finalBlockIDValue = Name::Component::makeSegment(totalSegmentsNum-1);
            
            if (segmentSize > 0)
            {
                Name segmentPrefix = prefix;
                segmentPrefix.appendSegment(segmentIdx);
                
                Data data(segmentPrefix);
                
                data.getMetaInfo().setFreshnessSeconds(freshness);
                data.getMetaInfo().setFinalBlockID(*finalBlockIDValue);
                data.getMetaInfo().setTimestampMilliseconds(millisecondTimestamp());
                data.setContent(segmentData, segmentSize);
                
                ndnKeyChain_->signData(data);
                
                ASSERT_TRUE(ndnTransport_->getIsConnected());
                
                Blob encodedData = data.wireEncode();
                ndnTransport_->send(*encodedData);
//                TRACE("published data %s", segmentPrefix.toUri().c_str());

            } // if
        } // for
        
        sentFrames_.push_back(frame->_timeStamp);
        
    } // publishFrame
};
#if 0
TEST_F(NdnReceiverTester, CreateDelete)
{
    NdnVideoReceiver *receiver = new NdnVideoReceiver(VideoSenderParams::defaultParams());
    delete receiver;
}

TEST_F(NdnReceiverTester, Init)
{
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_.get());
    
    EXPECT_EQ(0, receiver->init(ndnFace_));
    
    delete receiver;
}

TEST_F(NdnReceiverTester, EmptyFetching)
{
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_.get());
    
    receiver->setObserver(this);
    
    EXPECT_EQ(0, receiver->init(ndnFace_));
    EXPECT_FALSE(obtainedError_);
    
    EXPECT_EQ(0,receiver->startFetching());
    
    WAIT(100);
    
    EXPECT_EQ(0,receiver->stopFetching());
    
    delete receiver;
}
#endif
TEST_F(NdnReceiverTester, EmptyFetching30FPS)
{
    unsigned int framesNum = 15;
    unsigned int segmentSize = 100;
    unsigned int bufferSize = 5;
    unsigned int producerFrameRate = 30;
    
    params_->setIntParam(VideoReceiverParams::ParamNameFrameBufferSize, bufferSize);
    params_->setIntParam(VideoSenderParams::ParamNameSegmentSize, segmentSize);
    params_->setIntParam(VideoReceiverParams::ParamNameProducerRate, producerFrameRate);
    
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_.get());
    
    receiver->setObserver(this);
    
    EXPECT_EQ(0, receiver->init(ndnFace_));
    EXPECT_FALSE(obtainedError_);
    
    TRACE("start fetching");
    receiver->startFetching();
    
    // we should start publishing frames later, so that receiver will get first frame
    // by issuing interest with RightMostChild selector
    WAIT(100);
    
    
    for (int i = 0; i < framesNum; i++)
    {
        TRACE("publishing frame %d",i);
        publishFrame(i, segmentSize, 100);
        WAIT(1000/producerFrameRate);
    }

    
    EXPECT_TRUE_WAIT(receivedFrames_.size() == framesNum, 1000);
    ASSERT_EQ(framesNum, receivedFrames_.size());
    
    for (int i = 0; i < framesNum; i++)
        EXPECT_EQ(sentFrames_[i], receivedFrames_[i]);
    
    receiver->stopFetching();
    
    delete receiver;
}
