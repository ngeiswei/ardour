#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "evoral/ControlList.h"
#include "evoral/Note.h"
#include "temporal/beats.h"

#include "automation_pattern.h"
#include "midi_notes_pattern.h"
#include "tracker_utils.h"

#include "RowDiffTest.h"
#include "TestTrackerContext.h"

CPPUNIT_TEST_SUITE_REGISTRATION (RowDiffTest);

using namespace Tracker;

namespace {

/* ------------------------------------------------------------------ */
/* Notes                                                              */
/* ------------------------------------------------------------------ */

NotePtr
mknote (int row, int note, int channel = 0, int velocity = 100)
{
	return NotePtr (new NoteType (channel,
	                              Temporal::Beats::from_double (row),
	                              Temporal::Beats::from_double (0.5),
	                              note, velocity));
}

std::string
on_note_attrs (NotePtr n)
{
	std::stringstream ss;
	ss << (int) n->note () << "," << (int) n->channel () << ","
	   << (int) n->velocity () << "," << n->time ().to_ticks ();
	return ss.str ();
}

std::string
off_note_attrs (NotePtr n)
{
	return std::to_string (n->end_time ().to_ticks ());
}

/*
 * The visible content of the note cell at a row: "***" if undisplayable, the
 * on note attributes if an on note is present (an on note hides an off note),
 * otherwise the off note attributes, otherwise blank.
 */
std::string
render_note_row (const RowToNotes& on, const RowToNotes& off, int row)
{
	if (!MidiNotesPattern::is_row_displayable (on, off, row)) {
		return "***";
	}
	RowToNotes::const_iterator nit = on.find (row);
	if (nit != on.end ()) {
		return "on:" + on_note_attrs (nit->second);
	}
	RowToNotes::const_iterator oit = off.find (row);
	if (oit != off.end ()) {
		return "off:" + off_note_attrs (oit->second);
	}
	return "";
}

/* Reference: rows whose visible note cell differs between the two states. */
std::set<int>
notes_expected (const RowToNotes& l_on, const RowToNotes& l_off,
                const RowToNotes& r_on, const RowToNotes& r_off)
{
	std::set<int> candidate;
	for (RowToNotes::const_iterator it = l_on.begin (); it != l_on.end (); ++it) candidate.insert (it->first);
	for (RowToNotes::const_iterator it = l_off.begin (); it != l_off.end (); ++it) candidate.insert (it->first);
	for (RowToNotes::const_iterator it = r_on.begin (); it != r_on.end (); ++it) candidate.insert (it->first);
	for (RowToNotes::const_iterator it = r_off.begin (); it != r_off.end (); ++it) candidate.insert (it->first);
	std::set<int> expected;
	for (std::set<int>::const_iterator it = candidate.begin (); it != candidate.end (); ++it) {
		if (render_note_row (l_on, l_off, *it) != render_note_row (r_on, r_off, *it)) {
			expected.insert (*it);
		}
	}
	return expected;
}

std::set<int>
notes_actual (const RowToNotes& l_on, const RowToNotes& l_off,
              const RowToNotes& r_on, const RowToNotes& r_off)
{
	std::set<int> rows;
	MidiNotesPattern::rows_diff (l_on, l_off, r_on, r_off, rows);
	MidiNotesPattern::rows_diff (r_on, r_off, l_on, l_off, rows);
	return rows;
}

/* ------------------------------------------------------------------ */
/* Automation                                                         */
/* ------------------------------------------------------------------ */

/** Concrete AutomationPattern (insert() is pure virtual). */
class TestAutomationPattern : public AutomationPattern
{
public:
	TestAutomationPattern (TrackerContext& ctx)
		: AutomationPattern (ctx,
		                     Temporal::timepos_t (), Temporal::timepos_t (),
		                     Temporal::timecnt_t (), Temporal::timepos_t (),
		                     Temporal::timepos_t (), false) {}
	void insert (const Evoral::Parameter&) override {}
};

typedef std::vector<Evoral::ControlEvent*> EventPool;

Evoral::ControlEvent*
mk_control_event (EventPool& pool, int row, double value)
{
	Evoral::ControlEvent* ce = new Evoral::ControlEvent (Temporal::timepos_t (Temporal::Beats::from_double (row)), value);
	pool.push_back (ce);
	return ce;
}

void
free_events (EventPool& pool)
{
	for (size_t i = 0; i < pool.size (); ++i) {
		delete pool[i];
	}
	pool.clear ();
}

/*
 * Value displayed at a given row, matching ControlList::unlocked_eval for the
 * simple cases used here (no guards, linear interpolation): normal value when
 * there is no event, the event value when there is one, clamped to the first /
 * last event value outside of the event range, linear in between.
 */
double
displayed_value (const RowToControlEvents& m, int row, double normal)
{
	if (m.empty ()) {
		return normal;
	}
	if (m.size () == 1) {
		return m.begin ()->second->value;
	}
	const int first_row = m.begin ()->first;
	const int last_row = m.rbegin ()->first;
	if (row <= first_row) {
		return m.begin ()->second->value;
	}
	if (row >= last_row) {
		return m.rbegin ()->second->value;
	}
	RowToControlEvents::const_iterator next = m.upper_bound (row);
	RowToControlEvents::const_iterator prev = next;
	--prev;
	const double lval = prev->second->value;
	const double uval = next->second->value;
	const double frac = (double) (row - prev->first) / (double) (next->first - prev->first);
	return lval + frac * (uval - lval);
}

std::set<int>
automation_expected (const RowToControlEvents& l, const RowToControlEvents& r, int nrows)
{
	std::set<int> expected;
	for (int row = 0; row < nrows; ++row) {
		if (displayed_value (l, row, 0.0) != displayed_value (r, row, 0.0)) {
			expected.insert (row);
		}
	}
	return expected;
}

std::set<int>
automation_actual (AutomationPattern& ap, const RowToControlEvents& l, const RowToControlEvents& r)
{
	std::set<int> rows;
	ap.rows_diff (l, r, rows);
	ap.rows_diff (r, l, rows);
	return rows;
}

} // anonymous namespace

