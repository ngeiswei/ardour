#ifndef __ardour_tracker_test_tracker_utils_test_h_
#define __ardour_tracker_test_tracker_utils_test_h_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TrackerUtilsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (TrackerUtilsTest);

	CPPUNIT_TEST (testNumberPointHelpers);
	CPPUNIT_TEST (testPositionHelpers);
	CPPUNIT_TEST (testPadUnpad);
	CPPUNIT_TEST (testDigitHelpers);
	CPPUNIT_TEST (testSignAndHexPrefix);
	CPPUNIT_TEST (testNumberValidation);
	CPPUNIT_TEST (testChangeDigit);
	CPPUNIT_TEST (testChangeDigitOrSign);
	CPPUNIT_TEST (testPitch);
	CPPUNIT_TEST (testParsePitch);
	CPPUNIT_TEST (testChannel);
	CPPUNIT_TEST (testFormatting);
	CPPUNIT_TEST (testNoteAndEventEquality);
	CPPUNIT_TEST (testContainerAndClampHelpers);

	CPPUNIT_TEST_SUITE_END ();

public:
	void testNumberPointHelpers ();
	void testPositionHelpers ();
	void testPadUnpad ();
	void testDigitHelpers ();
	void testSignAndHexPrefix ();
	void testNumberValidation ();
	void testChangeDigit ();
	void testChangeDigitOrSign ();
	void testPitch ();
	void testParsePitch ();
	void testChannel ();
	void testFormatting ();
	void testNoteAndEventEquality ();
	void testContainerAndClampHelpers ();
};

#endif /* __ardour_tracker_test_tracker_utils_test_h_ */
