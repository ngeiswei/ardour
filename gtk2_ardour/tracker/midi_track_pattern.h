/*
    Copyright (C) 2017 Nil Geisweiller

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

#ifndef __ardour_gtk2_tracker_track_pattern_h_
#define __ardour_gtk2_tracker_track_pattern_h_

#include "note_pattern.h"
#include "region_automation_pattern.h"
#include "track_automation_pattern.h"

/**
 * Represent all patterns, midi notes, midi automations and track automations
 * of a given track, with possibly multiple regions, possibly overlapping, as
 * long as they all come from the same track.
 */
class MidiTrackPattern : public BasePattern {
public:
	MidiTrackPattern (ARDOUR::Session* session,
	                  boost::shared_ptr<ARDOUR::MidiRegion> region);
	virtual ~MidiTrackPattern ();

	NotePattern np;
	RegionAutomationPattern rap;
	TrackAutomationPattern tap;

	// TODO attempt to move MidiTrackerEditor::param2actrl + its ctor here
	// TODO attempt to move MidiTrackerEditor::update_automation_patterns here
	// TODO attempt to move MidiTrackerEditor::get_automation_pattern here
	// TODO attempt to move that sort of code here:
	// int delay_ticks = is_region_automation (param) ?
	// mtp->region_relative_delay_ticks(Temporal::Beats(awhen), rowidx) : mtp->delay_ticks((samplepos_t)awhen, rowidx);


	// Set the number of rows per beat. 0 means 1 row per bar (TODO: not fully
	// supported). After changing that you probably need to update the pattern,
	// see below.
	void set_rows_per_beat(uint16_t rpb);

	// Set position_row_beats, end_row_beats and nrows
	void set_row_range();

	// Build or rebuild note and automation pattern
	void update();
};

#endif /* __ardour_gtk2_tracker_track_pattern_h_ */
