//
//  frame-data.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <cmath>
#include <stdexcept>
#include <ndn-cpp/data.hpp>

#include "ndnrtc-common.h"
#include "frame-data.h"
#include "fec.h"

#define PREFIX_META_NCOMP 5

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//******************************************************************************
NetworkData::NetworkData(unsigned int dataLength, const unsigned char* rawData):
isValid_(true)
{
    copyFromRaw(dataLength, rawData);
}

NetworkData::NetworkData(const std::vector<uint8_t>& data):
isValid_(true)
{
    data_ = data;
}

NetworkData::NetworkData(const NetworkData& networkData)
{
    data_ = networkData.data_;
    isValid_ = networkData.isValid();
}

NetworkData::NetworkData(NetworkData&& networkData):
isValid_(networkData.isValid())
{
    data_.swap(networkData.data_);
    networkData.isValid_ = false;
}

NetworkData::NetworkData(std::vector<uint8_t>& data):
data_(boost::move(data)), isValid_(true)
{
}

NetworkData& NetworkData::operator=(const NetworkData& networkData)
{
    if (this != &networkData)
    {
        data_ = networkData.data_;
        isValid_ = networkData.isValid_;
    }

    return *this;
}

void NetworkData::swap(NetworkData& networkData)
{
    std::swap(isValid_, networkData.isValid_);
    data_.swap(networkData.data_);
}

void NetworkData::copyFromRaw(unsigned int dataLength, const uint8_t* rawData)
{
    data_.assign(rawData, rawData+dataLength);
}

//******************************************************************************
DataPacket::Blob::Blob(const std::vector<uint8_t>::const_iterator& begin, 
                    const std::vector<uint8_t>::const_iterator& end):
begin_(begin), end_(end)
{
}

DataPacket::Blob& DataPacket::Blob::operator=(const DataPacket::Blob& b)
{
    if (this != &b)
    {
        begin_ = b.begin_;
        end_ = b.end_;
    }

    return *this;
}

size_t DataPacket::Blob::size() const
{
    return (end_-begin_);
}

uint8_t DataPacket::Blob::operator[](size_t pos) const
{
    return *(begin_+pos);
}

const uint8_t* DataPacket::Blob::data() const
{
    return &(*begin_);
}

//******************************************************************************
DataPacket::DataPacket(unsigned int dataLength, const uint8_t* payload):
NetworkData(dataLength, payload)
{
    data_.insert(data_.begin(), 0);
    payloadBegin_ = ++data_.begin();
}

DataPacket::DataPacket(const std::vector<uint8_t>& payload):
NetworkData(payload)
{
    data_.insert(data_.begin(), 0);
    payloadBegin_ = ++data_.begin();
}

DataPacket::DataPacket(const DataPacket& dataPacket):
NetworkData(dataPacket.data_)
{
    reinit();
}

DataPacket::DataPacket(NetworkData&& networkData):
NetworkData(boost::move(networkData))
{
    reinit();
}

const DataPacket::Blob DataPacket::getPayload() const
{
    return Blob(payloadBegin_, data_.end());
}

void DataPacket::addBlob(uint16_t dataLength, const uint8_t* data)
{
    if (dataLength == 0) return;

    // increase blob counter
    data_[0]++;
    // save blob size
    uint8_t b1 = dataLength&0x00ff, b2 = (dataLength&0xff00)>>8;
    payloadBegin_ = data_.insert(payloadBegin_, b1);
    payloadBegin_++;
    payloadBegin_ = data_.insert(payloadBegin_, b2);
    payloadBegin_++;
    // insert blob
    data_.insert(payloadBegin_, data, data+dataLength);
    reinit();
}

size_t DataPacket::wireLength(size_t payloadLength, size_t blobLength)
{
    size_t wireLength = 1+payloadLength;
    if (blobLength > 0) wireLength += 2+blobLength;
    return wireLength;
}

size_t DataPacket::wireLength(size_t payloadLength, 
    std::vector<size_t> blobLengths)
{
    size_t wireLength = 1+payloadLength;
    for (auto b:blobLengths) if (b>0) wireLength += 2+b;
    return wireLength;
}

