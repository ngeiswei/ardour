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

#include "region_automation_pattern.h"

using namespace std;
using namespace ARDOUR;

/////////////////////////////
// RegionAutomationPattern //
/////////////////////////////

RegionAutomationPattern::RegionAutomationPattern(ARDOUR::Session* session,
                                                 boost::shared_ptr<ARDOUR::Region> region,
                                                 const AutomationControlSet& auto_ctrls)
	: AutomationPattern(session, region, auto_ctrls)
{
}

uint32_t
RegionAutomationPattern::event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event)
{
	Temporal::Beats relative_beats(event->when);

	if (relative_beats < start_beats || start_beats + length_beats <= relative_beats)
		return UNDEFINED_ROW;

	Temporal::Beats beats(relative_beats + position_beats - start_beats);
	uint32_t row = row_at_beats(beats);
	if (automations[param].count(row) != 0)
		row = row_at_beats_min_delay(beats);
	return row;
}
