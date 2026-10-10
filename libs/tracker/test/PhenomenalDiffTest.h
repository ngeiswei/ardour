#ifndef __ardour_tracker_test_phenomenal_diff_test_h_
#define __ardour_tracker_test_phenomenal_diff_test_h_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class PhenomenalDiffTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (PhenomenalDiffTest);

	CPPUNIT_TEST (testBasePatternDiff);
	CPPUNIT_TEST (testRowsDiff);
	CPPUNIT_TEST (testAutomationDiff);
	CPPUNIT_TEST (testMidiNotesDiff);
	CPPUNIT_TEST (testMidiRegionAutomationDiff);
	CPPUNIT_TEST (testMidiRegionDiff);
	CPPUNIT_TEST (testTrackAllAutomationsDiff);
	CPPUNIT_TEST (testTrackDiff);
	CPPUNIT_TEST (testMidiTrackDiff);
	CPPUNIT_TEST (testAudioTrackDiff);
	CPPUNIT_TEST (testPatternDiff);

	CPPUNIT_TEST_SUITE_END ();

public:
	void testBasePatternDiff ();
	void testRowsDiff ();
	void testAutomationDiff ();
	void testMidiNotesDiff ();
	void testMidiRegionAutomationDiff ();
	void testMidiRegionDiff ();
	void testTrackAllAutomationsDiff ();
	void testTrackDiff ();
	void testMidiTrackDiff ();
	void testAudioTrackDiff ();
	void testPatternDiff ();
};

#endif /* __ardour_tracker_test_phenomenal_diff_test_h_ */
