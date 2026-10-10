#include <memory>

#include "base_pattern.h"
#include "tracker_context.h"

#include "BasePatternTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (BasePatternTest);

using namespace Tracker;

namespace {

/** Minimal TrackerContext with no side effect, for testing the model alone. */
class NullTrackerContext : public TrackerContext
{
public:
	ARDOUR::Session* get_session () const override { return nullptr; }
	void connect_track (TrackPtr) override {}
	void connect_midi_region (MidiRegionPtr) override {}
	void connect_automation (AutomationControlPtr) override {}
	void begin_reversible_command (const std::string&) override {}
	void commit_reversible_command () override {}
	void add_command (PBD::Command*) override {}
	void set_dirty () override {}
	void track_headers_changed () override {}
};

/** Concrete BasePattern (BasePattern::update is pure virtual). */
class TestPattern : public BasePattern
{
public:
	TestPattern (TrackerContext& context,
	             Temporal::timepos_t position,
	             Temporal::timepos_t start,
	             Temporal::timecnt_t length,
	             Temporal::timepos_t end,
	             Temporal::timepos_t nt_last)
		: BasePattern (context, position, start, length, end, nt_last) {}
	void update () override {}
};

} // anonymous namespace

void
BasePatternTest::setUp ()
{
	_context = new NullTrackerContext ();
}

void
BasePatternTest::tearDown ()
{
	delete _context;
	_context = 0;
}

/** A pattern spanning beats [2, 6) at 4 rows per beat. */
static std::unique_ptr<TestPattern>
make_pattern (TrackerContext& ctx)
{
	Temporal::timepos_t position (Temporal::Beats::from_double (2.0));
	Temporal::timepos_t start (Temporal::Beats::from_double (0.0));
	Temporal::timecnt_t length (Temporal::Beats::from_double (4.0));
	Temporal::timepos_t end (Temporal::Beats::from_double (6.0));
	Temporal::timepos_t nt_last (Temporal::Beats::from_double (6.0));
	std::unique_ptr<TestPattern> p (new TestPattern (ctx, position, start, length, end, nt_last));
	p->set_rows_per_beat (4, true);
	p->set_row_range ();
	return p;
}

void
BasePatternTest::testRowRange ()
{
	std::unique_ptr<TestPattern> p (make_pattern (*_context));

	CPPUNIT_ASSERT (p->position_beats == Temporal::Beats::from_double (2.0));
	CPPUNIT_ASSERT (p->global_end_beats == Temporal::Beats::from_double (6.0));
	CPPUNIT_ASSERT (p->length_beats == Temporal::Beats::from_double (4.0));
	CPPUNIT_ASSERT (p->start_beats == Temporal::Beats::from_double (0.0));
	CPPUNIT_ASSERT (p->end_beats == Temporal::Beats::from_double (4.0));
	CPPUNIT_ASSERT (p->position_row_beats == Temporal::Beats::from_double (2.0));
	CPPUNIT_ASSERT (p->end_row_beats == Temporal::Beats::from_double (6.0));
	CPPUNIT_ASSERT_EQUAL (16, p->nrows);
}

void
BasePatternTest::testBeatsAtRow ()
{
	std::unique_ptr<TestPattern> p (make_pattern (*_context));

	CPPUNIT_ASSERT (p->beats_at_row (0) == p->position_row_beats);
	CPPUNIT_ASSERT (p->beats_at_row (4) == Temporal::Beats::from_double (3.0));
	CPPUNIT_ASSERT (p->beats_at_row (16) == p->end_row_beats);
	CPPUNIT_ASSERT (p->beats_at_row (0, 100) == p->beats_at_row (0) + Temporal::Beats::ticks (100));
	CPPUNIT_ASSERT (p->beats_at_row (4, -100) == p->beats_at_row (4) - Temporal::Beats::ticks (100));
}

