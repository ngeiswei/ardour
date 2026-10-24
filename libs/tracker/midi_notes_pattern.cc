/*
 * Copyright (C) 2015-2019 Nil Geisweiller <ngeiswei@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <algorithm>
#include <cmath>
#include <map>

#include "evoral/Note.h"
#include "evoral/midi_util.h"

#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "midi_notes_pattern.h"

using namespace ARDOUR;
using namespace Tracker;

//////////////////////
// MidiNotesPattern //
//////////////////////

MidiNotesPattern::MidiNotesPattern (TrackerContext& te,
                                    MidiRegionPtr region)
	: BasePattern (te, region)
	, ntracks (0)
	, nreqtracks (0)
	, _midi_region (region)
	, _midi_model (region->model ())
{
}

MidiNotesPattern&
MidiNotesPattern::operator= (const MidiNotesPattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	BasePattern::operator= (other);
	ntracks = other.ntracks;
	nreqtracks = other.nreqtracks;

	// Build this snapshot out of note clones.  Clones owned from the previous
	// snapshot are reused for every note whose relevant attributes did not
	// change, so a single edit only allocates for the notes it touched rather
	// than for every note in the region.
	std::map<Evoral::event_id_t, NotePtr> snapshot;

	// Clone other.track_to_notes into track_to_notes
	track_to_notes.clear ();
	track_to_notes.resize (other.track_to_notes.size ());
	std::vector<std::map<NotePtr, NotePtr> > to_clone (track_to_notes.size ());
	for (size_t i = 0; i < track_to_notes.size (); i++) {
		for (ARDOUR::MidiModel::Notes::const_iterator it = other.track_to_notes[i].begin (); it != other.track_to_notes[i].end (); ++it) {
			NotePtr clone = snapshot_note (*it);
			track_to_notes[i].insert (clone);
			to_clone[i][*it] = clone;
			snapshot[(*it)->id ()] = clone;
		}
	}

	// Install the new clone set, dropping the clones that are no longer used
	_snapshot_notes.swap (snapshot);

	// Copy other.on_notes using cloned notes
	on_notes.clear ();
	on_notes.resize (other.on_notes.size ());
	for (size_t i = 0; i < on_notes.size (); i++) {
		for (RowToNotes::const_iterator it = other.on_notes[i].begin (); it != other.on_notes[i].end (); ++it) {
			on_notes[i].insert (std::make_pair (it->first, to_clone[i][it->second]));
		}
	}

	// Copy other.off_notes using cloned notes
	off_notes.clear ();
	off_notes.resize (other.off_notes.size ());
	for (size_t i = 0; i < off_notes.size (); i++) {
		for (RowToNotes::const_iterator it = other.off_notes[i].begin (); it != other.off_notes[i].end (); ++it) {
			off_notes[i].insert (std::make_pair (it->first, to_clone[i][it->second]));
		}
	}

	_midi_model = other._midi_model;

	return *this;
}

NotePtr
MidiNotesPattern::clone_note (NotePtr note) const
{
	// Recreate the note instead of copying it to avoid calling next_event_id
	return NotePtr (new NoteType (note->channel (), note->time (), note->length (), note->note (), note->velocity ()));
}

NotePtr
MidiNotesPattern::snapshot_note (NotePtr note) const
{
	// is_on_equal () covers channel, time, note and velocity and
	// is_off_equal () covers the end time (i.e. the length), which is
	// exactly what clone_note () copies: when both hold, the clone already
	// owned from the previous snapshot is indistinguishable from a fresh one.
	std::map<Evoral::event_id_t, NotePtr>::const_iterator it = _snapshot_notes.find (note->id ());
	if (it != _snapshot_notes.end () &&
	    TrackerUtils::is_on_equal (it->second, note) &&
	    TrackerUtils::is_off_equal (it->second, note)) {
		return it->second;
	}
	return clone_note (note);
}

bool
MidiNotesPattern::is_row_displayable (const RowToNotes& on_notes, const RowToNotes& off_notes, int row)
{
	size_t off_notes_count = off_notes.count (row);
	size_t on_notes_count = on_notes.count (row);
	RowToNotes::const_iterator i_off = off_notes.find (row);
	RowToNotes::const_iterator i_on = on_notes.find (row);
	return off_notes_count <= 1 && on_notes_count <= 1
		&& (off_notes_count != 1 || on_notes_count != 1
		    || TrackerUtils::off_meets_on (i_off->second, i_on->second));
}

void
MidiNotesPattern::rows_diff (const RowToNotes& l_on, const RowToNotes& l_off,
                             const RowToNotes& r_on, const RowToNotes& r_off,
                             std::set<int>& rd)
{
	// Compare left on notes with right on notes
	for (RowToNotes::const_iterator it = l_on.begin (); it != l_on.end ();) {
		int row = it->first;

		// First, look at the difference in displayability
		bool is_cell_displayable = is_row_displayable (l_on, l_off, row);
		bool cell_diff = is_cell_displayable != is_row_displayable (r_on, r_off, row);
		if (cell_diff) {
			rd.insert (row);
		}
		if (!is_cell_displayable) {
			// It means there are more than one note, jump to the next row
			it = l_on.upper_bound (row);
			continue;
		}
		if (cell_diff) {
			++it;
			continue;
		}

		// Second, see if that single on note is present in other, and if it is, check if it is different
		RowToNotes::const_iterator rit = r_on.find (row);
		if (rit == r_on.end () || !TrackerUtils::is_on_equal (it->second, rit->second)) {
			rd.insert (row);
		}

		++it;
	}

	// Compare left off notes with right off notes
	for (RowToNotes::const_iterator it = l_off.begin (); it != l_off.end ();) {
		int row = it->first;

		// First, look at the difference in displayability
		bool is_cell_displayable = is_row_displayable (l_on, l_off, row);
		bool cell_diff = is_cell_displayable != is_row_displayable (r_on, r_off, row);
		if (cell_diff) {
			rd.insert (row);
		}
		if (!is_cell_displayable) {
			// It means there are more than one note, jump to the next row
			it = l_off.upper_bound (row);
			continue;
		}
		if (cell_diff) {
			++it;
			continue;
		}

		// Second, see if that single off note is present in other, and if it
		// is, check if it is different, and it is not make sure no on note is
		// present as on note can hide off note.
		RowToNotes::const_iterator rit = r_off.find (row);
		if (rit == r_off.end ()
		    || !TrackerUtils::is_off_equal (it->second, rit->second)
		    || (r_on.find (row) == r_on.end ()
		        && l_on.find (row) != l_on.end ())
		    || (l_on.find (row) == l_on.end ()
		        && r_on.find (row) != r_on.end ())) {
			rd.insert (row);
		}

		++it;
	}
}

void
MidiNotesPattern::rows_diff (int cgi, const MidiNotesPattern& mnp_l, const MidiNotesPattern& mnp_r, std::set<int>& rd)
{
	rows_diff (mnp_l.on_notes[cgi], mnp_l.off_notes[cgi],
	           mnp_r.on_notes[cgi], mnp_r.off_notes[cgi], rd);
}

MidiNotesPatternPhenomenalDiff
MidiNotesPattern::phenomenal_diff (const MidiNotesPattern& prev) const
{
	MidiNotesPatternPhenomenalDiff diff;
	if (!prev.enabled && !enabled) {
		return diff;
	}

	diff.full = prev.enabled != enabled
		|| prev.ntracks != ntracks
		|| prev.nreqtracks != nreqtracks;
	if (diff.full) {
		return diff;
	}

	assert (on_notes.size () == prev.on_notes.size ());
	assert (on_notes.size () == off_notes.size ());
	assert (off_notes.size () == prev.off_notes.size ());
	for (size_t cgi = 0; cgi != on_notes.size (); cgi++) {
		std::set<int> rows;
		rows_diff (cgi, *this, prev, rows);
		rows_diff (cgi, prev, *this, rows);
		if (!rows.empty ()) {
			// Partial diff: empty () now means "nothing to do".
			diff.cgi2rows_diff[cgi].full = false;
			diff.cgi2rows_diff[cgi].rows = rows;
		}
	}

	return diff;
}

void
MidiNotesPattern::update ()
{
	// set_row_range ();               // NEXT.4: maybe redundant
	update_track_to_notes ();
	update_row_to_notes ();
}

void
MidiNotesPattern::update_track_to_notes ()
{
	// Select visible notes within the region, and sort them with the
	// StrictNotes order so that simulatenous notes can be dispatched according
	// to some defined order.
	const MidiModel::Notes& notes = _midi_model->notes ();
	StrictNotes strict_notes;
	for (MidiModel::Notes::const_iterator it = _midi_model->note_lower_bound (start_beats);
	     it != notes.end () && (*it)->time () < end_beats; ++it)
		strict_notes.insert (*it);

	// Index the notes in scope by id.  Testing membership by scanning
	// strict_notes (as find_eq_id () does) made this quadratic in the number
	// of notes; the index keeps it at N log N.
	std::set<Evoral::event_id_t> live_ids;
	for (StrictNotes::const_iterator it = strict_notes.begin (); it != strict_notes.end (); ++it) {
		live_ids.insert ((*it)->id ());
	}

	// Index the notes already placed by id, so that the track a note used to
	// belong to is found directly instead of by scanning every track.
	std::map<Evoral::event_id_t, int> id_to_cgi;
	for (size_t cgi = 0; cgi < track_to_notes.size (); ++cgi) {
		for (MidiModel::Notes::const_iterator it = track_to_notes[cgi].begin (); it != track_to_notes[cgi].end (); ++it) {
			id_to_cgi[(*it)->id ()] = cgi;
		}
	}

	// Remove missing notes
	for (size_t cgi = 0; cgi < track_to_notes.size (); ++cgi) {
		MidiModel::Notes& track_notes = track_to_notes[cgi];
		MidiModel::Notes::iterator track_notes_it = track_notes.begin ();
		for (; track_notes_it != track_notes.end ();) {
			if (TrackerUtils::is_in ((*track_notes_it)->id (), live_ids)) {
				++track_notes_it;
			} else {
				id_to_cgi.erase ((*track_notes_it)->id ());
				track_notes.erase (track_notes_it++);
			}
		}
	}

	// Add new notes and move existing notes
	for (StrictNotes::const_iterator it = strict_notes.begin ();
	     it != strict_notes.end (); ++it) {

		std::map<Evoral::event_id_t, int>::const_iterator id_cgi_it = id_to_cgi.find ((*it)->id ());
		int cgi = id_cgi_it == id_to_cgi.end () ? INVALID_CGI : id_cgi_it->second;
		int freetrack = -1;		// index of the first free track
		if (-1 < cgi) {
			// The note is already in track_to_notes, remove it and check if it
			// can be re-inserted in the same track.
			erase_eq_id (cgi, *it);
			if (is_free (cgi, *it)) {
				freetrack = cgi;
			}
		}
		// Find the first available track
		if (freetrack < 0) {
			freetrack = find_free_track (*it);
		}
		// No free track found, create a new one.
		if (freetrack < 0) {
			freetrack = track_to_notes.size ();
			track_to_notes.push_back (MidiModel::Notes ());
		}
		// Insert the note in the first free track
		track_to_notes[freetrack].insert (*it);
		id_to_cgi[(*it)->id ()] = freetrack;
	}

	// Update nreqtracks and ntracks
	nreqtracks = track_to_notes.size ();
	ntracks = std::max (nreqtracks, ntracks);
}

void
MidiNotesPattern::update_row_to_notes ()
{
	// Clear on_notes and off_notes
	on_notes.clear ();
	on_notes.resize (ntracks);
	off_notes.clear ();
	off_notes.resize (ntracks);

	// Unless the rows per beat has just changed, temporarily save the state of
	// _on_note_to_row and _off_beats_to_row to keep track of which row each
	// note used to belong to.
	if (refresh) {
		_prev_on_note_to_row.clear ();
		_prev_off_beats_to_row.clear ();
		refresh = false;
	} else {
		_prev_on_note_to_row = _on_note_to_row;
		_prev_off_beats_to_row = _off_beats_to_row;
	}

	// Clear _on_note_to_row and _off_beats_to_row
	_on_note_to_row.clear ();
	_on_note_to_row.resize (ntracks);
	_off_beats_to_row.clear ();
	_off_beats_to_row.resize (ntracks);

	// Fill on_notes and off_notes
	for (uint16_t cgi = 0; cgi < nreqtracks; ++cgi) {
		update_row_to_notes_at_track (cgi);
	}
}

void
MidiNotesPattern::update_row_to_notes_at_track (uint16_t cgi)
{
	bool prev_off_meets_on = false;
	NotePtr prev_off_note = nullptr;
	for (MidiModel::Notes::iterator inote = track_to_notes[cgi].begin (); inote != track_to_notes[cgi].end ();) {
		NotePtr note = *inote;
		int on_row = find_nearest_on_row (cgi, inote);
		insert_on_note (cgi, on_row, note);
		// If previous off note meets current on note, then set it to that row
		if (prev_off_meets_on) {
			insert_off_note (cgi, on_row, prev_off_note);
		}
		MidiModel::Notes::iterator prev_inote = inote;
		prev_off_note = *prev_inote;
		++inote;
		// Only display off notes occuring within the region
		if (note_ends_within_region (note)) {
			prev_off_meets_on = is_valid (cgi, inote) ? TrackerUtils::off_meets_on (note, *inote) : false;
			if (not prev_off_meets_on) {
				int off_row = std::max (find_nearest_off_row (cgi, prev_inote), on_row);
				insert_off_note (cgi, off_row, note);
			}
		}
	}
}

bool
MidiNotesPattern::note_ends_within_region (NotePtr note) const
{
	return _midi_region->source_beats_to_absolute_beats (note->end_time ()) < global_end_beats;
}

bool
MidiNotesPattern::is_valid (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	return inote != track_to_notes[cgi].end ();
}

MidiModel::Notes::iterator
MidiNotesPattern::next_inote (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	return is_valid(cgi, inote) ? std::next(inote) : inote;
}

bool
MidiNotesPattern::on_row_gte (uint16_t cgi, MidiModel::Notes::iterator inote, int row) const
{
	if (is_valid (cgi, inote)) {
		auto it = _on_note_to_row[cgi].find (*inote);
		if (it != _on_note_to_row[cgi].end()) {
			return row_gte (row, it->second);
		} else {
			return true;
		}
	}
	return false;
}

int
MidiNotesPattern::row_at_note_time (uint16_t cgi, MidiModel::Notes::iterator inote, bool on, RowSelector sel) const
{
	if (!is_valid (cgi, inote)) {
		return INVALID_ROW;
	}

	const Temporal::Beats when = on ? (*inote)->time () : (*inote)->end_time ();
	const Temporal::Beats beats = _midi_region->source_beats_to_absolute_beats (when);

	int row;
	switch (sel) {
	case RowSelector::CENTERED: row = row_at_beats (beats); break;
	case RowSelector::PREVIOUS: row = row_at_beats_max_delay (beats); break;
	case RowSelector::NEXT:     row = row_at_beats_min_delay (beats); break;
	default:                    return INVALID_ROW;
	}

	return on ? row : (on_row_gte (cgi, inote, row) ? row : INVALID_ROW);
}

int
MidiNotesPattern::centered_on_row (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	return row_at_note_time (cgi, inote, true, RowSelector::CENTERED);
}

int
MidiNotesPattern::centered_off_row (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	return row_at_note_time (cgi, inote, false, RowSelector::CENTERED);
}

int
MidiNotesPattern::previous_on_row (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	return row_at_note_time (cgi, inote, true, RowSelector::PREVIOUS);
}

int
MidiNotesPattern::previous_off_row (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	return row_at_note_time (cgi, inote, false, RowSelector::PREVIOUS);
}

int
MidiNotesPattern::next_on_row (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	return row_at_note_time (cgi, inote, true, RowSelector::NEXT);
}

int
MidiNotesPattern::next_off_row (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	return row_at_note_time (cgi, inote, false, RowSelector::NEXT);
}

bool
MidiNotesPattern::is_on_row_available (uint16_t cgi, int row, MidiModel::Notes::iterator inote) const
{
	// Get off and on note counts at row
	size_t on_count_at_row = on_notes[cgi].count (row);
	size_t off_count_at_row = off_notes[cgi].count (row);

	// Grab off note at row, if it exists
	RowToNotes::const_iterator off_it = off_notes[cgi].find (row);

	// Get next note, if it exists
	MidiModel::Notes::iterator ninote = next_inote (cgi, inote);

	// The row is is available if it is empty or contains exactly one off note
	// that ends at the same time as this note begins, and its off note and the
	// next on note is not in this row or below it by default.
	bool is_available = (on_count_at_row == 0 &&
	                     (off_count_at_row == 0 ||
	                      (off_count_at_row == 1 &&
	                       TrackerUtils::off_meets_on (off_it->second, *inote))) &&
	                     (row_lt (row, centered_off_row (cgi, inote)) &&
	                      row_lt (row, centered_on_row (cgi, ninote))));
	return is_available;
}

bool
MidiNotesPattern::is_off_row_available (uint16_t cgi, int row, MidiModel::Notes::iterator inote) const
{
	// Get on note counts at row
	size_t on_count_at_row = on_notes[cgi].count (row);

	// Get next note, if it exists
	MidiModel::Notes::iterator ninote = next_inote (cgi, inote);

	// Get next next note, if it exists
	MidiModel::Notes::iterator nninote = next_inote (cgi, ninote);

	// The row is available if there is no on note, or if there is one, and only
	// one, the off note exactly meets the on note, and the next note is not in
	// this row or below it by default.
	bool is_available = (on_count_at_row == 0 &&
	                     (row_lt (row, centered_on_row (cgi, ninote)) ||
	                      TrackerUtils::off_meets_on (*inote, *ninote)) &&
	                     row_lt (row, centered_on_row (cgi, nninote)));
	return is_available;
}

int
MidiNotesPattern::ranked_row_suggestion (int cent_row, int prev_row, int next_row, int prev_note_row, int rank) const
{
	// Default ranking
	int ranked_row[3];
	ranked_row[0] = cent_row;
	ranked_row[1] = prev_row < cent_row ? prev_row : INVALID_ROW;
	ranked_row[2] = cent_row < next_row ? next_row : INVALID_ROW;
	// Overwrite ranking according to the row the note used to belong to
	if (prev_note_row == prev_row && prev_row < cent_row) {
		ranked_row[0] = prev_row;
		ranked_row[1] = cent_row;
	} else if (prev_note_row == next_row && cent_row < next_row) {
		ranked_row[0] = next_row;
		ranked_row[1] = cent_row;
		ranked_row[2] = prev_row < cent_row ? prev_row : INVALID_ROW;
	}
	// Select row according to its ranking
	repair_ranked_row (ranked_row);
	return rank < 3 ? ranked_row[rank] : INVALID_ROW;
}

int
MidiNotesPattern::on_row_suggestion (uint16_t cgi, MidiModel::Notes::iterator inote, int rank) const
{
	if (!is_valid (cgi, inote)) {
		return INVALID_ROW;
	}

	// Row the note used to belong to, if any
	int prev_note_row = INVALID_ROW;
	if (cgi < _prev_on_note_to_row.size ()) {
		NoteToRow::const_iterator it = _prev_on_note_to_row[cgi].find (*inote);
		if (it != _prev_on_note_to_row[cgi].end ()) {
			prev_note_row = it->second;
		}
	}

	return ranked_row_suggestion (centered_on_row (cgi, inote), previous_on_row (cgi, inote), next_on_row (cgi, inote), prev_note_row, rank);
}

int
MidiNotesPattern::off_row_suggestion (uint16_t cgi, MidiModel::Notes::iterator inote, int rank) const
{
	if (!is_valid (cgi, inote)) {
		return INVALID_ROW;
	}

	// Row the off note used to belong to, if any
	int prev_note_row = INVALID_ROW;
	if (cgi < _prev_off_beats_to_row.size ()) {
		BeatsToRow::const_iterator it = _prev_off_beats_to_row[cgi].find ((*inote)->end_time ());
		if (it != _prev_off_beats_to_row[cgi].end ()) {
			prev_note_row = it->second;
		}
	}

	return ranked_row_suggestion (centered_off_row (cgi, inote), previous_off_row (cgi, inote), next_off_row (cgi, inote), prev_note_row, rank);
}

int
MidiNotesPattern::find_nearest_on_row (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	int rank = 0;
	int row = on_row_suggestion (cgi, inote, rank);
	int default_row = row;
	do {
		if (is_on_row_available (cgi, row, inote)) {
			return row;
		}
		rank++;
		row = on_row_suggestion (cgi, inote, rank);
	} while (rank < 3 && 0 <= row);
	return default_row;
}

int
MidiNotesPattern::find_nearest_off_row (uint16_t cgi, MidiModel::Notes::iterator inote) const
{
	int rank = 0;
	int row = off_row_suggestion (cgi, inote, rank);
	int default_row = row;
	do {
		if (is_off_row_available (cgi, row, inote)) {
			return row;
		}
		rank++;
		row = off_row_suggestion (cgi, inote, rank);
	} while (rank < 3 && 0 <= row);
	return default_row;
}

void
MidiNotesPattern::set_ntracks (uint16_t n)
{
	if (nreqtracks <= n) {
		ntracks = n;
	}
}

void
MidiNotesPattern::inc_ntracks ()
{
	ntracks++;
}

void
MidiNotesPattern::dec_ntracks ()
{
	if (nreqtracks < ntracks) {
		ntracks--;
	}
}

NotePtr
MidiNotesPattern::find_prev_note (const RowToNotes& r2n, int row) const
{
	RowToNotes::const_reverse_iterator rit =
		std::reverse_iterator<RowToNotes::const_iterator> (r2n.lower_bound (row));
	while (rit != r2n.rend () && rit->first == row) { ++rit; };
	return rit != r2n.rend () ? lattest (r2n.equal_range (rit->first)) : NotePtr ();
}

NotePtr
MidiNotesPattern::find_next_note (const RowToNotes& r2n, int row) const
{
	RowToNotes::const_iterator it = r2n.upper_bound (row);
	while (it != r2n.end () && it->first == row) { ++it; };
	return it != r2n.end () ? earliest (r2n.equal_range (it->first)) : NotePtr ();
}

NotePtr
MidiNotesPattern::find_prev_on (int row, int cgi) const
{
	return find_prev_note (on_notes[cgi], row);
}

NotePtr
MidiNotesPattern::find_next_on (int row, int cgi) const
{
	return find_next_note (on_notes[cgi], row);
}

NotePtr
MidiNotesPattern::find_prev_off (int row, int cgi) const
{
	return find_prev_note (off_notes[cgi], row);
}

NotePtr
MidiNotesPattern::find_next_off (int row, int cgi) const
{
	return find_next_note (off_notes[cgi], row);
}

Temporal::Beats
MidiNotesPattern::next_on_beats (int row, int cgi) const
{
	NotePtr next_note = find_next_on (row, cgi);
	return next_note ? next_note->time () : end_beats;
}

Temporal::Beats
MidiNotesPattern::next_off_beats (int row, int cgi) const
{
	NotePtr next_note = find_next_off (row, cgi);
	return next_note ? next_note->end_time () : end_beats;
}

bool
MidiNotesPattern::is_displayable (int row, int cgi) const
{
	return is_row_displayable (on_notes[cgi], off_notes[cgi], row);
}

void
MidiNotesPattern::add (int cgi, NotePtr note)
{
	// Resize track_to_notes if necessary
	if (nreqtracks <= cgi) {
		nreqtracks = cgi + 1;
		track_to_notes.resize (nreqtracks);
	}

	// Insert the note at cgi
	track_to_notes[cgi].insert (note);
}

Temporal::BBT_Time
MidiNotesPattern::on_note_bbt (NotePtr note) const
{
	Temporal::Beats note_time = _midi_region->source_beats_to_absolute_beats (note->time ());
	return Temporal::TempoMap::use()->bbt_at (note_time);
}

Temporal::BBT_Time
MidiNotesPattern::off_note_bbt (NotePtr note) const
{
	Temporal::Beats end_note_time = _midi_region->source_beats_to_absolute_beats (note->end_time ());
	return Temporal::TempoMap::use()->bbt_at (end_note_time);
}

std::string
MidiNotesPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "MidiNotesPattern[" << this << "]";
	return ss.str ();
}

std::string
MidiNotesPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePattern::to_string (indent) << std::endl;

	std::string header = indent + self_to_string () + " ";
	ss << header << "ntracks = " << ntracks << std::endl;
	ss << header << "nreqtracks = " << nreqtracks << std::endl;
	for (size_t i = 0; i < track_to_notes.size (); i++) {
		const ARDOUR::MidiModel::Notes& notes = track_to_notes[i];
		ss << header << "track_to_notes[" << i << "]:" << std::endl;
		for (ARDOUR::MidiModel::Notes::const_iterator it = notes.begin (); it != notes.end (); ++it) {
			ss << indent + "  " << "[" << *it << "] " << **it << std::endl;
		}
	}
	for (size_t i = 0; i < on_notes.size (); i++) {
		ss << header << "on_notes[" << i << "]:" << std::endl;
		for (RowToNotes::const_iterator it = on_notes[i].begin (); it != on_notes[i].end (); ++it) {
			ss << indent + "  " << "row[" << it->first << "] = " << "[" << it->second << "] " << *it->second << std::endl;
		}
	}
	for (size_t i = 0; i < off_notes.size (); i++) {
		ss << header << "off_notes[" << i << "]:" << std::endl;
		for (RowToNotes::const_iterator it = off_notes[i].begin (); it != off_notes[i].end (); ++it) {
			ss << indent + "  " << "row[" << it->first << "] = " << "[" << it->second << "] " << *it->second << std::endl;
		}
	}
	ss << header << "_midi_region = " << _midi_region << std::endl;
	ss << header << "_midi_model = " << _midi_model;

	return ss.str ();
}

NotePtr
MidiNotesPattern::earliest (const RowToNotesRange& rng) const
{
	if (rng.first == rng.second) {
		return NotePtr ();
	}
	return std::min_element (rng.first, rng.second,
	                         [] (const RowToNotes::value_type& a, const RowToNotes::value_type& b) {
		                         return a.second->time () < b.second->time ();
	                         })->second;
}

NotePtr
MidiNotesPattern::lattest (const RowToNotesRange& rng) const
{
	if (rng.first == rng.second) {
		return NotePtr ();
	}
	return std::max_element (rng.first, rng.second,
	                         [] (const RowToNotes::value_type& a, const RowToNotes::value_type& b) {
		                         return a.second->time () < b.second->time ();
	                         })->second;
}

int
MidiNotesPattern::find_eq_id (NotePtr note) const
{
	for (int i = 0; i < (int)track_to_notes.size (); i++) {
		if (find_eq_id (i, note) != track_to_notes[i].end ()) {
			return i;
		}
	}
	return INVALID_CGI;
}

MidiModel::Notes::const_iterator
MidiNotesPattern::find_eq_id (int cgi, NotePtr note) const
{
	return find_eq_id (track_to_notes[cgi], note);
}

MidiModel::Notes::iterator
MidiNotesPattern::find_eq_id (int cgi, NotePtr note)
{
	return find_eq_id (track_to_notes[cgi], note);
}

void
MidiNotesPattern::erase_eq_id (int cgi, NotePtr note)
{
	erase_eq_id (track_to_notes[cgi], note);
}

void
MidiNotesPattern::erase_eq_id (MidiModel::Notes& notes, NotePtr note)
{
	notes.erase (find_eq_id (notes, note));
}

void
MidiNotesPattern::insert_on_note (int cgi, int row, NotePtr on_note)
{
	on_notes[cgi].insert (RowToNotes::value_type (row, on_note));
	_on_note_to_row[cgi][on_note] = row;
}

void
MidiNotesPattern::insert_off_note (int cgi, int row, NotePtr off_note)
{
	off_notes[cgi].insert (RowToNotes::value_type (row, off_note));
	_off_beats_to_row[cgi][off_note->end_time()] = row;
}

bool
MidiNotesPattern::is_free (int cgi, NotePtr note) const
{
	const MidiModel::Notes& notes = track_to_notes[cgi];
	MidiModel::Notes::iterator it = notes.begin ();
	for (; it != notes.end (); ++it) {
		if (overlap (*it, note)) {
			return false;
		}
	}
	return true;
}

int
MidiNotesPattern::find_free_track (NotePtr note) const
{
	for (int i = 0; i < (int)track_to_notes.size (); i++) {
		if (is_free (i, note)) {
			return i;
		}
	}
	return INVALID_CGI;
}

bool
MidiNotesPattern::overlap (NotePtr a, NotePtr b)
{
	Temporal::Beats sa = a->time ();
	Temporal::Beats ea  = a->end_time ();
	Temporal::Beats sb = b->time ();
	Temporal::Beats eb = b->end_time ();

	return (((sb > sa) && (eb <= ea)) ||
	        ((eb > sa) && (eb <= ea)) ||
	        ((sb > sa) && (sb < ea)) ||
	        ((sa >= sb) && (sa <= eb) && (ea <= eb)));
}
