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

#ifndef __ardour_tracker_midi_notes_pattern_h_
#define __ardour_tracker_midi_notes_pattern_h_

#include "ardour/midi_model.h"
#include "ardour/session_handle.h"

#include "base_pattern.h"
#include "midi_notes_pattern_phenomenal_diff.h"
#include "tracker_utils.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
};

namespace Tracker {

/**
 * Data structure holding the pattern of midi notes for the pattern
 * editor.  Plus some goodies method to generate a pattern given a
 * midi region. Warning: the notion of track is used instead of column
 * as this term is reserved to the gtk widget representing notes and
 * automations. The term track should not be confused with the term
 * track of the horizontal editor view.
 */
class MidiNotesPattern : public BasePattern {
public:
	MidiNotesPattern (TrackerEditor& te,
	                  MidiRegionPtr region);

	MidiNotesPattern& operator= (const MidiNotesPattern& other);
	NotePtr clone_note (NotePtr note) const;

	static void rows_diff (int cgi, const MidiNotesPattern& mnp_l, const MidiNotesPattern& mnp_r, std::set<int>& rd);

	MidiNotesPatternPhenomenalDiff phenomenal_diff (const MidiNotesPattern& prev) const;

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

	// Update the mapping from row to on and off notes for a given track.  It
	// assumes the on_notes and off_notes maps have been cleared and resized to
	// ntracks.
	void update_row_to_notes_at_track (uint16_t cgi);

	// Update the mapping from row to on and off notes for a given track and a
	// given note.  It assumes the on_notes and off_notes maps have been cleared
	// and resized to ntracks.
	void update_row_to_notes_at_track_note (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote);

	// Return true iff the row at cgi is free, meaning it can potentially host
	// the given note.
	bool is_on_row_available (uint16_t cgi, int row, ARDOUR::MidiModel::Notes::iterator inote);
	bool is_off_row_available (uint16_t cgi, int row, ARDOUR::MidiModel::Notes::iterator inote);

	// Return true if the note ends with the current region, otherwise, if the
	// note ends beyond it, return false.
	bool note_ends_within_region (NotePtr note) const;

	// Return default on/off row, centered around the on/off note.  If no such
	// note exists, return -1.  It takes a note iterator instead of a note in
	// order to be resilient on being called with a next note that does not
	// exists.
	int centered_on_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote) const;
	int centered_off_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote) const;

	// Return row right above the centered row around the on/off note.  If no
	// such note exists, return -1.
	int previous_on_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote) const;
	int previous_off_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote) const;

	// Return row right below the centered row around the on/off note.  If no
	// such note exists, return -1.
	int next_on_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote) const;
	int next_off_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote) const;

	// Return defacto on/off row, which is either the row that previously
	// contained the note, or if it's been moved, is centered around the on/off
	// note.
	int defacto_on_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote) const;
	int defacto_off_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote) const;

	// Given a rank of priority return the corresponding row for an on/off note.
	// The rank goes from 0 to at most 2.  If the provided rank goes out of
	// range, then return -1.  NEXT.4: could be const, maybe
	int on_row_suggestion (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote, int rank);
	int off_row_suggestion (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote, int rank);

	// In the process of updating the mapping from row to on (resp. off) notes,
	// find the nearest row to place that on (resp. off) note
	// NEXT.4: could be const, maybe
	int find_nearest_on_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote);
	int find_nearest_off_row (uint16_t cgi, ARDOUR::MidiModel::Notes::iterator inote);

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
	Temporal::Beats next_on_beats (int row, int cgi) const;
	Temporal::Beats next_off_beats (int row, int cgi) const;

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

	// Used to temporarily save the state of on_notes and off_notes during
	// update
	std::vector<RowToNotes> _prev_on_notes;
	std::vector<RowToNotes> _prev_off_notes;

	// Mapping from on/off note pointer to row per cgi.  Used to minimize the
	// amount of movements of a note.
	std::vector<NoteToRow> _on_note_to_row;
	std::vector<NoteToRow> _off_note_to_row;

	// Like _on_note_to_row and _off_note_to_row but used to temporarily save
	// the state of this mapping during update
	std::vector<NoteToRow> _prev_on_note_to_row;
	std::vector<NoteToRow> _prev_off_note_to_row;

	MidiRegionPtr _midi_region;
	MidiModelPtr _midi_model;
};

template<typename NoteContainer>
typename NoteContainer::const_iterator MidiNotesPattern::find_eq_id (const NoteContainer& notes, NotePtr note) const
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
typename NoteContainer::iterator MidiNotesPattern::find_eq_id (NoteContainer& notes, NotePtr note)
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

#endif /* __ardour_tracker_midi_notes_pattern_h_ */