void
BasePatternTest::testDelayBounds ()
{
	std::unique_ptr<TestPattern> p (make_pattern (*_context));
	const int ticks_per_row = Temporal::ticks_per_beat / 4;

	CPPUNIT_ASSERT_EQUAL (1 - ticks_per_row, p->delay_ticks_min ());
	CPPUNIT_ASSERT_EQUAL (ticks_per_row - 1, p->delay_ticks_max ());
}

void
BasePatternTest::testDelayRoundTrip ()
{
	std::unique_ptr<TestPattern> p (make_pattern (*_context));

	/* beats_at_row(r) is the center of row r, so row_at_beats must map it back */
	for (int r = 0; r < p->nrows; r++) {
		CPPUNIT_ASSERT_EQUAL (r, p->row_at_beats (p->beats_at_row (r)));
	}

	/* delay_ticks_at_row is the inverse of the delay passed to beats_at_row */
	const int delays[] = { -100, 0, 100, p->delay_ticks_max () };
	for (unsigned i = 0; i < sizeof (delays) / sizeof (delays[0]); i++) {
		const int d = delays[i];
		CPPUNIT_ASSERT_EQUAL ((int64_t) d, p->delay_ticks_at_row (p->beats_at_row (4, d), 4));
	}
}

void
BasePatternTest::testIsDefined ()
{
	std::unique_ptr<TestPattern> p (make_pattern (*_context));

	CPPUNIT_ASSERT (p->is_defined (0));
	CPPUNIT_ASSERT (p->is_defined (p->nrows - 1));
	CPPUNIT_ASSERT (!p->is_defined (p->nrows));
	CPPUNIT_ASSERT (!p->is_defined (-1));

	p->set_enabled (false);
	CPPUNIT_ASSERT (!p->is_defined (0));
}

void
BasePatternTest::testRegionRelativeBeats ()
{
	std::unique_ptr<TestPattern> p (make_pattern (*_context));

	/* start_beats is 0, position_beats is 2, so relative row 0 is beat 0 */
	CPPUNIT_ASSERT (p->region_relative_beats_at_row (0) == Temporal::Beats::from_double (0.0));
	CPPUNIT_ASSERT (p->region_relative_beats_at_row (4) == Temporal::Beats::from_double (1.0));
}

void
BasePatternTest::testRankedRowHelpers ()
{
	std::unique_ptr<TestPattern> p (make_pattern (*_context));

	CPPUNIT_ASSERT (p->row_lt (1, 2));
	CPPUNIT_ASSERT (!p->row_lt (2, 2));
	CPPUNIT_ASSERT (p->row_lt (-1, 2));
	CPPUNIT_ASSERT (p->row_lt (2, -1));

	CPPUNIT_ASSERT (p->row_gte (2, 1));
	CPPUNIT_ASSERT (!p->row_gte (1, 2));
	CPPUNIT_ASSERT (p->row_gte (-1, -1));

	int r1[3] = { 8, -1, 9 };
	p->repair_ranked_row (r1);
	CPPUNIT_ASSERT_EQUAL (8, r1[0]);
	CPPUNIT_ASSERT_EQUAL (9, r1[1]);
	CPPUNIT_ASSERT_EQUAL (-1, r1[2]);

	int r2[3] = { -1, -1, 5 };
	p->repair_ranked_row (r2);
	CPPUNIT_ASSERT_EQUAL (5, r2[0]);
	CPPUNIT_ASSERT_EQUAL (-1, r2[1]);
	CPPUNIT_ASSERT_EQUAL (-1, r2[2]);

	int r3[3] = { 1, 2, 3 };
	p->repair_ranked_row (r3);
	CPPUNIT_ASSERT_EQUAL (1, r3[0]);
	CPPUNIT_ASSERT_EQUAL (2, r3[1]);
	CPPUNIT_ASSERT_EQUAL (3, r3[2]);

	int r4[3] = { 1, -1, -1 };
	p->repair_ranked_row (r4);
	CPPUNIT_ASSERT_EQUAL (1, r4[0]);
	CPPUNIT_ASSERT_EQUAL (-1, r4[1]);
	CPPUNIT_ASSERT_EQUAL (-1, r4[2]);
}
