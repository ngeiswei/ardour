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
}

TrackAutomationPattern::TrackAutomationPattern (TrackerEditor& te,
                                                TrackPtr trk,
                                                Temporal::samplepos_t pos,
                                                Temporal::samplecnt_t len,
                                                Temporal::samplepos_t fst,
                                                Temporal::samplepos_t lst,
                                                bool connect)
	: AutomationPattern (te, pos, 0, len, fst, lst, connect)
	, track(trk)
{
}

void TrackAutomationPattern::insert (const Evoral::Parameter& param)
{
	AutomationPattern::insert_actl (track->automation_control (param, true), track->describe_parameter (param));
}

int
TrackAutomationPattern::event2row (const Evoral::Parameter& param, const Evoral::ControlEvent* event)
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
TrackAutomationPattern::automatable_parameters () const
{
	// NEXT.12
	return ParameterSet();
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
