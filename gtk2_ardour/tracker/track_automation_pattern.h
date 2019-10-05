/*
    Copyright (C) 2016-2017 Nil Geisweiller

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

#ifndef __ardour_tracker_track_automation_pattern_h_
#define __ardour_tracker_track_automation_pattern_h_

#include "ardour/track.h"

#include "track_pattern.h"

namespace Tracker {

/**
 * Data structure holding the automation list pattern held by a track.
 */
class TrackAutomationPattern : public TrackPattern {
public:
	TrackAutomationPattern(TrackerEditor& te,
	                       boost::shared_ptr<ARDOUR::Track> track,
	                       const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions);
	TrackAutomationPattern(TrackerEditor& te,
	                       boost::shared_ptr<ARDOUR::Track> track,
	                       Temporal::samplepos_t position,
	                       Temporal::samplecnt_t length,
	                       Temporal::samplepos_t first_sample,
	                       Temporal::samplepos_t last_sample);

	// Fill _automation_controls
	void setup_automation_controls ();
	void setup_main_automation_controls ();
	void setup_processors_automation_controls ();
	void setup_processor_automation_control (boost::weak_ptr<ARDOUR::Processor> p);

	// Insert the automation control corresponding to param in
	// _automation_controls, and connect it to the grid for connect changes.
	void insert(const Evoral::Parameter& param);

	// Assign a control event to a row
	virtual uint32_t event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event);
};

} // ~namespace tracker

#endif /* __ardour_tracker_track_automation_pattern_h_ */
