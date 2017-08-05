/*
    Copyright (C) 2015-2017 Nil Geisweiller

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>
#include <map>

#include "evoral/midi_util.h"
#include "evoral/Note.hpp"

#include "ardour/beats_frames_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "note_pattern.h"

using namespace std;
using namespace ARDOUR;
using Timecode::BBT_Time;

/////////////////
// NotePattern //
/////////////////

NotePattern::NotePattern(ARDOUR::Session* session,
                         boost::shared_ptr<ARDOUR::MidiRegion> region,
                         boost::shared_ptr<ARDOUR::MidiModel> midi_model)
	: Pattern(session, region),
	  ntracks(0), nreqtracks(0),
	  _midi_model(midi_model)
{
}

void NotePattern::update_pattern()
{
	set_row_range();
	update_track_to_notes();
	update_row_to_notes();
}

void NotePattern::update_track_to_notes()
{
	// Sort the notes with the StrictNotes order so that simulatenous notes can
	// be dispatched according to some defined order.
	const MidiModel::Notes& notes = _midi_model->notes();
	MidiModel::StrictNotes strict_notes(notes.begin(), notes.end());

	// Add new notes and move existing notes
	for (MidiModel::StrictNotes::const_iterator it = strict_notes.begin();
	     it != strict_notes.end(); ++it) {
		int track_idx = find_eq_id(*it);
		int freetrack = -1;		// index of the first free track
		if (-1 < track_idx) {
			// The note is already in track_to_notes, remove it and check if it
			// can be re-inserted in the same track.
			erase_eq_id(track_idx, *it);
			if (is_free(track_idx, *it)) {
				freetrack = track_idx;
			}
		}
		// Find the first available track
		if (freetrack < 0)
			freetrack = find_free_track(*it);
		// No free track found, create a new one.
		if (freetrack < 0) {
			freetrack = track_to_notes.size();
			track_to_notes.push_back(MidiModel::Notes());
		}
		// Insert the note in the first free track
		track_to_notes[freetrack].insert(*it);
	}

	// Remove missing notes
	for (size_t track_idx = 0; track_idx < track_to_notes.size(); ++track_idx) {
		ARDOUR::MidiModel::Notes& track_notes = track_to_notes[track_idx];
		ARDOUR::MidiModel::Notes::iterator track_notes_it = track_notes.begin();
		for (; track_notes_it != track_notes.end();) {
			ARDOUR::MidiModel::Notes::iterator notes_it = find_eq_id(notes, *track_notes_it);
			if (notes_it == notes.end()) {
				track_notes.erase(track_notes_it++);
			} else
				++track_notes_it;
		}
	}

	// Update nreqtracks and ntracks
	nreqtracks = track_to_notes.size();
	ntracks = std::max(nreqtracks, ntracks);
}

void NotePattern::update_row_to_notes()
{
	on_notes.clear();
	on_notes.resize(ntracks);
	off_notes.clear();
	off_notes.resize(ntracks);

	for (uint16_t itrack = 0; itrack < nreqtracks; ++itrack) {
		for (MidiModel::Notes::iterator inote = track_to_notes[itrack].begin();
		     inote != track_to_notes[itrack].end(); ++inote) {
			Evoral::Beats on_time = (*inote)->time() + first_beats;
			Evoral::Beats off_time = (*inote)->end_time() + first_beats;
			uint32_t on_max_delay_row = row_at_beats_max_delay(on_time);
			uint32_t on_row = row_at_beats(on_time);
			uint32_t off_min_delay_row = row_at_beats_min_delay(off_time);
			uint32_t off_row = row_at_beats(off_time);

			// TODO: make row assignement more intelligent. Given the possible
			// rows for each on and off notes find an assignement that
			// minimizes the number displayable rows. If however the number of
			// combinations to explore is too high fallback on the following
			// cheap strategy.

			if (on_row == off_row && on_row != off_min_delay_row) {
				on_notes[itrack].insert(RowToNotes::value_type(on_row, *inote));
				off_notes[itrack].insert(RowToNotes::value_type(off_min_delay_row, *inote));
			} else if (on_row == off_row && on_max_delay_row != off_row) {
				on_notes[itrack].insert(RowToNotes::value_type(on_max_delay_row, *inote));
				off_notes[itrack].insert(RowToNotes::value_type(off_row, *inote));
			} else {
				on_notes[itrack].insert(RowToNotes::value_type(on_row, *inote));
				off_notes[itrack].insert(RowToNotes::value_type(off_row, *inote));
			}
		}
	}
}

void NotePattern::inc_ntracks()
{
	ntracks++;
}

void NotePattern::dec_ntracks()
{
	ntracks--;
}

NotePattern::NoteTypePtr NotePattern::find_prev(uint32_t row, int track_idx) const
{
	const RowToNotes& r2n = on_notes[track_idx];
	RowToNotes::const_reverse_iterator rit =
		std::reverse_iterator<RowToNotes::const_iterator>(r2n.lower_bound(row));
	while (rit != r2n.rend() && rit->first == row) { ++rit; };
	return rit != r2n.rend() ? lattest(r2n.equal_range(rit->first)) : NoteTypePtr();
}

NotePattern::NoteTypePtr NotePattern::find_next(uint32_t row, int track_idx) const
{
	const RowToNotes& r2n = on_notes[track_idx];
	RowToNotes::const_iterator it = r2n.upper_bound(row);
	while (it != r2n.end() && it->first == row) { ++it; };
	return it != r2n.end() ? earliest(r2n.equal_range(it->first)) : NoteTypePtr();
}

Evoral::Beats NotePattern::next_off(uint32_t row, int track_idx) const
{
	NoteTypePtr next_note = find_next(row, track_idx);
	return next_note ? next_note->time() : last_beats;
}

bool NotePattern::is_displayable(uint32_t row, int track_idx) const
{
	size_t off_notes_count = off_notes[track_idx].count(row);
	size_t on_notes_count = on_notes[track_idx].count(row);
	NotePattern::RowToNotes::const_iterator i_off = off_notes[track_idx].find(row);
	NotePattern::RowToNotes::const_iterator i_on = on_notes[track_idx].find(row);
	return off_notes_count <= 1 && on_notes_count <= 1
		&& (off_notes_count != 1 || on_notes_count != 1
		    || i_off->second->end_time() == i_on->second->time());
}

NotePattern::NoteTypePtr NotePattern::earliest(const RowToNotesRange& rng) const
{
	NoteTypePtr result;
	for (RowToNotes::const_iterator it = rng.first; it != rng.second; ++it)
		if (!result || result->time() < it->second->time())
			result = it->second;
	return result;
}

NotePattern::NoteTypePtr NotePattern::lattest(const RowToNotesRange& rng) const
{
	NoteTypePtr result;
	for (RowToNotes::const_iterator it = rng.first; it != rng.second; ++it)
		if (!result || it->second->time() < result->time())
			result = it->second;
	return result;
}

int NotePattern::find_eq_id(NoteTypePtr note) const
{
	for (int i = 0; i < (int)track_to_notes.size(); i++)
		if (find_eq_id(i, note) != track_to_notes[i].end())
			return i;
	return -1;
}

ARDOUR::MidiModel::Notes::const_iterator NotePattern::find_eq_id(int track_idx, NoteTypePtr note) const
{
	return find_eq_id(track_to_notes[track_idx], note);
}

ARDOUR::MidiModel::Notes::iterator NotePattern::find_eq_id(int track_idx, NoteTypePtr note)
{
	return find_eq_id(track_to_notes[track_idx], note);
}

ARDOUR::MidiModel::Notes::const_iterator NotePattern::find_eq_id(const ARDOUR::MidiModel::Notes& notes, NoteTypePtr note) const
{
	Evoral::event_id_t id = note->id();
	ARDOUR::MidiModel::Notes::const_iterator it = notes.begin();
	for (; it != notes.end(); ++it)
		if ((*it)->id() == id)
			return it;
	return notes.end();
}

ARDOUR::MidiModel::Notes::iterator NotePattern::find_eq_id(ARDOUR::MidiModel::Notes& notes, NoteTypePtr note)
{
	Evoral::event_id_t id = note->id();
	ARDOUR::MidiModel::Notes::iterator it = notes.begin();
	for (; it != notes.end(); ++it)
		if ((*it)->id() == id)
			return it;
	return notes.end();
}

void NotePattern::erase_eq_id(int track_idx, NoteTypePtr note)
{
	erase_eq_id(track_to_notes[track_idx], note);
}

void NotePattern::erase_eq_id(ARDOUR::MidiModel::Notes& notes, NoteTypePtr note)
{
	notes.erase(find_eq_id(notes, note));
}

bool NotePattern::is_free(int track_idx, NoteTypePtr note) const
{
	const ARDOUR::MidiModel::Notes& notes = track_to_notes[track_idx];
	ARDOUR::MidiModel::Notes::iterator it = notes.begin();
	for (; it != notes.end(); ++it)
		if (overlap(*it, note))
			return false;
	return true;
}

int NotePattern::find_free_track(NoteTypePtr note) const
{
	for (int i = 0; i < (int)track_to_notes.size(); i++)
		if (is_free(i, note))
			return i;
	return -1;
}

bool NotePattern::overlap(NoteTypePtr a, NoteTypePtr b)
{
	Evoral::Beats sa = a->time();
	Evoral::Beats ea  = a->end_time();
	Evoral::Beats sb = b->time();
	Evoral::Beats eb = b->end_time();

	return (((sb > sa) && (eb <= ea)) ||
	        ((eb > sa) && (eb <= ea)) ||
	        ((sb > sa) && (sb < ea)) ||
	        ((sa >= sb) && (sa <= eb) && (ea <= eb)));
}
