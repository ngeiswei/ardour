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
#include "pbd/id.h"

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
	for (auto& idpap : id_to_processor_automation_pattern) {
		idpap.second->set_rows_per_beat (rpb);
	}
}

void
TrackAllAutomationsPattern::set_row_range ()
{
	main_automation_pattern.set_row_range ();
	for (auto& idpap : id_to_processor_automation_pattern) {
		idpap.second->set_row_range ();
	}
}

void
TrackAllAutomationsPattern::update ()
{
	main_automation_pattern.update ();
	for (auto& idpap : id_to_processor_automation_pattern) {
		idpap.second->update ();
	}
}

void
TrackAllAutomationsPattern::insert (const IDParameter& id_param)
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		main_automation_pattern.insert (param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			it->second->insert (param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::insert (id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
		}
	}
}

bool
TrackAllAutomationsPattern::is_empty (const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.is_empty (param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->is_empty (param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::is_empty (id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return false;
		}
	}
}

bool
TrackAllAutomationsPattern::is_displayable (int rowi, const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.is_displayable (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->is_displayable (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::is_empty (id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return false;
		}
	}
}

size_t
TrackAllAutomationsPattern::control_events_count (int rowi, const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.control_events_count (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->control_events_count (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::control_events_count (rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return 0;
		}
	}
}

std::vector<Temporal::BBT_Time>
TrackAllAutomationsPattern::get_automation_bbt_seq (int rowi, const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.get_automation_bbt_seq (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->get_automation_bbt_seq (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::get_automation_bbt_seq (rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return std::vector<Temporal::BBT_Time>();
		}
	}
}

std::pair<double, bool>
TrackAllAutomationsPattern::get_automation_value (int rowi, const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.get_automation_value (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->get_automation_value (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::get_automation_value (rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return std::pair<double, bool>();
		}
	}
}

std::vector<double>
TrackAllAutomationsPattern::get_automation_value_seq (int rowi, const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.get_automation_value_seq (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->get_automation_value_seq (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::get_automation_value_seq (rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return std::vector<double>();
		}
	}
}

double
TrackAllAutomationsPattern::get_automation_interpolation_value (int rowi, const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.get_automation_interpolation_value (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->get_automation_interpolation_value (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::get_automation_interpolation_value (rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return 0.0;
		}
	}
}

void
TrackAllAutomationsPattern::set_automation_value (double val, int rowi, const IDParameter& id_param, int delay)
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		main_automation_pattern.set_automation_value (val, rowi, param, delay);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			it->second->set_automation_value (val, rowi, param, delay);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::set_automation_value (val=" << val << ", rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ", delay=" << delay << ")" << std::endl;
		}
	}
}

void
TrackAllAutomationsPattern::delete_automation_value (int rowi, const IDParameter& id_param)
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		main_automation_pattern.delete_automation_value (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			it->second->delete_automation_value (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::delete_automation_value (rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
		}
	}
}

std::pair<int, bool>
TrackAllAutomationsPattern::get_automation_delay (int rowi, const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.get_automation_delay (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->get_automation_delay (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::get_automation_delay (rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return std::pair<int, bool>();
		}
	}
}

std::vector<int>
TrackAllAutomationsPattern::get_automation_delay_seq (int rowi, const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.get_automation_delay_seq (rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->get_automation_delay_seq (rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::get_automation_delay_seq (rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return std::vector<int>();
		}
	}
}

void
TrackAllAutomationsPattern::set_automation_delay (int delay, int rowi, const IDParameter& id_param)
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		main_automation_pattern.set_automation_delay (delay, rowi, param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			it->second->set_automation_delay (delay, rowi, param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::set_automation_delay (delay=" << delay << ", rowi=" << rowi << ", id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
		}
	}
}

std::string
TrackAllAutomationsPattern::get_name (const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.get_name (param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->get_name (param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::get_name (id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return "";
		}
	}
}

void
TrackAllAutomationsPattern::set_param_enabled (const IDParameter& id_param, bool enabled)
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		main_automation_pattern.set_param_enabled (param, enabled);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			it->second->set_param_enabled (param, enabled);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::set_param_enabled (id_param=" << TrackerUtils::id_param_to_string (id_param) << ", enabled=" << enabled << ")" << std::endl;
		}
	}
}

bool
TrackAllAutomationsPattern::is_param_enabled (const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.is_param_enabled (param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->is_param_enabled (param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::is_param_enabled (id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return false;
		}
	}
}

IDParameterSet
TrackAllAutomationsPattern::get_enabled_parameters () const
{
	IDParameterSet ieps;
	for (const Evoral::Parameter& param : main_automation_pattern.get_enabled_parameters ()) {
		ieps.insert(IDParameter (PBD::ID (0), param));
	}
	for (auto& idpap : id_to_processor_automation_pattern) {
		const PBD::ID& id = idpap.first;
		ParameterSet pep = idpap.second->get_enabled_parameters ();
		for (const Evoral::Parameter& param : pep) {
			ieps.insert(IDParameter (id, param));
		}
	}
	return ieps;
}

double
TrackAllAutomationsPattern::lower (const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.lower (param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->lower (param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::lower (id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return 0.0;
		}
	}
}

double
TrackAllAutomationsPattern::upper (const IDParameter& id_param) const
{
	const PBD::ID& id = id_param.first;
	const Evoral::Parameter& param = id_param.second;
	if (id == 0) {
		return main_automation_pattern.upper (param);
	} else {
		auto it = id_to_processor_automation_pattern.find (id);
		if (it != id_to_processor_automation_pattern.end()) {
			return it->second->upper (param);
		} else {
			std::cerr << "Cannot find processor in TrackAllAutomationsPattern::upper (id_param=" << TrackerUtils::id_param_to_string (id_param) << ")" << std::endl;
			return 0.0;
		}
	}
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
		const PBD::ID& id = id_pap.first;
		ss << std::endl << indent2 << "id_to_processor_automation_pattern[" << id.to_s () << "]:" << std::endl;
		ss << id_pap.second->to_string(indent3);
	}

	return ss.str ();
}
