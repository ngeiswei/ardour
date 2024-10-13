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

#include <cmath>
#include <map>

#include "evoral/Note.h"
#include "evoral/midi_util.h"

#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "midi_notes_pattern.h"

using namespace ARDOUR;
using namespace Tracker;

//////////////////////
// MidiNotesPattern //
//////////////////////

MidiNotesPattern::MidiNotesPattern (TrackerEditor& te,
                                    MidiRegionPtr region)
	: BasePattern (te, region)
	, ntracks (0)
	, nreqtracks (0)
	, _midi_model (region->midi_source (0)->model ())
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

	// Clone other.track_to_notes into track_to_notes
	track_to_notes.clear ();
	track_to_notes.resize (other.track_to_notes.size ());
	std::vector<std::map<NotePtr, NotePtr> > to_clone (track_to_notes.size ());
	for (size_t i = 0; i < track_to_notes.size (); i++) {
		for (ARDOUR::MidiModel::Notes::const_iterator it = other.track_to_notes[i].begin (); it != other.track_to_notes[i].end (); ++it) {
			NotePtr clone = clone_note (*it);
			track_to_notes[i].insert (clone);
			to_clone[i][*it] = clone;
		}
	}

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

void
MidiNotesPattern::rows_diff (int cgi, const MidiNotesPattern& mnp_l, const MidiNotesPattern& mnp_r, std::set<int>& rd)
{
	// Compare left on notes with right on notes
	for (RowToNotes::const_iterator it = mnp_l.on_notes[cgi].begin (); it != mnp_l.on_notes[cgi].end ();) {
		int row = it->first;

		// First, look at the difference in displayability
		bool is_cell_displayable = mnp_l.is_displayable (row, cgi);
		bool cell_diff = is_cell_displayable != mnp_r.is_displayable (row, cgi);
		if (cell_diff) {
			rd.insert (row);
		}
		if (!is_cell_displayable) {
			// It means there are more than one note, jump to the next row
			it = mnp_l.on_notes[cgi].upper_bound (row);
			continue;
		}
		if (cell_diff) {
			++it;
			continue;
		}

		// Second, see if that single on note is present in other, and if it is, check if it is different
		RowToNotes::const_iterator rit = mnp_r.on_notes[cgi].find (row);
		if (rit == mnp_r.on_notes[cgi].end () || !TrackerUtils::is_on_equal (it->second, rit->second)) {
			rd.insert (row);
		}

		++it;
	}

	// Compare left off notes with right off notes
	for (RowToNotes::const_iterator it = mnp_l.off_notes[cgi].begin (); it != mnp_l.off_notes[cgi].end ();) {
		int row = it->first;

		// First, look at the difference in displayability
		bool is_cell_displayable = mnp_l.is_displayable (row, cgi);
		bool cell_diff = is_cell_displayable != mnp_r.is_displayable (row, cgi);
		if (cell_diff) {
			rd.insert (row);
		} if (!is_cell_displayable) {
			// It means there are more than one note, jump to the next row
			it = mnp_l.off_notes[cgi].upper_bound (row);
			continue;
		}
		if (cell_diff) {
			++it;
			continue;
		}

		// Second, see if that single off note is present in other, and if it
		// is, check if it is different, and it is not make sure no on note is
		// present as on note can hide off note.
		RowToNotes::const_iterator rit = mnp_r.off_notes[cgi].find (row);
		if (rit == mnp_r.off_notes[cgi].end ()
		    || !TrackerUtils::is_off_equal (it->second, rit->second)
		    || (mnp_r.on_notes[cgi].find (row) == mnp_r.on_notes[cgi].end ()
		        && mnp_l.on_notes[cgi].find (row) != mnp_l.on_notes[cgi].end ())
		    || (mnp_l.on_notes[cgi].find (row) == mnp_l.on_notes[cgi].end ()
		        && mnp_r.on_notes[cgi].find (row) != mnp_r.on_notes[cgi].end ())) {
			rd.insert (row);
		}

		++it;
	}
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
			diff.cgi2rows_diff[cgi].rows = rows;
		}
	}

	return diff;
}

void
MidiNotesPattern::update ()
{
	set_row_range ();
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
	MidiModel::StrictNotes strict_notes;
	Temporal::Beats end_time = start_beats + Temporal::timepos_t (length_sample).beats ();
	for (MidiModel::Notes::const_iterator it = _midi_model->note_lower_bound (start_beats);
	     it != notes.end () && (*it)->time () < end_time; ++it)
		strict_notes.insert (*it);

	// Remove missing notes
	for (size_t cgi = 0; cgi < track_to_notes.size (); ++cgi) {
		MidiModel::Notes& track_notes = track_to_notes[cgi];
		MidiModel::Notes::iterator track_notes_it = track_notes.begin ();
		for (; track_notes_it != track_notes.end ();) {
			MidiModel::StrictNotes::iterator notes_it = find_eq_id (strict_notes, *track_notes_it);
			if (notes_it == strict_notes.end ()) {
				track_notes.erase (track_notes_it++);
			} else {
				++track_notes_it;
			}
		}
	}

	// Add new notes and move existing notes
	for (MidiModel::StrictNotes::const_iterator it = strict_notes.begin ();
	     it != strict_notes.end (); ++it) {

		int cgi = find_eq_id (*it);
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
	}

	// Update nreqtracks and ntracks
	nreqtracks = track_to_notes.size ();
	ntracks = std::max (nreqtracks, ntracks);
}

