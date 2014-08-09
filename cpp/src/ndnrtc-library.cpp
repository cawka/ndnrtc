//
//  ndnrtc-library.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "ndnrtc-library.h"
#include "sender-channel.h"
#include "consumer-channel.h"
#include "objc/cocoa-renderer.h"

#include <stdlib.h>
#include <string.h>
#include <memory>

#define CHECK_AND_SET_INT(paramSet, paramName, paramValue){ \
if ((int)paramValue >= 0) \
paramSet.setIntParam(paramName, paramValue); \
}

#define CHECK_AND_SET_STR(paramSet, paramName, paramValue){\
if (paramValue)\
paramSet.setStringParam(paramName, string(paramValue));\
}

using namespace boost;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;

typedef std::map<std::string, shared_ptr<ConsumerChannel> > ProducerMap;

static shared_ptr<NdnSenderChannel> SenderChannel;
static ProducerMap Producers;

//********************************************************************************
#pragma mark module loading
__attribute__((constructor))
static void initializer(int argc, char** argv, char** envp) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
    }
}

__attribute__((destructor))
static void destructor(){
}

extern "C" NdnRtcLibrary* create_ndnrtc(void *libHandle)
{
    signal(SIGPIPE, SIG_IGN);
    return new NdnRtcLibrary(libHandle);
}

extern "C" void destroy_ndnrtc( NdnRtcLibrary* object )
{
    if (SenderChannel.get())
        object->stopPublishing();
    
    if (Producers.size())
    {
        ProducerMap::iterator it;
        
        for (it = Producers.begin(); it != Producers.end(); it++)
        {
            shared_ptr<ConsumerChannel> producer = it->second;
            producer->stopTransmission();
        }
        
        Producers.clear();
    }
    
    delete object;
}

//********************************************************************************
#pragma mark - all static
static const char *DefaultLogFile = NULL;

//******************************************************************************
#pragma mark - construction/destruction
NdnRtcLibrary::NdnRtcLibrary(void *libHandle):
observer_(NULL),
libraryHandle_(libHandle),
libParams_(DefaultParams),
libAudioParams_(DefaultParamsAudio)
{
    fclose(stderr);    
    NdnRtcUtils::sharedVoiceEngine();
}
NdnRtcLibrary::~NdnRtcLibrary()
{
    NdnRtcUtils::releaseVoiceEngine();
}
//******************************************************************************
#pragma mark - public
void NdnRtcLibrary::configure(const ParamsStruct &params,
                              const ParamsStruct &audioParams)
{
    ParamsStruct validatedVideoParams, validatedAudioParams;
    
    char libVersion[255];
    memset((void*)libVersion, 0, 255);
    getVersionString((char**)&libVersion);
    notifyObserverWithState("info", "library version %s", libVersion);
    
    bool wasModified = false;
    int res = ParamsStruct::validateVideoParams(params, validatedVideoParams);
    
    if (RESULT_FAIL(res))
    {
        notifyObserverWithError("error", "bad video parameteres");
        return;
    }
    
    wasModified = RESULT_WARNING(res);
    res = ParamsStruct::validateAudioParams(audioParams, validatedAudioParams);
    
    if (RESULT_FAIL(res))
    {
        notifyObserverWithError("bad audio parameters");
        return;
    }
    
    wasModified |= RESULT_WARNING(res);
    
    libParams_ = validatedVideoParams;
    libAudioParams_ = validatedAudioParams;
    
    if (params.useTlv)
    {
        notifyObserver("info", "using TLV wire format");
        WireFormat::setDefaultWireFormat(TlvWireFormat::get());
    }
    else
    {
        notifyObserver("info", "using BinaryXML wire format");
        WireFormat::setDefaultWireFormat(BinaryXmlWireFormat::get());
    }
    
    notifyObserverWithState("info", "in-memory cache: %s", (params.useCache?"ENABLED":"DISABLED"));
    notifyObserverWithState("info", "FEC: %s", (params.useFec?"ENABLED":"DISABLED"));
    notifyObserverWithState("info", "retransmissions: %s", (params.useRtx?"ENABLED":"DISABLED"));
    
    if (wasModified)
        notifyObserverWithState("warn", "some parameters were malformed. using default"
                                " instead");
    else
        notifyObserverWithState("init", "initialized with new parameters");
}
void NdnRtcLibrary::currentParams(ParamsStruct &params,
                                  ParamsStruct &audioParams)
{
    params = libParams_;
    audioParams = libAudioParams_;
}

void NdnRtcLibrary::getDefaultParams(ParamsStruct &videoParams,
                                     ParamsStruct &audioParams) const
{
    videoParams = DefaultParams;
    audioParams = DefaultParamsAudio;
}

int NdnRtcLibrary::getStatistics(const char *producerId,
                                 NdnLibStatistics &stat) const
{
    memset((void*)&stat, 0, sizeof(NdnLibStatistics));
    
    if (SenderChannel.get())
    {
        SenderChannel->getChannelStatistics(stat.sendStat_);
    }
    
    if (!producerId || Producers.find(std::string(producerId)) == Producers.end())
        return -1; //notifyObserverWithError("producer was not found");
    
    shared_ptr<ConsumerChannel> producer = Producers[std::string(producerId)];
    
    stat.producerId_ = producerId;
    producer->getChannelStatistics(stat.receiveStat_);
    
    return 0;
}

int NdnRtcLibrary::startPublishing(const char *username)
{
    return startPublishing(username, nullptr);
}

int NdnRtcLibrary::stopPublishing()
{
    if (SenderChannel.get())
    {
        SenderChannel->stopTransmission();
        SenderChannel.reset();
    }
    
    return notifyObserverWithState("stopped", "stopped publishing media");
}