size_t DataPacket::wireLength(size_t blobLength)
{
    if (blobLength) return blobLength+2;
    return 0;
}

size_t DataPacket::wireLength(std::vector<size_t>  blobLengths)
{
    size_t wireLength = 0;
    for (auto b:blobLengths) wireLength += 2+b;
    return wireLength;
}

void DataPacket::reinit()
{
    blobs_.clear();
    if (!data_.size()) { isValid_ = false; return; }

    std::vector<uint8_t>::iterator p1 = (data_.begin()+1), p2;
    uint8_t nBlobs = data_[0];
    bool invalid = false;

    for (int i = 0; i < nBlobs; i++)
    {
        uint8_t b1 = *p1++, b2 = *p1;
        uint16_t blobSize = b1|((uint16_t)b2)<<8;

        if (p1-data_.begin()+blobSize > data_.size())
        {
            invalid = true;
            break;
        }

        p2 = ++p1+blobSize;
        blobs_.push_back(Blob(p1,p2));
        p1 = p2;
    }
    
    if (!invalid) payloadBegin_ = p1;
    else isValid_ = false;
}

//******************************************************************************
VideoFramePacket::VideoFramePacket(const webrtc::EncodedImage& frame):
SamplePacket(frame._length, frame._buffer), isSyncListSet_(false)
{
    Header hdr;
    hdr.encodedWidth_ = frame._encodedWidth;
    hdr.encodedHeight_ = frame._encodedHeight;
    hdr.timestamp_ = frame._timeStamp;
    hdr.capture_time_ms_ = frame.capture_time_ms_;
    hdr.frameType_ = frame._frameType;
    hdr.completeFrame_ = frame._completeFrame;
    addBlob(sizeof(hdr), (uint8_t*)&hdr);
}

VideoFramePacket::VideoFramePacket(NetworkData&& networkData):
CommonSamplePacket(boost::move(networkData))
{}

const webrtc::EncodedImage& VideoFramePacket::getFrame()
{
    Header *hdr = (Header*)blobs_[0].data();
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, hdr->encodedWidth_, 
        hdr->encodedHeight_);
    frame_ = webrtc::EncodedImage((uint8_t*)(data_.data()+(payloadBegin_-data_.begin())), getPayload().size(), size);
    frame_._encodedWidth = hdr->encodedWidth_;
    frame_._encodedHeight = hdr->encodedHeight_;
    frame_._timeStamp = hdr->timestamp_;
    frame_.capture_time_ms_ = hdr->capture_time_ms_;
    frame_._frameType = hdr->frameType_;
    frame_._completeFrame = hdr->completeFrame_;

    return frame_;
}

boost::shared_ptr<NetworkData>
VideoFramePacket::getParityData(size_t segmentLength, double ratio)
{
    if (!isValid_)
        throw std::runtime_error("Can't compute FEC parity data on invalid packet");

    size_t nDataSegmets = getLength()/segmentLength + (getLength()%segmentLength?1:0);
    size_t nParitySegments = ceil(ratio*nDataSegmets);
    if (nParitySegments == 0) nParitySegments = 1;
    
    std::vector<uint8_t> fecData(nParitySegments*segmentLength, 0);
    fec::Rs28Encoder enc(nDataSegmets, nParitySegments, segmentLength);
    size_t padding =  (nDataSegmets*segmentLength - getLength());
    boost::shared_ptr<NetworkData> parityData;

    // expand data with zeros
    data_.resize(nDataSegmets*segmentLength, 0);
    if (enc.encode(data_.data(), fecData.data()) >= 0)
        parityData = boost::make_shared<NetworkData>(boost::move(fecData));
    // shrink data back
    data_.resize(getLength()-padding);

    return parityData;
}

void
VideoFramePacket::setSyncList(const std::map<std::string, PacketNumber>& syncList)
{
    if (isHeaderSet()) throw std::runtime_error("Can't add more data to this packet"
        " as header has been set already");
    if (isSyncListSet_) throw std::runtime_error("Sync list has been already set");

    for (auto it:syncList)
    {
        addBlob(it.first.size(), (uint8_t*)it.first.c_str());
        addBlob(sizeof(it.second), (uint8_t*)&it.second);
    }

    isSyncListSet_ = true;
}

