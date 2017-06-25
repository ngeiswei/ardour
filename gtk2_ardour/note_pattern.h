/*
    Copyleft (C) 2015 Nil Geisweiller

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

#ifndef __ardour_gtk2_note_pattern_h_
#define __ardour_gtk2_note_pattern_h_

#include "evoral/types.hpp"
#include "ardour/session_handle.h"

#include "pattern.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
};

/**
 * Data structure holding the pattern of midi notes for the pattern editor.
 * Plus some goodies method to generate a pattern given a midi region. The
 * notion of track is used instead of column as this term is reserved to the
 * gtk widget representing notes and automations. The term track shouldn't not
 * be confused with the term track of the horizontal editor view.
 */
class NotePattern : public Pattern {
public:
	// Holds a note and its associated track number (a maximum of 4096
	// tracks should be more than enough).
	typedef Evoral::Note<Evoral::Beats> NoteType;
	typedef boost::shared_ptr<NoteType> NoteTypePtr;
	typedef std::multimap<uint32_t, NoteTypePtr> RowToNotes;
	typedef std::pair<RowToNotes::const_iterator, RowToNotes::const_iterator> RowToNotesRange;

	NotePattern(ARDOUR::Session* session,
	            boost::shared_ptr<ARDOUR::MidiRegion> region,
	            boost::shared_ptr<ARDOUR::MidiModel> midi_model);

	// Build or rebuild the pattern (implement Pattern::update_pattern)
	void update_pattern();

	// Increase and decrease the number of tracks
	void inc_ntracks();
	void dec_ntracks();

	// Find the previous (resp. next) note of a given row on a given track
	// index.
	// TODO: maybe we want to use uint16_t instead of int
	NoteTypePtr find_prev(uint32_t row, int track_idx) const;
	NoteTypePtr find_next(uint32_t row, int track_idx) const;

	// Return the Beats of the note off as far as it can go (i.e. the next on
	// note or the end of the region.)
	Evoral::Beats next_off(uint32_t row, int track_idx) const;

	// Return true if the notes are displayable at this resolution. Basically
	// if there are too many notes, unless its a pair (note off, note on)
	// perfectly contiguous, it is not displayable.
	bool is_displayable(uint32_t row, int track_idx) const;

	// Number of tracks of that midi track (determined by the number of
	// overlapping notes)
	uint16_t ntracks;

	// Minimum of number of tracks required to display all notes
	uint16_t nreqtracks;

	// Map row index to on notes for each track
	std::vector<RowToNotes> on_notes;

	// Map row index to off notes (basically the same corresponding on notes)
	// for each track
	std::vector<RowToNotes> off_notes;

private:
	NoteTypePtr earliest(const RowToNotesRange& rng) const;
	NoteTypePtr lattest(const RowToNotesRange& rng) const;

	boost::shared_ptr<ARDOUR::MidiModel> _midi_model;
};

#endif /* __ardour_gtk2_note_pattern_h_ */
