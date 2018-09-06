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

#include <cmath>
#include <map>

#include "track_automation_pattern.h"
#include "ardour/region.h"

using namespace std;
using namespace ARDOUR;

////////////////////////////
// TrackAutomationPattern //
////////////////////////////

TrackAutomationPattern::TrackAutomationPattern(const TrackerEditor& te,
                                               boost::shared_ptr<ARDOUR::MidiTrack> mt,
                                               Temporal::samplepos_t pos,
                                               Temporal::samplecnt_t len,
                                               Temporal::samplepos_t fir,
                                               Temporal::samplepos_t las)
	: AutomationPattern(te, pos, 0, len, fir, las)
	, midi_track(mt)
{
	setup_automation_controls ();
}

void TrackAutomationPattern::setup_automation_controls ()
{
	setup_main_automation_controls ();
	setup_processors_automation_controls ();
}

void TrackAutomationPattern::setup_main_automation_controls ()
{
	// Gain
	insert(midi_track->gain_control());
	tracker_editor.automation_connect(midi_track->gain_control());

	// Mute
	insert(midi_track->mute_control());
	tracker_editor.automation_connect(midi_track->mute_control());

	// Pan
	set<Evoral::Parameter> const & pan_params = midi_track->pannable()->what_can_be_automated ();
	for (set<Evoral::Parameter>::const_iterator p = pan_params.begin(); p != pan_params.end(); ++p) {
		insert(midi_track->pannable()->automation_control(*p));
		tracker_editor.automation_connect(param2actrl, *p);
	}
}

void TrackAutomationPattern::insert(const Evoral::Parameter& param)
{
	insert(midi_track->automation_control(param, true));
}

uint32_t
TrackAutomationPattern::event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event)
{
	samplepos_t sample = event->when;

	if (sample < first_sample || last_sample < sample)
		return INVALID_ROW;

	uint32_t row = row_at_sample(sample);
	if (automations[param].count(row) != 0)
		row = row_at_sample_min_delay(sample);
	return row;
}