const std::map<std::string, PacketNumber> 
VideoFramePacket::getSyncList() const
{
    std::map<std::string, PacketNumber> syncList;

    for (std::vector<Blob>::const_iterator blob = blobs_.begin()+1; blob+1 < blobs_.end(); blob+=2)
    {
        syncList[std::string((const char*)blob->data(), blob->size())] = *(PacketNumber*)(blob+1)->data();
    }

    return boost::move(syncList);
}

//******************************************************************************
AudioBundlePacket::AudioSampleBlob::
AudioSampleBlob(const std::vector<uint8_t>::const_iterator begin,
               const std::vector<uint8_t>::const_iterator& end):
Blob(begin, end), fromBlob_(true)
{
    header_ = *(AudioSampleHeader*)Blob::data();
}

size_t AudioBundlePacket::AudioSampleBlob::size() const
{ 
    return Blob::size()+(fromBlob_?0:sizeof(AudioSampleHeader)); 
}

size_t AudioBundlePacket::AudioSampleBlob::wireLength(size_t payloadLength)
{
    return payloadLength+sizeof(AudioSampleHeader); 
}

AudioBundlePacket::AudioBundlePacket(size_t wireLength):
CommonSamplePacket(std::vector<uint8_t>()),
wireLength_(wireLength)
{
    clear();
}

AudioBundlePacket::AudioBundlePacket(NetworkData&& data):
CommonSamplePacket(boost::move(data))
{
}

bool 
AudioBundlePacket::hasSpace(const AudioBundlePacket::AudioSampleBlob& sampleBlob) const
{
    return ((long)remainingSpace_ - (long)DataPacket::wireLength(sampleBlob.size())) >= 0;
}

AudioBundlePacket& 
AudioBundlePacket::operator<<(const AudioBundlePacket::AudioSampleBlob& sampleBlob)
{
    if (hasSpace(sampleBlob))
    {
        data_[0]++;

        uint8_t b1 = sampleBlob.size()&0x00ff, b2 = (sampleBlob.size()&0xff00)>>8; 
        payloadBegin_ = data_.insert(payloadBegin_, b1);
        payloadBegin_++;
        payloadBegin_ = data_.insert(payloadBegin_, b2);
        payloadBegin_++;
        for (int i = 0; i < sizeof(sampleBlob.getHeader()); ++i)
        {
            payloadBegin_ = data_.insert(payloadBegin_, ((uint8_t*)&sampleBlob.getHeader())[i]);
            payloadBegin_++;
        }
        // insert blob
        data_.insert(payloadBegin_, sampleBlob.data(), 
            sampleBlob.data()+(sampleBlob.size()-sizeof(sampleBlob.getHeader())));
        reinit();
        remainingSpace_ -= DataPacket::wireLength(sampleBlob.size());
    }

    return *this;
}

void AudioBundlePacket::clear()
{
    data_.clear();
    data_.insert(data_.begin(),0);
    payloadBegin_ = data_.begin()+1;
    blobs_.clear();
    remainingSpace_ = AudioBundlePacket::payloadLength(wireLength_);
}

size_t AudioBundlePacket::getSamplesNum() const
{
    return blobs_.size() - isHeaderSet();
}

void AudioBundlePacket::swap(AudioBundlePacket& bundle)
{ 
    CommonSamplePacket::swap(bundle);
    std::swap(wireLength_, bundle.wireLength_);
    std::swap(remainingSpace_, bundle.remainingSpace_);
}

const AudioBundlePacket::AudioSampleBlob 
AudioBundlePacket::operator[](size_t pos) const
{
    return AudioSampleBlob(blobs_[pos].begin(), blobs_[pos].end());
}

size_t AudioBundlePacket::wireLength(size_t wireLength, size_t sampleSize)
{
    size_t sampleWireLength = AudioSampleBlob::wireLength(sampleSize);
    size_t nSamples = AudioBundlePacket::payloadLength(wireLength)/DataPacket::wireLength(sampleWireLength);
    std::vector<size_t> sampleSizes(nSamples, sampleWireLength);
    sampleSizes.push_back(sizeof(CommonHeader));

    return DataPacket::wireLength(0, sampleSizes);
}

