//
//  camera-capturer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/16/13
//

#ifndef __ndnrtc__camera_capturer__
#define __ndnrtc__camera_capturer__

#include "webrtc.h"
#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"

namespace ndnrtc {
    class IRawFrameConsumer;
    
    class CameraCapturerParams : public NdnParams
    {
    public:
        // construction/desctruction
        CameraCapturerParams():NdnParams(){};
        
        // static public
        static CameraCapturerParams *defaultParams()
        {
            CameraCapturerParams *p = new CameraCapturerParams();
            
            p->setIntParam(CameraCapturerParams::ParamNameDeviceId, 0);
            p->setIntParam(CameraCapturerParams::ParamNameWidth, 640);
            p->setIntParam(CameraCapturerParams::ParamNameHeight, 480);
            p->setIntParam(CameraCapturerParams::ParamNameFPS, 30);
            
            return p;
        };
        
        // public methods
        int getDeviceId(unsigned int *did) const { return getParamAsInt(ParamNameDeviceId, (int*)did); };
        int getWidth(unsigned int *width) const { return getParamAsInt(ParamNameWidth, (int*)width); };
        int getHeight(unsigned int *height) const { return getParamAsInt(ParamNameHeight, (int*)height); };
        int getFPS(unsigned int *fps) const { return getParamAsInt(ParamNameFPS, (int*)fps); };
    };
    
    class CameraCapturer : public NdnRtcObject, public webrtc::VideoCaptureDataCallback
    {
    public:
        // construction/desctruction
        CameraCapturer(const NdnParams *params);
        ~CameraCapturer();
        
        // public methods go here
        void setFrameConsumer(IRawFrameConsumer *frameConsumer){ frameConsumer_ = frameConsumer; }
        bool isCapturing() { return (vcm_)?vcm_->CaptureStarted():false; }         
        int init();
        int startCapture();
        int stopCapture();
        int numberOfCaptureDevices();
        vector<std::string>* availableCaptureDevices();
        void printCapturingInfo();
        
        // interface conformance - webrtc::VideoCaptureDataCallback
        void OnIncomingCapturedFrame(const int32_t id,
                                     webrtc::I420VideoFrame& videoFrame);
        void OnCaptureDelayChanged(const int32_t id,
                                   const int32_t delay);
        
        // statistics
        double getCapturingFrequency() { return NdnRtcUtils::currentFrequencyMeterValue(meterId_); }
        
    private:        
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> capture_cs_;
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
        webrtc::ThreadWrapper &captureThread_;
        webrtc::EventWrapper &captureEvent_;
        webrtc::I420VideoFrame capturedFrame_, deliverFrame_;
        // private attributes go here
        webrtc::VideoCaptureCapability capability_;
        webrtc::VideoCaptureModule* vcm_ = nullptr;
        IRawFrameConsumer *frameConsumer_ = nullptr;
        
        // statistics
        unsigned int meterId_;
        
        // private static
        static bool deliverCapturedFrame(void *obj) { return ((CameraCapturer*)obj)->process(); }
        
        // private methods
        const CameraCapturerParams *getParams() const { return static_cast<const CameraCapturerParams*>(params_); }
        bool process();
    };
    
    class IRawFrameConsumer
    {
    public:
        virtual void onDeliverFrame(webrtc::I420VideoFrame &frame) = 0;
    };
}

#endif /* defined(__ndnrtc__camera_capturer__) */
