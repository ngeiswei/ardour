/*
    Copyright (C) 2016 Nil Geisweiller

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

#include <cmath>
#include <map>

#include "automation_pattern.h"

using namespace std;
using namespace ARDOUR;
using Timecode::BBT_Time;

///////////////////////
// AutomationPattern //
///////////////////////

AutomationPattern::AutomationPattern(ARDOUR::Session* session,
                                     boost::shared_ptr<ARDOUR::Region> region,
                                     const AutomationControlSet& auto_ctrls)
	: Pattern(session, region), _automation_controls(auto_ctrls)
{
}

void AutomationPattern::update_pattern()
{
	set_row_range();

	automations.clear();

	for (AutomationControlSet::const_iterator actrl = _automation_controls.begin(); actrl != _automation_controls.end(); ++actrl) {
		boost::shared_ptr<AutomationList> al = (*actrl)->alist();
		const Evoral::Parameter& param = (*actrl)->parameter();
		// Build automation pattern
		for (AutomationList::iterator it = al->begin(); it != al->end(); ++it) {
			uint32_t row = control_event2row(param, *it);
			automations[param].insert(RowToAutomationIt::value_type(row, it));
		}
	}
}

bool AutomationPattern::is_displayable(uint32_t row, const Evoral::Parameter& param) const
{
	return automations.find(param) == automations.end()
		|| automations.find(param)->second.count(row) <= 1;
}