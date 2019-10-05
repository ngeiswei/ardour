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

#include <cmath>
#include <map>

#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/region.h"

#include "track_automation_pattern.h"
#include "tracker_utils.h"

using namespace std;
using namespace ARDOUR;
using namespace Tracker;

////////////////////////////
// TrackAutomationPattern //
////////////////////////////

TrackAutomationPattern::TrackAutomationPattern (TrackerEditor& te,
                                                boost::shared_ptr<ARDOUR::Track> trk,
                                                const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions)
	: TrackPattern (te, trk,
	                TrackerUtils::get_position (regions),
	                TrackerUtils::get_length (regions),
	                TrackerUtils::get_first_sample (regions),
	                TrackerUtils::get_last_sample (regions))
{
	setup_automation_controls ();
}

TrackAutomationPattern::TrackAutomationPattern (TrackerEditor& te,
                                                boost::shared_ptr<ARDOUR::Track> trk,
                                                Temporal::samplepos_t pos,
                                                Temporal::samplecnt_t len,
                                                Temporal::samplepos_t fst,
                                                Temporal::samplepos_t lst)
	: TrackPattern (te, trk, pos, len, fst, lst)
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
	AutomationPattern::insert (track->gain_control (), track->describe_parameter (Evoral::Parameter (GainAutomation)));

	// Trim
	AutomationPattern::insert (track->trim_control (), track->describe_parameter (Evoral::Parameter (TrimAutomation)));

	// Mute
	AutomationPattern::insert (track->mute_control (), track->describe_parameter (Evoral::Parameter (MuteAutomation)));

	// Pan
	set<Evoral::Parameter> const & pan_params = track->pannable ()->what_can_be_automated ();
	for (set<Evoral::Parameter>::const_iterator p = pan_params.begin (); p != pan_params.end (); ++p) {
		AutomationPattern::insert (track->pannable ()->automation_control (*p), track->panner ()->describe_parameter (*p));
	}
}

void TrackAutomationPattern::setup_processors_automation_controls ()
{
	track->foreach_processor (sigc::mem_fun (*this, &TrackAutomationPattern::setup_processor_automation_control));
}

void
TrackAutomationPattern::setup_processor_automation_control (boost::weak_ptr<ARDOUR::Processor> p)
{
	boost::shared_ptr<ARDOUR::Processor> processor (p.lock ());

	if (!processor || !processor->display_to_user ()) {
		return;
	}

	const std::set<Evoral::Parameter>& automatable = processor->what_can_be_automated ();
	for (std::set<Evoral::Parameter>::const_iterator ait = automatable.begin (); ait != automatable.end (); ++ait) {
		AutomationPattern::insert (boost::dynamic_pointer_cast<AutomationControl> (processor->control (*ait)), processor->describe_parameter (*ait));
	}
}

void TrackAutomationPattern::insert (const Evoral::Parameter& param)
{
	AutomationPattern::insert (track->automation_control (param, true), track->describe_parameter (param));
}

uint32_t
TrackAutomationPattern::event2row (const Evoral::Parameter& param, const Evoral::ControlEvent* event)
{
	samplepos_t sample = event->when;

	if (sample < first_sample || last_sample < sample) {
		return INVALID_ROW;
	}

	uint32_t row = row_at_sample (sample);
	if (param_to_row_to_ali[param].count (row) != 0) {
		row = row_at_sample_min_delay (sample);
	}
	return row;
}
