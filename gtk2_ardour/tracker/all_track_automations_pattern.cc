/*
 * Copyright (C) 2016-2023 Nil Geisweiller <ngeiswei@gmail.com>
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

#include "all_track_automations_pattern.h"
#include "tracker_utils.h"

using namespace std;
using namespace ARDOUR;
using namespace Tracker;

////////////////////////////////
// AllTrackAutomationsPattern //
////////////////////////////////

// NEXT.13: understand what to keep or not.

AllTrackAutomationsPattern::AllTrackAutomationsPattern (TrackerEditor& te,
                                                        TrackPtr trk,
                                                        const RegionSeq& regions,
                                                        bool connect)
	: AutomationPattern (te,
	                     TrackerUtils::get_position_sample (regions),
	                     0,
	                     TrackerUtils::get_length_sample (regions),
	                     TrackerUtils::get_first_sample (regions),
	                     TrackerUtils::get_last_sample (regions),
	                     connect)
	, track(trk)
{
	setup_automation_controls ();
}

AllTrackAutomationsPattern::AllTrackAutomationsPattern (TrackerEditor& te,
                                                        TrackPtr trk,
                                                        Temporal::samplepos_t pos,
                                                        Temporal::samplecnt_t len,
                                                        Temporal::samplepos_t fst,
                                                        Temporal::samplepos_t lst,
                                                        bool connect)
	: AutomationPattern (te, pos, 0, len, fst, lst, connect)
	, track(trk)
{
	setup_automation_controls ();
}

void AllTrackAutomationsPattern::setup_automation_controls ()
{
	setup_main_automation_controls ();
	setup_processors_automation_controls ();
}

void AllTrackAutomationsPattern::setup_main_automation_controls ()
{
	// Gain
	AutomationPattern::insert_actl (track->gain_control (), track->describe_parameter (Evoral::Parameter (GainAutomation)));

	// Trim
	AutomationPattern::insert_actl (track->trim_control (), track->describe_parameter (Evoral::Parameter (TrimAutomation)));

	// Mute
	AutomationPattern::insert_actl (track->mute_control (), track->describe_parameter (Evoral::Parameter (MuteAutomation)));

	// Pan
	set<Evoral::Parameter> const & pan_params = track->pannable ()->what_can_be_automated ();
	for (set<Evoral::Parameter>::const_iterator p = pan_params.begin (); p != pan_params.end (); ++p) {
		AutomationPattern::insert_actl (track->pannable ()->automation_control (*p), track->describe_parameter (*p));
	}
}

void AllTrackAutomationsPattern::setup_processors_automation_controls ()
{
	track->foreach_processor (sigc::mem_fun (*this, &AllTrackAutomationsPattern::setup_processor_automation_control));
}

void
AllTrackAutomationsPattern::setup_processor_automation_control (boost::weak_ptr<ARDOUR::Processor> p)
{
	ProcessorPtr processor (p.lock ());

	if (!processor || !processor->display_to_user ()) {
		return;
	}

	// NEXT.2: move to processor_pattern, maybe
	const ParameterSet& automatable = processor->what_can_be_automated ();
	for (ParameterSetConstIt ait = automatable.begin (); ait != automatable.end (); ++ait) {
		AutomationPattern::insert_actl (boost::dynamic_pointer_cast<AutomationControl> (processor->control (*ait)), processor->describe_parameter (*ait));
	}
}

void AllTrackAutomationsPattern::insert (const Evoral::Parameter& param)
{
	// NEXT.15: that is what is called for MIDI automation, is that normal?
	AutomationPattern::insert_actl (track->automation_control (param, true), track->describe_parameter (param));
}

int
AllTrackAutomationsPattern::event2row (const Evoral::Parameter& param, const Evoral::ControlEvent* event)
{
	timepos_t when = event->when;

	if (when < Temporal::timepos_t (first_sample) || Temporal::timepos_t (last_sample) < when) {
		return INVALID_ROW;
	}

	int row = row_at_sample (Temporal::timepos_t (when).samples ());
	if (AutomationPattern::control_events_count (row, param) != 0) {
		row = row_at_sample_min_delay (Temporal::timepos_t (when).samples ());
	}
	return row;
}

const ParameterSet&
AllTrackAutomationsPattern::automatable_parameters () const
{
	// NEXT.12
	return ParameterSet();
}

std::string
AllTrackAutomationsPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "AllTrackAutomationsPattern[" << this << "]";
	return ss.str ();
}

std::string
AllTrackAutomationsPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << AutomationPattern::to_string (indent);

	std::string header = indent + self_to_string () + " ";

	// Print track pointer address
	ss << std::endl << header << "track = " << track;

	return ss.str ();
}
