
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

#include <boost/test/unit_test.hpp>

#include "TestHelpers.h"

#include <opendnp3/ASIOExecutor.h>
#include <opendnp3/IOServiceThreadPool.h>
#include <opendnp3/Exception.h>
#include <opendnp3/Location.h>
#include <opendnp3/Log.h>
#include <opendnp3/ExecutorPause.h>

#include <map>
#include <functional>
#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;
using namespace opendnp3;

class TimerTestObject
{
public:
	TimerTestObject() :
		mLog(),
		mPool(mLog.GetLogger(LEV_ERROR, "thread-pool"), 1),
		mStrand(*mPool.GetIOService()),
		exe(&mStrand),
		mLast(-1),
		mNum(0),
		mMonotonic(true) {

	}

	void Receive(int aVal) {
		if(aVal <= mLast) mMonotonic = false;
		++mNum;
		mLast = aVal;
	}

	bool IsMonotonic() {
		return mMonotonic;
	}

	int Num() {
		return mNum;
	}

private:
	EventLog mLog;
	IOServiceThreadPool mPool;
	asio::strand mStrand;

public:
	ASIOExecutor exe;

private:

	int mLast;
	int mNum;
	bool mMonotonic;
};

class MockTimerHandler
{
public:
	MockTimerHandler() : mCount(0)
	{}

	void OnExpiration() {
		++mCount;
	}
	size_t GetCount() {
		return mCount;
	}

private:
	size_t mCount;
};

BOOST_AUTO_TEST_SUITE(TimersTestSuite)


BOOST_AUTO_TEST_CASE(TestOrderedDispatch)
{
	const int NUM = 10000;

	TimerTestObject test;

	for(int i = 0; i < NUM; ++i) {
		test.exe.Post([&test, i]() {
			test.Receive(i);
		});
	}

	{
		ExecutorPause p(&test.exe);
	}

	BOOST_REQUIRE_EQUAL(NUM, test.Num());
	BOOST_REQUIRE(test.IsMonotonic());
}


BOOST_AUTO_TEST_CASE(ExpirationAndReuse)
{
	MockTimerHandler mth;
	asio::io_service srv;
	asio::strand strand(srv);
	ASIOExecutor exe(&strand);

	ITimer* pT1 = exe.Start(milliseconds(1), std::bind(&MockTimerHandler::OnExpiration, &mth));
	BOOST_REQUIRE_EQUAL(1, srv.run_one());
	BOOST_REQUIRE_EQUAL(1, mth.GetCount());
	ITimer* pT2 = exe.Start(milliseconds(1), std::bind(&MockTimerHandler::OnExpiration, &mth));
	srv.reset();
	BOOST_REQUIRE_EQUAL(1, srv.run_one());
	BOOST_REQUIRE_EQUAL(pT1, pT2); //The ASIO implementation should reuse timers
}

BOOST_AUTO_TEST_CASE(Cancelation)
{
	MockTimerHandler mth;
	asio::io_service srv;
	asio::strand strand(srv);
	ASIOExecutor exe(&strand);
	ITimer* pT1 = exe.Start(milliseconds(1), std::bind(&MockTimerHandler::OnExpiration, &mth));
	pT1->Cancel();
	BOOST_REQUIRE_EQUAL(1, srv.run_one());
	BOOST_REQUIRE_EQUAL(0, mth.GetCount());
	ITimer* pT2 = exe.Start(milliseconds(1), std::bind(&MockTimerHandler::OnExpiration, &mth));
	srv.reset();
	BOOST_REQUIRE_EQUAL(1, srv.run_one());
	BOOST_REQUIRE_EQUAL(pT1, pT2);
}


BOOST_AUTO_TEST_CASE(MultipleOutstanding)
{
	MockTimerHandler mth1;
	MockTimerHandler mth2;
	asio::io_service srv;
	asio::strand strand(srv);
	ASIOExecutor ts(&strand);
	ITimer* pT1 = ts.Start(milliseconds(0), std::bind(&MockTimerHandler::OnExpiration, &mth1));
	ITimer* pT2 = ts.Start(milliseconds(100), std::bind(&MockTimerHandler::OnExpiration, &mth2));

	BOOST_REQUIRE_NOT_EQUAL(pT1, pT2);

	BOOST_REQUIRE_EQUAL(1, srv.run_one());
	BOOST_REQUIRE_EQUAL(1, mth1.GetCount());
	BOOST_REQUIRE_EQUAL(0, mth2.GetCount());

	BOOST_REQUIRE_EQUAL(1, srv.run_one());
	BOOST_REQUIRE_EQUAL(1, mth1.GetCount());
	BOOST_REQUIRE_EQUAL(1, mth2.GetCount());
}

BOOST_AUTO_TEST_SUITE_END()
