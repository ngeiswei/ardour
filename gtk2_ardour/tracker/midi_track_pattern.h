/*
    Copyright (C) 2017-2018 Nil Geisweiller

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

#ifndef __ardour_tracker_midi_track_pattern_h_
#define __ardour_tracker_midi_track_pattern_h_

#include "midi_region_pattern.h"
#include "track_automation_pattern.h"

/**
 * Represent patterns, midi notes, midi automations and track automations of a
 * given track, with possibly multiple regions, possibly overlapping, as long
 * as they all come from the same track.
 */
class MidiTrackPattern : public BasePattern {
public:
	MidiTrackPattern (const TrackerEditor& te,
	                  boost::shared_ptr<ARDOUR::MidiTrack> midi_track,
	                  const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions);
	virtual ~MidiTrackPattern ();

	// TODO attempt to move TrackerEditor::get_automation_pattern here
	// TODO attempt to move that sort of code here:
	// int delay_ticks = is_region_automation (param) ?
	// mtp->region_relative_delay_ticks(Temporal::Beats(awhen), rowidx) : mtp->delay_ticks((samplepos_t)awhen, rowidx);

	// TODO: maybe need get_actls for region parameters
	boost::shared_ptr<ARDOUR::AutomationControl> get_actl(const Evoral::Parameter& param);

	// Insert the automation control(s) corresponding to param (and connect it
	// to the grid for changes)
	void insert(const Evoral::Parameter& param);

	// Set the number of rows per beat. 0 means 1 row per bar (TODO: not fully
	// supported). After changing that you probably need to update the pattern,
	// see below.
	void set_rows_per_beat(uint16_t rpb);

	// Set position_row_beats, end_row_beats and nrows
	void set_row_range();

	// Build or rebuild note and automation pattern
	void update();

	// Return true iff the row is defined, that is if such a row points to an
	// existing region.
	bool is_defined (int rowi) const;

	// Return the row index relative to the start of pattern at region index mri
	int to_rrri (uint32_t rowi, size_t mri) const;

	// Given the row index, calculate the corresponding midi region index. This
	// can only work assuming that regions do not overlap in time. If no such
	// mri is defined, then return -1.
	int to_mri (uint32_t rowi) const;

	// Number of note tracks
	size_t get_ntracks () const;

	boost::shared_ptr<ARDOUR::MidiTrack> midi_track;
	TrackAutomationPattern tap;
	std::vector<MidiRegionPattern> mrps;
	std::vector<uint32_t> row_offset;
};

#endif /* __ardour_tracker_midi_track_pattern_h_ */
