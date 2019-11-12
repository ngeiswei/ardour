/*
 * Copyright (C) 2016-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_region_automation_pattern_h_
#define __ardour_tracker_region_automation_pattern_h_

#include "ardour/midi_model.h"
#include "ardour/midi_track.h"
#include "ardour/region.h"

#include "automation_pattern.h"
#include "region_automation_pattern_phenomenal_diff.h"

namespace Tracker {

/**
 * Data structure holding the automation list pattern held by a region.
 */
class RegionAutomationPattern : public AutomationPattern {
public:
	RegionAutomationPattern (TrackerEditor& te,
	                         MidiTrackPtr midi_track,
	                         MidiRegionPtr region);

	RegionAutomationPattern& operator= (const RegionAutomationPattern& other);

	RegionAutomationPatternPhenomenalDiff phenomenal_diff (const RegionAutomationPattern& prev) const;

	// Insert all existing region (midi) automation controls in
	// _automation_controls and connect then to the grid
	void setup_automation_controls ();

	// Insert automation control corresponding to param in _automation_controls
	// (and connect it to the grid for changes)
	void insert (const Evoral::Parameter& param);

	// Assign a control event to a row
	virtual uint32_t event2row (const Evoral::Parameter& param, const Evoral::ControlEvent* event);

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	MidiTrackPtr midi_track;
	MidiModelPtr midi_model;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_region_automation_pattern_h_ */
