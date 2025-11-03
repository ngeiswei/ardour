/*
 * Copyright (C) 2018-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_track_pattern_h_
#define __ardour_tracker_track_pattern_h_

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/region.h"
#include "ardour/track.h"

#include "base_pattern.h"
#include "automation_pattern.h"
#include "track_all_automations_pattern.h"
#include "track_pattern_phenomenal_diff.h"
#include "tracker_utils.h"

namespace Tracker {

class MidiTrackPattern;
class AudioTrackPattern;

/**
 * Abstract class to represent track patterns (midi, audio, etc).
 */
class TrackPattern : public BasePattern {
public:
	TrackPattern (TrackerEditor& te,
	              TrackPtr track,
	              Temporal::timepos_t position,
	              Temporal::timecnt_t length,
	              Temporal::timepos_t end,
	              Temporal::timepos_t nt_last,
	              bool connect);
	virtual ~TrackPattern ();

	virtual void setup (const RegionSeq&);

	TrackPattern& operator= (const TrackPattern& other);

	virtual TrackPatternPhenomenalDiff* phenomenal_diff_ptr (const TrackPattern* prev) const = 0;

	MidiTrackPtr midi_track ();
	AudioTrackPtr audio_track ();

	// Set the number of rows per beat. 0 means 1 row per bar.  Overload
	// BasePattern::set_rows_per_beat to trickle down to track automations and
	// such.
	virtual void set_rows_per_beat (uint16_t rpb, bool refresh=false);

	// Set position_row_beats, end_row_beats and nrows.  Overload
	// BasePattern::set_rows_per_beat to trickle down to track automations and
	// such.
	virtual void set_row_range ();

	// Self cast classes
	bool is_midi_track_pattern () const;
	bool is_audio_track_pattern () const;
	const MidiTrackPattern* midi_track_pattern () const;
	const AudioTrackPattern* audio_track_pattern () const;
	MidiTrackPattern* midi_track_pattern ();
	AudioTrackPattern* audio_track_pattern ();

	// Build or rebuild note and automation pattern
	virtual void update ();

	// Default implementation is for tracks not supporting regions
	virtual Temporal::Beats region_relative_beats (int rowi, int mri, int delay) const;
	virtual int64_t region_relative_delay_ticks (const Temporal::Beats& event_time, int rowi, int mri) const;
	virtual bool is_automation_displayable (int rowi, int mri, const IDParameter& id_param) const;
	virtual size_t control_events_count (int rowi, int mri, const IDParameter& id_param) const;
	virtual bool is_region_defined (int rowi) const;
	virtual int to_rrri (int rowi, int mri) const;
	virtual int to_rrri (int rowi) const;
	virtual int to_mri (int rowi) const;
	virtual void insert (const IDParameter& id_param);

	// Return whether the automation associated to param is empty.
	virtual bool is_empty (const IDParameter& id_param) const;

	// Return the sequence in chronological order of BBTs of each value of the
	// given automation at the given row and mri.
	virtual std::vector<Temporal::BBT_Time> get_automation_bbt_seq (int rowi, int mri, const IDParameter& id_param) const;

	// Return a pair with the automation value and whether it is defined or not
	virtual std::pair<double, bool> get_automation_value (int rowi, int mri, const IDParameter& id_param) const;

	// Return the sequence in chronological order of automation values at the
	// given row and mri.
	virtual std::vector<double> get_automation_value_seq (int rowi, int mri, const IDParameter& id_param) const;

	// Return the automation interpolation value of param at given location
	virtual double get_automation_interpolation_value (int rowi, int mri, const IDParameter& id_param) const;

	// Set the automation value val at rowi and mri for param
	virtual void set_automation_value (double val, int rowi, int mri, const IDParameter& id_param, int delay);

	// Delete automation value at rowi and mri for param
	virtual void delete_automation_value (int rowi, int mri, const IDParameter& id_param);

	// Return pair with automation delay in tick at rowi of param as first
	// element and whether it is defined as second element. Return (0, false) if
	// undefined.
	virtual std::pair<int, bool> get_automation_delay (int rowi, int mri, const IDParameter& id_param) const;

	// Get a sequence in chronological order of automation delays in tick at the
	// given row amd mri.
	virtual std::vector<int> get_automation_delay_seq (int rowi, int mri, const IDParameter& id_param) const;

	// Set the automation delay in tick at rowi, mri and mri for param
	virtual void set_automation_delay (int delay, int rowi, int mri, const IDParameter& id_param);

	virtual std::string get_name (const IDParameter& id_param) const;

	virtual void set_param_enabled (const IDParameter& id_param, bool enabled);

	virtual bool is_param_enabled (const IDParameter& id_param) const;

	virtual IDParameterSet get_enabled_parameters (int mri) const;

	virtual double lower (int rowi, const IDParameter& id_param) const;

	virtual double upper (int rowi, const IDParameter& id_param) const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	TrackPtr track;

	TrackAllAutomationsPattern track_all_automations_pattern;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_track_pattern_h_ */
