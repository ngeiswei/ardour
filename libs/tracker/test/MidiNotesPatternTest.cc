/*
 * Unit tests for MidiNotesPattern::rows_diff (), the phenomenal-diff routine
 * that decides which rows of a MIDI note column must be repainted.
 *
 * The tests operate on synthetic on/off note maps (row -> NotePtr), so they do
 * not need a Session, a Region or a MidiModel.
 */

#include <set>
#include <sstream>

#include "temporal/beats.h"

#include "midi_notes_pattern.h"

#include "MidiNotesPatternTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (MidiNotesPatternTest);

using namespace Tracker;

namespace {

NotePtr
mknote (double time, double length, int pitch = 60, int velocity = 100, int channel = 0)
{
	return NotePtr (new NoteType ((uint8_t) channel,
	                              Temporal::Beats::from_double (time),
	                              Temporal::Beats::from_double (length),
	                              (uint8_t) pitch,
	                              (uint8_t) velocity));
}

/** Holds the on/off note maps of a single cgi. */
struct NoteMaps
{
	RowToNotes on;
	RowToNotes off;

	void add (int on_row, int off_row, double time, double length, int pitch = 60)
	{
		NotePtr n = mknote (time, length, pitch);
		on.insert (RowToNotes::value_type (on_row, n));
		off.insert (RowToNotes::value_type (off_row, n));
	}

	void add_on (int row, double time, double length, int pitch = 60)
	{
		on.insert (RowToNotes::value_type (row, mknote (time, length, pitch)));
	}

