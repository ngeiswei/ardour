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

#include "base_pattern.h"
#include "tracker_types.h"

namespace ARDOUR {
	class Session;
	class AutomationControl;
};

// AutomationControl as opposed to AutomationList is used to easily retrieve
// the associated Parameter of each AutomationList.
typedef std::set<boost::shared_ptr<ARDOUR::AutomationControl> > AutomationControlSet;

/**
 * Data structure holding the automation list pattern.
 */
class AutomationPattern : public BasePattern {
public:
	AutomationPattern(const TrackerEditor& te,
	                  boost::shared_ptr<ARDOUR::Region> region);
	AutomationPattern(const TrackerEditor& te,
	                  Temporal::samplepos_t position,
	                  Temporal::samplepos_t start,
	                  Temporal::samplecnt_t length,
	                  Temporal::samplepos_t first_sample,
	                  Temporal::samplepos_t last_sample);

	typedef ARDOUR::AutomationList::iterator AutomationListIt;
	typedef std::multimap<uint32_t, AutomationListIt> RowToAutomationIt;

	// Assign a control event to a row
	virtual uint32_t event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event) = 0;

	// Build or rebuild the pattern (implement BasePattern::update_pattern)
	void update();

	// Add an automation control in the automation control set
	void insert(boost::shared_ptr<ARDOUR::AutomationControl> actrl);

	// Return automation control associated to the given parameter
	boost::shared_ptr<ARDOUR::AutomationControl> get_actl(const Evoral::Parameter& param);

	// Return all automation controls
	const AutomationControlSet& get_actls() const;
	
	// Return true iff the automation point is displayable, i.e. iff there is
	// only one of them.
	bool is_displayable(uint32_t row, const Evoral::Parameter& param) const;

	// Map parameters to maps of row to automation range
	std::map<Evoral::Parameter, RowToAutomationIt> automations;

protected:
	AutomationControlSet _automation_controls;
};

#endif /* __ardour_tracker_automation_pattern_h_ */
