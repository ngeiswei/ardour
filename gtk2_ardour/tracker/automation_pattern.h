/*
    Copyright (C) 2015-2017 Nil Geisweiller

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

#ifndef __ardour_tracker_automation_pattern_h_
#define __ardour_tracker_automation_pattern_h_

#include <set>

#include "ardour/automation_control.h"
#include "ardour/automation_list.h"
#include "evoral/ControlList.hpp"

#include "automation_pattern_phenomenal_diff.h"
#include "base_pattern.h"
#include "tracker_types.h"

namespace Tracker {

// AutomationControl as opposed to AutomationList is used to easily retrieve
// the associated Parameter of each AutomationList.
typedef std::set<boost::shared_ptr<ARDOUR::AutomationControl> > AutomationControlSet;

/**
 * Data structure holding the automation list pattern.
 */
class AutomationPattern : public BasePattern {
public:
	AutomationPattern(TrackerEditor& te,
	                  boost::shared_ptr<ARDOUR::Region> region);
	AutomationPattern(TrackerEditor& te,
	                  Temporal::samplepos_t position,
	                  Temporal::samplepos_t start,
	                  Temporal::samplecnt_t length,
	                  Temporal::samplepos_t first_sample,
	                  Temporal::samplepos_t last_sample);

	typedef ARDOUR::AutomationList::iterator AutomationListIt;
	typedef std::multimap<uint32_t, AutomationListIt> RowToAutomationIt;

	// Make a deep copy of the automation controls
	AutomationPattern& operator=(const AutomationPattern& other);
	boost::shared_ptr<ARDOUR::AutomationControl> clone_actrl(boost::shared_ptr<ARDOUR::AutomationControl> actrl) const;

	static void rows_diff(const RowToAutomationIt& l_row2auto, const RowToAutomationIt& r_row2auto, std::set<size_t>& rd);

	AutomationPatternPhenomenalDiff phenomenal_diff(const AutomationPattern& other) const;

	// Assign a control event to a row
	virtual uint32_t event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event);

	// Build or rebuild the pattern (implement BasePattern::update())
	virtual void update();
	void update_automations();

	// Add an automation control in the automation control set and connect it to
	// the grid to update it when some value changes
	void insert(boost::shared_ptr<ARDOUR::AutomationControl> actrl);
	virtual void insert(const Evoral::Parameter& param) = 0;

	// Return whether the automation associated to param is empty.
	virtual bool is_empty (const Evoral::Parameter& param) const;

	// Return automation control associated to the given parameter. If absent,
	// return 0.
	boost::shared_ptr<ARDOUR::AutomationControl> get_actrl(const Evoral::Parameter& param);
	const boost::shared_ptr<ARDOUR::AutomationControl> get_actrl(const Evoral::Parameter& param) const;

	// Return automation list associated to the given parameter. If absent
	// return 0.
	boost::shared_ptr<ARDOUR::AutomationList> get_alist (const Evoral::Parameter& param);
	const boost::shared_ptr<ARDOUR::AutomationList> get_alist (const Evoral::Parameter& param) const;
	
	// Return true iff the automation point is displayable, i.e. iff there is
	// only one of them.
	bool is_displayable(uint32_t row, const Evoral::Parameter& param) const;

	// Return the control list iterator associated to param at rowi if exists or
	// end() pointer if it does not.
	//
	// Warning: it assumes the iterator exists, otherwise it crashes or returns garbage!
	AutomationListIt get_alist_iterator (size_t rowi, const Evoral::Parameter& param);

	// Return the control event associated to param at rowi if exists or null
	// pointer if it does not.
	Evoral::ControlEvent* get_control_event (size_t rowi, const Evoral::Parameter& param);
	const Evoral::ControlEvent* get_control_event (size_t rowi, const Evoral::Parameter& param) const;
	
	// Return a pair with the automation value and whether it is defined or not	
	std::pair<double, bool> get_automation_value (size_t rowi, const Evoral::Parameter& param);

	// Set the automation value val at rowi for param
	void set_automation_value (double val, size_t rowi, const Evoral::Parameter& param, int delay);

	// Delete automation value at rowi for param
	void delete_automation_value (int rowi, const Evoral::Parameter& param);

	// Return pair with automation delay in tick at rowi of param as first
	// element and whether it is defined as second element. Return (0, false) if
	// undefined.
	std::pair<int, bool> get_automation_delay (int rowi, const Evoral::Parameter& param);

	// Set the automation delay in tick at rowi, mri and mri for param
	void set_automation_delay (int delay, int rowi, const Evoral::Parameter& param);

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string() const;
	virtual std::string to_string(const std::string& indent = std::string()) const;

	// Map parameters to maps of row to automation range
	typedef std::map<Evoral::Parameter, RowToAutomationIt> ParamToRowToAutomationIt;
	ParamToRowToAutomationIt automations;

protected:
	AutomationControlSet _automation_controls;
};

} // ~namespace tracker

#endif /* __ardour_tracker_automation_pattern_h_ */
