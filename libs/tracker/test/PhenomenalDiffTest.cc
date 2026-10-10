#include "evoral/Parameter.h"

#include "automation_pattern_phenomenal_diff.h"
#include "base_pattern_phenomenal_diff.h"
#include "midi_notes_pattern_phenomenal_diff.h"
#include "midi_region_automation_pattern_phenomenal_diff.h"
#include "midi_region_pattern_phenomenal_diff.h"
#include "midi_track_pattern_phenomenal_diff.h"
#include "audio_track_pattern_phenomenal_diff.h"
#include "pattern_phenomenal_diff.h"
#include "rows_phenomenal_diff.h"
#include "track_all_automations_pattern_phenomenal_diff.h"
#include "track_pattern_phenomenal_diff.h"

#include "PhenomenalDiffTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PhenomenalDiffTest);

using namespace Tracker;

/*
 * These tests document the semantics of the "full" flag and of empty().
 *
 * The invariant is:
 *   empty() == there is nothing to do, i.e. !full && no fine grained
 *              difference recorded.
 *
 * A node that is full is never empty (it asks for a full repaint).
 */

void
PhenomenalDiffTest::testBasePatternDiff ()
{
	BasePatternPhenomenalDiff d;
	CPPUNIT_ASSERT (d.full);
	CPPUNIT_ASSERT (!d.empty ());

	d.full = false;
	CPPUNIT_ASSERT (d.empty ());
}

void
PhenomenalDiffTest::testRowsDiff ()
{
	RowsPhenomenalDiff d;

	/* default is full */
	CPPUNIT_ASSERT (d.full);
	CPPUNIT_ASSERT (!d.empty ());

	/* partial but empty means nothing to do */
	d.full = false;
	CPPUNIT_ASSERT (d.empty ());

	/* partial with rows means something to do */
	d.rows.insert (3);
	CPPUNIT_ASSERT (!d.empty ());

	/* full with no rows still means something to do (repaint everything) */
	d.rows.clear ();
	d.full = true;
	CPPUNIT_ASSERT (!d.empty ());
}

void
PhenomenalDiffTest::testAutomationDiff ()
{
	AutomationPatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	CPPUNIT_ASSERT (d.empty ());

	d.param2rows_diff[Evoral::Parameter (0)] = RowsPhenomenalDiff ();
	CPPUNIT_ASSERT (!d.empty ());
}

void
PhenomenalDiffTest::testMidiNotesDiff ()
{
	MidiNotesPatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	CPPUNIT_ASSERT (d.empty ());

	d.cgi2rows_diff[0] = RowsPhenomenalDiff ();
	CPPUNIT_ASSERT (!d.empty ());
}

void
PhenomenalDiffTest::testMidiRegionAutomationDiff ()
{
	MidiRegionAutomationPatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	/* ap_diff is still full by default */
	CPPUNIT_ASSERT (!d.empty ());

	d.ap_diff.full = false;
	CPPUNIT_ASSERT (d.empty ());
}

void
PhenomenalDiffTest::testMidiRegionDiff ()
{
	MidiRegionPatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* mnp_diff full by default */

	d.mnp_diff.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* mrap_diff full by default */

	d.mrap_diff.full = false;
	/* mrap_diff still embeds a full ap_diff */
	CPPUNIT_ASSERT (!d.empty ());

	d.mrap_diff.ap_diff.full = false;
	CPPUNIT_ASSERT (d.empty ());
}

void
PhenomenalDiffTest::testTrackAllAutomationsDiff ()
{
	TrackAllAutomationsPatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* main automation diff full by default */

	d.main_automation_pattern_phenomenal_diff.full = false;
	CPPUNIT_ASSERT (d.empty ());

	d.id_to_processor_automation_pattern_phenomenal_diff[PBD::ID (1)] = AutomationPatternPhenomenalDiff ();
	CPPUNIT_ASSERT (!d.empty ()); /* processor diff full by default */

	d.id_to_processor_automation_pattern_phenomenal_diff[PBD::ID (1)].full = false;
	CPPUNIT_ASSERT (d.empty ());
}

void
PhenomenalDiffTest::testTrackDiff ()
{
	TrackPatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* taap full by default */

	d.taap_diff.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* main automation diff full by default */

	d.taap_diff.main_automation_pattern_phenomenal_diff.full = false;
	CPPUNIT_ASSERT (d.empty ());
}

void
PhenomenalDiffTest::testMidiTrackDiff ()
{
	MidiTrackPatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* taap full by default */

	d.taap_diff.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* main automation diff full by default */

	d.taap_diff.main_automation_pattern_phenomenal_diff.full = false;
	CPPUNIT_ASSERT (d.empty ());

	d.mri2mrp_diff[0] = MidiRegionPatternPhenomenalDiff ();
	CPPUNIT_ASSERT (!d.empty ()); /* region map non empty */

	d.mri2mrp_diff.clear ();
	CPPUNIT_ASSERT (d.empty ());
}

void
PhenomenalDiffTest::testAudioTrackDiff ()
{
	AudioTrackPatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* taap full by default */

	d.taap_diff.full = false;
	CPPUNIT_ASSERT (!d.empty ()); /* main automation diff full by default */

	d.taap_diff.main_automation_pattern_phenomenal_diff.full = false;
	CPPUNIT_ASSERT (d.empty ());
}

void
PhenomenalDiffTest::testPatternDiff ()
{
	PatternPhenomenalDiff d;
	CPPUNIT_ASSERT (!d.empty ()); /* full by default */

	d.full = false;
	CPPUNIT_ASSERT (d.empty ());

	d.mti2tp_diff[0] = 0;
	CPPUNIT_ASSERT (!d.empty ());
}