size_t AudioBundlePacket::payloadLength(size_t wireLength)
{
    long payloadLength = wireLength-1-DataPacket::wireLength(sizeof(CommonHeader));
    return (payloadLength > 0 ? payloadLength : 0);
}

//******************************************************************************
AudioThreadMeta::AudioThreadMeta(double rate, const std::string& codec):
DataPacket(std::vector<uint8_t>())
{
    addBlob(sizeof(rate), (uint8_t*)&rate);
    if (codec.size()) addBlob(codec.size(), (uint8_t*)codec.c_str());
    else isValid_ = false;
}

AudioThreadMeta::AudioThreadMeta(NetworkData&& data):
DataPacket(boost::move(data))
{
    isValid_ = (blobs_.size() == 2 && blobs_[0].size() == sizeof(double));
}

double 
AudioThreadMeta::getRate() const
{
    return *(const double*)blobs_[0].data();
}

std::string
AudioThreadMeta::getCodec() const
{
    return std::string((const char*)blobs_[1].data(), blobs_[1].size());
}

//******************************************************************************
VideoThreadMeta::VideoThreadMeta(double rate, const FrameSegmentsInfo& segInfo,
    const VideoCoderParams& coder):
DataPacket(std::vector<uint8_t>())
{
    Meta m({rate, coder.gop_, coder.startBitrate_, coder.encodeWidth_, coder.encodeHeight_, 
        segInfo.deltaAvgSegNum_, segInfo.deltaAvgParitySegNum_,
        segInfo.keyAvgSegNum_, segInfo.keyAvgParitySegNum_});
    addBlob(sizeof(m), (uint8_t*)&m);
}

VideoThreadMeta::VideoThreadMeta(NetworkData&& data):
DataPacket(boost::move(data))
{
    isValid_ = (blobs_.size() == 1 && blobs_[0].size() == sizeof(Meta));
}

double VideoThreadMeta::getRate() const
{
    return ((Meta*)blobs_[0].data())->rate_;
}

FrameSegmentsInfo VideoThreadMeta::getSegInfo() const
{
    Meta *m = (Meta*)blobs_[0].data();
    return FrameSegmentsInfo({m->deltaAvgSegNum_, m->deltaAvgParitySegNum_, 
        m->keyAvgSegNum_, m->keyAvgParitySegNum_});
}

VideoCoderParams VideoThreadMeta::getCoderParams() const
{
    Meta *m = (Meta*)blobs_[0].data();
    VideoCoderParams c;
    c.gop_ = m->gop_;
    c.startBitrate_ = m->bitrate_;
    c.encodeWidth_ = m->width_;
    c.encodeHeight_ = m->height_;
    return c;
}

//******************************************************************************
#define SYNC_MARKER "sync:"
MediaStreamMeta::MediaStreamMeta():
DataPacket(std::vector<uint8_t>())
{}

MediaStreamMeta::MediaStreamMeta(std::vector<std::string> threads):
DataPacket(std::vector<uint8_t>())
{
    for (auto t:threads) addThread(t);
}

void
MediaStreamMeta::addThread(const std::string& thread)
{
     addBlob(thread.size(), (uint8_t*)thread.c_str());
}

void 
MediaStreamMeta::addSyncStream(const std::string& stream)
{
    addThread("sync:"+stream);
}

std::vector<std::string>
MediaStreamMeta::getSyncStreams() const
{
    std::vector<std::string> syncStreams;
    for (auto b:blobs_)
    {
        std::string thread = std::string((const char*)b.data(), b.size());
        size_t p = thread.find(SYNC_MARKER);
        if (p != std::string::npos)
            syncStreams.push_back(std::string(thread.begin()+sizeof(SYNC_MARKER)-1, 
                thread.end()));
            
    }
    return syncStreams;
}

std::vector<std::string>
MediaStreamMeta::getThreads() const 
{
    std::vector<std::string> threads;
    for (auto b:blobs_) 
    {
        std::string thread = std::string((const char*)b.data(), b.size());
        if (thread.find("sync:") == std::string::npos)
            threads.push_back(thread);
    }
    return threads;
}