void
MidiNotesPattern::update_row_to_notes ()
{
	on_notes.clear ();
	on_notes.resize (ntracks);
	off_notes.clear ();
	off_notes.resize (ntracks);

	for (uint16_t itrack = 0; itrack < nreqtracks; ++itrack) {
		for (MidiModel::Notes::iterator inote = track_to_notes[itrack].begin ();
		     inote != track_to_notes[itrack].end (); ++inote) {
			// On and off times are absolute, not relative to the region
			// NEXT.14: use Region::region_beats_to_absolute_time and such instead
			Temporal::Beats on_time = (*inote)->time () + position_beats - start_beats;
			Temporal::Beats off_time = (*inote)->end_time () + position_beats - start_beats;
			int on_max_delay_row = row_at_beats_max_delay (on_time);
			int on_row = row_at_beats (on_time);
			int off_min_delay_row = row_at_beats_min_delay (off_time);
			int off_row = row_at_beats (off_time);

			// TODO: make row assignement more intelligent. Given the possible
			// rows for each on and off notes find an assignement that
			// maximizes the number displayable rows. If however the number of
			// combinations to explore is too high fallback on the following
			// cheap strategy.
			if (on_row == off_row && on_row != off_min_delay_row) {
				off_row = off_min_delay_row;
			} else if (on_row == off_row && on_max_delay_row != off_row) {
				on_row = on_max_delay_row;
			}

			on_notes[itrack].insert (RowToNotes::value_type (on_row, *inote));
			// Do no display off notes occuring at the very end of the region
			if (off_time < global_end_beats) {
				off_notes[itrack].insert (RowToNotes::value_type (off_row, *inote));
			}
		}
	}
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
MidiNotesPattern::find_prev_on (int row, int cgi) const
{
	const RowToNotes& r2n = on_notes[cgi];
	RowToNotes::const_reverse_iterator rit =
		std::reverse_iterator<RowToNotes::const_iterator> (r2n.lower_bound (row));
	while (rit != r2n.rend () && rit->first == row) { ++rit; };
	return rit != r2n.rend () ? lattest (r2n.equal_range (rit->first)) : NotePtr ();
}

NotePtr
MidiNotesPattern::find_next_on (int row, int cgi) const
{
	const RowToNotes& r2n = on_notes[cgi];
	RowToNotes::const_iterator it = r2n.upper_bound (row);
	while (it != r2n.end () && it->first == row) { ++it; };
	return it != r2n.end () ? earliest (r2n.equal_range (it->first)) : NotePtr ();
}

NotePtr
MidiNotesPattern::find_prev_off (int row, int cgi) const
{
	// TODO: implementation could simplified with a template of something
	const RowToNotes& r2n = off_notes[cgi];
	RowToNotes::const_reverse_iterator rit =
		std::reverse_iterator<RowToNotes::const_iterator> (r2n.lower_bound (row));
	while (rit != r2n.rend () && rit->first == row) { ++rit; };
	return rit != r2n.rend () ? lattest (r2n.equal_range (rit->first)) : NotePtr ();
}

NotePtr
MidiNotesPattern::find_next_off (int row, int cgi) const
{
	const RowToNotes& r2n = off_notes[cgi];
	RowToNotes::const_iterator it = r2n.upper_bound (row);
	while (it != r2n.end () && it->first == row) { ++it; };
	return it != r2n.end () ? earliest (r2n.equal_range (it->first)) : NotePtr ();
}

Temporal::Beats
MidiNotesPattern::next_on (int row, int cgi) const
{
	NotePtr next_note = find_next_on (row, cgi);
	return next_note ? next_note->time () : end_beats;
}

Temporal::Beats
MidiNotesPattern::next_off (int row, int cgi) const
{
	NotePtr next_note = find_next_off (row, cgi);
	return next_note ? next_note->end_time () : end_beats;
}

bool
MidiNotesPattern::is_displayable (int row, int cgi) const
{
	size_t off_notes_count = off_notes[cgi].count (row);
	size_t on_notes_count = on_notes[cgi].count (row);
	RowToNotes::const_iterator i_off = off_notes[cgi].find (row);
	RowToNotes::const_iterator i_on = on_notes[cgi].find (row);
	return off_notes_count <= 1 && on_notes_count <= 1
		&& (off_notes_count != 1 || on_notes_count != 1
		    || i_off->second->end_time () == i_on->second->time ());
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
	Temporal::Beats note_time = position_beats - start_beats + note->time ();
	return Temporal::TempoMap::use()->bbt_at (note_time);
}

Temporal::BBT_Time
MidiNotesPattern::off_note_bbt (NotePtr note) const
{
	Temporal::Beats end_note_time = position_beats - start_beats + note->end_time ();
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
	ss << header << "_midi_model = " << _midi_model;

	return ss.str ();
}

NotePtr
MidiNotesPattern::earliest (const RowToNotesRange& rng) const
{
	NotePtr result;
	for (RowToNotes::const_iterator it = rng.first; it != rng.second; ++it) {
		if (!result || result->time () < it->second->time ()) {
			result = it->second;
		}
	}
	return result;
}

NotePtr
MidiNotesPattern::lattest (const RowToNotesRange& rng) const
{
	NotePtr result;
	for (RowToNotes::const_iterator it = rng.first; it != rng.second; ++it) {
		if (!result || it->second->time () < result->time ()) {
			result = it->second;
		}
	}
	return result;
}

int
MidiNotesPattern::find_eq_id (NotePtr note) const
{
	for (int i = 0; i < (int)track_to_notes.size (); i++) {
		if (find_eq_id (i, note) != track_to_notes[i].end ()) {
			return i;
		}
	}
	return -1;
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
	return -1;
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
