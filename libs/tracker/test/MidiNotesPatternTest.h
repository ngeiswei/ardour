#ifndef __ardour_tracker_test_midi_notes_pattern_test_h_
#define __ardour_tracker_test_midi_notes_pattern_test_h_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class MidiNotesPatternTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (MidiNotesPatternTest);

	CPPUNIT_TEST (testIsRowDisplayable);
	CPPUNIT_TEST (testEmpty);
	CPPUNIT_TEST (testIdentical);
	CPPUNIT_TEST (testAddedNote);
	CPPUNIT_TEST (testRemovedNote);
	CPPUNIT_TEST (testOneWayOnlyVisitsLeft);
	CPPUNIT_TEST (testOnNoteChanged);
	CPPUNIT_TEST (testOffNoteChanged);
	CPPUNIT_TEST (testMovedNote);
	CPPUNIT_TEST (testDisplayabilityChange);
	CPPUNIT_TEST (testBothNonDisplayable);
	CPPUNIT_TEST (testOffMeetsOn);
	CPPUNIT_TEST (testOnHidesOff);
	CPPUNIT_TEST (testOverlapDisjoint);
	CPPUNIT_TEST (testOverlapPartial);
	CPPUNIT_TEST (testOverlapContainment);
	CPPUNIT_TEST (testOverlapZeroLength);

	CPPUNIT_TEST_SUITE_END ();

public:
	void testIsRowDisplayable ();
	void testEmpty ();
	void testIdentical ();
	void testAddedNote ();
	void testRemovedNote ();
	void testOneWayOnlyVisitsLeft ();
	void testOnNoteChanged ();
	void testOffNoteChanged ();
	void testMovedNote ();
	void testDisplayabilityChange ();
	void testBothNonDisplayable ();
	void testOffMeetsOn ();
	void testOnHidesOff ();
	void testOverlapDisjoint ();
	void testOverlapPartial ();
	void testOverlapContainment ();
	void testOverlapZeroLength ();

	void assert_overlap (double a_start, double a_end, double b_start, double b_end, bool expected);
};

#endif /* __ardour_tracker_test_midi_notes_pattern_test_h_ */
