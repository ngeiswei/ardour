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

#ifndef __ardour_tracker_automation_pattern_h_
#define __ardour_tracker_automation_pattern_h_

#include <set>

#include "evoral/ControlList.h"

#include "ardour/automation_control.h"
#include "ardour/automation_list.h"

#include "automation_pattern_phenomenal_diff.h"
#include "base_pattern.h"

namespace Tracker {

// AutomationControl as opposed to AutomationList is used to easily retrieve
// the associated Parameter of each AutomationList.
typedef std::map<AutomationControlPtr, std::string> AutomationControlStringMap;

/**
 * Data structure holding the automation list pattern.  Multiple
 * automations (typically belonging to a track) are stored here.
 */
class AutomationPattern : public BasePattern {
public:
	AutomationPattern (TrackerEditor& te,
	                   RegionPtr region,
	                   bool connect);
	AutomationPattern (TrackerEditor& te,
	                   Temporal::timepos_t position,
	                   Temporal::timepos_t start,
	                   Temporal::timecnt_t length,
	                   Temporal::timepos_t end,
	                   Temporal::timepos_t nt_last,
	                   bool connect);

	// Make a deep copy of the automation controls
	AutomationPattern& operator= (const AutomationPattern& other);
	AutomationControlPtr clone_actl (AutomationControlPtr actl) const;
	ParamAutomationControlPair clone_param_actl (const ParamAutomationControlPair& param_name) const;
	AutomationListPtr clone_alist (AutomationListPtr alist) const;

	void rows_diff (const RowToControlEvents& l_row2ces, const RowToControlEvents& r_row2ces, std::set<int>& rd) const;

	AutomationPatternPhenomenalDiff phenomenal_diff (const AutomationPattern& prev) const;

	// Return the (absolute) beats of a control event
	virtual Temporal::Beats event2beats (const Evoral::Parameter& param, const Evoral::ControlEvent* event);

	// Build or rebuild the pattern (implement BasePattern::update ())
	virtual void update ();
	void update_automations ();

	// Add an automation control in the automation control set and connect it to
	// the grid to update it when some value changes
	void insert_actl (AutomationControlPtr actl, const std::string& name);

	// Retrieve the AutomationControlPtr and its name corresponding to
	// the given parameter, and insert it.
	//
	// Purely virtual because the modality of insertion depends on the
	// automation pattern.  MidiRegionAutomationPattern requires a
	// midi_model, while TrackAutomationPattern requires a track.
	virtual void insert (const Evoral::Parameter& param) = 0;

	// Return whether the automation associated to param is empty.
	virtual bool is_empty (const Evoral::Parameter& param) const;

	// Return automation control associated to the given parameter. If absent,
	// return 0.
	AutomationControlPtr get_actl (const Evoral::Parameter& param);
	const AutomationControlPtr get_actl (const Evoral::Parameter& param) const;

	// Return the number of values within the same row. If undefined return 0.
	size_t control_events_count (int rowi, const Evoral::Parameter& param) const;

	// Return a range of iterators of all events at the given row index and
	// parameter.
	//
	// Warning: if no such event exist the behavior is undefined (it might even
	// crash).
	RowToControlEventsRange control_events_range (int rowi, const Evoral::Parameter& param) const;

	virtual std::string get_name (const Evoral::Parameter& param) const;

	// Return automation list associated to the given parameter. If absent
	// return 0.
	AutomationListPtr get_alist (const Evoral::Parameter& param);
	const AutomationListPtr get_alist (const Evoral::Parameter& param) const;

	// Return true iff the automation point is displayable, i.e. iff there is
	// only one of them.
	bool is_displayable (int row, const Evoral::Parameter& param) const;
	static bool is_displayable (int row, const RowToControlEvents& r2ces);

	// Return the control list iterator associated to param at rowi if exists or
	// end () pointer if it does not.
	//
	// Warning: it assumes the iterator exists, otherwise it crashes or returns garbage!
	AutomationListIt get_alist_iterator (int rowi, const Evoral::Parameter& param);

	// Return some control event associated to param at rowi if exists or null
	// pointer if it does not.  If more than one control event is present at
	// that row, then return the first one according to the order specified by
	// the RowToControlEvents multimap, which may not be the chronological
	// order.
	Evoral::ControlEvent* get_control_event (int rowi, const Evoral::Parameter& param);
	const Evoral::ControlEvent* get_control_event (int rowi, const Evoral::Parameter& param) const;

	// Return the sequence in chronological order of BBTs of each value of the
	// given automation at the given row.
	std::vector<Temporal::BBT_Time> get_automation_bbt_seq (int rowi, const Evoral::Parameter& param) const;

	// Return a pair with some automation value at rowi and whether it exists or
	// not.  If multiple automation values exist at that row, then return the
	// first one according to some arbitrary order, not necessarily the
	// chronological order.
	std::pair<double, bool> get_automation_value (int rowi, const Evoral::Parameter& param) const;
	double get_automation_value (RowToControlEvents::const_iterator it) const;

