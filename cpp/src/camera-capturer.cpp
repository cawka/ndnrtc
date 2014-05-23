//
//  camera-capturer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/16/13
//

//#undef DEBUG

#include "camera-capturer.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"

#define USE_I420

using namespace std;
using namespace ndnrtc;
using namespace webrtc;
using namespace ndnlog;

static unsigned char *frameBuffer = nullptr;

//********************************************************************************
//********************************************************************************

#pragma mark - public
//********************************************************************************
#pragma mark - construction/destruction
CameraCapturer::CameraCapturer(const ParamsStruct &params) :
NdnRtcObject(params),
vcm_(nullptr),
frameConsumer_(nullptr),
capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
captureEvent_(*EventWrapper::Create()),
captureThread_(*ThreadWrapper::CreateThread(deliverCapturedFrame, this,  kHighPriority))
{
    description_ = "capturer";
}
CameraCapturer::~CameraCapturer()
{
    if (vcm_)
    {
        if (isCapturing())
            stopCapture();
        
        vcm_->Release();
    }
};

//********************************************************************************
#pragma mark - public
int CameraCapturer::init()
{
    int deviceID = params_.captureDeviceId;
    
    LogTraceC << "acquiring device " << deviceID << endl;
    
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(deviceID);
    
    if (!devInfo)
        return notifyError(-1, "can't get deivce info");
    
    char deviceName [256];
    char deviceUniqueName [256];
    
    devInfo->GetDeviceName(deviceID, deviceName, 256, deviceUniqueName, 256);
    delete devInfo;
    
    LogTraceC
    << "got device name: " << deviceName
    << " (unique: " << deviceUniqueName << ")" << endl;
    
    vcm_ = VideoCaptureFactory::Create(deviceID, deviceUniqueName);
    
    if (vcm_ == NULL)
        return notifyError(RESULT_ERR, "can't get video capture module");
    
    int res = RESULT_OK;
    
    capability_.width = params_.captureWidth;
    capability_.height = params_.captureHeight;
    capability_.maxFPS = params_.captureFramerate;
    capability_.rawType = webrtc::kVideoI420; //webrtc::kVideoUnknown;
    
    vcm_->RegisterCaptureDataCallback(*this);
    
    meterId_ = NdnRtcUtils::setupFrequencyMeter();
    
    LogInfoC << "initialized (device: " << deviceUniqueName << ")" << endl;
    
    return 0;
}
int CameraCapturer::startCapture()
{
    unsigned int tid = 0;
    
    if (!captureThread_.Start(tid))
        return notifyError(-1, "can't start capturing thread");
    
    if (vcm_->StartCapture(capability_) < 0)
        return notifyError(-1, "capture failed to start");
    
    if (!vcm_->CaptureStarted())
        return notifyError(-1, "capture failed to start");
    
    LogInfoC << "started" << endl;
    
    return 0;
}
int CameraCapturer::stopCapture()
{
    vcm_->DeRegisterCaptureDataCallback();
    vcm_->StopCapture();
    captureThread_.SetNotAlive();
    captureEvent_.Set();
    
    if (!captureThread_.Stop())
        return notifyError(-1, "can't stop capturing thread");
    
    LogInfoC << "stopped" << endl;
    
    return 0;
}
int CameraCapturer::numberOfCaptureDevices()
{
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(0);
    
    if (!devInfo)
        return notifyError(-1, "can't get deivce info");
    
    return devInfo->NumberOfDevices();
}
vector<std::string>* CameraCapturer::availableCaptureDevices()
{
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(0);
    
    if (!devInfo)
    {
        notifyError(-1, "can't get deivce info");
        return nullptr;
    }
    
    vector<std::string> *devices = new vector<std::string>();
    
    static char deviceName[256];
    static char uniqueId[256];
    int numberOfDevices = numberOfCaptureDevices();
    
    for (int deviceIdx = 0; deviceIdx < numberOfDevices; deviceIdx++)
    {
        memset(deviceName, 0, 256);
        memset(uniqueId, 0, 256);
        
        if (devInfo->GetDeviceName(deviceIdx, deviceName, 256, uniqueId, 256) < 0)
        {
            notifyError(-1, "can't get info for deivce %d", deviceIdx);
            break;
        }
        
        devices->push_back(deviceName);
    }
    
    return devices;
}
void CameraCapturer::printCapturingInfo()
{
    cout << "*** Capturing info: " << endl;
    cout << "\tNumber of capture devices: " << numberOfCaptureDevices() << endl;
    cout << "\tCapture devices: " << endl;
    
    vector<std::string> *devices = availableCaptureDevices();
    
    if (devices)
    {
        vector<std::string>::iterator it;
        int idx = 0;
        
        for (it = devices->begin(); it != devices->end(); ++it)
        {
            cout << "\t\t"<< idx++ << ". " << *it << endl;
        }
        delete devices;
    }
    else
        cout << "\t\t <no capture devices>" << endl;
}

//********************************************************************************
#pragma mark - overriden - webrtc::VideoCaptureDataCallback
void CameraCapturer::OnIncomingCapturedFrame(const int32_t id,
                                             I420VideoFrame& videoFrame)
{
//    if (videoFrame.render_time_ms() >= NdnRtcUtils::millisecondTimestamp()-30 &&
//        videoFrame.render_time_ms() <= NdnRtcUtils::millisecondTimestamp())
//        TRACE("..delayed");
    NdnRtcUtils::frequencyMeterTick(meterId_);
    
    capture_cs_->Enter();
    capturedFrame_.SwapFrame(&videoFrame);
    capturedTimeStamp_ = NdnRtcUtils::unixTimestamp();
    capture_cs_->Leave();
    
    captureEvent_.Set();
}

void CameraCapturer::OnCaptureDelayChanged(const int32_t id, const int32_t delay)
{
    LogWarnC << "delay changed: " << delay << endl;
}

//********************************************************************************
#pragma mark - private
bool CameraCapturer::process()
{
    if (captureEvent_.Wait(100) == kEventSignaled) {
        deliver_cs_->Enter();
        if (!capturedFrame_.IsZeroSize()) {
            // New I420 frame.
            capture_cs_->Enter();
            double timestamp = capturedTimeStamp_;
            deliverFrame_.SwapFrame(&capturedFrame_);
            capturedFrame_.ResetSize();
            capture_cs_->Leave();
            
            if (frameConsumer_)
                frameConsumer_->onDeliverFrame(deliverFrame_, timestamp);
        }
        deliver_cs_->Leave();
    }
    // We're done!
    return true;
}
