/*
 * Unit tests for the automation part of the phenomenal diff, i.e. the
 * computation that decides which rows of an automation column must be
 * repainted after the pattern changed.
 *
 * The tests operate on synthetic RowToControlEvents maps so that they do not
 * need a Session, a Region or an AutomationList.
 */

#include <list>
#include <set>
#include <sstream>

#include "evoral/ControlList.h"
#include "temporal/beats.h"

#include "tracker_context.h"
#include "tracker_utils.h"
#include "automation_pattern.h"

#include "AutomationPatternTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (AutomationPatternTest);

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

/** Concrete AutomationPattern (AutomationPattern::insert is pure virtual). */
class TestAutomationPattern : public AutomationPattern
{
public:
	TestAutomationPattern (TrackerContext& context)
		: AutomationPattern (context,
		                     Temporal::timepos_t (Temporal::Beats::from_double (0.0)),
		                     Temporal::timepos_t (Temporal::Beats::from_double (0.0)),
		                     Temporal::timecnt_t (Temporal::Beats::from_double (16.0)),
		                     Temporal::timepos_t (Temporal::Beats::from_double (16.0)),
		                     Temporal::timepos_t (Temporal::Beats::from_double (16.0)),
		                     false)
	{
	}

	void insert (const Evoral::Parameter&) override {}
};

/** Owns the ControlEvents so that the pointers stored in the map stay valid. */
class Events
{
public:
	void add (int row, double value)
	{
		_events.emplace_back (Temporal::timepos_t (Temporal::Beats::from_double ((double) row)), value);
		_map.insert (RowToControlEvents::value_type (row, &_events.back ()));
	}

	/** Simulate an in-place edit of an existing event, as ControlList::modify () does. */
	void set_value (int row, double value)
	{
		RowToControlEvents::iterator it = _map.find (row);
		if (it != _map.end ()) {
			it->second->value = value;
		}
	}

	const RowToControlEvents& map () const { return _map; }

private:
	std::list<Evoral::ControlEvent> _events;
	RowToControlEvents _map;
};

/** Same union of both directions as AutomationPattern::phenomenal_diff (). */
std::set<int>
diff_rows (AutomationPattern& pattern, const RowToControlEvents& l, const RowToControlEvents& r)
{
	std::set<int> rows;
	pattern.rows_diff (l, r, rows);
	pattern.rows_diff (r, l, rows);
	return rows;
}

std::set<int>
rows_from_to (int first, int last)
{
	std::set<int> rows;
	for (int r = first; r <= last; ++r) {
		rows.insert (r);
	}
	return rows;
}

void
assert_rows (const std::set<int>& got, const std::set<int>& expected)
{
	std::stringstream ss;
	ss << "rows = {";
	for (std::set<int>::const_iterator it = got.begin (); it != got.end (); ++it) {
		ss << (it == got.begin () ? "" : ",") << *it;
	}
	ss << "}, expected {";
	for (std::set<int>::const_iterator it = expected.begin (); it != expected.end (); ++it) {
		ss << (it == expected.begin () ? "" : ",") << *it;
	}
	ss << "}";
	CPPUNIT_ASSERT_MESSAGE (ss.str (), got == expected);
}

} // anonymous namespace

void
AutomationPatternTest::setUp ()
{
	_context = new NullTrackerContext ();
	TestAutomationPattern* pattern = new TestAutomationPattern (*_context);
	pattern->nrows = 16;
	_pattern = pattern;
}

void
AutomationPatternTest::tearDown ()
{
	delete _pattern;
	delete _context;
	_pattern = 0;
	_context = 0;
}

void
AutomationPatternTest::testEmpty ()
{
	Events l, r;
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), std::set<int> ());
}

void
AutomationPatternTest::testIdentical ()
{
	Events l, r;
	l.add (4, 0.2); l.add (8, 0.5); l.add (12, 0.8);
	r.add (4, 0.2); r.add (8, 0.5); r.add (12, 0.8);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), std::set<int> ());
}

void
AutomationPatternTest::testAddedEvent ()
{
	/* An event is added at row 12: the interpolation from the previous event
	 * (row 8) up to the last row changes. */
	Events l, r;
	l.add (4, 0.2); l.add (8, 0.5); l.add (12, 0.8);
	r.add (4, 0.2); r.add (8, 0.5);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (9, 15));
}

void
AutomationPatternTest::testRemovedEvent ()
{
	/* Symmetric of testAddedEvent. */
	Events l, r;
	l.add (4, 0.2); l.add (8, 0.5);
	r.add (4, 0.2); r.add (8, 0.5); r.add (12, 0.8);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (9, 15));
}

void
AutomationPatternTest::testValueChange ()
{
	/* Only the value at row 8 changes: the interpolated rows strictly between
	 * its neighbours (rows 4 and 12) must be repainted. */
	Events l, r;
	l.add (4, 0.2); l.add (8, 0.5); l.add (12, 0.8);
	r.add (4, 0.2); r.add (8, 0.7); r.add (12, 0.8);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (5, 11));
}

