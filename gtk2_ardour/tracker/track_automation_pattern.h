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
 * Data structure holding the automation list pattern of a track (as
 * opposed to a region).
 */
class TrackAutomationPattern : public AutomationPattern {
public:
	TrackAutomationPattern (TrackerEditor& te,
	                        TrackPtr track,
	                        const RegionSeq& regions, // NEXT: do we need that ctor?
	                        bool connect);
	TrackAutomationPattern (TrackerEditor& te,
	                        TrackPtr track,
	                        Temporal::samplepos_t position,
	                        Temporal::samplecnt_t length,
	                        Temporal::samplepos_t first_sample,
	                        Temporal::samplepos_t last_sample,
	                        bool connect);

	// Fill _automation_controls
	void setup_automation_controls ();
	// NEXT.12: maybe before having MainAutomationPattern and
	// ProcessorAutomationPattern inherit from TrackAutomationPattern, let's try
	// to simply use TrackAutomationPattern as attibute of TrackPattern (instead
	// of MainAutomationPattern and ProcessorAutomationPattern), that might be
	// simpler and better.
	void setup_main_automation_controls ();
	void setup_processors_automation_controls ();
	void setup_processor_automation_control (boost::weak_ptr<ARDOUR::Processor> p);

	// Insert the automation control corresponding to param in
	// _automation_controls, and connect it to the grid for connect changes.
	void insert (const Evoral::Parameter& param);

	// Assign a control event to a row
	virtual int event2row (const Evoral::Parameter& param, const Evoral::ControlEvent* event);

	TrackPtr track;
};

} // ~namespace tracker

#endif /* __ardour_tracker_track_automation_pattern_h_ */
