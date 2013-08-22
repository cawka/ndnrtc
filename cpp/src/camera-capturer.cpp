//
//  camera-capturer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/16/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "camera-capturer.h"
#include <system_wrappers/interface/tick_util.h>

#include "nsXPCOMCIDInternal.h"
#include "nsServiceManagerUtils.h"

#define USE_I420

using namespace ndnrtc;
using namespace webrtc;

static unsigned char *frameBuffer = NULL;

//********************************************************************************
#pragma mark - construction/destruction
void CameraCapturer::CameraCapturer(CameraCapturerParams *params) : NdnRtcObject(params)
{
    TRACE("");
}

//********************************************************************************
#pragma mark - public
int CameraCapturer::init()
{
    if (!hasParams())
        return notifyNoParams();
    
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(0);
    
    if (!devInfo)
        return notifyError(-1, "can't get deivce info")
    
    unsigned int deviceID = getParams()->getDeviceID();
    char deviceName [256];
    char deviceUniqueName [256];
    
    devInfo->GetDeviceName(deviceID, deviceName, 256, deviceUniqueName, 256);
    
    TRACE("got device name: %s, unique name: %s",deviceName, deviceUniqueName);
    
    vcm_ = VideoCaptureFactory::Create(deviceID, deviceUniqueName);
    
    if (!vcm_)
        return notifyError(-1,"can't get video capture module");
        
    capability_.width = getParams()->getWidth();
    capability_.height = getParams()->getHeight();
    capability_.maxFPS = getParams()->getFPS();
    capability_.rawType = webrtc::kVideoI420; //webrtc::kVideoUnknown;
    
    vcm_->RegisterCaptureDataCallback(*this);
    
    return 0;
}
int CameraCapturer::startCapture()
{
    vcm_->StartCapture(capability_);
    
    if (!vcm_->CaptureStarted())
    {
        return notifyError(-1, "capture failed to start");
    }
    
    INFO("started camera capture");
    
    return 0;
}
int CameraCapturer::stopCapture()
{
    vcm_->StopCapture();
    
    return 0;
}

//********************************************************************************
#pragma mark - overriden - webrtc::VideoCaptureDataCallback
void CameraCapturer::OnIncomingCapturedFrame(const int32_t id, I420VideoFrame& videoFrame)
{
    TRACE("captured new frame %ld",videoFrame.render_time_ms());
    
    if (videoFrame.render_time_ms() >= TickTime::MillisecondTimestamp()-30 &&
        videoFrame.render_time_ms() <= TickTime::MillisecondTimestamp())
        TRACE("..delayed");
    
#ifdef USE_I420
    if (frameConsumer_)
        frameConsumer_->onDeliverFrame(videoFrame);
#else
    int bufSize = CalcBufferSize(kARGB, videoFrame.width(), videoFrame.height());
    
    if (!frameBuffer)
    {
        TRACE ("creating frame buffer of size %d", bufSize);
        frameBuffer = (unsigned char*)malloc(bufSize);
    }
    
    if (ConvertFromI420(videoFrame, kARGB, 0, frameBuffer) < 0)
    {
        ERROR("can't convert from I420 to RGB");
        return;
    }
    
    if (delegate_)
        delegate_->onDeliverFrame(frameBuffer, bufSize,
                                  videoFrame.width(), videoFrame.height(),
                                  TickTime::MillisecondTimestamp(), videoFrame.render_time_ms());
    else
        TRACE("..skipping");
#endif
}

void CameraCapturer::OnCaptureDelayChanged(const int32_t id, const int32_t delay)
{
    TRACE("capture delay changed: %d", delay);
}

//********************************************************************************
#pragma mark - private
//void CameraCapturer::startBackgroundCapturingThread()
//{
//#if 0
//    vcm_->StartCapture(capability_);
//    
//    if (!vcm_->CaptureStarted())
//    {
//        ERROR("capture failed to start");
//        return;
//    }
//    
//    INFO("started camera capture");
////    nsCOMPtr<nsRunnable> captureTask = new CameraCapturerTask(capability_,vcm_);
////    captureTask->Run();
//#else
//#warning clarify the neccessity of capturing thread
//    nsresult rv;
//    nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
//    
//    if (!capturingThread_)
//    {
//        INFO("creating new capture thread");
//        rv = tm->NewThread(0, 0, getter_AddRefs(capturingThread_));
//    }
//
//    if (NS_SUCCEEDED(rv))
//    {
//        nsCOMPtr<nsRunnable> captureTask = new CameraCapturerTask(capability_,vcm_);
//        capturingThread_->Dispatch(captureTask, nsIThread::DISPATCH_NORMAL);
//    }
//    else
//        ERROR("spin thread creation failed");
//#endif
//    
//}