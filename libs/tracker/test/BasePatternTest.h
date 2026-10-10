#ifndef __ardour_tracker_test_base_pattern_test_h_
#define __ardour_tracker_test_base_pattern_test_h_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace Tracker { class TrackerContext; }

class BasePatternTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (BasePatternTest);

	CPPUNIT_TEST (testRowRange);
	CPPUNIT_TEST (testBeatsAtRow);
	CPPUNIT_TEST (testDelayBounds);
	CPPUNIT_TEST (testDelayRoundTrip);
	CPPUNIT_TEST (testIsDefined);
	CPPUNIT_TEST (testRegionRelativeBeats);
	CPPUNIT_TEST (testRankedRowHelpers);

	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();

	void testRowRange ();
	void testBeatsAtRow ();
	void testDelayBounds ();
	void testDelayRoundTrip ();
	void testIsDefined ();
	void testRegionRelativeBeats ();
	void testRankedRowHelpers ();

private:
	Tracker::TrackerContext* _context;
};

#endif /* __ardour_tracker_test_base_pattern_test_h_ */
