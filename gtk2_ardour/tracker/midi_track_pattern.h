/*
 * Copyright (C) 2017-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_midi_track_pattern_h_
#define __ardour_tracker_midi_track_pattern_h_

#include "midi++2/midi++/midnam_patch.h"

#include "midi_region_pattern.h"
#include "track_automation_pattern.h"
#include "track_pattern.h"

namespace Tracker {

/**
 * Represent patterns, midi notes, midi automations and track automations of a
 * given track, with possibly multiple regions, possibly overlapping, as long
 * as they all come from the same track.
 */
class MidiTrackPattern : public TrackPattern {
public:
	MidiTrackPattern (TrackerEditor& te,
	                  TrackPtr track,
	                  const std::vector<RegionView*>& region_views,
	                  const RegionSeq& regions,
	                  Temporal::timepos_t position,
	                  Temporal::timecnt_t length,
	                  Temporal::timepos_t end,
	                  Temporal::timepos_t nt_last,
	                  bool connect);
	virtual ~MidiTrackPattern ();

	void setup (const RegionSeq& regions);

	MidiTrackPattern& operator= (const MidiTrackPattern& other);

	// TODO: for now do not worry about memory leaking, create a new
	// PhenomenalDiff object at every call
	TrackPatternPhenomenalDiff* phenomenal_diff_ptr (const TrackPattern* prev) const;

	MidiTrackPatternPhenomenalDiff phenomenal_diff (const MidiTrackPattern& prev) const;

	std::string get_name (const IDParameter& id_param) const;

	void set_param_enabled (const IDParameter& id_param, bool enabled);
	bool is_param_enabled (const IDParameter& id_param) const;
	IDParameterSet get_enabled_parameters (int mri) const;

	// Enable/disable all tracks
	void enable ();
	void disable ();

	// Insert the automation control (s) corresponding to param (and connect it
	// to the grid for changes)
	void insert (const IDParameter& id_param);

	// Set the number of rows per beat. 0 means 1 row per bar (TODO: not fully
	// supported). After changing that you probably need to update the pattern,
	// see below.
	void set_rows_per_beat (uint16_t rpb);

	// Set position_row_beats, end_row_beats and nrows
	void set_row_range ();

	// Build or rebuild note and automation pattern
	void update ();

	// Update midi region patterns
	void update_midi_regions ();

	// Update enabled flag
	void update_enabled ();

	// Update row_offset (representing row offsets of region patterns)
	void update_row_offset ();

	// Return true iff the row is defined, that is if such a row points to an
	// existing region.
	bool is_region_defined (int rowi) const;

	// Return the row index relative to the start of the midi region pattern at
	// region index mri.
	int to_rrri (int rowi, int mri) const;

	// Return the row index relative to the start of the midi region pattern
	// pattern at region placed at index rowi.
	int to_rrri (int rowi) const;

	// Given the row index, calculate the corresponding midi region index. This
	// can only work assuming that regions do not overlap in time. If no such
	// mri is defined, then return -1.
	int to_mri (int rowi) const;

	// Number of note tracks
	uint16_t get_ntracks () const;

	// Set the number of note tracks
	void set_ntracks (uint16_t);

	// Increase the number of note tracks
	void inc_ntracks ();

	// Decrease the number of note tracks, if possible
	void dec_ntracks ();

	// Minimum number of note tracks required
	uint16_t get_nreqtracks () const;

	// Return whether the automation associated to param is empty
	bool is_empty (const IDParameter& id_param) const;

	// Return the sequence in chronological order of BBTs of each value of the
	// given automation at the given row and mri.
	std::vector<Temporal::BBT_Time> get_automation_bbt_seq (int rowi, int mri, const IDParameter& id_param) const;

	// Return a pair with the automation value and whether it is defined or not
	std::pair<double, bool> get_automation_value (int rowi, int mri, const IDParameter& id_param) const;

	// Return the sequence in chronological order of automation values at the
	// given row and mri.
	std::vector<double> get_automation_value_seq (int rowi, int mri, const IDParameter& id_param) const;

	// Return the automation interpolation value of param at given location
	double get_automation_interpolation_value (int rowi, int mri, const IDParameter& id_param) const;

	// Set the automation value val at rowi and mri for param
	void set_automation_value (double val, int rowi, int mri, const IDParameter& id_param, int delay);

	// Delete automation value at rowi and mri for param
	void delete_automation_value (int rowi, int mri, const IDParameter& id_param);

	// Return pair with automation delay in tick at rowi of param as first
	// element and whether it is defined as second element. Return (0, false) if
	// undefined.
	std::pair<int, bool> get_automation_delay (int rowi, int mri, const IDParameter& id_param) const;

	// Get a sequence in chronological order of automation delays in tick at the
	// given row amd mri.
	std::vector<int> get_automation_delay_seq (int rowi, int mri, const IDParameter& id_param) const;

	// Set the automation delay in tick at rowi, mri and mri for param
	void set_automation_delay (int delay, int rowi, int mri, const IDParameter& id_param);

	// Get the relative beats w.r.t. region position at rowi, and region mri
	Temporal::Beats region_relative_beats (int rowi, int mri, int delay) const;

	int64_t region_relative_delay_ticks (const Temporal::Beats& event_time, int rowi, int mri) const;
	bool is_automation_displayable (int rowi, int mri, const IDParameter& id_param) const;
	size_t control_events_count (int rowi, int mri, const IDParameter& id_param) const;

	// Return point of midi region pattern corresponding to midi_region, or 0 if it doesn't exist
	MidiRegionPattern* find_midi_region_pattern (MidiRegionPtr midi_region);

	std::shared_ptr<MIDI::Name::MasterDeviceNames> get_device_names () const;

	double lower (int rowi, const IDParameter& id_param) const;
	double upper (int rowi, const IDParameter& id_param) const;

	// Enable/disable midi track pattern and all its sub-patterns
	void set_enabled (bool e);

	// Select/deselect all region patterns
	void set_selected (bool s);

	// For representing pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	MidiTrackPtr midi_track;
	std::vector<RegionView*> rvs; // to get access to device names
	std::vector<MidiRegionPattern*> mrps;

	// Keep track of enabled region automations
	ParameterSet enabled_region_params;

	// Associate each mri to row_offset
	std::vector<int> row_offset;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_midi_track_pattern_h_ */
