// 
// tests-client.cc
//
//  Created by Peter Gusev on 09 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/chrono.hpp>
#include <ndn-cpp/threadsafe-face.hpp>

#include "gtest/gtest.h"
#include "client/src/client.h"
#include "client/src/config.h"
#include "tests-helpers.h"

#include "mock-objects/external-capturer-mock.h"

using namespace ::testing;
using namespace std;
using namespace boost::chrono;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

TEST(TestClient, TestRunClientPhony)
{
	boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io](){
        io.run();
    });

	std::string appPrefix = "/ndn/edu/ucla/remap/test/headless";
	boost::shared_ptr<Face> face(boost::make_shared<ThreadsafeFace>(io));
	boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelNone);
	Client c(io, face, keyChain);

	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	c.run(3, 10, ClientParams());
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
    
    auto duration = duration_cast<seconds>( t2 - t1 ).count();

	EXPECT_EQ(0, duration);
    
    work.reset();
    t.join();
    io.stop();
}
#if 0
TEST(TestClient, TestConsumer)
{
	boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io](){
        io.run();
    });

	std::string appPrefix = "/ndn/edu/ucla/remap/test/headless";
	boost::shared_ptr<Face> face(boost::make_shared<ThreadsafeFace>(io));
	boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
	{
		Client c(io, face, keyChain);

		ClientParams cp = sampleConsumerParams();
		boost::shared_ptr<StatisticsStorage> sampleStats = 
			boost::shared_ptr<StatisticsStorage>(StatisticsStorage::createConsumerStatistics());

		c.run(3, 100, cp);
	}

	EXPECT_TRUE(std::ifstream("/tmp/buffer-ndn-edu-ucla-remap-client1-camera.stat").good());
	EXPECT_TRUE(std::ifstream("/tmp/buffer-ndn-edu-ucla-remap-client1-mic.stat").good());
	remove("/tmp/buffer-ndn-edu-ucla-remap-client1-mic.stat");
	remove("/tmp/buffer-ndn-edu-ucla-remap-client1-camera.stat");

	face->shutdown();
    work.reset();
    t.join();
    io.stop();
}
#endif
TEST(TestClient, TestProducer)
{
	boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io](){
        io.run();
    });

	std::string appPrefix = "/ndn/edu/ucla/remap/test/headless";
	boost::shared_ptr<Face> face(boost::make_shared<ThreadsafeFace>(io));
	boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);

	ClientParams cp = sampleProducerParams();
	// ASSERT_EQ(2, cp.getProducerParams().publishedStreams_.size());

	// create frame file source
	std::string sourceName = cp.getProducerParams().publishedStreams_[0].source_;
	{
		unsigned int w,h;
		cp.getProducerParams().publishedStreams_[0].getMaxResolution(w,h);
		FileSink sink(sourceName);
		ArgbFrame frame(w, h);
		uint8_t *b = frame.getBuffer().get();

		for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
			b[i] = (i%256);

		for (int i = 0; i < 10; i++)
			sink << frame;
	}

	{
		Client c(io, face, keyChain);
		c.run(3, 0, cp);
	}

	ndnlog::new_api::Logger::releaseAsyncLogging();
	face->shutdown();
    work.reset();
    t.join();
}

// TEST(TestClient, TestThrowsAtBadProducerParams)
// {
// 	ProducerClientParams pcp;
// 	pcp.publishedStreams_.push_back(ProducerStreamParams());

// 	ClientParams cp;
// 	cp.setProducerParams(pcp);

// 	EXPECT_ANY_THROW(
// 		Client::getSharedInstance().run(5, 10, cp)
// 		);
// }

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
