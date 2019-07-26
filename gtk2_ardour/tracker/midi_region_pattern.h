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

#ifndef __ardour_tracker_midi_region_pattern_h_
#define __ardour_tracker_midi_region_pattern_h_

#include "ardour/midi_track.h"
#include "ardour/midi_model.h"

#include "note_pattern.h"
#include "region_automation_pattern.h"
#include "midi_region_pattern_phenomenal_diff.h"

namespace Tracker {

/**
 * Represent pattern of midi notes and midi automations of a given region.
 */
class MidiRegionPattern : public BasePattern {
public:
	MidiRegionPattern (TrackerEditor& te,
	                   boost::shared_ptr<ARDOUR::MidiTrack> midi_track,
	                   boost::shared_ptr<ARDOUR::MidiRegion> region);
	virtual ~MidiRegionPattern ();

	MidiRegionPattern& operator=(const MidiRegionPattern& other);

	MidiRegionPatternPhenomenalDiff phenomenal_diff(const MidiRegionPattern& prev) const;

	// TODO attempt to move TrackerEditor::param2actrl + its ctor here
	// TODO attempt to move TrackerEditor::update_automation_patterns here
	// TODO attempt to move TrackerEditor::get_automation_pattern here
	// TODO attempt to move that sort of code here:
	// int delay_ticks = is_region_automation (param) ?
	// mtp->region_relative_delay_ticks(Temporal::Beats(awhen), rowidx) : mtp->delay_ticks((samplepos_t)awhen, rowidx);

	// Set the number of rows per beat. 0 means 1 row per bar (TODO: not fully
	// supported). After changing that you probably need to update the pattern,
	// i.e. call update().
	void set_rows_per_beat(uint16_t rpb);

	// Update position, etc, of this, np and rap, based on midi_region
	void update_position_etc();

	// Set position_row_beats, end_row_beats and nrows
	void set_row_range();

	// Build or rebuild note and automation pattern
	void update();

	// Insert the automation control corresponding to param
	void insert (const Evoral::Parameter& param);

	virtual std::string self_to_string() const;
	virtual std::string to_string(const std::string& indent = std::string()) const;

	NotePattern np;
	RegionAutomationPattern rap;
	boost::shared_ptr<ARDOUR::MidiModel> midi_model;
	boost::shared_ptr<ARDOUR::MidiRegion> midi_region;
};

} // ~namespace tracker

#endif /* __ardour_tracker_midi_track_pattern_h_ */
