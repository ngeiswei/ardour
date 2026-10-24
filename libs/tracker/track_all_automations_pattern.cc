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

TrackAllAutomationsPattern::TrackAllAutomationsPattern (TrackerContext& te,
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

TrackAllAutomationsPattern::~TrackAllAutomationsPattern ()
{
	for (auto& idpap : id_to_processor_automation_pattern) {
		delete idpap.second;
	}
}

TrackAllAutomationsPatternPhenomenalDiff
TrackAllAutomationsPattern::phenomenal_diff (const TrackAllAutomationsPattern& prev) const
{
	TrackAllAutomationsPatternPhenomenalDiff diff;

	// Partial diff: empty () now means "nothing to do".
	diff.full = false;

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

	// Owned by id_to_processor_automation_pattern, see
	// ~TrackAllAutomationsPattern.  Delete any pre-existing pattern for
	// that processor to remain leak-free if setup is run twice.
	delete id_to_processor_automation_pattern[processor->id ()];
	id_to_processor_automation_pattern[processor->id()] =
		new ProcessorAutomationPattern(context, track, position, length, end, nt_last, connect, processor);
}

void
TrackAllAutomationsPattern::set_rows_per_beat (uint16_t rpb, bool rfs)
{
	main_automation_pattern.set_rows_per_beat (rpb, rfs);
	for (auto& idpap : id_to_processor_automation_pattern) {
		idpap.second->set_rows_per_beat (rpb, rfs);
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

AutomationPattern*
TrackAllAutomationsPattern::automation_pattern (const PBD::ID& id)
{
	// Delegate to the const overload
	return const_cast<AutomationPattern*> (static_cast<const TrackAllAutomationsPattern*> (this)->automation_pattern (id));
}

const AutomationPattern*
TrackAllAutomationsPattern::automation_pattern (const PBD::ID& id) const
{
	if (id == 0) {
		return &main_automation_pattern;
	}
	auto it = id_to_processor_automation_pattern.find (id);
	if (it != id_to_processor_automation_pattern.end ()) {
		return it->second;
	}
	std::cerr << "Cannot find processor in TrackAllAutomationsPattern (id=" << id.to_s () << ")" << std::endl;
	return nullptr;
}

void
TrackAllAutomationsPattern::insert (const IDParameter& id_param)
{
	if (AutomationPattern* ap = automation_pattern (id_param.first)) {
		ap->insert (id_param.second);
	}
}

bool
TrackAllAutomationsPattern::is_empty (const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->is_empty (id_param.second) : false;
}

bool
TrackAllAutomationsPattern::is_displayable (int rowi, const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->is_displayable (rowi, id_param.second) : false;
}

size_t
TrackAllAutomationsPattern::control_events_count (int rowi, const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->control_events_count (rowi, id_param.second) : 0;
}

std::vector<Temporal::BBT_Time>
TrackAllAutomationsPattern::get_automation_bbt_seq (int rowi, const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->get_automation_bbt_seq (rowi, id_param.second) : std::vector<Temporal::BBT_Time> ();
}

std::pair<double, bool>
TrackAllAutomationsPattern::get_automation_value (int rowi, const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->get_automation_value (rowi, id_param.second) : std::pair<double, bool> ();
}

std::vector<double>
TrackAllAutomationsPattern::get_automation_value_seq (int rowi, const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->get_automation_value_seq (rowi, id_param.second) : std::vector<double> ();
}

double
TrackAllAutomationsPattern::get_automation_interpolation_value (int rowi, const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->get_automation_interpolation_value (rowi, id_param.second) : 0.0;
}

void
TrackAllAutomationsPattern::set_automation_value (double val, int rowi, const IDParameter& id_param, int delay)
{
	if (AutomationPattern* ap = automation_pattern (id_param.first)) {
		ap->set_automation_value (val, rowi, id_param.second, delay);
	}
}

void
TrackAllAutomationsPattern::delete_automation_value (int rowi, const IDParameter& id_param)
{
	if (AutomationPattern* ap = automation_pattern (id_param.first)) {
		ap->delete_automation_value (rowi, id_param.second);
	}
}

std::pair<int, bool>
TrackAllAutomationsPattern::get_automation_delay (int rowi, const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->get_automation_delay (rowi, id_param.second) : std::pair<int, bool> ();
}

std::vector<int>
TrackAllAutomationsPattern::get_automation_delay_seq (int rowi, const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->get_automation_delay_seq (rowi, id_param.second) : std::vector<int> ();
}

void
TrackAllAutomationsPattern::set_automation_delay (int delay, int rowi, const IDParameter& id_param)
{
	if (AutomationPattern* ap = automation_pattern (id_param.first)) {
		ap->set_automation_delay (delay, rowi, id_param.second);
	}
}

std::string
TrackAllAutomationsPattern::get_name (const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->get_name (id_param.second) : "";
}

void
TrackAllAutomationsPattern::set_param_enabled (const IDParameter& id_param, bool enabled)
{
	if (AutomationPattern* ap = automation_pattern (id_param.first)) {
		ap->set_param_enabled (id_param.second, enabled);
	}
}

bool
TrackAllAutomationsPattern::is_param_enabled (const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->is_param_enabled (id_param.second) : false;
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
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->lower (id_param.second) : 0.0;
}

double
TrackAllAutomationsPattern::upper (const IDParameter& id_param) const
{
	const AutomationPattern* ap = automation_pattern (id_param.first);
	return ap ? ap->upper (id_param.second) : 0.0;
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
