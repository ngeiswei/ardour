/*
 * Copyright (C) 2016-2023 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_track_all_automations_pattern_h_
#define __ardour_tracker_track_all_automations_pattern_h_

#include "ardour/track.h"

#include "automation_pattern.h"
#include "track_automation_pattern.h"
#include "track_all_automations_pattern_phenomenal_diff.h"

namespace Tracker {

/**
 * Data structure holding the pattern for all track automations of a particular
 * track.  This includes main automations such as fader, pan, etc, as well as
 * processor automations.  It does not however include region automations which
 * are hold in MidiRegionAutomationPattern.
 */
class TrackAllAutomationsPattern : public BasePattern {
public:
	TrackAllAutomationsPattern (TrackerEditor& te, // NEXT: do we need that ctor?
	                            TrackPtr track,
	                            const RegionSeq& regions,
	                            bool connect);
	TrackAllAutomationsPattern (TrackerEditor& te,
	                            TrackPtr track,
	                            Temporal::timepos_t position,
	                            Temporal::timecnt_t length,
	                            Temporal::timepos_t end,
	                            Temporal::timepos_t nt_last,
	                            bool connect);

	TrackAllAutomationsPatternPhenomenalDiff phenomenal_diff (const TrackAllAutomationsPattern& prev) const;

	// Fill _automation_controls
	void setup_automation_controls ();
	void setup_main_automation_controls ();
	void setup_processors_automation_controls ();
	void setup_processor_automation_control (std::weak_ptr<ARDOUR::Processor> p);

	// Update the pattern to reflect the current state
	void update ();

	// Insert the automation control corresponding to param in
	// _automation_controls, and connect it to the grid for connect changes.
	void insert (const Evoral::Parameter& param);

	// Return whether the automation associated to param is empty.  NEXT: this
	// will probably need to take a pair (Processor, Parameter).
	bool is_empty (const Evoral::Parameter& param) const;

	virtual const ParameterSet& automatable_parameters () const;

	bool is_displayable (int rowi, const Evoral::Parameter& param) const;
	size_t control_events_count (int rowi, const Evoral::Parameter& param) const;
	std::vector<Temporal::BBT_Time> get_automation_bbt_seq (int rowi, const Evoral::Parameter& param) const;
	std::pair<double, bool> get_automation_value (int rowi, const Evoral::Parameter& param) const;
	std::vector<double> get_automation_value_seq (int rowi, const Evoral::Parameter& param) const;
	double get_automation_interpolation_value (int rowi, const Evoral::Parameter& param) const;
	void set_automation_value (double val, int rowi, const Evoral::Parameter& param, int delay);
	void delete_automation_value (int rowi, const Evoral::Parameter& param);
	std::pair<int, bool> get_automation_delay (int rowi, const Evoral::Parameter& param) const;
	std::vector<int> get_automation_delay_seq (int rowi, const Evoral::Parameter& param) const;
	void set_automation_delay (int delay, int rowi, const Evoral::Parameter& param);
	std::string get_name (const Evoral::Parameter& param) const;
	void set_param_enabled (const Evoral::Parameter& param, bool enabled);
	bool is_param_enabled (const Evoral::Parameter& param) const;
	ParameterSet get_enabled_parameters () const;
	double lower (const Evoral::Parameter& param) const;
	double upper (const Evoral::Parameter& param) const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	TrackPtr track;

	// NEXT.12: replace by main automation pattern and vector of processor
	// automation patterns.
	TrackAutomationPattern track_automation_pattern;
};

} // ~namespace tracker

#endif /* __ardour_tracker_track_all_automations_pattern_h_ */
