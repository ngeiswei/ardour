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

	// Distribute the notes across N tracks so that no overlapping notes can
	// exist on the same track. When a note on hits, it is placed on the first
	// available track, ordered by vector index. In case several notes on are
	// hit simultaneously, then the lowest pitch one is placed on the first
	// available track, ordered by vector index.
	const MidiModel::Notes& notes = _midi_model->notes();
	MidiModel::StrictNotes strict_notes(notes.begin(), notes.end());
	std::vector<MidiModel::Notes> notes_per_track;
	for (MidiModel::StrictNotes::const_iterator note = strict_notes.begin();
	     note != strict_notes.end(); ++note) {
		int freetrack = -1;		// index of the first free track
		for (int i = 0; i < (int)notes_per_track.size(); i++) {
			if ((*notes_per_track[i].rbegin())->end_time() <= (*note)->time()) {
				freetrack = i;
				break;
			}
		}
		// No free track found, create a new one.
		if (freetrack < 0) {
			freetrack = notes_per_track.size();
			notes_per_track.push_back(MidiModel::Notes());
		}
		// Insert the note in the first free track
		notes_per_track[freetrack].insert(*note);
	}
	nreqtracks = notes_per_track.size();
	ntracks = std::max(nreqtracks, ntracks);

	on_notes.clear();
	on_notes.resize(ntracks);
	off_notes.clear();
	off_notes.resize(ntracks);

	for (uint16_t itrack = 0; itrack < nreqtracks; ++itrack) {
		for (MidiModel::Notes::iterator inote = notes_per_track[itrack].begin();
		     inote != notes_per_track[itrack].end(); ++inote) {
			Evoral::Beats on_time = (*inote)->time() + first_beats;
			Evoral::Beats off_time = (*inote)->end_time() + first_beats;
			uint32_t row_on_max_delay = row_at_beats_max_delay(on_time);
			uint32_t row_on = row_at_beats(on_time);
			uint32_t row_off_min_delay = row_at_beats_min_delay(off_time);
			uint32_t row_off = row_at_beats(off_time);

			if (row_on == row_off && row_on != row_off_min_delay) {
				on_notes[itrack].insert(RowToNotes::value_type(row_on, *inote));
				off_notes[itrack].insert(RowToNotes::value_type(row_off_min_delay, *inote));
			} else if (row_on == row_off && row_on_max_delay != row_off) {
				on_notes[itrack].insert(RowToNotes::value_type(row_on_max_delay, *inote));
				off_notes[itrack].insert(RowToNotes::value_type(row_off, *inote));
			} else {
				on_notes[itrack].insert(RowToNotes::value_type(row_on, *inote));
				off_notes[itrack].insert(RowToNotes::value_type(row_off, *inote));
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

NotePattern::NoteTypePtr NotePattern::find_prev(uint32_t row, int col) const
{
	const RowToNotes& r2n = on_notes[col];
	RowToNotes::const_reverse_iterator rit =
		std::reverse_iterator<RowToNotes::const_iterator>(r2n.lower_bound(row));
	while (rit != r2n.rend() && rit->first == row) { ++rit; };
	return rit != r2n.rend() ? lattest(r2n.equal_range(rit->first)) : NoteTypePtr();
}

NotePattern::NoteTypePtr NotePattern::find_next(uint32_t row, int col) const
{
	const RowToNotes& r2n = on_notes[col];
	RowToNotes::const_iterator it = r2n.upper_bound(row);
	while (it != r2n.end() && it->first == row) { ++it; };
	return it != r2n.end() ? earliest(r2n.equal_range(it->first)) : NoteTypePtr();
}

Evoral::Beats NotePattern::next_off(uint32_t row, int col) const
{
	NoteTypePtr next_note = find_next(row, col);
	return next_note ? next_note->time() : last_beats;
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
