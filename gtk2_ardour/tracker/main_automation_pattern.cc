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

#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/region.h"

#include "main_automation_pattern.h"

using namespace ARDOUR;
using namespace Tracker;

///////////////////////////
// MainAutomationPattern //
///////////////////////////

MainAutomationPattern::MainAutomationPattern (TrackerEditor& te,
                                              TrackPtr trk,
                                              Temporal::timepos_t pos,
                                              Temporal::timecnt_t len,
                                              Temporal::timepos_t ed,
                                              Temporal::timepos_t ntl,
                                              bool connect)
	: TrackAutomationPattern (te, trk, pos, len, ed, ntl, connect)
{
	// NEXT.3: fill _automatable_parameters, maybe...
	setup_main_automation_controls ();
}

void
MainAutomationPattern::setup_main_automation_controls ()
{
	// Gain
	AutomationPattern::insert_actl (track->gain_control (), track->describe_parameter (Evoral::Parameter (GainAutomation)));

	// Trim
	AutomationPattern::insert_actl (track->trim_control (), track->describe_parameter (Evoral::Parameter (TrimAutomation)));

	// Mute
	AutomationPattern::insert_actl (track->mute_control (), track->describe_parameter (Evoral::Parameter (MuteAutomation)));

	// Pan
	ParameterSet const & pan_params = track->pannable ()->what_can_be_automated ();
	for (ParameterSet::value_type p : pan_params) {
		AutomationPattern::insert_actl (track->pannable ()->automation_control (p), track->describe_parameter (p));
	}
}

const ParameterSet&
MainAutomationPattern::automatable_parameters () const
{
	return _automatable_parameters;
}

std::string
MainAutomationPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "MainAutomationPattern[" << this << "]";
	return ss.str ();
}

std::string
MainAutomationPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << TrackAutomationPattern::to_string (indent) << std::endl;
	std::string header = indent + self_to_string () + " ";
	ss << header << "_automatable_parameters:";
	// NEXT.11: print _automatable_parameters
	return ss.str ();
}
