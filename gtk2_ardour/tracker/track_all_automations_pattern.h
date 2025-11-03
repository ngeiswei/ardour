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

#include "pbd/id.h"
#include "ardour/track.h"

#include "automation_pattern.h"
#include "main_automation_pattern.h"
#include "processor_automation_pattern.h"
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
	TrackAllAutomationsPattern (TrackerEditor& te,
	                            TrackPtr track,
	                            Temporal::timepos_t position,
	                            Temporal::timecnt_t length,
	                            Temporal::timepos_t end,
	                            Temporal::timepos_t nt_last,
	                            bool connect);

	TrackAllAutomationsPatternPhenomenalDiff phenomenal_diff (const TrackAllAutomationsPattern& prev) const;

	// Fill id_to_processor_automation_pattern
	void setup_processors_automation_controls ();
	void setup_processor_automation_control (std::weak_ptr<ARDOUR::Processor> p);

	// Set the number of rows per beat. 0 means 1 row per bar (TODO: not fully
	// supported). After changing that you probably need to update the pattern,
	// see below.
	void set_rows_per_beat (uint16_t rpb, bool refresh=false);

	// Set position_row_beats, end_row_beats and nrows
	void set_row_range ();

	// Update the pattern to reflect the current state
	void update ();

	// Insert the automation control corresponding to param in
	// _automation_controls, and connect it to the grid for connect changes.
	void insert (const IDParameter& id_param);

	// Return whether the automation associated to param is empty.
	bool is_empty (const IDParameter& id_param) const;

	bool is_displayable (int rowi, const IDParameter& id_param) const;
	size_t control_events_count (int rowi, const IDParameter& id_param) const;
	std::vector<Temporal::BBT_Time> get_automation_bbt_seq (int rowi, const IDParameter& id_param) const;
	std::pair<double, bool> get_automation_value (int rowi, const IDParameter& id_param) const;
	std::vector<double> get_automation_value_seq (int rowi, const IDParameter& id_param) const;
	double get_automation_interpolation_value (int rowi, const IDParameter& id_param) const;
	void set_automation_value (double val, int rowi, const IDParameter& id_param, int delay);
	void delete_automation_value (int rowi, const IDParameter& id_param);
	std::pair<int, bool> get_automation_delay (int rowi, const IDParameter& id_param) const;
	std::vector<int> get_automation_delay_seq (int rowi, const IDParameter& id_param) const;
	void set_automation_delay (int delay, int rowi, const IDParameter& id_param);
	std::string get_name (const IDParameter& id_param) const;
	void set_param_enabled (const IDParameter& id_param, bool enabled);
	bool is_param_enabled (const IDParameter& id_param) const;

	// Return the set of enabled automation parameters across all processors of
	// the track.
	IDParameterSet get_enabled_parameters () const;

	double lower (const IDParameter& id_param) const;
	double upper (const IDParameter& id_param) const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	TrackPtr track;

	// Automation pattern for Fade, Pan, Mute, etc.
	MainAutomationPattern main_automation_pattern;

	// Automation patterns for other processors, plugins, etc.
	std::map<PBD::ID, ProcessorAutomationPattern*> id_to_processor_automation_pattern;

	// Whether the pattern is supposed to connect to the tracker editor
	bool connect;
};

} // ~namespace tracker

#endif /* __ardour_tracker_track_all_automations_pattern_h_ */
