#ifndef __ardour_tracker_test_tracker_context_h_
#define __ardour_tracker_test_tracker_context_h_

#include <string>

#include "tracker_context.h"

/**
 * Minimal TrackerContext with no side effect, for testing the model in
 * isolation.  It can be given a real session when a session is needed.
 */
class TestTrackerContext : public Tracker::TrackerContext
{
public:
	TestTrackerContext (ARDOUR::Session* session) : _session (session) {}

	ARDOUR::Session* get_session () const override { return _session; }
	void connect_track (Tracker::TrackPtr) override {}
	void connect_midi_region (Tracker::MidiRegionPtr, Tracker::TrackPtr) override {}
	void connect_automation (Tracker::AutomationControlPtr, Tracker::TrackPtr) override {}
	void begin_reversible_command (const std::string&) override {}
	void commit_reversible_command () override {}
	void add_command (PBD::Command*) override {}
	void set_dirty () override {}
	void track_headers_changed () override {}

private:
	ARDOUR::Session* _session;
};

#endif /* __ardour_tracker_test_tracker_context_h_ */
