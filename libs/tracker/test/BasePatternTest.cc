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
	void begin_bulk_edit (const std::string&) override {}
	void end_bulk_edit () override {}
	bool bulk_edit_in_progress () const override { return false; }
	void track_headers_changed () override {}
};

/** TrackerContext that records bulk-edit begin/end calls. */
class CountingTrackerContext : public NullTrackerContext
{
public:
	CountingTrackerContext () : begins (0), ends (0), in_progress (false) {}

	void begin_bulk_edit (const std::string&) override { ++begins; in_progress = true; }
	void end_bulk_edit () override { ++ends; in_progress = false; }
	bool bulk_edit_in_progress () const override { return in_progress; }

	int  begins;
	int  ends;
	bool in_progress;
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

/** A pattern spanning beats [2, 6) at the given rows per beat. */
static std::unique_ptr<TestPattern>
make_pattern (TrackerContext& ctx, uint16_t rows_per_beat = 4)
{
	Temporal::timepos_t position (Temporal::Beats::from_double (2.0));
	Temporal::timepos_t start (Temporal::Beats::from_double (0.0));
	Temporal::timecnt_t length (Temporal::Beats::from_double (4.0));
	Temporal::timepos_t end (Temporal::Beats::from_double (6.0));
	Temporal::timepos_t nt_last (Temporal::Beats::from_double (6.0));
	std::unique_ptr<TestPattern> p (new TestPattern (ctx, position, start, length, end, nt_last));
	p->set_rows_per_beat (rows_per_beat, true);
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
BasePatternTest::testIntegerTickRows ()
{
	/* Ardour defines one grid step at N rows per beat as
	 * ticks_per_beat / N ticks (integer division), not as
	 * Beats::from_double(1.0 / N).  The difference matters for divisions
	 * that do not divide 1920 exactly, such as 7, 14 and 28.
	 */
	const uint16_t divisions[] = { 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32 };

	for (unsigned i = 0; i < sizeof (divisions) / sizeof (divisions[0]); ++i) {
		const uint16_t rpb = divisions[i];
		const int64_t ticks_per_row = Temporal::ticks_per_beat / rpb;
		std::unique_ptr<TestPattern> p (make_pattern (*_context, rpb));

		CPPUNIT_ASSERT_EQUAL (ticks_per_row, (int64_t) p->beats_per_row.to_ticks ());
		CPPUNIT_ASSERT_EQUAL (1 - ticks_per_row, (int64_t) p->delay_ticks_min ());
		CPPUNIT_ASSERT_EQUAL (ticks_per_row - 1, (int64_t) p->delay_ticks_max ());

		for (int r = 0; r < p->nrows; ++r) {
			const int64_t expected = p->position_row_beats.to_ticks () + r * ticks_per_row;
			CPPUNIT_ASSERT_EQUAL (expected, p->beats_at_row (r).to_ticks ());
			CPPUNIT_ASSERT_EQUAL ((int64_t) 0, p->delay_ticks_at_row (p->beats_at_row (r), r));
			CPPUNIT_ASSERT_EQUAL (r, p->row_at_beats (p->beats_at_row (r)));
		}

		CPPUNIT_ASSERT_EQUAL ((int64_t) p->nrows,
		                      (p->end_row_beats - p->position_row_beats).to_ticks () / ticks_per_row);
	}
}

void
BasePatternTest::testDelayRowSelection ()
{
	const uint16_t divisions[] = { 7, 14, 28 };

	for (unsigned i = 0; i < sizeof (divisions) / sizeof (divisions[0]); ++i) {
		const uint16_t rpb = divisions[i];
		const int64_t ticks_per_row = Temporal::ticks_per_beat / rpb;
		std::unique_ptr<TestPattern> p (make_pattern (*_context, rpb));

		/* On a row boundary both selectors return that row. */
		CPPUNIT_ASSERT_EQUAL (0, p->row_at_beats_max_delay (p->beats_at_row (0)));
		CPPUNIT_ASSERT_EQUAL (0, p->row_at_beats_min_delay (p->beats_at_row (0)));

		/* One tick after: previous row with positive delay, next row with
		 * negative delay. */
		const Temporal::Beats one_tick_later = p->beats_at_row (0) + Temporal::Beats::ticks (1);
		CPPUNIT_ASSERT_EQUAL (0, p->row_at_beats_max_delay (one_tick_later));
		CPPUNIT_ASSERT_EQUAL (1, p->row_at_beats_min_delay (one_tick_later));

		/* Same at the end of a row. */
		const Temporal::Beats just_before_next = p->beats_at_row (1) - Temporal::Beats::ticks (1);
		CPPUNIT_ASSERT_EQUAL (0, p->row_at_beats_max_delay (just_before_next));
		CPPUNIT_ASSERT_EQUAL (1, p->row_at_beats_min_delay (just_before_next));

		CPPUNIT_ASSERT_EQUAL (1, p->row_at_beats_max_delay (p->beats_at_row (1)));
		CPPUNIT_ASSERT_EQUAL (1, p->row_at_beats_min_delay (p->beats_at_row (1)));

		/* Nearest-row mapping avoids the ambiguous half-way positions. */
		CPPUNIT_ASSERT_EQUAL (0, p->row_at_beats (p->beats_at_row (1) - Temporal::Beats::ticks (ticks_per_row / 2 + 1)));
		CPPUNIT_ASSERT_EQUAL (1, p->row_at_beats (p->beats_at_row (1) - Temporal::Beats::ticks (ticks_per_row / 2 - 1)));
		CPPUNIT_ASSERT_EQUAL (1, p->row_at_beats (p->beats_at_row (1) + Temporal::Beats::ticks (ticks_per_row / 2 - 1)));
		CPPUNIT_ASSERT_EQUAL (2, p->row_at_beats (p->beats_at_row (1) + Temporal::Beats::ticks (ticks_per_row / 2 + 1)));

		CPPUNIT_ASSERT_EQUAL (5, p->row_distance (p->beats_at_row (0), p->beats_at_row (5)));
		CPPUNIT_ASSERT_EQUAL (-5, p->row_distance (p->beats_at_row (5), p->beats_at_row (0)));
	}
}

void
BasePatternTest::test28RowsPerBeat ()
{
	std::unique_ptr<TestPattern> p (make_pattern (*_context, 28));

	/* Ardour uses 1920 / 28 == 68 ticks per row, not 69. */
	CPPUNIT_ASSERT_EQUAL ((int64_t) 68, (int64_t) p->beats_per_row.to_ticks ());

	/* 28 rows span 1904 ticks; the remaining 16 ticks of the beat are lost,
	 * exactly as with Ardour's integer grid. */
	CPPUNIT_ASSERT_EQUAL ((int64_t) 1904,
	                      (p->beats_at_row (28) - p->beats_at_row (0)).to_ticks ());

	/* The region starts at beat 2 (3840 ticks) and ends at beat 6 (11520
	 * ticks).  The nearest 68-tick boundaries are 3808 and 11492. */
	CPPUNIT_ASSERT_EQUAL ((int64_t) 3808, (int64_t) p->position_row_beats.to_ticks ());
	CPPUNIT_ASSERT_EQUAL ((int64_t) 11492, (int64_t) p->end_row_beats.to_ticks ());
	CPPUNIT_ASSERT_EQUAL (113, p->nrows);
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

void
BasePatternTest::testBulkEdit ()
{
	CountingTrackerContext ctx;

	CPPUNIT_ASSERT (!ctx.bulk_edit_in_progress ());

	{
		BulkEdit outer (ctx, "outer");
		CPPUNIT_ASSERT (ctx.bulk_edit_in_progress ());
		CPPUNIT_ASSERT_EQUAL (1, ctx.begins);

		{
			// A nested scope joins the outer transaction instead of
			// committing it.
			BulkEdit inner (ctx, "inner");
			CPPUNIT_ASSERT (ctx.bulk_edit_in_progress ());
			CPPUNIT_ASSERT_EQUAL (1, ctx.begins);
		}

		CPPUNIT_ASSERT (ctx.bulk_edit_in_progress ());
		CPPUNIT_ASSERT_EQUAL (0, ctx.ends);
	}

	CPPUNIT_ASSERT (!ctx.bulk_edit_in_progress ());
	CPPUNIT_ASSERT_EQUAL (1, ctx.begins);
	CPPUNIT_ASSERT_EQUAL (1, ctx.ends);
}
