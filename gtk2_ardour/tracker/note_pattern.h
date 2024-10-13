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

#ifndef __ardour_tracker_note_pattern_h_
#define __ardour_tracker_note_pattern_h_

#include "ardour/midi_model.h"
#include "ardour/session_handle.h"

#include "base_pattern.h"
#include "note_pattern_phenomenal_diff.h"
#include "tracker_utils.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
};

namespace Tracker {

/**
 * Data structure holding the pattern of midi notes for the pattern editor.
 * Plus some goodies method to generate a pattern given a midi region. The
 * notion of track is used instead of column as this term is reserved to the
 * gtk widget representing notes and automations. The term track should not be
 * confused with the term track of the horizontal editor view.
 */
class NotePattern : public BasePattern {
public:
	NotePattern (TrackerEditor& te,
	             MidiRegionPtr region);

	NotePattern& operator= (const NotePattern& other);
	NotePtr clone_note (NotePtr note) const;

	static void rows_diff (int cgi, const NotePattern& lnp, const NotePattern& rnp, std::set<int>& rd);

	NotePatternPhenomenalDiff phenomenal_diff (const NotePattern& prev) const;

	// Build or rebuild the pattern (implement BasePattern::update)
	void update ();

	// Update track_to_notes. Distribute new notes across N tracks so that no
	// overlapping notes can exist on the same track. Likewise modified notes
	// are moved to a new track if overlapping cannot be avoided otherwise. A
	// new note is placed on the first available track, ordered by vector
	// index. A modified note attempt to remain on the same track, and
	// otherwise placed in the first available track as if it were a new note.
	//
	// Since new tracks can be created if necessary, nreqtracks and ntracks are
	// updated as well.
	void update_track_to_notes ();

	// Update the mapping from row to on and off notes.
	void update_row_to_notes ();

	// Increase and decrease the number of tracks
	void set_ntracks (uint16_t n);
	void inc_ntracks ();
	void dec_ntracks ();

	// Find the previous (resp. next) on note of a given row on a given track
	// index.
	NotePtr find_prev_on (int row, int cgi) const;
	NotePtr find_next_on (int row, int cgi) const;

	// Find the previous (resp. next) off note of a given row on a given track
	// index.
	NotePtr find_prev_off (int row, int cgi) const;
	NotePtr find_next_off (int row, int cgi) const;

	// Return the Beats of the next note on (resp. off) or the end of the region
	// if none
	Temporal::Beats next_on (int row, int cgi) const;
	Temporal::Beats next_off (int row, int cgi) const;

	// Return true if the notes are displayable at this resolution. Basically
	// if there are too many notes, unless its a pair (note off, note on)
	// perfectly contiguous, it is not displayable.
	bool is_displayable (int row, int cgi) const;

	// Add note in track_to_notes. Used by the grid so that the note pattern can
	// know on which track idx a new note should be.
	void add (int cgi, NotePtr note);

	// Get the bbt of an on (resp. off) note
	Temporal::BBT_Time on_note_bbt (NotePtr note) const;
	Temporal::BBT_Time off_note_bbt (NotePtr note) const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	// Number of tracks of that midi track (initially determined by the number
	// of overlapping notes)
	uint16_t ntracks;

	// Minimum of number of tracks required to display all notes
	// (track_to_notes.size ())
	uint16_t nreqtracks;

	// Store the distribution of notes for each track. Makes sure that no
	// overlapping notes are on the same track.
	std::vector<ARDOUR::MidiModel::Notes> track_to_notes;

	// Map row index to on notes for each note column group
	std::vector<RowToNotes> on_notes;

	// Map row index to off notes (basically the same corresponding on notes)
	// for each note column group
	std::vector<RowToNotes> off_notes;

private:
	NotePtr earliest (const RowToNotesRange& rng) const;
	NotePtr lattest (const RowToNotesRange& rng) const;

	// Find track in track_to_notes containing a note with the same id, return
	// the track index if found, -1 otherwise.
	int find_eq_id (NotePtr note) const;

	// Find the note in track_to_notes[cgi] containing the same id.
	ARDOUR::MidiModel::Notes::const_iterator find_eq_id (int cgi, NotePtr note) const;
	ARDOUR::MidiModel::Notes::iterator find_eq_id (int cgi, NotePtr note);
	template <typename NoteContainer>
	typename NoteContainer::const_iterator find_eq_id (const NoteContainer& notes, NotePtr note) const;
	template <typename NoteContainer>
	typename NoteContainer::iterator find_eq_id (NoteContainer& notes, NotePtr note);

	// Erase note in given track with the id of the given note.
	void erase_eq_id (int cgi, NotePtr note);

	// Erase note in given Notes with the id of the given note
	void erase_eq_id (ARDOUR::MidiModel::Notes& notes, NotePtr note);

	// Erase pointer iterator from notes and return the next iterator
	ARDOUR::MidiModel::Notes::iterator erase (ARDOUR::MidiModel::Notes& notes, ARDOUR::MidiModel::Notes::iterator it);

	// Check if a track is available to receive a note.
	bool is_free (int cgi, NotePtr note) const;

	// Find the first track ready of receive a note. Return the track index if
	// found, -1 otherwise.
	int find_free_track (NotePtr note) const;

	static bool overlap (NotePtr a, NotePtr b);

	MidiModelPtr _midi_model;
};

template<typename NoteContainer>
typename NoteContainer::const_iterator NotePattern::find_eq_id (const NoteContainer& notes, NotePtr note) const
{
	Evoral::event_id_t id = note->id ();
	typename NoteContainer::const_iterator it = notes.begin ();
	for (; it != notes.end (); ++it) {
		if ((*it)->id () == id) {
			return it;
		}
	}
	return notes.end ();
}

template <typename NoteContainer>
typename NoteContainer::iterator NotePattern::find_eq_id (NoteContainer& notes, NotePtr note)
{
	Evoral::event_id_t id = note->id ();
	typename NoteContainer::iterator it = notes.begin ();
	for (; it != notes.end (); ++it) {
		if ((*it)->id () == id) {
			return it;
		}
	}
	return notes.end ();
}

} // ~namespace Tracker

#endif /* __ardour_tracker_note_pattern_h_ */
