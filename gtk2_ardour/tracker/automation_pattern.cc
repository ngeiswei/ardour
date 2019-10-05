/*
    Copyright (C) 2016-2017 Nil Geisweiller

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>
#include <map>

#include "pbd/i18n.h"

#include "automation_pattern.h"
#include "tracker_editor.h"

using namespace Tracker;

///////////////////////
// AutomationPattern //
///////////////////////

AutomationPattern::AutomationPattern(TrackerEditor& te,
                                     boost::shared_ptr<ARDOUR::Region> region)
	: BasePattern(te, region)
{
}

AutomationPattern::AutomationPattern(TrackerEditor& te,
                                     Temporal::samplepos_t pos,
                                     Temporal::samplepos_t sta,
                                     Temporal::samplecnt_t len,
                                     Temporal::samplepos_t fir,
                                     Temporal::samplepos_t las)
	: BasePattern(te, pos, sta, len, fir, las)
{
}

AutomationPattern&
AutomationPattern::operator=(const AutomationPattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	BasePattern::operator=(other);

	// Deep copy _automation_controls to be sure that changing the event doesn't
	// change the copy
	param_to_actrl.clear();
	for (ParamToAutomationControl::const_iterator it = other.param_to_actrl.begin(); it != other.param_to_actrl.end(); ++it) {
		param_to_actrl.insert(clone_param_actrl(*it));
	}

	param_to_name.clear();
	for (ParamToName::const_iterator it = other.param_to_name.begin(); it != other.param_to_name.end(); ++it) {
		param_to_name.insert(*it);
	}

	// Update automations using the deep copy
	update_automations();

	return *this;
}

boost::shared_ptr<ARDOUR::AutomationControl>
AutomationPattern::clone_actrl(boost::shared_ptr<ARDOUR::AutomationControl> actrl) const
{
	boost::shared_ptr<ARDOUR::AutomationControl> actrl_cp(new ARDOUR::AutomationControl(const_cast<ARDOUR::Session&>(actrl->session()), actrl->parameter(), actrl->desc(), clone_alist(actrl->alist())));
	return actrl_cp;
}

std::pair<Evoral::Parameter, boost::shared_ptr<ARDOUR::AutomationControl> >
AutomationPattern::clone_param_actrl(const std::pair<Evoral::Parameter, boost::shared_ptr<ARDOUR::AutomationControl> >& param_actrl) const
{
	return std::make_pair(param_actrl.first, clone_actrl(param_actrl.second));
}

boost::shared_ptr<ARDOUR::AutomationList>
AutomationPattern::clone_alist(boost::shared_ptr<ARDOUR::AutomationList> alist) const
{
	// TODO: optimize by only copying events within the time range in
	// consideration
	boost::shared_ptr<ARDOUR::AutomationList> alist_cp(new ARDOUR::AutomationList(*alist));
	return alist_cp;
}

void
AutomationPattern::rows_diff(const RowToAutomationListIt& l_row2auto, const RowToAutomationListIt& r_row2auto, std::set<size_t>& rows) const
{
	for (RowToAutomationListIt::const_iterator l_it = l_row2auto.begin(); l_it != l_row2auto.end();) {
		uint32_t row = l_it->first;
		const Evoral::ControlEvent& l_ce = **l_it->second;

		// First, look at the difference in displayability
		bool is_cell_displayable = is_displayable(row, l_row2auto);
		bool cell_diff = is_cell_displayable != is_displayable(row, r_row2auto);
		if (cell_diff) {
			rows.insert(row);
		}
		if (!is_cell_displayable) {
			// It means there are more than one note, jump to the next row
			l_it = l_row2auto.upper_bound(row);
			continue;
		}
		if (cell_diff) {
			++l_it;
			continue;
		}

		// Check if the event exist in other, and if so if it is equal
		RowToAutomationListIt::const_iterator r_it = r_row2auto.find(row);
		if (r_it != r_row2auto.end()) {
			const Evoral::ControlEvent& r_ce = **r_it->second;
			if (!TrackerUtils::is_equal(l_ce, r_ce)) {
				// Update all affected row, taking interpolation into account
				std::pair<uint32_t, uint32_t> r = prev_next_range(r_it, l_row2auto);
				for (uint32_t rowi = r.first; rowi <= r.second; rowi++) {
					rows.insert(rowi);
				}
			}
		} else {
			// Update all affected row, taking interpolation into account
			std::pair<uint32_t, uint32_t> r = prev_next_range(row, l_row2auto);
			for (uint32_t rowi = r.first; rowi <= r.second; rowi++) {
				rows.insert(rowi);
			}
		}
		++l_it;
	}
}

AutomationPatternPhenomenalDiff
AutomationPattern::phenomenal_diff(const AutomationPattern& other) const
{
	AutomationPatternPhenomenalDiff diff;
	if (!enabled) {
		return diff;
	}

	for (ParamToEnabled::const_iterator pe_it = param_to_enabled.begin(); pe_it != param_to_enabled.end(); ++pe_it) {
		const Evoral::Parameter& param = pe_it->first;
		RowsPhenomenalDiff rd;
		
		// If this is disabled ignore their differences, otherwise consider full
		// phenomenal diff if one is enabled while the other is not.
		if (!pe_it->second) {
			continue;
		}
		if (pe_it->second != other.param_to_enabled[param]) {
			rd.full = true;
			diff.param2rows_diff[param] = rd;
			continue;
		}

		// Make sure the parameter is in other, otherwise make the corresponding RowsPhenomenalDiff full
		ParamToRowToAutomationListIt::const_iterator pra_it = param_to_row_to_ali.find(param);
		ParamToRowToAutomationListIt::const_iterator other_pra_it = other.param_to_row_to_ali.find(param);
		if (pra_it == param_to_row_to_ali.end() && other_pra_it == other.param_to_row_to_ali.end()) {
			continue;
		}
		if (pra_it == param_to_row_to_ali.end() || other_pra_it == other.param_to_row_to_ali.end())
		{
			rd.full = true;
			diff.param2rows_diff[param] = rd;
			continue;
		}

		// Both are defined, consider the rows for phenomenal diff
		const RowToAutomationListIt& row2auto = pra_it->second;
		const RowToAutomationListIt& other_row2auto = other_pra_it->second;
		rows_diff(row2auto, other_row2auto, rd.rows);
		rows_diff(other_row2auto, row2auto, rd.rows);
		if (!rd.empty()) {
			diff.param2rows_diff[param] = rd;
		}
	}

	return diff;
}

uint32_t
AutomationPattern::event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event)
{
	std::cerr << "AutomationPattern::event2row not implemented" << std::endl;
	assert (false);
	return 0;
}

void
AutomationPattern::update()
{
	set_row_range();
	update_automations();
}

void
AutomationPattern::update_automations()
{
	param_to_row_to_ali.clear();
	for (ParamToAutomationControl::const_iterator param_actrl = param_to_actrl.begin(); param_actrl != param_to_actrl.end(); ++param_actrl) {
		boost::shared_ptr<ARDOUR::AutomationControl> actrl = param_actrl->second;
		boost::shared_ptr<ARDOUR::AutomationList> al = actrl->alist();
		const Evoral::Parameter& param = actrl->parameter();

		// Same CPU resources
		if (!param_to_enabled[param]) {
			continue;
		}

		// Build automation pattern
		for (ARDOUR::AutomationList::iterator it = al->begin(); it != al->end(); ++it) {
			uint32_t row = event2row(param, *it);
			if (row != INVALID_ROW) {
				param_to_row_to_ali[param].insert(RowToAutomationListIt::value_type(row, it));
			}
		}
	}
}

void
AutomationPattern::insert(boost::shared_ptr<ARDOUR::AutomationControl> actrl, const std::string& name)
{
	// NEXT TODO: try to understand why some controls are inserted twice
	Evoral::Parameter param = actrl->parameter();
	std::pair<ParamToAutomationControl::iterator, bool> actrl_result = param_to_actrl.insert(std::make_pair(param, actrl));
	param_to_name.insert(std::make_pair(param, name));
	if (actrl_result.second) {
		tracker_editor.connect_automation(actrl);
	}
}

bool
AutomationPattern::is_empty (const Evoral::Parameter& param) const
{
	return get_alist(param)->size() == 0;
}

boost::shared_ptr<ARDOUR::AutomationControl> AutomationPattern::get_actrl(const Evoral::Parameter& param)
{
	if (!param) {
		return 0;
	}

	ParamToAutomationControl::iterator it = param_to_actrl.find(param);
	if (it != param_to_actrl.end()) {
		return it->second;
	}

	return 0;
}

const boost::shared_ptr<ARDOUR::AutomationControl>
AutomationPattern::get_actrl(const Evoral::Parameter& param) const
{
	if (!param) {
		return 0;
	}

	ParamToAutomationControl::const_iterator it = param_to_actrl.find(param);
	if (it != param_to_actrl.end()) {
		return it->second;
	}

	return 0;
}

size_t
AutomationPattern::get_automation_list_count (uint32_t rowi, const Evoral::Parameter& param) const
{
	ParamToRowToAutomationListIt::const_iterator it = param_to_row_to_ali.find(param);
	if (it != param_to_row_to_ali.end()) {
		return it->second.count(rowi);
	}
	return 0;
}

std::string
AutomationPattern::get_name (const Evoral::Parameter& param) const
{
	if (!param) {
		return "";
	}

	ParamToName::const_iterator it = param_to_name.find(param);
	if (it != param_to_name.end()) {
		return it->second;
	}

	return "";
}

boost::shared_ptr<ARDOUR::AutomationList>
AutomationPattern::get_alist (const Evoral::Parameter& param)
{
	if (boost::shared_ptr<ARDOUR::AutomationControl> actrl = get_actrl(param)) {
		return actrl->alist();
	}

	return 0;
}

const boost::shared_ptr<ARDOUR::AutomationList>
AutomationPattern::get_alist (const Evoral::Parameter& param) const
{
	if (const boost::shared_ptr<ARDOUR::AutomationControl> actrl = get_actrl(param)) {
		return actrl->alist();
	}

	return 0;
}

bool
AutomationPattern::is_displayable(uint32_t row, const Evoral::Parameter& param) const
{
	return param_to_row_to_ali.find(param) == param_to_row_to_ali.end()
		|| is_displayable(row, param_to_row_to_ali.find(param)->second);
}

bool
AutomationPattern::is_displayable(uint32_t row, RowToAutomationListIt r2a)
{
	return r2a.count(row) <= 1;
}

AutomationPattern::AutomationListIt
AutomationPattern::get_alist_iterator (size_t rowi, const Evoral::Parameter& param)
{
	return param_to_row_to_ali.find(param)->second.find(rowi)->second;
}

Evoral::ControlEvent*
AutomationPattern::get_control_event (size_t rowi, const Evoral::Parameter& param)
{
	ParamToRowToAutomationListIt::iterator it = param_to_row_to_ali.find(param);
	if (it == param_to_row_to_ali.end()) {
		return 0;
	}

	AutomationPattern::RowToAutomationListIt::iterator auto_it = it->second.find(rowi);
	if (auto_it != it->second.end()) {
		return *auto_it->second;
	}

	return 0;
}

const Evoral::ControlEvent*
AutomationPattern::get_control_event (size_t rowi, const Evoral::Parameter& param) const
{
	ParamToRowToAutomationListIt::const_iterator it = param_to_row_to_ali.find(param);
	if (it == param_to_row_to_ali.end()) {
		return 0;
	}

	AutomationPattern::RowToAutomationListIt::const_iterator auto_it = it->second.find(rowi);
	if (auto_it != it->second.end()) {
		return *auto_it->second;
	}

	return 0;
}

std::pair<double, bool>
AutomationPattern::get_automation_value (size_t rowi, const Evoral::Parameter& param) const
{
	if (const Evoral::ControlEvent* ce = get_control_event (rowi, param)) {
		return std::make_pair(ce->value, true);
	}

	return std::make_pair(0.0, false);
}

void
AutomationPattern::set_automation_value (double val, size_t rowi, const Evoral::Parameter& param, int delay)
{
	// Fetch automation control and list
	boost::shared_ptr<ARDOUR::AutomationControl> actrl = get_actrl(param);
	if (!actrl) {
		return;
	}
	boost::shared_ptr<ARDOUR::AutomationList> alist = actrl->alist();

	// Clamp val to its range
	val = TrackerUtils::clamp (val, actrl->lower (), actrl->upper ());

	// Save state for undo
	XMLNode& before = alist->get_state ();

	// Find control event at rowi, if any
	Evoral::ControlEvent* ce = get_control_event (rowi, param);

	// If no existing value, insert one
	if (!ce) {
		Temporal::Beats row_relative_beats = region_relative_beats_at_row(rowi, delay);
		uint32_t row_sample = sample_at_row(rowi, delay);
		double awhen = TrackerUtils::is_region_automation (param) ? row_relative_beats.to_double() : row_sample;
		if (alist->editor_add (awhen, val, false)) {
			XMLNode& after = alist->get_state ();
			tracker_editor.grid.register_automation_undo (alist, _("add automation event"), before, after);
		}
		return;
	}

	// Change existing value
	double awhen = ce->when;
	alist->modify (get_alist_iterator(rowi, param), awhen, val);
	XMLNode& after = alist->get_state ();
	tracker_editor.grid.register_automation_undo (alist, _("change automation event"), before, after);
}

void
AutomationPattern::delete_automation_value(int rowi, const Evoral::Parameter& param)
{
	boost::shared_ptr<ARDOUR::AutomationList> alist = get_alist (param);
	if (!alist) {
		return;
	}

	// Save state for undo
	XMLNode& before = alist->get_state ();

	alist->erase (get_alist_iterator (rowi, param));
	XMLNode& after = alist->get_state ();
	tracker_editor.grid.register_automation_undo (alist, _("delete automation event"), before, after);
}

std::pair<int, bool>
AutomationPattern::get_automation_delay (int rowi, const Evoral::Parameter& param) const
{
	if (const Evoral::ControlEvent* ce = get_control_event(rowi, param)) {
		double awhen = ce->when;
		int delay = TrackerUtils::is_region_automation (param) ?
			region_relative_delay_ticks_at_row(Temporal::Beats(awhen), rowi)
			: delay_ticks_at_row((samplepos_t)awhen, rowi);
		return std::make_pair(delay, true);
	}
	return std::make_pair(0, false);
}

void
AutomationPattern::set_automation_delay (int delay, int rowi, const Evoral::Parameter& param)
{
	// Check if within acceptable boundaries
	if (delay < delay_ticks_min() || delay_ticks_max() < delay) {
		return;
	}

	Temporal::Beats row_relative_beats = region_relative_beats_at_row(rowi, delay);
	uint32_t row_sample = sample_at_row(rowi, delay);

	// Make sure alist is defined
	boost::shared_ptr<ARDOUR::AutomationList> alist = get_alist (param);
	if (!alist) {
		return;
	}

	// Make sure a value is defined
	Evoral::ControlEvent* ce = get_control_event (rowi, param);
	if (!ce) {
		return;
	}

	// Change existing delay
	XMLNode& before = alist->get_state ();
	double awhen = TrackerUtils::is_region_automation (param) ?
		(row_relative_beats < start_beats ? start_beats : row_relative_beats).to_double()
		: row_sample;
	AutomationPattern::AutomationListIt auto_lst_it = get_alist_iterator (rowi, param);
	alist->modify (auto_lst_it, awhen, ce->value);
	XMLNode& after = alist->get_state ();
	tracker_editor.grid.register_automation_undo (alist, _("change automation event delay"), before, after);
}

AutomationPattern::RowToAutomationListIt::const_iterator
AutomationPattern::find_prev(RowToAutomationListIt::const_iterator it) const
{
	return --it;
}

AutomationPattern::RowToAutomationListIt::const_iterator
AutomationPattern::find_next(RowToAutomationListIt::const_iterator it) const
{
	return ++it;
}

AutomationPattern::RowToAutomationListIt::const_iterator
AutomationPattern::find_prev(uint32_t row, const RowToAutomationListIt& r2a) const
{
	RowToAutomationListIt::const_reverse_iterator rit =
		std::reverse_iterator<RowToAutomationListIt::const_iterator>(r2a.lower_bound(row));
	while (rit != r2a.rend() && rit->first == row) { ++rit; };
	return rit != r2a.rend() ? lattest(r2a.equal_range(rit->first)) : rit.base();
}

AutomationPattern::RowToAutomationListIt::const_iterator
AutomationPattern::find_next(uint32_t row, const RowToAutomationListIt& r2a) const
{
	RowToAutomationListIt::const_iterator it = r2a.upper_bound(row);
	while (it != r2a.end() && it->first == row) { ++it; };
	return it != r2a.end() ? earliest(r2a.equal_range(it->first)) : it;
}

std::pair<uint32_t, uint32_t>
AutomationPattern::prev_next_range(RowToAutomationListIt::const_iterator it, const RowToAutomationListIt& row2auto) const
{
	RowToAutomationListIt::const_iterator p_it = find_prev(it);
	RowToAutomationListIt::const_iterator n_it = find_next(it);
	uint32_t p_row = p_it != row2auto.end() ? p_it->first : 0;
	uint32_t n_row = n_it != row2auto.end() ? n_it->first : nrows - 1;
	p_row = std::min(p_row + 1, it->first);
	n_row = std::max(n_row - 1, it->first);
	return std::make_pair(p_row, n_row);
}

std::pair<uint32_t, uint32_t>
AutomationPattern::prev_next_range(uint32_t row, const RowToAutomationListIt& row2auto) const
{
	RowToAutomationListIt::const_iterator p_it = find_prev(row, row2auto);
	RowToAutomationListIt::const_iterator n_it = find_next(row, row2auto);
	uint32_t p_row = p_it != row2auto.end() ? p_it->first : 0;
	uint32_t n_row = n_it != row2auto.end() ? n_it->first : nrows - 1;
	p_row = std::min(p_row + 1, row);
	n_row = std::max(n_row - 1, row);
	return std::make_pair(p_row, n_row);
}

AutomationPattern::RowToAutomationListIt::const_iterator
AutomationPattern::earliest(const RowToAutomationListItRange& rng) const
{
	RowToAutomationListIt::const_iterator res_it = rng.first;
	RowToAutomationListIt::const_iterator it = res_it;
	++it;
	for (; it != rng.second; ++it) {
		if ((*res_it->second)->when < (*it->second)->when) {
			res_it = it;
		}
	}
	return res_it;
}

AutomationPattern::RowToAutomationListIt::const_iterator
AutomationPattern::lattest(const RowToAutomationListItRange& rng) const
{
	RowToAutomationListIt::const_iterator res_it = rng.first;
	RowToAutomationListIt::const_iterator it = res_it;
	++it;
	for (; it != rng.second; ++it) {
		if ((*it->second)->when < (*res_it->second)->when) {
			res_it = it;
		}
	}
	return res_it;
}

void
AutomationPattern::set_enabled(const Evoral::Parameter& param, bool enabled)
{
	param_to_enabled[param] = enabled;
}

bool
AutomationPattern::is_enabled(const Evoral::Parameter& param) const
{
	return param_to_enabled[param];
}

double
AutomationPattern::lower (const Evoral::Parameter& param) const
{
	const boost::shared_ptr<ARDOUR::AutomationControl> actrl = get_actrl(param);
	if (!actrl) {
		return -1e16;             // TODO: replace by something better
	}
	return actrl->lower ();
}

double
AutomationPattern::upper (const Evoral::Parameter& param) const
{
	const boost::shared_ptr<ARDOUR::AutomationControl> actrl = get_actrl(param);
	if (!actrl) {
		return 1e16;             // TODO: replace by something better
	}
	return actrl->upper ();
}

std::string
AutomationPattern::self_to_string() const
{
	std::stringstream ss;
	ss << "AutomationPattern[" << this << "]";
	return ss.str();
}

std::string
AutomationPattern::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePattern::to_string(indent);

	std::string header = indent + self_to_string() + " ";
	std::string indent_l1 = indent + "  ";
	std::string indent_l2 = indent_l1 + "  ";
	ss << std::endl << header << "param_to_row_to_ali:";
	for (ParamToRowToAutomationListIt::const_iterator it = param_to_row_to_ali.begin(); it != param_to_row_to_ali.end(); ++it) {
		ss << std::endl << indent_l1 << "param[" << it->first << "]:";
		for (std::multimap<uint32_t, AutomationListIt>::const_iterator r2a_it = it->second.begin(); r2a_it != it->second.end(); ++r2a_it) {
			ss << std::endl << indent_l2 << "row = " << r2a_it->first << ", auto_lst[" << *r2a_it->second << "] = (when=" << (*r2a_it->second)->when << ", value=" << (*r2a_it->second)->value << ")";
		}
	}
	ss << std::endl << header << "param_to_actrl:";
	for (ParamToAutomationControl::const_iterator it = param_to_actrl.begin(); it != param_to_actrl.end(); ++it) {
		const Evoral::Parameter& param = it->first;
		ss << std::endl << indent_l1 << "param[" << param << "] name = " << get_name(param) << ", actrl = " << it->second;
	}
	ss << std::endl << header << "param_to_enabled:";
	for (ParamToEnabled::const_iterator it = param_to_enabled.begin(); it != param_to_enabled.end(); ++it) {
		const Evoral::Parameter& param = it->first;
		ss << std::endl << indent_l1 << "param[" << param << "] enabled = " << it->second;
	}

	return ss.str();
}
