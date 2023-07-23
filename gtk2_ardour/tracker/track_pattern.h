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
	              const RegionSeq& regions,
	              bool connect);
	TrackPattern (TrackerEditor& te,
	              TrackPtr track,
	              Temporal::samplepos_t pos,
	              Temporal::samplecnt_t len,
	              Temporal::samplepos_t fst,
	              Temporal::samplepos_t lst,
	              bool connect);
	virtual ~TrackPattern ();

	virtual void setup (const RegionSeq&);

	TrackPattern& operator= (const TrackPattern& other);

	// TODO: for now do not worry about memory leaking, create a new
	// PhenomenalDiff object at every call
	TrackPatternPhenomenalDiff* phenomenal_diff_ptr (const TrackPattern* prev) const;

	MidiTrackPtr midi_track ();
	AudioTrackPtr audio_track ();

	// Self cast classes
	bool is_midi_track_pattern () const;
	bool is_audio_track_pattern () const;
	const MidiTrackPattern* midi_track_pattern () const;
	const AudioTrackPattern* audio_track_pattern () const;
	MidiTrackPattern* midi_track_pattern ();
	AudioTrackPattern* audio_track_pattern ();

	// Build or rebuild note and automation pattern
	void update ();

	// Default implementation is for tracks not supporting regions
	// NEXT.2:
	// 1. Do not change that API, just make it work buggy like before
	// 2. Then change the API to make it work
	virtual Temporal::Beats region_relative_beats (int rowi, int mri, int delay) const;
	virtual int64_t region_relative_delay_ticks (const Temporal::Beats& event_time, int rowi, int mri) const;
	// NEXT.2:
	virtual bool is_automation_displayable (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual size_t control_events_count (int rowi, int mri, const Evoral::Parameter& param) const;
	// NEXT.14: delete
	// virtual RowToControlEventsRange control_events_range (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual bool is_region_defined (int rowi) const;
	virtual int to_rrri (int rowi, int mri) const;
	virtual int to_rrri (int rowi) const;
	virtual int to_mri (int rowi) const;
	virtual void insert (const Evoral::Parameter& param);

	// Return whether the automation associated to param is empty.  NEXT: this
	// will probably need to take a pair (Processor, Parameter).
	virtual bool is_empty (const Evoral::Parameter& param) const;

	virtual std::vector<Temporal::BBT_Time> get_automation_bbt_seq (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual std::pair<double, bool> get_automation_value (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual std::vector<double> get_automation_value_seq (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual double get_automation_interpolation_value (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual void set_automation_value (double val, int rowi, int mri, const Evoral::Parameter& param, int delay);
	virtual void delete_automation_value (int rowi, int mri, const Evoral::Parameter& param);
	virtual std::pair<int, bool> get_automation_delay (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual std::vector<int> get_automation_delay_seq (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual void set_automation_delay (int delay, int rowi, int mri, const Evoral::Parameter& param);
	virtual std::string get_name (const Evoral::Parameter& param) const;
	virtual void set_param_enabled (const Evoral::Parameter& param, bool enabled);
	virtual bool is_param_enabled (const Evoral::Parameter& param) const;
	virtual ParameterSet get_enabled_parameters (int mri) const;
	virtual double lower (int rowi, const Evoral::Parameter& param) const;
	virtual double upper (int rowi, const Evoral::Parameter& param) const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	TrackPtr track;

	TrackAllAutomationsPattern track_all_automations_pattern;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_track_pattern_h_ */