	// Return the sequence in chronological order of automation values at the
	// given row.
	std::vector<double> get_automation_value_seq (int rowi, const Evoral::Parameter& param) const;

	// Return the automation interpolation value of a given param at a given row index
	double get_automation_interpolation_value (int rowi, const Evoral::Parameter& param) const;

	// Set the automation value val at rowi for param
	void set_automation_value (double val, int rowi, const Evoral::Parameter& param, int delay);

	// Delete automation value at rowi for param
	void delete_automation_value (int rowi, const Evoral::Parameter& param);

	// Return pair with automation delay in tick at rowi of param as first
	// element and whether it is defined as second element. Return (0, false) if
	// undefined.
	std::pair<int, bool> get_automation_delay (int rowi, const Evoral::Parameter& param) const;
	std::pair<int, bool> get_automation_delay (int rowi, const Evoral::Parameter& param, const Evoral::ControlEvent* ce) const;
	int get_automation_delay (const Evoral::Parameter& param, RowToControlEvents::const_iterator it) const;

	// Get a sequence in chronological order of automation delays in tick at the
	// given row.
	std::vector<int> get_automation_delay_seq (int rowi, const Evoral::Parameter& param) const;

	// Set the automation delay in tick at rowi, mri and mri for param
	void set_automation_delay (int delay, int rowi, const Evoral::Parameter& param);

	// Get the bbt of an automation point
	Temporal::BBT_Time get_automation_bbt (const Evoral::Parameter& param, RowToControlEvents::const_iterator it) const;

	// Add, modidy or erase automation point, and record undo history
	void add_automation_point (AutomationListPtr alist, Temporal::timepos_t when, double val);
	void modify_automation_point (AutomationListPtr alist, AutomationListIt it, Temporal::timepos_t when, double val);
	void erase_automation_point (AutomationListPtr alist, AutomationListIt it);

	void register_automation_undo (AutomationListPtr alist, const std::string& opname, XMLNode& before, XMLNode& after);

	// Return RowToAutomationListIt corresponding to the previous (resp. next)
	// event. If there is no such previous or next event then return end ()
	// iterator.
	RowToControlEvents::const_iterator find_prev (RowToControlEvents::const_iterator it) const;
	RowToControlEvents::const_iterator find_next (RowToControlEvents::const_iterator it) const;
	RowToControlEvents::const_iterator find_prev (int row, const RowToControlEvents& row2auto) const;
	RowToControlEvents::const_iterator find_next (int row, const RowToControlEvents& row2auto) const;

	// Return the range of rows that must be updated if the event on the given
	// row is modified
	std::pair<int, int> prev_next_range (RowToControlEvents::const_iterator it, const RowToControlEvents& row2ces) const;
	std::pair<int, int> prev_next_range (int row, const RowToControlEvents& row2ces) const;

	RowToControlEvents::const_iterator earliest (const RowToControlEventsRange& rng) const;
	RowToControlEvents::const_iterator lattest (const RowToControlEventsRange& rng) const;

	void set_param_enabled (const Evoral::Parameter& param, bool enabled);
	bool is_param_enabled (const Evoral::Parameter& param) const;

	// Return the set of enabled parameters, that is any parameter p such that
	// param_to_enabled[p] is true.
	ParameterSet get_enabled_parameters () const;

	// Return the lower and upper value bounds of the given parameter
	double lower (const Evoral::Parameter& param) const;
	double upper (const Evoral::Parameter& param) const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	// Map parameters to maps of row to automation range.  Note that
	// Evoral::parameter do not uniquely identify parameters across
	// plugins, so such mapping is only valid within single processor,
	// main or midi parameter set.
	typedef std::map<Evoral::Parameter, RowToControlEvents> ParamToRowToControlEvents;
	ParamToRowToControlEvents param_to_row_to_ces;

	// Map parameters to actl. See comment about the non-uniqueness of
	// Evoral::parameter above.
	ParamAutomationControlMap param_to_actl;

	// Map parameters to name. See comment about the non-uniqueness of
	// Evoral::parameter above.
	typedef std::map<Evoral::Parameter, std::string> ParamNameMap;
	ParamNameMap param_to_name;

	// Map parameters to whether the automation is enabled. This is
	// kept track of here in case it can affect updating its content,
	// to save calculations or measure its phenomenal difference. See
	// comment about the non-uniqueness of Evoral::parameter above.
	typedef std::map<Evoral::Parameter, bool> ParamEnabledMap;
	mutable ParamEnabledMap param_to_enabled; // disabled by default

	// Whether the pattern is supposed to connect to the tracker editor
	bool connect;
};

} // ~namespace tracker

#endif /* __ardour_tracker_automation_pattern_h_ */