/* ------------------------------------------------------------------ */
/* Notes tests                                                        */
/* ------------------------------------------------------------------ */

void
RowDiffTest::testNotesRowsDiffExact ()
{
	// empty vs empty
	{
		RowToNotes on, off;
		CPPUNIT_ASSERT (notes_actual (on, off, on, off).empty ());
	}

	// add an on note
	{
		RowToNotes l_on, l_off, r_on, r_off;
		l_on.insert (std::make_pair (3, mknote (3, 60)));
		std::set<int> actual = notes_actual (l_on, l_off, r_on, r_off);
		CPPUNIT_ASSERT_EQUAL ((size_t) 1, actual.size ());
		CPPUNIT_ASSERT (actual.count (3));
	}

	// change an on note (different pitch)
	{
		RowToNotes l_on, l_off, r_on, r_off;
		l_on.insert (std::make_pair (3, mknote (3, 60)));
		r_on.insert (std::make_pair (3, mknote (3, 62)));
		std::set<int> actual = notes_actual (l_on, l_off, r_on, r_off);
		CPPUNIT_ASSERT_EQUAL ((size_t) 1, actual.size ());
		CPPUNIT_ASSERT (actual.count (3));
	}

	// add an off note
	{
		RowToNotes l_on, l_off, r_on, r_off;
		l_off.insert (std::make_pair (5, mknote (4, 60)));
		std::set<int> actual = notes_actual (l_on, l_off, r_on, r_off);
		CPPUNIT_ASSERT (actual.count (5));
	}

	// two on notes at the same row: undisplayable
	{
		RowToNotes l_on, l_off, r_on, r_off;
		l_on.insert (std::make_pair (2, mknote (2, 60)));
		l_on.insert (std::make_pair (2, mknote (2, 64)));
		r_on.insert (std::make_pair (2, mknote (2, 60)));
		std::set<int> actual = notes_actual (l_on, l_off, r_on, r_off);
		CPPUNIT_ASSERT (actual.count (2));
	}
}

