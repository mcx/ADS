// SPDX-License-Identifier: MIT
/**
   Copyright (c) Beckhoff Automation GmbH & Co. KG
 */

#include <AdsLib.h>

#include "Frame.h"
#include "RingBuffer.h"
#include "SymbolAccess.h"

#include <cstring>
#include <iostream>
#include <limits>

#include <fructose/fructose.h>
using namespace fructose;

struct TestAmsAddr : test_base<TestAmsAddr> {
	std::ostream &out;

	TestAmsAddr(std::ostream &outstream)
		: out(outstream)
	{
	}

	void testAmsAddrCompare(const std::string &)
	{
		static const AmsAddr testee{ AmsNetId{ 192, 168, 0, 231, 1, 1 },
					     1000 };
		static const AmsAddr lower_last{
			AmsNetId{ 192, 168, 0, 231, 1, 0 }, 1000
		};
		static const AmsAddr lower_middle{
			AmsNetId{ 192, 168, 0, 1, 1, 1 }, 1000
		};
		static const AmsAddr lower_port{
			AmsNetId{ 192, 168, 0, 231, 1, 1 }, 999
		};

		fructose_assert(lower_last < testee);
		fructose_assert(lower_middle < testee);
		fructose_assert(lower_port < testee);
		fructose_assert(!(testee < lower_last));
		fructose_assert(!(testee < lower_middle));
		fructose_assert(!(testee < lower_port));
		fructose_assert(!(testee < testee));
	}
};

struct TestIpV4 : test_base<TestIpV4> {
	std::ostream &out;

	TestIpV4(std::ostream &outstream)
		: out(outstream)
	{
	}

	void testComparsion(const std::string &)
	{
		static const IpV4 testee{ "192.168.0.1" };
		static const IpV4 localhost{ "localhost" };
		static const IpV4 lower{ "192.167.0.1" };
		static const IpV4 higher{ "193.0.0.0" };

		fructose_assert_eq(0xC0A80001, testee.value);
		fructose_assert_eq(0x7F000001U, localhost.value);
		fructose_assert_eq(0xC0A70001, lower.value);
		fructose_assert_eq(0xC1000000, higher.value);
		fructose_assert_exception(IpV4{ "192.168.0." },
					  std::runtime_error); // too short
		fructose_assert_exception(IpV4{ "0.0.0.257" },
					  std::runtime_error); // too high
		fructose_assert_exception(IpV4{ "-1.0.0.254" },
					  std::runtime_error); // too low
		fructose_assert_exception(IpV4{ "192.d.0.254" },
					  std::runtime_error); // invalid
		fructose_assert(lower < testee);
		fructose_assert(testee < higher);
	}
};

struct TestRingBuffer : test_base<TestRingBuffer> {
	static const int NUM_TEST_LOOPS = 1024;
	std::ostream &out;

	TestRingBuffer(std::ostream &outstream)
		: out(outstream)
	{
	}

	void testBytesFree(const std::string &)
	{
		RingBuffer testee{ 1 };
		const auto data = testee.write;
		fructose_assert(0 == testee.BytesAvailable());
		fructose_assert(1 == testee.BytesFree());
		fructose_assert(testee.write == testee.read);

		*testee.write = 0xA5;
		testee.Write(1);
		fructose_assert(1 == testee.BytesAvailable());
		fructose_assert(0 == testee.BytesFree());
		fructose_assert(0xA5 == *testee.read);
		fructose_assert(data + 1 == testee.write);

		testee.ReadFromLittleEndian<uint8_t>();
		fructose_assert(0 == testee.BytesAvailable());
		fructose_assert(1 == testee.BytesFree());
		fructose_assert(testee.write == testee.read);

		*testee.write = 0x5A;
		testee.Write(1);
		fructose_assert(1 == testee.BytesAvailable());
		fructose_assert(0 == testee.BytesFree());
		fructose_assert(0x5A == *testee.read);
	}

	void testWriteChunk(const std::string &)
	{
		RingBuffer testee{ 1 };

		for (int i = 0; i < NUM_TEST_LOOPS; ++i) {
			fructose_assert(1 == testee.WriteChunk());
			testee.Write(1);
			fructose_assert(0 == testee.WriteChunk());
			testee.ReadFromLittleEndian<uint8_t>();
		}
	}
};

struct TestSymbolEntry : test_base<TestSymbolEntry> {
	std::ostream &out;

	union TestBuffer {
		AdsSymbolEntry header;
		uint8_t raw[128];
	};

	static TestBuffer CreateTestBuffer(const uint32_t entryLength = 0)
	{
		TestBuffer b;
		memset(&b, 0, sizeof(b));
		b.header.entryLength = bhf::ads::htole<uint32_t>(entryLength);
		return b;
	}

	TestSymbolEntry(std::ostream &outstream)
		: out(outstream)
	{
	}

	void testAllEmptyStrings(const std::string &)
	{
		const auto testData =
			CreateTestBuffer(sizeof(AdsSymbolEntry) + 3);
		const auto allEmpty = bhf::ads::SymbolEntry::Parse(
			testData.raw, sizeof(AdsSymbolEntry) + 3);
		fructose_assert(allEmpty.second.name.empty());
		fructose_assert(allEmpty.second.typeName.empty());
		fructose_assert(allEmpty.second.comment.empty());
	}

