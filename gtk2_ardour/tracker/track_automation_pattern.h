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

#ifndef __ardour_tracker_track_automation_pattern_h_
#define __ardour_tracker_track_automation_pattern_h_

#include "ardour/track.h"

#include "automation_pattern.h"

namespace Tracker {

/**
 * Data structure holding the pattern for an automation list of a track (as
 * opposed to a region).  Such automation list typically belongs to a
 * processor, though it may also belong to the main route for fader, pan, etc.
 *
 * For the pattern holding all the track automations of a particular, see
 * TrackAllAutomationsPattern.
 */
class TrackAutomationPattern : public AutomationPattern {
public:
	TrackAutomationPattern (TrackerEditor& te,
	                        TrackPtr track,
	                        Temporal::timepos_t position,
	                        Temporal::timecnt_t length,
	                        Temporal::timepos_t end,
	                        Temporal::timepos_t nt_last,
	                        bool connect);

	// Insert the automation control corresponding to param in
	// _automation_controls, and connect it to the grid for connect changes.
	void insert (const Evoral::Parameter& param);

	// Return the (absolute) beats of a control event
	virtual Temporal::Beats event2beats (const Evoral::Parameter& param, const Evoral::ControlEvent* event);

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	TrackPtr track;
};

} // ~namespace tracker

#endif /* __ardour_tracker_track_automation_pattern_h_ */
