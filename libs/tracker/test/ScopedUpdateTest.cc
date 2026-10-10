#include <glibmm/miscutils.h>

#include "ardour/session.h"
#include "ardour/audio_track.h"
#include "ardour/automation_list.h"
#include "ardour/presentation_info.h"
#include "ardour/region.h"
#include "ardour/route.h"

#include "audio_track_pattern.h"
#include "audio_track_pattern_phenomenal_diff.h"
#include "pattern.h"
#include "track_pattern.h"

#include "test_util.h"
#include "TestTrackerContext.h"
#include "ScopedUpdateTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ScopedUpdateTest);

using namespace ARDOUR;
using namespace Tracker;

void
ScopedUpdateTest::setUp ()
{
	create_and_start_dummy_backend ();
	std::string dir = Glib::build_filename (new_test_output_dir (), "tracker_scoped");
	_session = load_session (dir, "tracker_scoped");
}

void
ScopedUpdateTest::tearDown ()
{
	delete _session;
	_session = 0;
	stop_and_destroy_backend ();
}

void
ScopedUpdateTest::testAudioAutomationScopedDiff ()
{
	TestTrackerContext ctx (_session);

	AudioTrackList tracks = _session->new_audio_track (1, 1, std::shared_ptr<RouteGroup> (), 1, "Track", PresentationInfo::max_order, Normal);
	CPPUNIT_ASSERT (!tracks.empty ());
	AudioTrackPtr track = tracks.front ();

	Temporal::timepos_t position (Temporal::Beats::from_double (0.0));
	Temporal::timecnt_t length (Temporal::Beats::from_double (4.0));
	Temporal::timepos_t end (Temporal::Beats::from_double (4.0));
	Temporal::timepos_t nt_last (end);

	AudioTrackPattern atp (ctx, track, RegionSeq (), position, length, end, nt_last, false);
	atp.set_rows_per_beat (4, true);
	atp.set_row_range ();
	atp.set_param_enabled (IDParameter (PBD::ID (0), Evoral::Parameter (GainAutomation)), true);
	atp.update ();

	AudioTrackPattern prev (ctx, track, RegionSeq (), position, length, end, nt_last, false);
	prev.set_rows_per_beat (4, true);
	prev.set_row_range ();
	prev.set_param_enabled (IDParameter (PBD::ID (0), Evoral::Parameter (GainAutomation)), true);
	prev.copy_prev (atp);

	/* Nothing changed yet */
	CPPUNIT_ASSERT (atp.phenomenal_diff (prev).empty ());

	/* Add a gain automation event */
	AutomationControlPtr gain = track->gain_control ();
	CPPUNIT_ASSERT (gain);
	AutomationListPtr alist = gain->alist ();
	CPPUNIT_ASSERT (alist);
	alist->add (Temporal::timepos_t ((samplepos_t) 22050), 0.5, false, false);
	atp.update ();

	AudioTrackPatternPhenomenalDiff d1 = atp.phenomenal_diff (prev);
	CPPUNIT_ASSERT (!d1.empty ());
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, d1.taap_diff.main_automation_pattern_phenomenal_diff.param2rows_diff.count (Evoral::Parameter (GainAutomation)));

	/* Sync the snapshot: the diff must be empty again */
	prev.copy_prev (atp);
	CPPUNIT_ASSERT (atp.phenomenal_diff (prev).empty ());

	/* Automation-only update: add another event and use the automation-only
	 * update/sync path (update_automations + copy_prev_automations). */
	alist->add (Temporal::timepos_t ((samplepos_t) 66150), 0.25, false, false);
	atp.update_automations ();

	CPPUNIT_ASSERT (!atp.phenomenal_diff (prev).empty ());

	prev.copy_prev_automations (atp);
	CPPUNIT_ASSERT (atp.phenomenal_diff (prev).empty ());
}