	void testBufferTooShort(const std::string &)
	{
		const auto testData = CreateTestBuffer();
		// Buffer too short to hold header
		for (size_t i = 0; i < sizeof(testData.header); ++i) {
			fructose_assert_exception(
				bhf::ads::SymbolEntry::Parse(testData.raw, i),
				AdsException);
		}
	}

	void testCommentLengthLargerThanBuffer(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(TestBuffer));
		testData.header.commentLength = bhf::ads::htole<uint16_t>(
			sizeof(testData) - sizeof(AdsSymbolEntry));
		fructose_assert_exception(
			bhf::ads::SymbolEntry::Parse(testData.raw,
						     sizeof(testData)),
			AdsException);
	}

	void testCommentLengthOverflow(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(TestBuffer));
		testData.header.commentLength = bhf::ads::htole<uint16_t>(
			std::numeric_limits<uint16_t>::max());
		fructose_assert_exception(
			bhf::ads::SymbolEntry::Parse(testData.raw,
						     sizeof(testData)),
			AdsException);
	}

	void testCommentOnly(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(AdsSymbolEntry) + 4);
		testData.header.commentLength = bhf::ads::htole<uint16_t>(1);
		testData.raw[sizeof(AdsSymbolEntry) + 2] = 'x';
		const auto onlyComment = bhf::ads::SymbolEntry::Parse(
			testData.raw, sizeof(testData));
		fructose_assert(onlyComment.second.name.empty());
		fructose_assert(onlyComment.second.typeName.empty());
		fructose_assert(onlyComment.second.comment == "x");
	}

	void testEntryLengthLargerThanBuffer(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(TestBuffer) + 1);
		testData.header.nameLength = bhf::ads::htole<uint16_t>(1);
		testData.raw[sizeof(AdsSymbolEntry) + 0] = 'n'; // name string
		testData.header.typeLength = bhf::ads::htole<uint16_t>(1);
		testData.raw[sizeof(AdsSymbolEntry) + 2] = 't'; // type string
		testData.header.commentLength = bhf::ads::htole<uint16_t>(1);
		testData.raw[sizeof(AdsSymbolEntry) + 4] =
			'n'; // comment string
		fructose_assert_exception(
			bhf::ads::SymbolEntry::Parse(testData.raw,
						     sizeof(testData)),
			AdsException);
	}

	void testEntryLengthOverflow(const std::string &)
	{
		const auto testData =
			CreateTestBuffer(std::numeric_limits<uint32_t>::max());
		fructose_assert_exception(
			bhf::ads::SymbolEntry::Parse(testData.raw,
						     sizeof(testData)),
			AdsException);
	}

	void testEntryLengthTooShort(const std::string &)
	{
		auto testData = CreateTestBuffer();
		// entryLength too short to hold header and three empty NUL terminated strings
		for (uint32_t i = 0; i < sizeof(testData.header) + 3; ++i) {
			testData.header.entryLength =
				bhf::ads::htole<uint32_t>(i);
			fructose_assert_exception(
				bhf::ads::SymbolEntry::Parse(testData.raw,
							     sizeof(testData)),
				AdsException);
		}
	}

	void testNameLengthLargerThanBuffer(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(TestBuffer));
		testData.header.nameLength = bhf::ads::htole<uint16_t>(
			sizeof(testData) - sizeof(AdsSymbolEntry));
		fructose_assert_exception(
			bhf::ads::SymbolEntry::Parse(testData.raw,
						     sizeof(testData)),
			AdsException);
	}

	void testNameLengthOverflow(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(TestBuffer));
		testData.header.nameLength = bhf::ads::htole<uint16_t>(
			std::numeric_limits<uint16_t>::max());
		fructose_assert_exception(
			bhf::ads::SymbolEntry::Parse(testData.raw,
						     sizeof(testData)),
			AdsException);
	}

	void testNameOnly(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(AdsSymbolEntry) + 4);
		testData.header.nameLength = bhf::ads::htole<uint16_t>(1);
		testData.raw[sizeof(AdsSymbolEntry) + 0] = 'x';
		const auto onlyName = bhf::ads::SymbolEntry::Parse(
			testData.raw, sizeof(testData));
		fructose_assert(onlyName.second.name == "x");
		fructose_assert(onlyName.second.typeName.empty());
		fructose_assert(onlyName.second.comment.empty());
	}

	void testTypeLengthLargerThanBuffer(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(TestBuffer));
		testData.header.typeLength = bhf::ads::htole<uint16_t>(
			sizeof(testData) - sizeof(AdsSymbolEntry));
		fructose_assert_exception(
			bhf::ads::SymbolEntry::Parse(testData.raw,
						     sizeof(testData)),
			AdsException);
	}

	void testTypeLengthOverflow(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(TestBuffer));
		testData.header.typeLength = bhf::ads::htole<uint16_t>(
			std::numeric_limits<uint16_t>::max());
		fructose_assert_exception(
			bhf::ads::SymbolEntry::Parse(testData.raw,
						     sizeof(testData)),
			AdsException);
	}

	void testTypeOnly(const std::string &)
	{
		auto testData = CreateTestBuffer(sizeof(AdsSymbolEntry) + 4);
		testData.header.typeLength = bhf::ads::htole<uint16_t>(1);
		testData.raw[sizeof(AdsSymbolEntry) + 1] = 'x';
		const auto onlyType = bhf::ads::SymbolEntry::Parse(
			testData.raw, sizeof(testData));
		fructose_assert(onlyType.second.name.empty());
		fructose_assert(onlyType.second.typeName == "x");
		fructose_assert(onlyType.second.comment.empty());
	}
};

