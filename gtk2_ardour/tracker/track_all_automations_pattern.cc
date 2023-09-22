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

#include "track_all_automations_pattern.h"
#include "tracker_utils.h"

using namespace ARDOUR;
using namespace Tracker;

////////////////////////////////
// TrackAllAutomationsPattern //
////////////////////////////////

TrackAllAutomationsPattern::TrackAllAutomationsPattern (TrackerEditor& te,
                                                        TrackPtr trk,
                                                        Temporal::timepos_t pos,
                                                        Temporal::timecnt_t len,
                                                        Temporal::timepos_t ed,
                                                        Temporal::timepos_t ntl,
                                                        bool connect)
	: BasePattern (te, pos, Temporal::timepos_t (), len, ed, ntl)
	, track(trk)
	, track_automation_pattern (te,
	                            track,
	                            pos,
	                            len,
	                            ed,
	                            ntl,
	                            connect)
{
	setup_automation_controls ();
}

TrackAllAutomationsPatternPhenomenalDiff
TrackAllAutomationsPattern::phenomenal_diff (const TrackAllAutomationsPattern& prev) const
{
	TrackAllAutomationsPatternPhenomenalDiff diff;
	diff.automation_pattern_phenomenal_diff = track_automation_pattern.phenomenal_diff (prev.track_automation_pattern);
	return diff;
}

void TrackAllAutomationsPattern::setup_automation_controls ()
{
	setup_main_automation_controls (); // NEXT.13: can be removed since it is in MainAutomationPattern
	setup_processors_automation_controls (); // NEXT.13. move to ProcessorAutomationPattern
}

// NEXT.13: remove
void TrackAllAutomationsPattern::setup_main_automation_controls ()
{
	// Gain
	track_automation_pattern.insert_actl (track->gain_control (), track->describe_parameter (Evoral::Parameter (GainAutomation)));

	// Trim
	track_automation_pattern.insert_actl (track->trim_control (), track->describe_parameter (Evoral::Parameter (TrimAutomation)));

	// Mute
	track_automation_pattern.insert_actl (track->mute_control (), track->describe_parameter (Evoral::Parameter (MuteAutomation)));

	// Pan
	ParameterSet const & pan_params = track->pannable ()->what_can_be_automated ();
	for (ParameterSet::const_iterator p = pan_params.begin (); p != pan_params.end (); ++p) {
		track_automation_pattern.insert_actl (track->pannable ()->automation_control (*p), track->describe_parameter (*p));
	}
}

void TrackAllAutomationsPattern::setup_processors_automation_controls ()
{
	track->foreach_processor (sigc::mem_fun (*this, &TrackAllAutomationsPattern::setup_processor_automation_control));
}

void
TrackAllAutomationsPattern::setup_processor_automation_control (std::weak_ptr<ARDOUR::Processor> p)
{
	ProcessorPtr processor (p.lock ());

	if (!processor || !processor->display_to_user ()) {
		return;
	}

	// NEXT.2: move to processor_pattern, maybe
	const ParameterSet& automatable = processor->what_can_be_automated ();
	for (ParameterSetConstIt ait = automatable.begin (); ait != automatable.end (); ++ait) {
		// NEXT.13: replace track_automation_pattern by mapping from p to
		// TrackAutomationPattern (maybe using ProcessprAutomationPattern which
		// can retain information about the processor)
		track_automation_pattern.insert_actl (std::dynamic_pointer_cast<AutomationControl> (processor->control (*ait)), processor->describe_parameter (*ait));
	}
}

void
TrackAllAutomationsPattern::set_rows_per_beat (uint16_t rpb)
{
	track_automation_pattern.set_rows_per_beat (rpb);
}

void
TrackAllAutomationsPattern::set_row_range ()
{
	track_automation_pattern.set_row_range ();
}

void
TrackAllAutomationsPattern::update ()
{
	track_automation_pattern.update ();
}

void
TrackAllAutomationsPattern::insert (const Evoral::Parameter& param)
{
	track_automation_pattern.insert_actl (track->automation_control (param, true), track->describe_parameter (param));
}

bool
TrackAllAutomationsPattern::is_empty (const Evoral::Parameter& param) const
{
	return track_automation_pattern.is_empty (param);
}

const ParameterSet&
TrackAllAutomationsPattern::automatable_parameters () const
{
	// NEXT.12
	return ParameterSet();
}

bool
TrackAllAutomationsPattern::is_displayable (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.is_displayable (rowi, param);
}

size_t
TrackAllAutomationsPattern::control_events_count (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.control_events_count (rowi, param);
}

std::vector<Temporal::BBT_Time>
TrackAllAutomationsPattern::get_automation_bbt_seq (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_automation_bbt_seq (rowi, param);
}

std::pair<double, bool>
TrackAllAutomationsPattern::get_automation_value (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_automation_value (rowi, param);
}

std::vector<double>
TrackAllAutomationsPattern::get_automation_value_seq (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_automation_value_seq (rowi, param);
}

double
TrackAllAutomationsPattern::get_automation_interpolation_value (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_automation_interpolation_value (rowi, param);
}

void
TrackAllAutomationsPattern::set_automation_value (double val, int rowi, const Evoral::Parameter& param, int delay)
{
	track_automation_pattern.set_automation_value (val, rowi, param, delay);
}

void
TrackAllAutomationsPattern::delete_automation_value (int rowi, const Evoral::Parameter& param)
{
	track_automation_pattern.delete_automation_value (rowi, param);
}

std::pair<int, bool>
TrackAllAutomationsPattern::get_automation_delay (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_automation_delay (rowi, param);
}

std::vector<int>
TrackAllAutomationsPattern::get_automation_delay_seq (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_automation_delay_seq (rowi, param);
}

void
TrackAllAutomationsPattern::set_automation_delay (int delay, int rowi, const Evoral::Parameter& param)
{
	track_automation_pattern.set_automation_delay (delay, rowi, param);
}

std::string
TrackAllAutomationsPattern::get_name (const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_name (param);
}

void
TrackAllAutomationsPattern::set_param_enabled (const Evoral::Parameter& param, bool enabled)
{
	track_automation_pattern.set_param_enabled (param, enabled);
}

bool
TrackAllAutomationsPattern::is_param_enabled (const Evoral::Parameter& param) const
{
	return track_automation_pattern.is_param_enabled (param);
}

ParameterSet
TrackAllAutomationsPattern::get_enabled_parameters () const
{
	return track_automation_pattern.get_enabled_parameters ();
}

double
TrackAllAutomationsPattern::lower (const Evoral::Parameter& param) const
{
	return track_automation_pattern.lower (param);
}

double
TrackAllAutomationsPattern::upper (const Evoral::Parameter& param) const
{
	return track_automation_pattern.upper (param);
}

std::string
TrackAllAutomationsPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "TrackAllAutomationsPattern[" << this << "]";
	return ss.str ();
}

std::string
TrackAllAutomationsPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePattern::to_string (indent);

	std::string header = indent + self_to_string () + " ";

	// Print track pointer address
	ss << std::endl << header << "track = " << track;

	// Print content of track_automation_pattern
	ss << std::endl << header << "track_automation_pattern:" << std::endl
	   << track_automation_pattern.to_string (header + " ");

	return ss.str ();
}
