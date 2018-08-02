/*
    Copyright (C) 2018 Nil Geisweiller

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

#ifndef __ardour_tracker_multi_track_pattern_h_
#define __ardour_tracker_multi_track_pattern_h_

#include "midi_track_pattern.h"

#include "tracker_utils.h"

class TrackerEditor;

/**
 * Represent patterns over multiple tracks.
 */
class MultiTrackPattern // : public BasePattern /* TODO: derive or not derive? */
{
public:
	MultiTrackPattern (const TrackerEditor& te);
	~MultiTrackPattern ();

	void setup_regions_per_track ();
	void setup ();

	void update ();
	void update_rows_per_beat ();
	void update_content ();
	void update_earliest_mtp ();  // earliest midi track
	void update_global_nrows ();  // row_offset, nrows, global_nrows

	// Return true iff the row at mti is defined, that is if such a row and mti
	// points to an existing region.
	bool is_defined (uint32_t rowi, size_t mti) const;

	// Like beats_at_row but the beats is calculated in reference to the
	// region's position
	Temporal::Beats region_relative_beats_at_row (uint32_t rowi, size_t mti, int32_t delay=0) const;

	// Like delay_ticks but the event_time is relative to the region position of a given mti
	int64_t region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mti) const;

	// Return an event's delay in a certain row and mti in ticks
	int64_t delay_ticks (samplepos_t when, uint32_t rowi, size_t mti) const;
	
	// Return the sample at the corresponding row index and delay in relative
	// ticks
	uint32_t sample_at_row (uint32_t rowi, size_t mti, int32_t delay=0) const;

	size_t off_notes_count (uint32_t rowi, size_t mti, size_t cgi) const;
	size_t on_notes_count (uint32_t rowi, size_t mti, size_t cgi) const;
	bool is_note_displayable (uint32_t rowi, size_t mti, size_t cgi) const;
	NoteTypePtr off_note (uint32_t rowi, size_t mti, size_t cgi) const;
	NoteTypePtr on_note (uint32_t rowi, size_t mti, size_t cgi) const;

	bool is_auto_displayable (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const;

	const AutomationPattern* get_automation_pattern (size_t mti, const Evoral::Parameter& param) const;
	AutomationPattern* get_automation_pattern (size_t mti, const Evoral::Parameter& param);

	typedef ARDOUR::AutomationList::iterator AutomationListIt;
	size_t get_automation_list_count (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const;
	Evoral::ControlEvent* get_automation_control_event (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const;

	NoteTypePtr find_prev_note(uint32_t rowi, size_t mti, int cgi) const;
	NoteTypePtr find_next_note(uint32_t rowi, size_t mti, int cgi) const;

	// Return the Beats of the note off as far as it can go (i.e. the next on
	// note or the end of the region.)
	Temporal::Beats next_off(uint32_t rowi, size_t mti, int cgi) const;

	// Return the row index relative to the start of pattern at mti.
	int to_rri (uint32_t rowi, size_t mti) const;

	const TrackerEditor& tracker_editor;

	// Mapping between tracks and regions
	typedef std::map<boost::shared_ptr<ARDOUR::MidiTrack>, std::vector<boost::shared_ptr<ARDOUR::MidiRegion> > > TrackRegionsMap;
	TrackRegionsMap regions_per_track;

	// Pattern per midi track
	std::vector<MidiTrackPattern*> mtps;

	// Row index offset and number of valid rows per mti
	MidiTrackPattern*            earliest_mtp;
	std::vector<uint32_t>        row_offset;
	std::vector<uint32_t>        nrows;
	uint32_t                     global_nrows;
};

#endif /* __ardour_tracker_multi_track_pattern_h_ */
