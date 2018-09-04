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

#include "automation_pattern.h"

/**
 * Data structure holding the automation list pattern held by a track.
 */
class TrackAutomationPattern : public AutomationPattern {
public:
	TrackAutomationPattern(const TrackerEditor& te,
	                       Temporal::samplepos_t position,
	                       Temporal::samplepos_t start,
	                       Temporal::samplecnt_t length,
	                       Temporal::samplepos_t first_sample,
	                       Temporal::samplepos_t last_sample);

	// Fill _automation_controls
	void setup_automation_controls ();
	void setup_main_automation_controls ();
	void setup_processors_automation_controls ();

	// Assign a control event to a row
	virtual uint32_t event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event);

	boost::shared_ptr<ARDOUR::MidiTrack> midi_track;
};

#endif /* __ardour_tracker_track_automation_pattern_h_ */