	void add_off (int row, double time, double length, int pitch = 60)
	{
		off.insert (RowToNotes::value_type (row, mknote (time, length, pitch)));
	}
};

std::set<int>
diff_rows_one_way (const NoteMaps& l, const NoteMaps& r)
{
	std::set<int> rows;
	MidiNotesPattern::rows_diff (l.on, l.off, r.on, r.off, rows);
	return rows;
}

/** Same union of both directions as MidiNotesPattern::phenomenal_diff (). */
std::set<int>
diff_rows (const NoteMaps& l, const NoteMaps& r)
{
	std::set<int> rows = diff_rows_one_way (l, r);
	MidiNotesPattern::rows_diff (r.on, r.off, l.on, l.off, rows);
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
MidiNotesPatternTest::testIsRowDisplayable ()
{
	/* Empty row */
	{
		NoteMaps m;
		CPPUNIT_ASSERT (MidiNotesPattern::is_row_displayable (m.on, m.off, 4));
	}

	/* One on note */
	{
		NoteMaps m;
		m.add_on (4, 4, 2, 60);
		CPPUNIT_ASSERT (MidiNotesPattern::is_row_displayable (m.on, m.off, 4));
	}

	/* Two on notes: not displayable */
	{
		NoteMaps m;
		m.add_on (4, 4, 2, 60);
		m.add_on (4, 4, 2, 62);
		CPPUNIT_ASSERT (!MidiNotesPattern::is_row_displayable (m.on, m.off, 4));
	}

	/* One off note ending where the on note starts: displayable */
	{
		NoteMaps m;
		m.add_off (4, 0, 4, 55); // ends at 4
		m.add_on (4, 4, 2, 60);  // starts at 4
		CPPUNIT_ASSERT (MidiNotesPattern::is_row_displayable (m.on, m.off, 4));
	}

	/* One off note not meeting the on note: not displayable */
	{
		NoteMaps m;
		m.add_off (4, 0, 3, 55); // ends at 3
		m.add_on (4, 4, 2, 60);  // starts at 4
		CPPUNIT_ASSERT (!MidiNotesPattern::is_row_displayable (m.on, m.off, 4));
	}
}

void
MidiNotesPatternTest::testEmpty ()
{
	NoteMaps l, r;
	assert_rows (diff_rows (l, r), std::set<int> ());
}

void
MidiNotesPatternTest::testIdentical ()
{
	NoteMaps l, r;
	l.add (4, 6, 4, 2, 60);
	r.add (4, 6, 4, 2, 60);
	assert_rows (diff_rows (l, r), std::set<int> ());
}

void
MidiNotesPatternTest::testAddedNote ()
{
	/* A new note affects both its on and off cells. */
	NoteMaps l, r;
	l.add (4, 6, 4, 2, 60);
	std::set<int> expected = rows_from_to (4, 4);
	expected.insert (6);
	assert_rows (diff_rows (l, r), expected);
}

void
MidiNotesPatternTest::testRemovedNote ()
{
	NoteMaps l, r;
	r.add (4, 6, 4, 2, 60);
	std::set<int> expected = rows_from_to (4, 4);
	expected.insert (6);
	assert_rows (diff_rows (l, r), expected);
}

void
MidiNotesPatternTest::testOneWayOnlyVisitsLeft ()
{
	/* rows_diff (l, r) only walks l's maps, so a row that exists only in r is
	 * not reported by a single call; phenomenal_diff () calls it both ways. */
	NoteMaps l, r;
	r.add (4, 6, 4, 2, 60);
	assert_rows (diff_rows_one_way (l, r), std::set<int> ());
	std::set<int> expected = rows_from_to (4, 4);
	expected.insert (6);
	assert_rows (diff_rows (l, r), expected);
}

void
MidiNotesPatternTest::testOnNoteChanged ()
{
	/* Only the pitch changes: the on cell must be repainted, the off cell
	 * (same end time) need not. */
	NoteMaps l, r;
	l.add (4, 6, 4, 2, 60);
	r.add (4, 6, 4, 2, 62);
	assert_rows (diff_rows (l, r), rows_from_to (4, 4));
}

void
MidiNotesPatternTest::testOffNoteChanged ()
{
	/* The length changes while the on row stays the same: only the off cell
	 * must be repainted (is_on_equal ignores length, is_off_equal catches it). */
	NoteMaps l, r;
	l.add (4, 6, 4, 2.2, 60);
	r.add (4, 6, 4, 2.0, 60);
	assert_rows (diff_rows (l, r), rows_from_to (6, 6));
}

void
MidiNotesPatternTest::testMovedNote ()
{
	/* The note moves to another on/off row: both the old and new cells are
	 * affected, in both directions. */
	NoteMaps l, r;
	l.add (4, 6, 4, 2, 60);
	r.add (5, 7, 4, 2, 60);
	std::set<int> expected = rows_from_to (4, 4);
	expected.insert (5);
	expected.insert (6);
	expected.insert (7);
	assert_rows (diff_rows (l, r), expected);
}

void
MidiNotesPatternTest::testDisplayabilityChange ()
{
	/* Two notes on row 4 become one: the row itself must be repainted. */
	NoteMaps l, r;
	l.add_on (4, 4, 2, 60);
	l.add_on (4, 4, 2, 62);
	r.add_on (4, 4, 2, 60);
	assert_rows (diff_rows (l, r), rows_from_to (4, 4));
}

void
MidiNotesPatternTest::testBothNonDisplayable ()
{
	/* If both sides have several notes on the same row, the cells are not
	 * displayable and the diff does not ask for a repaint (current
	 * behaviour), even if the notes differ. */
	NoteMaps l, r;
	l.add_on (4, 4, 2, 60);
	l.add_on (4, 4, 2, 62);
	r.add_on (4, 4, 2, 60);
	r.add_on (4, 4, 2, 64);
	assert_rows (diff_rows (l, r), std::set<int> ());
}

void
MidiNotesPatternTest::testOffMeetsOn ()
{
	/* Whether the off/on pair on a row is contiguous changes its
	 * displayability, so the row must be repainted. */
	NoteMaps l, r;
	l.add_off (4, 0, 4, 55); // ends at 4
	l.add_on (4, 4, 2, 60);  // starts at 4 -> displayable
	r.add_off (4, 0, 3, 55); // ends at 3
	r.add_on (4, 4, 2, 60);  // starts at 4 -> not displayable
	assert_rows (diff_rows (l, r), rows_from_to (4, 4));
}

void
MidiNotesPatternTest::testOnHidesOff ()
{
	/* An on note can hide an off note on the same row, so a row that goes from
	 * off-only to on-only (or vice versa) must be repainted. */
	NoteMaps l, r;
	l.add_on (4, 4, 2, 60);
	r.add_off (4, 0, 4, 55); // ends at 4
	assert_rows (diff_rows (l, r), rows_from_to (4, 4));
}