void
RowDiffTest::testNotesRowsDiffOracle ()
{
	std::mt19937 rng (20240517);

	for (int t = 0; t < 1000; ++t) {
		RowToNotes l_on, l_off, r_on, r_off;

		// left state
		int ln = rng () % 6;
		for (int i = 0; i < ln; ++i) {
			int row = rng () % 16;
			NotePtr n = mknote (row, 60 + (rng () % 13), rng () % 2, 40 + (rng () % 80));
			if (rng () % 2) {
				l_on.insert (std::make_pair (row, n));
			} else {
				l_off.insert (std::make_pair (row, n));
			}
		}

		// right state
		int rn = rng () % 6;
		for (int i = 0; i < rn; ++i) {
			int row = rng () % 16;
			NotePtr n = mknote (row, 60 + (rng () % 13), rng () % 2, 40 + (rng () % 80));
			if (rng () % 2) {
				r_on.insert (std::make_pair (row, n));
			} else {
				r_off.insert (std::make_pair (row, n));
			}
		}

		std::set<int> expected = notes_expected (l_on, l_off, r_on, r_off);
		std::set<int> actual = notes_actual (l_on, l_off, r_on, r_off);

		for (std::set<int>::const_iterator it = expected.begin (); it != expected.end (); ++it) {
			CPPUNIT_ASSERT_MESSAGE ("notes rows_diff missed a visibly changed row",
			                        actual.count (*it) > 0);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Automation tests                                                   */
/* ------------------------------------------------------------------ */

void
RowDiffTest::testAutomationRowsDiffExact ()
{
	TestTrackerContext ctx (0);
	TestAutomationPattern ap (ctx);
	ap.nrows = 16;

	EventPool lpool, rpool;

	// Change the value of the first event (row 4).  All rows up to the next
	// event (row 10) clamp/interpolate to a different value, including row 0.
	RowToControlEvents l, r;
	l.insert (std::make_pair (4, mk_control_event (lpool, 4, 0.0)));
	l.insert (std::make_pair (10, mk_control_event (lpool, 10, 1.0)));
	r.insert (std::make_pair (4, mk_control_event (rpool, 4, 0.25)));
	r.insert (std::make_pair (10, mk_control_event (rpool, 10, 1.0)));

	std::set<int> expected = automation_expected (l, r, 16);
	std::set<int> actual = automation_actual (ap, l, r);

	for (std::set<int>::const_iterator it = expected.begin (); it != expected.end (); ++it) {
		CPPUNIT_ASSERT_MESSAGE ("automation rows_diff missed a visibly changed row",
		                        actual.count (*it) > 0);
	}

	// Exact check: rows 0..9 must be reported (row 0 is the boundary case).
	for (int row = 0; row <= 9; ++row) {
		CPPUNIT_ASSERT (actual.count (row) > 0);
	}

	free_events (lpool);
	free_events (rpool);
}

void
RowDiffTest::testAutomationRowsDiffOracle ()
{
	TestTrackerContext ctx (0);

	std::mt19937 rng (424242);

	for (int t = 0; t < 1000; ++t) {
		EventPool lpool, rpool;
		RowToControlEvents l, r;

		// left: 1..4 events at distinct rows
		std::set<int> lrows;
		int ln = 1 + rng () % 4;
		for (int i = 0; i < ln; ++i) {
			int row = rng () % 16;
			if (!lrows.insert (row).second) {
				continue;
			}
			l.insert (std::make_pair (row, mk_control_event (lpool, row, (rng () % 5) / 4.0)));
		}

		// right: 1..4 events at distinct rows
		std::set<int> rrows;
		int rn = 1 + rng () % 4;
		for (int i = 0; i < rn; ++i) {
			int row = rng () % 16;
			if (!rrows.insert (row).second) {
				continue;
			}
			r.insert (std::make_pair (row, mk_control_event (rpool, row, (rng () % 5) / 4.0)));
		}

		TestAutomationPattern ap (ctx);
		ap.nrows = 16;

		std::set<int> expected = automation_expected (l, r, 16);
		std::set<int> actual = automation_actual (ap, l, r);

		for (std::set<int>::const_iterator it = expected.begin (); it != expected.end (); ++it) {
			if (!actual.count (*it)) {
				std::stringstream msg;
				msg << "automation rows_diff missed row " << *it << "; l={";
				for (RowToControlEvents::const_iterator e = l.begin (); e != l.end (); ++e) {
					msg << e->first << ":" << e->second->value << " ";
				}
				msg << "} r={";
				for (RowToControlEvents::const_iterator e = r.begin (); e != r.end (); ++e) {
					msg << e->first << ":" << e->second->value << " ";
				}
				msg << "}";
				CPPUNIT_FAIL (msg.str ());
			}
		}

		free_events (lpool);
		free_events (rpool);
	}
}
