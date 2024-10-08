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

namespace Tracker {

/**
 * Data structure holding the automation list pattern for common automations
 * across track and track type, such as fade, pan, mute, etc.
 */
class MainAutomationPattern : public TrackAutomationPattern {
public:
	MainAutomationPattern (TrackerEditor& te,
	                       TrackPtr track,
	                       Temporal::timepos_t position,
	                       Temporal::timecnt_t length,
	                       Temporal::timepos_t end,
	                       Temporal::timepos_t nt_last,
	                       bool connect);

	// Insert Gain, Trim, Mute and Pan automations
	void setup_main_automation_controls ();

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;
};

} // ~namespace tracker

#endif /* __ardour_tracker_main_automation_pattern_h_ */