void
AutomationPatternTest::testValueChangeLastEvent ()
{
	/* A value change on the last event must not read past the end of the
	 * right map, and must repaint all the rows after its previous neighbour. */
	Events l, r;
	l.add (4, 0.2); l.add (8, 0.5); l.add (12, 0.8);
	r.add (4, 0.2); r.add (8, 0.5); r.add (12, 0.9);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (9, 15));
}

void
AutomationPatternTest::testMoveToAnotherRow ()
{
	/* An event moves from row 8 to row 9 (same value): the whole span between
	 * the surrounding events (rows 4 and 12) is affected. */
	Events l, r;
	l.add (4, 0.2); l.add (9, 0.5); l.add (12, 0.8);
	r.add (4, 0.2); r.add (8, 0.5); r.add (12, 0.8);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (5, 11));
}

void
AutomationPatternTest::testFirstEventBoundary ()
{
	/* Adding the very first event at row 0 changes the interpolation of every
	 * row, including row 0 itself. */
	Events l, r;
	l.add (0, 0.1);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (0, 15));
}

void
AutomationPatternTest::testLastEventBoundary ()
{
	/* Adding an event on the last row changes every row after the previous
	 * event, including the last row. */
	Events l, r;
	l.add (0, 0.1);
	r.add (0, 0.1); r.add (15, 0.9);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (1, 15));
}

void
AutomationPatternTest::testDisplayabilityChange ()
{
	/* Two events on row 4 become one: the row itself must be repainted. */
	Events l, r;
	l.add (4, 0.1); l.add (4, 0.2); l.add (8, 0.5);
	r.add (4, 0.1); r.add (8, 0.5);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (4, 4));
}

void
AutomationPatternTest::testBothNonDisplayable ()
{
	/* If both sides have several events on the same row, the cells are not
	 * displayable and the diff does not ask for a repaint (current
	 * behaviour). */
	Events l, r;
	l.add (4, 0.1); l.add (4, 0.2); l.add (8, 0.5);
	r.add (4, 0.1); r.add (4, 0.3); r.add (8, 0.5);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), std::set<int> ());
}

void
AutomationPatternTest::testOneSidedNonDisplayable ()
{
	/* A non displayable row appears/disappears: only the row itself can be
	 * repainted, not the whole interpolation span. */
	Events l, r;
	l.add (4, 0.1); l.add (4, 0.2); l.add (8, 0.5);
	r.add (8, 0.5);
	assert_rows (diff_rows (*_pattern, l.map (), r.map ()), rows_from_to (4, 4));
}

void
AutomationPatternTest::testPhenomenalDiffPartial ()
{
	/* A value change must yield a partial diff (full == false) carrying the
	 * affected rows, so that the grid only repaints those. */
	const Evoral::Parameter param (0);
	Events l, r;
	l.add (4, 0.2); l.add (8, 0.7); l.add (12, 0.8);
	r.add (4, 0.2); r.add (8, 0.5); r.add (12, 0.8);

	TestAutomationPattern prev (*_context);
	prev.nrows = 16;
	_pattern->nrows = 16;
	_pattern->param_to_enabled[param] = true;
	prev.param_to_enabled[param] = true;
	_pattern->param_to_row_to_ces[param] = l.map ();
	prev.param_to_row_to_ces[param] = r.map ();

	AutomationPatternPhenomenalDiff diff = _pattern->phenomenal_diff (prev);
	CPPUNIT_ASSERT (!diff.full);
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, diff.param2rows_diff.count (param));
	const RowsPhenomenalDiff& rd = diff.param2rows_diff.find (param)->second;
	CPPUNIT_ASSERT (!rd.full);
	assert_rows (rd.rows, rows_from_to (5, 11));
}

void
AutomationPatternTest::testPhenomenalDiffUnchanged ()
{
	/* Nothing changed: the diff is partial and empty, i.e. nothing to do. */
	const Evoral::Parameter param (0);
	Events l, r;
	l.add (4, 0.2); l.add (8, 0.5); l.add (12, 0.8);
	r.add (4, 0.2); r.add (8, 0.5); r.add (12, 0.8);

	TestAutomationPattern prev (*_context);
	prev.nrows = 16;
	_pattern->nrows = 16;
	_pattern->param_to_enabled[param] = true;
	prev.param_to_enabled[param] = true;
	_pattern->param_to_row_to_ces[param] = l.map ();
	prev.param_to_row_to_ces[param] = r.map ();

	AutomationPatternPhenomenalDiff diff = _pattern->phenomenal_diff (prev);
	CPPUNIT_ASSERT (!diff.full);
	CPPUNIT_ASSERT (diff.empty ());
	CPPUNIT_ASSERT_EQUAL ((size_t) 0, diff.param2rows_diff.size ());
}

