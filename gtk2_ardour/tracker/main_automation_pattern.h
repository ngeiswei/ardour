/*
 * Copyright (C) 2021 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_main_automation_pattern_h_
#define __ardour_tracker_main_automation_pattern_h_

#include "track_automation_pattern.h"
// #include "main_automation_pattern_phenomenal_diff.h" // VERY VERY NEXT: is that really useful?  Can't we simply use automation_pattern_phenomenal_diff?

namespace Tracker {

// NEXT.4: alternatively, just have it as list of processors alongside the other plugins

/**
 * Data structure holding the automation list pattern for standard
 * main processings (fade, pan, etc).
 */
class MainAutomationPattern : public TrackAutomationPattern {
public:
	MainAutomationPattern (TrackerEditor& te,
	                       TrackPtr track,
	                       RegionSeq regions,
	                       bool connect);
	MainAutomationPattern (TrackerEditor& te,
	                       TrackPtr track,
	                       Temporal::samplepos_t position,
	                       Temporal::samplecnt_t length,
	                       Temporal::samplepos_t first_sample,
	                       Temporal::samplepos_t last_sample,
	                       bool connect);

	// Overload AutomationPattern::automatable_parameters ()
	const ParameterSet& automatable_parameters () const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

private:
	ParameterSet _automatable_parameters;
};

} // ~namespace tracker

#endif /* __ardour_tracker_main_automation_pattern_h_ */