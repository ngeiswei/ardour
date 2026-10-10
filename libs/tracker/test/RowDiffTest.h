#ifndef __ardour_tracker_test_row_diff_test_h_
#define __ardour_tracker_test_row_diff_test_h_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class RowDiffTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (RowDiffTest);

	CPPUNIT_TEST (testNotesRowsDiffExact);
	CPPUNIT_TEST (testNotesRowsDiffOracle);
	CPPUNIT_TEST (testAutomationRowsDiffExact);
	CPPUNIT_TEST (testAutomationRowsDiffOracle);

	CPPUNIT_TEST_SUITE_END ();

public:
	void testNotesRowsDiffExact ();
	void testNotesRowsDiffOracle ();
	void testAutomationRowsDiffExact ();
	void testAutomationRowsDiffOracle ();
};

#endif /* __ardour_tracker_test_row_diff_test_h_ */