void NdnRtcLibrary::getPublisherPrefix(const char** userPrefix)
{
    shared_ptr<std::string> prefix;
    ParamsStruct p = libParams_;
    p.producerId = publisherId_;
    
    prefix = NdnRtcNamespace::getUserPrefix(p);

    if (prefix.get())
        memcpy((void*)*userPrefix, (void*)(prefix->c_str()), prefix->size());
}

void NdnRtcLibrary::getProducerPrefix(const char* producerId,
                                      const char** producerPrefx)
{
    ParamsStruct p  = libParams_;
    p.producerId = producerId;
    
    shared_ptr<std::string> prefix;
    prefix = NdnRtcNamespace::getUserPrefix(p);
    
    memcpy((void*)*producerPrefx, prefix->c_str(), prefix->size());
}

int NdnRtcLibrary::startFetching(const char *producerId)
{
    return startFetching(producerId, nullptr);
}

int NdnRtcLibrary::startFetching(const char *producerId,
                                 IExternalRenderer* const renderer)
{
    if (strcmp(producerId, "") == 0)
        return notifyObserverWithError("username cannot be empty string");
    
    if (Producers.find(std::string(producerId)) != Producers.end())
        return notifyObserverWithError("already fetching");
    
    // setup params
    ParamsStruct params = libParams_;
    ParamsStruct audioParams = libAudioParams_;
    
    params.setProducerId(producerId);
    audioParams.setProducerId(producerId);
    
    try
    {
        shared_ptr<ConsumerChannel> producer(new ConsumerChannel(params,
                                                                 audioParams,
                                                                 renderer));
        
        producer->setObserver(this);
        
        if (RESULT_FAIL(producer->init()))
            return -1;
        
        if (RESULT_FAIL(producer->startTransmission()))
            return -1;
        
        Producers[std::string(producerId)] = producer;
    }
    catch (std::exception &e)
    {
        return notifyObserverWithError("couldn't initiate fetching due to exception: %s",
                                       e.what());
    }
    
    shared_ptr<std::string> producerPrefix = NdnRtcNamespace::getUserPrefix(params);
    
    return notifyObserverWithState("fetching",
                                   "fetching from the user %s",
                                   producerPrefix->c_str());
}

int NdnRtcLibrary::stopFetching(const char *producerId)
{
    if (Producers.find(std::string(producerId)) == Producers.end())
        return notifyObserverWithError("fetching from user was not started");
    
    shared_ptr<ConsumerChannel> producer = Producers[std::string(producerId)];
    
    if (producer->stopTransmission() < 0)
        notifyObserverWithError("can't stop fetching");
    
    std::string producerKey = std::string(producerId);
    
    Producers.erase(producerKey);
    
    return notifyObserverWithState("leave", "stopped fetching from %s",
                                   producerId);
}

void NdnRtcLibrary::onErrorOccurred(const char *errorMessage)
{
    notifyObserverWithError(errorMessage);
}

int NdnRtcLibrary::startPublishing(const char* username,
                                   IExternalRenderer* const renderer)
{
    ParamsStruct params = libParams_;
    ParamsStruct audioParams = libAudioParams_;
    
    if (username)
    {
        if (strcmp(username, "") == 0)
            return notifyObserverWithError("username cannot be empty string");
        
        if (publisherId_)
            free(publisherId_);
        
        publisherId_ = (char*)malloc(strlen(username)+1);
        memset((void*)publisherId_, 0, strlen(username)+1);
        memcpy((void*)publisherId_, username, strlen(username));
        
        params.producerId = username;
        audioParams.producerId = username;
    }
    
    if (SenderChannel.get())
        stopPublishing();
    
    shared_ptr<NdnSenderChannel> sc(new NdnSenderChannel(params, audioParams,
                                                         (IExternalRenderer*)renderer));
    
    sc->setObserver(this);
    
    if (RESULT_FAIL(sc->init()))
        return -1;
    
    if (RESULT_FAIL(sc->startTransmission()))
        return -1;
    
    SenderChannel = sc;
    shared_ptr<std::string> producerPrefix = NdnRtcNamespace::getUserPrefix(params),
    framePrefix = NdnRtcNamespace::getStreamFramePrefix(params, 0);
    notifyObserverWithState("transmitting",
                            "started publishing under the user prefix: %s",
                            producerPrefix->c_str());
    
    return RESULT_OK;
}

void
NdnRtcLibrary::getVersionString(char **versionString)
{
    if (versionString)
        memcpy((void*)versionString, PACKAGE_VERSION, strlen(PACKAGE_VERSION));
               
    return;
}

void
NdnRtcLibrary::arrangeWindows()
{
    arrangeAllWindows();
}

//********************************************************************************
#pragma mark - private
int NdnRtcLibrary::notifyObserverWithError(const char *format, ...) const
{
    va_list args;
    
    static char emsg[256];
    
    va_start(args, format);
    vsprintf(emsg, format, args);
    va_end(args);
    
    notifyObserver("error", emsg);
    
    return RESULT_ERR;
}
int NdnRtcLibrary::notifyObserverWithState(const char *stateName, const char *format, ...) const
{
    va_list args;
    
    static char msg[256];
    
    va_start(args, format);
    vsprintf(msg, format, args);
    va_end(args);
    
    notifyObserver(stateName, msg);
    
    return RESULT_OK;
}
void NdnRtcLibrary::notifyObserver(const char *state, const char *args) const
{
    if (observer_)
        observer_->onStateChanged(state, args);
}
