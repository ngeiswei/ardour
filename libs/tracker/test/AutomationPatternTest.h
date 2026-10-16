#ifndef __ardour_tracker_test_automation_pattern_test_h_
#define __ardour_tracker_test_automation_pattern_test_h_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace Tracker {
class TrackerContext;
class AutomationPattern;
}

class AutomationPatternTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (AutomationPatternTest);

	CPPUNIT_TEST (testEmpty);
	CPPUNIT_TEST (testIdentical);
	CPPUNIT_TEST (testAddedEvent);
	CPPUNIT_TEST (testRemovedEvent);
	CPPUNIT_TEST (testValueChange);
	CPPUNIT_TEST (testValueChangeLastEvent);
	CPPUNIT_TEST (testMoveToAnotherRow);
	CPPUNIT_TEST (testFirstEventBoundary);
	CPPUNIT_TEST (testLastEventBoundary);
	CPPUNIT_TEST (testDisplayabilityChange);
	CPPUNIT_TEST (testBothNonDisplayable);
	CPPUNIT_TEST (testOneSidedNonDisplayable);
	CPPUNIT_TEST (testPhenomenalDiffPartial);
	CPPUNIT_TEST (testPhenomenalDiffUnchanged);
	CPPUNIT_TEST (testPhenomenalDiffEnabledChange);
	CPPUNIT_TEST (testPhenomenalDiffOneSided);
	CPPUNIT_TEST (testSnapshotCarriesEnabled);
	CPPUNIT_TEST (testSnapshotIndependentEvents);
	CPPUNIT_TEST (testPhenomenalDiffThroughSnapshot);

	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();

	void testEmpty ();
	void testIdentical ();
	void testAddedEvent ();
	void testRemovedEvent ();
	void testValueChange ();
	void testValueChangeLastEvent ();
	void testMoveToAnotherRow ();
	void testFirstEventBoundary ();
	void testLastEventBoundary ();
	void testDisplayabilityChange ();
	void testBothNonDisplayable ();
	void testOneSidedNonDisplayable ();
	void testPhenomenalDiffPartial ();
	void testPhenomenalDiffUnchanged ();
	void testPhenomenalDiffEnabledChange ();
	void testPhenomenalDiffOneSided ();
	void testSnapshotCarriesEnabled ();
	void testSnapshotIndependentEvents ();
	void testPhenomenalDiffThroughSnapshot ();

private:
	Tracker::TrackerContext* _context;
	Tracker::AutomationPattern* _pattern;
};

#endif /* __ardour_tracker_test_automation_pattern_test_h_ */
