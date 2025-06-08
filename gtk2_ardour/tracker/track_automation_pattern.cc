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
                                                TrackPtr trk,
                                                Temporal::timepos_t pos,
                                                Temporal::timecnt_t len,
                                                Temporal::timepos_t ed,
                                                Temporal::timepos_t ntl,
                                                bool connect)
	: AutomationPattern (te, pos, Temporal::timepos_t (), len, ed, ntl, connect)
	, track(trk)
{
}

void TrackAutomationPattern::insert (const Evoral::Parameter& param)
{
	AutomationPattern::insert_actl (track->automation_control (param, true), track->describe_parameter (param));
}

Temporal::Beats
TrackAutomationPattern::event2beats (const Evoral::Parameter& param, const Evoral::ControlEvent* event)
{
	return event->when.beats ();
}

std::string
TrackAutomationPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "TrackAutomationPattern[" << this << "]";
	return ss.str ();
}

std::string
TrackAutomationPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << AutomationPattern::to_string (indent);

	std::string header = indent + self_to_string () + " ";

	// Print track pointer address
	ss << std::endl << header << "track = " << track;

	return ss.str ();
}
