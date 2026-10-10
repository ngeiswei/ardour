/*
 * Copyright (C) 2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_midi_region_automation_pattern_phenomenal_diff_h_
#define __ardour_tracker_midi_region_automation_pattern_phenomenal_diff_h_

#include <map>

#include "evoral/Parameter.h"

#include "automation_pattern_phenomenal_diff.h"
#include "base_pattern_phenomenal_diff.h"
#include "rows_phenomenal_diff.h"

namespace Tracker {

// Represent the differences that may impact grid rendition.
struct MidiRegionAutomationPatternPhenomenalDiff : public BasePatternPhenomenalDiff
{
	AutomationPatternPhenomenalDiff ap_diff;

	bool empty () const;
	std::string to_string (const std::string& indent = std::string ()) const;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_midi_region_automation_pattern_phenomenal_diff_h_ */