void
AutomationPatternTest::testPhenomenalDiffEnabledChange ()
{
	/* A parameter enabled in the current pattern but not in the previous one
	 * must request a full repaint of that parameter. */
	const Evoral::Parameter param (0);
	Events l;
	l.add (4, 0.2); l.add (8, 0.5);

	TestAutomationPattern prev (*_context);
	prev.nrows = 16;
	_pattern->nrows = 16;
	_pattern->param_to_enabled[param] = true;
	prev.param_to_enabled[param] = false;
	_pattern->param_to_row_to_ces[param] = l.map ();
	prev.param_to_row_to_ces[param] = l.map ();

	AutomationPatternPhenomenalDiff diff = _pattern->phenomenal_diff (prev);
	CPPUNIT_ASSERT (!diff.full);
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, diff.param2rows_diff.count (param));
	CPPUNIT_ASSERT (diff.param2rows_diff.find (param)->second.full);
}

void
AutomationPatternTest::testPhenomenalDiffOneSided ()
{
	/* The parameter has events in the current pattern but none in the previous
	 * one: its whole column must be repainted. */
	const Evoral::Parameter param (0);
	Events l;
	l.add (4, 0.2); l.add (8, 0.5);

	TestAutomationPattern prev (*_context);
	prev.nrows = 16;
	_pattern->nrows = 16;
	_pattern->param_to_enabled[param] = true;
	prev.param_to_enabled[param] = true;
	_pattern->param_to_row_to_ces[param] = l.map ();
	/* prev has no row mapping for param */

	AutomationPatternPhenomenalDiff diff = _pattern->phenomenal_diff (prev);
	CPPUNIT_ASSERT (!diff.full);
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, diff.param2rows_diff.count (param));
	CPPUNIT_ASSERT (diff.param2rows_diff.find (param)->second.full);
}

void
AutomationPatternTest::testSnapshotCarriesEnabled ()
{
	/* prev_pattern = pattern must carry the enabled flags, otherwise
	 * phenomenal_diff () sees a bogus enabled-state change and marks the whole
	 * parameter full. */
	const Evoral::Parameter param (0);
	Events l;
	l.add (4, 0.2);

	TestAutomationPattern src (*_context);
	src.nrows = 16;
	src.param_to_enabled[param] = true;
	src.param_to_row_to_ces[param] = l.map ();

	TestAutomationPattern prev (*_context);
	prev.nrows = 16;
	prev = src;

	CPPUNIT_ASSERT_EQUAL ((size_t) 1, prev.param_to_enabled.count (param));
	CPPUNIT_ASSERT (prev.param_to_enabled[param]);
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, prev.param_to_row_to_ces.count (param));
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, prev.param_to_row_to_ces.find (param)->second.size ());
}

void
AutomationPatternTest::testSnapshotIndependentEvents ()
{
	/* The snapshot must copy the control events: the live automation list is
	 * mutated in place on edit and must not change the snapshot. */
	const Evoral::Parameter param (0);
	Events l;
	l.add (4, 0.2); l.add (8, 0.5);

	TestAutomationPattern src (*_context);
	src.nrows = 16;
	src.param_to_enabled[param] = true;
	src.param_to_row_to_ces[param] = l.map ();

	TestAutomationPattern prev (*_context);
	prev.nrows = 16;
	prev = src;

	/* In-place edit of the live event */
	l.set_value (8, 0.7);

	CPPUNIT_ASSERT_EQUAL ((size_t) 1, prev.param_to_row_to_ces.count (param));
	const RowToControlEvents& prev_events = prev.param_to_row_to_ces.find (param)->second;
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, prev_events.count (8));
	CPPUNIT_ASSERT_EQUAL (0.5, prev_events.find (8)->second->value);
}

void
AutomationPatternTest::testPhenomenalDiffThroughSnapshot ()
{
	/* End-to-end through the snapshot: an in-place value edit must produce a
	 * partial diff with the affected interpolation rows. */
	const Evoral::Parameter param (0);
	Events l;
	l.add (4, 0.2); l.add (8, 0.5); l.add (12, 0.8);

	TestAutomationPattern src (*_context);
	src.nrows = 16;
	src.param_to_enabled[param] = true;
	src.param_to_row_to_ces[param] = l.map ();

	TestAutomationPattern prev (*_context);
	prev.nrows = 16;
	prev = src;

	l.set_value (8, 0.7);

	AutomationPatternPhenomenalDiff diff = src.phenomenal_diff (prev);
	CPPUNIT_ASSERT (!diff.full);
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, diff.param2rows_diff.count (param));
	const RowsPhenomenalDiff& rd = diff.param2rows_diff.find (param)->second;
	CPPUNIT_ASSERT (!rd.full);
	assert_rows (rd.rows, rows_from_to (5, 11));
}
