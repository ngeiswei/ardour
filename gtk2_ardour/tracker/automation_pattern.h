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
#include "tracker_types.h"

namespace Tracker {

// AutomationControl as opposed to AutomationList is used to easily retrieve
// the associated Parameter of each AutomationList.
typedef std::map<AutomationControlPtr, std::string> AutomationControlStringMap;

/**
 * Data structure holding the automation list pattern.
 */
class AutomationPattern : public BasePattern {
public:
	AutomationPattern (TrackerEditor& te,
	                   RegionPtr region,
	                   bool connect);
	AutomationPattern (TrackerEditor& te,
	                   Temporal::samplepos_t position,
	                   Temporal::samplepos_t start,
	                   Temporal::samplecnt_t length,
	                   Temporal::samplepos_t first_sample,
	                   Temporal::samplepos_t last_sample,
	                   bool connect);

	typedef ARDOUR::AutomationList::iterator AutomationListIt;
	typedef std::multimap<uint32_t, AutomationListIt> RowToAutomationListIt;
	typedef std::pair<RowToAutomationListIt::const_iterator, RowToAutomationListIt::const_iterator> RowToAutomationListItRange;

	// Make a deep copy of the automation controls
	AutomationPattern& operator= (const AutomationPattern& other);
	AutomationControlPtr clone_actrl (AutomationControlPtr actrl) const;
	ParamAutomationControlPair clone_param_actrl (const ParamAutomationControlPair& param_name) const;
	AutomationListPtr clone_alist (AutomationListPtr alist) const;

	void rows_diff (const RowToAutomationListIt& l_row2auto, const RowToAutomationListIt& r_row2auto, std::set<size_t>& rd) const;

	AutomationPatternPhenomenalDiff phenomenal_diff (const AutomationPattern& prev) const;

	// Assign a control event to a row
	virtual uint32_t event2row (const Evoral::Parameter& param, const Evoral::ControlEvent* event);

	// Build or rebuild the pattern (implement BasePattern::update ())
	virtual void update ();
	void update_automations ();

	// Add an automation control in the automation control set and connect it to
	// the grid to update it when some value changes
	void insert (AutomationControlPtr actrl, const std::string& name);
	virtual void insert (const Evoral::Parameter& param) = 0;

	// Return whether the automation associated to param is empty.
	virtual bool is_empty (const Evoral::Parameter& param) const;

	// Return automation control associated to the given parameter. If absent,
	// return 0.
	AutomationControlPtr get_actrl (const Evoral::Parameter& param);
	const AutomationControlPtr get_actrl (const Evoral::Parameter& param) const;

	// Return the number of values within the same row. If undefined return 0.
	size_t get_automation_list_count (uint32_t rowi, const Evoral::Parameter& param) const;

	virtual std::string get_name (const Evoral::Parameter& param) const;

	// Return automation list associated to the given parameter. If absent
	// return 0.
	AutomationListPtr get_alist (const Evoral::Parameter& param);
	const AutomationListPtr get_alist (const Evoral::Parameter& param) const;
	
	// Return true iff the automation point is displayable, i.e. iff there is
	// only one of them.
	bool is_displayable (uint32_t row, const Evoral::Parameter& param) const;
	static bool is_displayable (uint32_t row, RowToAutomationListIt r2a);

	// Return the control list iterator associated to param at rowi if exists or
	// end () pointer if it does not.
	//
	// Warning: it assumes the iterator exists, otherwise it crashes or returns garbage!
	AutomationListIt get_alist_iterator (size_t rowi, const Evoral::Parameter& param);

	// Return the control event associated to param at rowi if exists or null
	// pointer if it does not.
	Evoral::ControlEvent* get_control_event (size_t rowi, const Evoral::Parameter& param);
	const Evoral::ControlEvent* get_control_event (size_t rowi, const Evoral::Parameter& param) const;
	
	// Return a pair with the automation value and whether it is defined or not	
	std::pair<double, bool> get_automation_value (size_t rowi, const Evoral::Parameter& param) const;

	// Set the automation value val at rowi for param
	void set_automation_value (double val, size_t rowi, const Evoral::Parameter& param, int delay);

	// Delete automation value at rowi for param
	void delete_automation_value (int rowi, const Evoral::Parameter& param);

	// Return pair with automation delay in tick at rowi of param as first
	// element and whether it is defined as second element. Return (0, false) if
	// undefined.
	std::pair<int, bool> get_automation_delay (int rowi, const Evoral::Parameter& param) const;

	// Set the automation delay in tick at rowi, mri and mri for param
	void set_automation_delay (int delay, int rowi, const Evoral::Parameter& param);

	// Add, modidy or erase automation point, and record undo history
	void add_automation_point (AutomationListPtr alist, double when, double val);
	void modify_automation_point (AutomationListPtr alist, AutomationListIt it, double when, double val);
	void erase_automation_point (AutomationListPtr alist, AutomationListIt it);

	// Return RowToAutomationListIt corresponding to the previous (resp. next)
	// event. If there is no such previous or next event then return end ()
	// iterator.
	RowToAutomationListIt::const_iterator find_prev (RowToAutomationListIt::const_iterator it) const;
	RowToAutomationListIt::const_iterator find_next (RowToAutomationListIt::const_iterator it) const;
	RowToAutomationListIt::const_iterator find_prev (uint32_t row, const RowToAutomationListIt& row2auto) const;
	RowToAutomationListIt::const_iterator find_next (uint32_t row, const RowToAutomationListIt& row2auto) const;

	// Return the range of rows that must be updateing if the event on the given
	// row of it is modified
	std::pair<uint32_t, uint32_t> prev_next_range (RowToAutomationListIt::const_iterator it, const RowToAutomationListIt& row2auto) const;
	std::pair<uint32_t, uint32_t> prev_next_range (uint32_t row, const RowToAutomationListIt& row2auto) const;

	RowToAutomationListIt::const_iterator earliest (const RowToAutomationListItRange& rng) const;
	RowToAutomationListIt::const_iterator lattest (const RowToAutomationListItRange& rng) const;

	virtual void set_param_enabled (const Evoral::Parameter& param, bool enabled);
	virtual bool is_param_enabled (const Evoral::Parameter& param) const;

	// Return the lower and upper value bounds of the given parameter
	double lower (const Evoral::Parameter& param) const;
	double upper (const Evoral::Parameter& param) const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	// Map parameters to maps of row to automation range
	typedef std::map<Evoral::Parameter, RowToAutomationListIt> ParamToRowToAutomationListIt;
	ParamToRowToAutomationListIt param_to_row_to_ali;

	// Map parameters to actrl
	ParamAutomationControlMap param_to_actrl;

	// Map parameters to name
	typedef std::map<Evoral::Parameter, std::string> ParamNameMap;
	ParamNameMap param_to_name;

	// Map parameters to whether the automation is enabled. This is kept track
	// of here in case it can affect updating its content, to save calculations
	// or measure its phenomenal difference.
	typedef std::map<Evoral::Parameter, bool> ParamEnabledMap;
	mutable ParamEnabledMap param_to_enabled; // disabled by default

protected:
	bool _connect;
};

} // ~namespace tracker

#endif /* __ardour_tracker_automation_pattern_h_ */
