
//
// Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
// more contributor license agreements. See the NOTICE file distributed
// with this work for additional information regarding copyright ownership.
// Green Energy Corp licenses this file to you under the Apache License,
// Version 2.0 (the "License"); you may not use this file except in
// compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file was forked on 01/01/2013 by Automatak, LLC and modifications
// have been made to this file. Automatak, LLC licenses these modifications to
// you under the terms of the License.
//

#include "TransportLoopbackTestObject.h"

#include <asio.hpp>
#include <boost/test/unit_test.hpp>
#include <functional>

#include <opendnp3/Log.h>
#include <opendnp3/ProtocolUtil.h>
#include <opendnp3/Exception.h>
#include <opendnp3/PhysicalLayerAsyncSerial.h>

#include "TestHelpers.h"
#include "BufferHelpers.h"
#include "LoopbackPhysicalLayerAsync.h"

using namespace std;
using namespace opendnp3;
using namespace boost;

BOOST_AUTO_TEST_SUITE(AsyncTransportLoopback)

// Do a bidirectional send operation and proceed until both sides have correctly
// received all the data
void TestLoopback(TransportLoopbackTestObject* apTest, size_t aNumBytes)
{
	apTest->Start();

	BOOST_REQUIRE(apTest->ProceedUntil(std::bind(&TransportLoopbackTestObject::LayersUp, apTest)));

	ByteStr b(aNumBytes, 0);

	apTest->mUpperA.SendDown(b, b.Size());
	apTest->mUpperB.SendDown(b, b.Size());

	BOOST_REQUIRE(apTest->ProceedUntil(std::bind(&MockUpperLayer::SizeEquals, &(apTest->mUpperA), b.Size())));
	BOOST_REQUIRE(apTest->ProceedUntil(std::bind(&MockUpperLayer::SizeEquals, &(apTest->mUpperB), b.Size())));
	BOOST_REQUIRE(apTest->mUpperA.BufferEquals(b, b.Size()));
	BOOST_REQUIRE(apTest->mUpperB.BufferEquals(b, b.Size()));
}

BOOST_AUTO_TEST_CASE(TestTransportWithMockLoopback)
{
	LinkConfig cfgA(true, true);
	LinkConfig cfgB(false, true);

	EventLog log;
	asio::io_service service;
	LoopbackPhysicalLayerAsync phys(log.GetLogger(LEV_WARNING, "loopback"), &service);
	TransportLoopbackTestObject t(&service, &phys, cfgA, cfgB);

	TestLoopback(&t, DEFAULT_FRAG_SIZE);
}

// Run this test on ARM to give us some regression protection for serial
#ifdef SERIAL_PORT
BOOST_AUTO_TEST_CASE(TestTransportWithSerialLoopback)
{
	LinkConfig cfgA(true, true);
	LinkConfig cfgB(false, true);

	cfgA.NumRetry = cfgB.NumRetry = 3;

	SerialSettings s;
	s.mDevice = TOSTRING(SERIAL_PORT);
	s.mBaud = 57600;
	s.mDataBits = 8;
	s.mStopBits = 1;
	s.mParity = PAR_NONE;
	s.mFlowType = FLOW_NONE;

	EventLog log;
	asio::io_service service;
	PhysicalLayerAsyncSerial phys(log.GetLogger(LEV_WARNING, "serial"), &service, s);
	TransportLoopbackTestObject t(&service, &phys, cfgA, cfgB);

	TestLoopback(&t, DEFAULT_FRAG_SIZE);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
