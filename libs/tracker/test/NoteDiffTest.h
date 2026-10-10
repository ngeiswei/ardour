#ifndef __ardour_tracker_test_note_diff_test_h_
#define __ardour_tracker_test_note_diff_test_h_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class NoteDiffTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (NoteDiffTest);

	CPPUNIT_TEST (testApplyNoteDiffInvariants);
	CPPUNIT_TEST (testApplyNoteDiffScenario);

	CPPUNIT_TEST_SUITE_END ();

public:
	void testApplyNoteDiffInvariants ();
	void testApplyNoteDiffScenario ();
};

#endif /* __ardour_tracker_test_note_diff_test_h_ */