struct TestFrame : test_base<TestFrame> {
	std::ostream &out;

	TestFrame(std::ostream &outstream)
		: out(outstream)
	{
	}

	/** pop() used to bound itself by capacity(), which stays at the size
	 * of the allocation, so a drained frame kept reading from m_Pos.
	 */
	void testPopBeyondEnd(const std::string &)
	{
		const uint8_t data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		Frame testee{ sizeof(data), data };

		fructose_assert(sizeof(data) == testee.size());
		fructose_assert(0x04030201 == testee.pop_letoh<uint32_t>());
		fructose_assert(4 == testee.size());
		fructose_assert(0x08070605 == testee.pop_letoh<uint32_t>());
		fructose_assert(0 == testee.size());

		/** empty now, but capacity() still reports the allocation */
		fructose_assert(sizeof(data) == testee.capacity());
		fructose_assert(0 == testee.pop_letoh<uint32_t>());
		fructose_assert(0 == testee.size());
	}

	void testPopLargerThanFrame(const std::string &)
	{
		const uint8_t data[] = { 1, 2 };
		Frame testee{ sizeof(data), data };

		/** a T that never fitted into the frame at all */
		fructose_assert(sizeof(data) == testee.size());
		fructose_assert(0 == testee.pop_letoh<uint32_t>());
		fructose_assert(0 == testee.size());
	}
};

int main()
{
	std::ostream &errorstream = std::cout;
	int failedTests = 0;

	TestAmsAddr amsAddrTest(errorstream);
	amsAddrTest.add_test("testAmsAddrCompare",
			     &TestAmsAddr::testAmsAddrCompare);
	failedTests += amsAddrTest.run();

	TestIpV4 ipv4Test(errorstream);
	ipv4Test.add_test("testComparsion", &TestIpV4::testComparsion);
	failedTests += ipv4Test.run();

	TestFrame frameTest(errorstream);
	frameTest.add_test("testPopBeyondEnd", &TestFrame::testPopBeyondEnd);
	frameTest.add_test("testPopLargerThanFrame",
			   &TestFrame::testPopLargerThanFrame);
	failedTests += frameTest.run();

	TestRingBuffer ringBufferTest(errorstream);
	ringBufferTest.add_test("testBytesFree",
				&TestRingBuffer::testBytesFree);
	ringBufferTest.add_test("testWriteChunk",
				&TestRingBuffer::testWriteChunk);
	failedTests += ringBufferTest.run();

	TestSymbolEntry symbolEntryTest(errorstream);
	symbolEntryTest.add_test("testAllEmptyStrings",
				 &TestSymbolEntry::testAllEmptyStrings);
	symbolEntryTest.add_test("testBufferTooShort",
				 &TestSymbolEntry::testBufferTooShort);
	symbolEntryTest.add_test(
		"testCommentLengthLargerThanBuffer",
		&TestSymbolEntry::testCommentLengthLargerThanBuffer);
	symbolEntryTest.add_test("testCommentLengthOverflow",
				 &TestSymbolEntry::testCommentLengthOverflow);
	symbolEntryTest.add_test("testCommentOnly",
				 &TestSymbolEntry::testCommentOnly);
	symbolEntryTest.add_test(
		"testEntryLengthLargerThanBuffer",
		&TestSymbolEntry::testEntryLengthLargerThanBuffer);
	symbolEntryTest.add_test("testEntryLengthOverflow",
				 &TestSymbolEntry::testEntryLengthOverflow);
	symbolEntryTest.add_test("testEntryLengthTooShort",
				 &TestSymbolEntry::testEntryLengthTooShort);
	symbolEntryTest.add_test(
		"testNameLengthLargerThanBuffer",
		&TestSymbolEntry::testNameLengthLargerThanBuffer);
	symbolEntryTest.add_test("testNameLengthOverflow",
				 &TestSymbolEntry::testNameLengthOverflow);
	symbolEntryTest.add_test("testNameOnly",
				 &TestSymbolEntry::testNameOnly);
	symbolEntryTest.add_test(
		"testTypeLengthLargerThanBuffer",
		&TestSymbolEntry::testTypeLengthLargerThanBuffer);
	symbolEntryTest.add_test("testTypeLengthOverflow",
				 &TestSymbolEntry::testTypeLengthOverflow);
	symbolEntryTest.add_test("testTypeOnly",
				 &TestSymbolEntry::testTypeOnly);
	failedTests += symbolEntryTest.run();

	return failedTests;
}
