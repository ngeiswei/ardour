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
                                                        bool cnct)
	: BasePattern (te, pos, Temporal::timepos_t (), len, ed, ntl)
	, track(trk)
	, main_automation_pattern (te, track, pos, len, ed, ntl, cnct)
	, connect(cnct)
{
	setup_processors_automation_controls ();
}

TrackAllAutomationsPatternPhenomenalDiff
TrackAllAutomationsPattern::phenomenal_diff (const TrackAllAutomationsPattern& prev) const
{
	TrackAllAutomationsPatternPhenomenalDiff diff;

	// Calculate phenomenal diff of main automations
	diff.main_automation_pattern_phenomenal_diff = main_automation_pattern.phenomenal_diff (prev.main_automation_pattern);

	// Calculate phenomenal diff of processor automations
	for (const auto& id_prauto : id_to_processor_automation_pattern) {
		const PBD::ID& id = id_prauto.first;
		const ProcessorAutomationPattern* auto_ptrn = id_prauto.second;
		auto prev_it = prev.id_to_processor_automation_pattern.find(id);
		AutomationPatternPhenomenalDiff pppd;
		if (prev_it != prev.id_to_processor_automation_pattern.end ()) {
			pppd = auto_ptrn->phenomenal_diff (*prev_it->second);
		}

		// No need to add if empty
		if (pppd.empty ())
			continue;

		// Add non empty phenomenal diff for that processor
		diff.id_to_processor_automation_pattern_phenomenal_diff[id] = pppd;
	}

	// In case a processor track is present in prev but missing in this, insert
	// a full phenomenal diff as well
	for (const auto& prev_id_prauto : prev.id_to_processor_automation_pattern) {
		const PBD::ID& prev_id = prev_id_prauto.first;
		auto it = id_to_processor_automation_pattern.find(prev_id);
		if (it == id_to_processor_automation_pattern.end ()) {
			AutomationPatternPhenomenalDiff pppd;
			diff.id_to_processor_automation_pattern_phenomenal_diff[prev_id] = pppd;
		}
	}

	return diff;
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

	// NEXT: deal with memory leak
	id_to_processor_automation_pattern[processor->id()] =
		new ProcessorAutomationPattern(tracker_editor, track, position, length, end, nt_last, connect, processor);
}

void
TrackAllAutomationsPattern::set_rows_per_beat (uint16_t rpb)
{
	main_automation_pattern.set_rows_per_beat (rpb);
	// NEXT.13: processor
}

void
TrackAllAutomationsPattern::set_row_range ()
{
	main_automation_pattern.set_row_range ();
	// NEXT.13: processor
}

void
TrackAllAutomationsPattern::update ()
{
	main_automation_pattern.update ();
	// NEXT.13: processor
}

void
TrackAllAutomationsPattern::insert (const Evoral::Parameter& param)
{
	main_automation_pattern.insert_actl (track->automation_control (param, true), track->describe_parameter (param));
	// NEXT.13: processor
}

bool
TrackAllAutomationsPattern::is_empty (const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.is_empty (param);
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
	// NEXT.13: processor
	return main_automation_pattern.is_displayable (rowi, param);
}

size_t
TrackAllAutomationsPattern::control_events_count (int rowi, const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.control_events_count (rowi, param);
}

std::vector<Temporal::BBT_Time>
TrackAllAutomationsPattern::get_automation_bbt_seq (int rowi, const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.get_automation_bbt_seq (rowi, param);
}

std::pair<double, bool>
TrackAllAutomationsPattern::get_automation_value (int rowi, const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.get_automation_value (rowi, param);
}

std::vector<double>
TrackAllAutomationsPattern::get_automation_value_seq (int rowi, const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.get_automation_value_seq (rowi, param);
}

double
TrackAllAutomationsPattern::get_automation_interpolation_value (int rowi, const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.get_automation_interpolation_value (rowi, param);
}

void
TrackAllAutomationsPattern::set_automation_value (double val, int rowi, const Evoral::Parameter& param, int delay)
{
	// NEXT.13: processor
	main_automation_pattern.set_automation_value (val, rowi, param, delay);
}

void
TrackAllAutomationsPattern::delete_automation_value (int rowi, const Evoral::Parameter& param)
{
	// NEXT.13: processor
	main_automation_pattern.delete_automation_value (rowi, param);
}

std::pair<int, bool>
TrackAllAutomationsPattern::get_automation_delay (int rowi, const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.get_automation_delay (rowi, param);
}

std::vector<int>
TrackAllAutomationsPattern::get_automation_delay_seq (int rowi, const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.get_automation_delay_seq (rowi, param);
}

void
TrackAllAutomationsPattern::set_automation_delay (int delay, int rowi, const Evoral::Parameter& param)
{
	// NEXT.13: processor
	main_automation_pattern.set_automation_delay (delay, rowi, param);
}

std::string
TrackAllAutomationsPattern::get_name (const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.get_name (param);
}

void
TrackAllAutomationsPattern::set_param_enabled (const Evoral::Parameter& param, bool enabled)
{
	// NEXT.13: processor
	main_automation_pattern.set_param_enabled (param, enabled);
}

bool
TrackAllAutomationsPattern::is_param_enabled (const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.is_param_enabled (param);
}

ParameterSet
TrackAllAutomationsPattern::get_enabled_parameters () const
{
	// NEXT.13: processor
	return main_automation_pattern.get_enabled_parameters ();
}

double
TrackAllAutomationsPattern::lower (const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.lower (param);
}

double
TrackAllAutomationsPattern::upper (const Evoral::Parameter& param) const
{
	// NEXT.13: processor
	return main_automation_pattern.upper (param);
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
	std::string indent2 = indent + "  ";
	std::string indent3 = indent2 + "  ";

	// Print track pointer address
	ss << std::endl << header << "track = " << track;

	// Print content of track_automation_pattern
	ss << std::endl << header << "track_automation_pattern:" << std::endl
	   << main_automation_pattern.to_string (indent2);
	for (const auto& id_pap : id_to_processor_automation_pattern) {
		ss << std::endl << indent2 << "id_to_processor_automation_pattern[" << id_pap.first << "]:" << std::endl;
		ss << id_pap.second->to_string(indent3);
	}

	return ss.str ();
}
