#include <map>
#include <random>
#include <set>
#include <vector>

#include "evoral/Note.h"
#include "evoral/types.h"
#include "temporal/beats.h"

#include "ardour/midi_model.h"

#include "midi_notes_pattern.h"

#include "NoteDiffTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (NoteDiffTest);

using namespace Tracker;

namespace {

typedef ARDOUR::MidiModel::NoteDiffCommand::NoteList NoteList;
typedef ARDOUR::MidiModel::NoteDiffCommand::ChangeList ChangeList;

NotePtr
mknote (Evoral::event_id_t id, int row, double length = 0.5)
{
	NotePtr n (new NoteType (0,
	                         Temporal::Beats::from_double (row),
	                         Temporal::Beats::from_double (length),
	                         60, 100));
	n->set_id (id);
	return n;
}

void
change_time (ChangeList& changes, NotePtr n, int row)
{
	n->set_time (Temporal::Beats::from_double (row));
	ARDOUR::MidiModel::NoteDiffCommand::NoteChange ch;
	ch.property = ARDOUR::MidiModel::NoteDiffCommand::StartTime;
	ch.note = n;
	ch.note_id = n->id ();
	changes.push_back (ch);
}

void
change_length (ChangeList& changes, NotePtr n, double length)
{
	n->set_length (Temporal::Beats::from_double (length));
	ARDOUR::MidiModel::NoteDiffCommand::NoteChange ch;
	ch.property = ARDOUR::MidiModel::NoteDiffCommand::Length;
	ch.note = n;
	ch.note_id = n->id ();
	changes.push_back (ch);
}

bool
overlaps (NotePtr a, NotePtr b)
{
	Temporal::Beats sa = a->time ();
	Temporal::Beats ea = a->end_time ();
	Temporal::Beats sb = b->time ();
	Temporal::Beats eb = b->end_time ();
	return (((sb > sa) && (eb <= ea)) ||
	        ((eb > sa) && (eb <= ea)) ||
	        ((sb > sa) && (sb < ea)) ||
	        ((sa >= sb) && (sa <= eb) && (ea <= eb)));
}

/* Check that the packing is valid and covers exactly the expected notes. */
void
check_packing (const std::vector<ARDOUR::MidiModel::Notes>& ttn,
               uint16_t nreq,
               const std::map<Evoral::event_id_t, NotePtr>& expected)
{
	std::set<Evoral::event_id_t> present;
	for (size_t cgi = 0; cgi < ttn.size (); ++cgi) {
		for (ARDOUR::MidiModel::Notes::const_iterator a = ttn[cgi].begin (); a != ttn[cgi].end (); ++a) {
			CPPUNIT_ASSERT (present.insert ((*a)->id ()).second);
			ARDOUR::MidiModel::Notes::const_iterator b = a;
			for (++b; b != ttn[cgi].end (); ++b) {
				CPPUNIT_ASSERT (!overlaps (*a, *b));
			}
		}
	}
	CPPUNIT_ASSERT_EQUAL (expected.size (), present.size ());
	for (std::map<Evoral::event_id_t, NotePtr>::const_iterator it = expected.begin (); it != expected.end (); ++it) {
		CPPUNIT_ASSERT (present.count (it->first) > 0);
	}
	CPPUNIT_ASSERT_EQUAL ((size_t) nreq, ttn.size ());
}

} // anonymous namespace

void
NoteDiffTest::testApplyNoteDiffInvariants ()
{
	std::mt19937 rng (13579);

	std::vector<ARDOUR::MidiModel::Notes> ttn;
	uint16_t nreq = 0;
	const Temporal::Beats start (Temporal::Beats::from_double (0));
	const Temporal::Beats end (Temporal::Beats::from_double (16));

	std::map<Evoral::event_id_t, NotePtr> model_notes;
	Evoral::event_id_t next_id = 1;

	for (int step = 0; step < 5000; ++step) {
		int op = rng () % 4;

		if (model_notes.empty () || op == 0) {
			// add a note at a random row
			NotePtr n = mknote (next_id++, rng () % 16, 0.25 + (rng () % 8) * 0.25);
			model_notes[n->id ()] = n;
			NoteList added;
			added.push_back (n);
			MidiNotesPattern::apply_note_diff (ttn, nreq, start, end, added, NoteList (), ChangeList ());
		} else if (op == 1) {
			// remove a random note
			std::map<Evoral::event_id_t, NotePtr>::iterator it = model_notes.begin ();
			std::advance (it, rng () % model_notes.size ());
			NotePtr n = it->second;
			model_notes.erase (it);
			NoteList removed;
			removed.push_back (n);
			MidiNotesPattern::apply_note_diff (ttn, nreq, start, end, NoteList (), removed, ChangeList ());
		} else if (op == 2) {
			// change a random note's start time (may move it out of range)
			std::map<Evoral::event_id_t, NotePtr>::iterator it = model_notes.begin ();
			std::advance (it, rng () % model_notes.size ());
			NotePtr n = it->second;
			int row = rng () % 20;
			ChangeList changes;
			change_time (changes, n, row);
			MidiNotesPattern::apply_note_diff (ttn, nreq, start, end, NoteList (), NoteList (), changes);
			if (row < 0 || row >= 16) {
				model_notes.erase (n->id ());
			}
		} else {
			// change a random note's length
			std::map<Evoral::event_id_t, NotePtr>::iterator it = model_notes.begin ();
			std::advance (it, rng () % model_notes.size ());
			NotePtr n = it->second;
			ChangeList changes;
			change_length (changes, n, 0.25 + (rng () % 16) * 0.25);
			MidiNotesPattern::apply_note_diff (ttn, nreq, start, end, NoteList (), NoteList (), changes);
		}

		check_packing (ttn, nreq, model_notes);
	}
}

void
NoteDiffTest::testApplyNoteDiffScenario ()
{
	std::vector<ARDOUR::MidiModel::Notes> ttn;
	uint16_t nreq = 0;
	const Temporal::Beats start (Temporal::Beats::from_double (0));
	const Temporal::Beats end (Temporal::Beats::from_double (16));

	// Two non-overlapping notes end up on the same sub-track
	NotePtr a = mknote (1, 0, 0.5);
	NotePtr b = mknote (2, 4, 0.5);
	{
		NoteList added;
		added.push_back (a);
		added.push_back (b);
		MidiNotesPattern::apply_note_diff (ttn, nreq, start, end, added, NoteList (), ChangeList ());
	}
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, ttn.size ());

	// Stretch a so that it overlaps b: it must move to another sub-track
	{
		ChangeList changes;
		change_length (changes, a, 8.0);
		MidiNotesPattern::apply_note_diff (ttn, nreq, start, end, NoteList (), NoteList (), changes);
	}
	CPPUNIT_ASSERT_EQUAL ((size_t) 2, ttn.size ());

	std::map<Evoral::event_id_t, NotePtr> expected;
	expected[a->id ()] = a;
	expected[b->id ()] = b;
	check_packing (ttn, nreq, expected);

	// Remove a: b should be found on its own
	{
		NoteList removed;
		removed.push_back (a);
		MidiNotesPattern::apply_note_diff (ttn, nreq, start, end, NoteList (), removed, ChangeList ());
		expected.erase (a->id ());
	}
	check_packing (ttn, nreq, expected);
}
