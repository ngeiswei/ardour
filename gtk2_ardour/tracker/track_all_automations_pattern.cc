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

using namespace std;
using namespace ARDOUR;
using namespace Tracker;

////////////////////////////////
// TrackAllAutomationsPattern //
////////////////////////////////

TrackAllAutomationsPattern::TrackAllAutomationsPattern (TrackerEditor& te,
                                                        TrackPtr trk,
                                                        const RegionSeq& regions,
                                                        bool connect)
	: BasePattern (te,
	               TrackerUtils::get_position_sample (regions),
	               0,
	               TrackerUtils::get_length_sample (regions),
	               TrackerUtils::get_first_sample (regions),
	               TrackerUtils::get_last_sample (regions))
	, track (trk)
	, track_automation_pattern (te,
	                            track,
	                            TrackerUtils::get_position_sample (regions),
	                            TrackerUtils::get_length_sample (regions),
	                            TrackerUtils::get_first_sample (regions),
	                            TrackerUtils::get_last_sample (regions),
	                            connect)
{
	setup_automation_controls ();
}

TrackAllAutomationsPattern::TrackAllAutomationsPattern (TrackerEditor& te,
                                                        TrackPtr trk,
                                                        Temporal::samplepos_t pos,
                                                        Temporal::samplecnt_t len,
                                                        Temporal::samplepos_t fst,
                                                        Temporal::samplepos_t lst,
                                                        bool connect)
	: BasePattern (te, pos, 0, len, fst, lst)
	, track(trk)
	, track_automation_pattern (te,
	                            track,
	                            pos,
	                            len,
	                            fst,
	                            lst,
	                            connect)
{
	setup_automation_controls ();
}

void TrackAllAutomationsPattern::setup_automation_controls ()
{
	setup_main_automation_controls ();
	setup_processors_automation_controls ();
}

void TrackAllAutomationsPattern::setup_main_automation_controls ()
{
	// Gain
	track_automation_pattern.insert_actl (track->gain_control (), track->describe_parameter (Evoral::Parameter (GainAutomation)));

	// Trim
	track_automation_pattern.insert_actl (track->trim_control (), track->describe_parameter (Evoral::Parameter (TrimAutomation)));

	// Mute
	track_automation_pattern.insert_actl (track->mute_control (), track->describe_parameter (Evoral::Parameter (MuteAutomation)));

	// Pan
	set<Evoral::Parameter> const & pan_params = track->pannable ()->what_can_be_automated ();
	for (set<Evoral::Parameter>::const_iterator p = pan_params.begin (); p != pan_params.end (); ++p) {
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
		track_automation_pattern.insert_actl (std::dynamic_pointer_cast<AutomationControl> (processor->control (*ait)), processor->describe_parameter (*ait));
	}
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

int
TrackAllAutomationsPattern::event2row (const Evoral::Parameter& param, const Evoral::ControlEvent* event)
{
	timepos_t when = event->when;

	if (when < Temporal::timepos_t (first_sample) || Temporal::timepos_t (last_sample) < when) {
		return INVALID_ROW;
	}

	int row = row_at_sample (Temporal::timepos_t (when).samples ());
	if (track_automation_pattern.control_events_count (row, param) != 0) {
		row = row_at_sample_min_delay (Temporal::timepos_t (when).samples ());
	}
	return row;
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

std::pair<double, bool>
TrackAllAutomationsPattern::get_automation_value (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_automation_value (rowi, param);
}

std::vector<double>
TrackAllAutomationsPattern::get_automation_value_seq (int rowi, const Evoral::Parameter& param) const
{
	return track_automation_pattern.get_automation_value_seq (rowi, param); // NEXT.14
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
	return track_automation_pattern.get_automation_delay_seq (rowi, param); // NEXT.14
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
	track_automation_pattern.is_param_enabled (param);
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
