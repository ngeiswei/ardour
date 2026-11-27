#ifndef __ardour_tracker_test_automation_integer_test_h_
#define __ardour_tracker_test_automation_integer_test_h_

#include <memory>

#include "ardour/test/test_needing_session.h"
#include "evoral/Parameter.h"

#include "tracker_utils.h"

namespace Tracker {
class TrackerContext;
}

class AutomationIntegerTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (AutomationIntegerTest);

	CPPUNIT_TEST (testAutomationPatternDescriptor);
	CPPUNIT_TEST (testTrackAllAutomationsDescriptor);
	CPPUNIT_TEST (testTrackPatternDescriptor);
	CPPUNIT_TEST (testMidiTrackPatternDescriptor);
	CPPUNIT_TEST (testPatternDescriptor);
	CPPUNIT_TEST (testProcessorRegistryRefresh);
	CPPUNIT_TEST (testAutomationRangeAfterRegionMove);

	CPPUNIT_TEST_SUITE_END ();

public:
	AutomationIntegerTest ();

	void setUp () override;
	void tearDown () override;

	void testAutomationPatternDescriptor ();
	void testTrackAllAutomationsDescriptor ();
	void testTrackPatternDescriptor ();
	void testMidiTrackPatternDescriptor ();
	void testPatternDescriptor ();
	void testProcessorRegistryRefresh ();
	void testAutomationRangeAfterRegionMove ();

private:
	Tracker::AutomationControlPtr make_control (const Evoral::Parameter& param, bool integer);
	Tracker::AudioTrackPtr audio_track ();
	Tracker::MidiTrackPtr midi_track ();
	Tracker::MidiRegionPtr midi_region ();

	Tracker::TrackerContext* _context;
	Tracker::AudioTrackPtr _audio_track;
	Tracker::MidiTrackPtr _midi_track;
	Tracker::MidiRegionPtr _midi_region;

	Evoral::Parameter _integer_param;
	Evoral::Parameter _float_param;
	Evoral::Parameter _midi_param;
	Evoral::Parameter _missing_param;
};

#endif /* __ardour_tracker_test_automation_integer_test_h_ */
