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

#include "pbd/i18n.h"

#include "automation_pattern.h"
#include "tracker_editor.h"
#include "tracker_utils.h"

using namespace Tracker;

///////////////////////
// AutomationPattern //
///////////////////////

AutomationPattern::AutomationPattern (TrackerEditor& te,
                                      RegionPtr region,
                                      bool cnct)
	: BasePattern (te, region)
	, connect (cnct)
{
}

AutomationPattern::AutomationPattern (TrackerEditor& te,
                                      Temporal::timepos_t pos,
                                      Temporal::timepos_t sta,
                                      Temporal::timecnt_t len,
                                      Temporal::timepos_t ed,
                                      Temporal::timepos_t ntl,
                                      bool cnct)
	: BasePattern (te, pos, sta, len, ed, ntl)
	, connect(cnct)
{
}

AutomationPattern&
AutomationPattern::operator= (const AutomationPattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	BasePattern::operator= (other);

	// Deep copy _automation_controls to be sure that changing the event doesn't
	// change the copy
	param_to_actl.clear ();
	for (ParamAutomationControlMap::const_iterator it = other.param_to_actl.begin (); it != other.param_to_actl.end (); ++it) {
		param_to_actl.insert (clone_param_actl (*it));
	}

	param_to_name.clear ();
	for (ParamNameMap::const_iterator it = other.param_to_name.begin (); it != other.param_to_name.end (); ++it) {
		param_to_name.insert (*it);
	}

	// Update automations using the deep copy
	update_automations ();

	return *this;
}

AutomationControlPtr
AutomationPattern::clone_actl (AutomationControlPtr actl) const
{
	AutomationControlPtr actl_cp (new ARDOUR::AutomationControl (const_cast<ARDOUR::Session&> (actl->session ()), actl->parameter (), actl->desc (), clone_alist (actl->alist ())));
	return actl_cp;
}

ParamAutomationControlPair
AutomationPattern::clone_param_actl (const ParamAutomationControlPair& param_actl) const
{
	return std::make_pair (param_actl.first, clone_actl (param_actl.second));
}

AutomationListPtr
AutomationPattern::clone_alist (AutomationListPtr alist) const
{
	// TODO: optimize by only copying events within the time range in
	// consideration
	// NEXT: Make freeze!!!!
	// AutomationListPtr alist_cp (new ARDOUR::AutomationList (*alist));
	// return alist_cp;
	return alist;
}

void
AutomationPattern::rows_diff (const RowToControlEvents& l_row2ces, const RowToControlEvents& r_row2ces, std::set<int>& rows) const
{
	for (RowToControlEvents::const_iterator l_it = l_row2ces.begin (); l_it != l_row2ces.end ();) {
		int row = l_it->first;
		const Evoral::ControlEvent& l_ce = l_it->second;

		// First, look at the difference in displayability
		bool is_cell_displayable = is_displayable (row, l_row2ces);
		bool cell_diff = is_cell_displayable != is_displayable (row, r_row2ces);
		if (cell_diff) {
			rows.insert (row);
		}
		if (!is_cell_displayable) {
			// It means there are more than one value, jump to the next row
			l_it = l_row2ces.upper_bound (row);
			continue;
		}
		if (cell_diff) {
			++l_it;
			continue;
		}

		// Check if the event exist in other, and if so if it is equal
		RowToControlEvents::const_iterator r_it = r_row2ces.find (row);
		if (r_it != r_row2ces.end ()) {
			const Evoral::ControlEvent& r_ce = r_it->second;
			if (!TrackerUtils::is_equal (l_ce, r_ce)) {
				// Update all affected row, taking interpolation into account
				std::pair<int, int> r = prev_next_range (r_it, l_row2ces);
				for (int rowi = r.first; rowi <= r.second; rowi++) {
					rows.insert (rowi);
				}
			}
		} else {
			// Update all affected row, taking interpolation into account
			std::pair<int, int> r = prev_next_range (row, l_row2ces);
			for (int rowi = r.first; rowi <= r.second; rowi++) {
				rows.insert (rowi);
			}
		}
		++l_it;
	}
}

AutomationPatternPhenomenalDiff
AutomationPattern::phenomenal_diff (const AutomationPattern& prev) const
{
	AutomationPatternPhenomenalDiff diff;
	if (!prev.enabled && !enabled) {
		return diff;
	}

	for (ParamEnabledMap::const_iterator pe_it = param_to_enabled.begin (); pe_it != param_to_enabled.end (); ++pe_it) {
		const Evoral::Parameter& param = pe_it->first;
		RowsPhenomenalDiff rd;

		// If this is disabled ignore their differences, otherwise consider full
		// phenomenal diff if one is enabled while the other is not.
		if (!pe_it->second) {
			continue;
		}
		if (pe_it->second != prev.param_to_enabled[param]) {
			rd.full = true;
			diff.param2rows_diff[param] = rd;
			continue;
		}

		// Make sure the parameter is in prev, otherwise make the corresponding RowsPhenomenalDiff full
		ParamToRowToControlEvents::const_iterator pra_it = param_to_row_to_ces.find (param);
		ParamToRowToControlEvents::const_iterator prev_pra_it = prev.param_to_row_to_ces.find (param);
		if (pra_it == param_to_row_to_ces.end () && prev_pra_it == prev.param_to_row_to_ces.end ()) {
			continue;
		}
		if (pra_it == param_to_row_to_ces.end () || prev_pra_it == prev.param_to_row_to_ces.end ()) {
			rd.full = true;
			diff.param2rows_diff[param] = rd;
			continue;
		}

		// Both are defined, consider the rows for phenomenal diff
		const RowToControlEvents& row2ces = pra_it->second;
		const RowToControlEvents& prev_row2ces = prev_pra_it->second;
		rows_diff (row2ces, prev_row2ces, rd.rows);
		rows_diff (prev_row2ces, row2ces, rd.rows);
		if (!rd.empty ()) {
			diff.param2rows_diff[param] = rd;
		}
	}

	return diff;
}

Temporal::Beats
AutomationPattern::event2beats (const Evoral::Parameter& param, const Evoral::ControlEvent* event) const
{
	std::cerr << "AutomationPattern::event2beats not implemented" << std::endl;
	assert (false);
	return Temporal::Beats();
}

void
AutomationPattern::update ()
{
	set_row_range ();
	update_automations ();
}

void
AutomationPattern::update_automations ()
{
	param_to_row_to_ces.clear ();
	for (ParamAutomationControlMap::const_iterator param_actl = param_to_actl.begin (); param_actl != param_to_actl.end (); ++param_actl) {
		AutomationControlPtr actl = param_actl->second;
		AutomationListPtr al = actl->alist ();
		const Evoral::Parameter& param = actl->parameter ();

		// Save CPU resources
		if (!param_to_enabled[param]) {
			continue;
		}

		// Not sure that is needed, but better safe than sorry.
		if (!al) {
			continue;
		}

		update_automation (param, al);
	}
}

void
AutomationPattern::update_automation (const Evoral::Parameter& param, AutomationListPtr alist)
{
	// Build automation pattern
	for (ARDOUR::AutomationList::iterator it = alist->begin (); it != alist->end (); ++it) {
		update_automation_event (param, *it);
	}
}

void
AutomationPattern::update_automation_event (const Evoral::Parameter& param, Evoral::ControlEvent* event)
{
	int row = find_nearest_row (param, event);
	if (row != INVALID_ROW) {
		param_to_row_to_ces[param].insert (RowToControlEvents::value_type (row, *event));
	}
}

bool
AutomationPattern::is_row_available (const Evoral::Parameter& param, int row, Evoral::ControlEvent* event) const
{
	// NEXT.4: Hint: take inspiration from MidiNotesPattern::is_on_row_available
	//         in midi_notes_pattern.cc
	return true;
}

int
AutomationPattern::row_suggestion (const Evoral::Parameter& param, Evoral::ControlEvent* event, int rank) const
{
	// NEXT.4: Hint: take inspiration from MidiNotesPattern::on_row_suggestion
	//         in midi_notes_pattern.cc
	Temporal::Beats beats = event2beats (param, event);
	return row_at_beats (beats);
}

int
AutomationPattern::find_nearest_row (const Evoral::Parameter& param, Evoral::ControlEvent* event) const
{
	int rank = 0;
	int row = row_suggestion (param, event, rank);
	int default_row = row;
	do {
		if (is_row_available (param, row, event)) {
			return row;
		}
		rank++;
		row = row_suggestion (param, event, rank);
	} while (rank < 3 && 0 <= row);
	return default_row;

	// Temporal::Beats beats = event2beats (param, event);
	// // NEXT.4: this is where the fix is.  Hint: take inspiration from
	// //         MidiNotesPattern::find_nearest_on_row in midi_notes_pattern.cc
	// return row_at_beats (beats);
}

void
AutomationPattern::insert_actl (AutomationControlPtr actl, const std::string& name)
{
	Evoral::Parameter param = actl->parameter ();
	std::pair<ParamAutomationControlMap::iterator, bool> actl_result = param_to_actl.insert (std::make_pair (param, actl));
	param_to_name.insert (std::make_pair (param, name));
	if (actl_result.second && connect) {
		tracker_editor.connect_automation (actl);
	}
}

bool
AutomationPattern::is_empty (const Evoral::Parameter& param) const
{
	return get_alist (param)->size () == 0;
}

AutomationControlPtr AutomationPattern::get_actl (const Evoral::Parameter& param)
{
	if (!param) {
		return 0;
	}

	ParamAutomationControlMap::iterator it = param_to_actl.find (param);
	if (it != param_to_actl.end ()) {
		return it->second;
	}

	return 0;
}

const AutomationControlPtr
AutomationPattern::get_actl (const Evoral::Parameter& param) const
{
	if (!param) {
		return 0;
	}

	ParamAutomationControlMap::const_iterator it = param_to_actl.find (param);
	if (it != param_to_actl.end ()) {
		return it->second;
	}

	return 0;
}

size_t
AutomationPattern::control_events_count (int rowi, const Evoral::Parameter& param) const
{
	ParamToRowToControlEvents::const_iterator it = param_to_row_to_ces.find (param);
	if (it != param_to_row_to_ces.end ()) {
		return it->second.count (rowi);
	}
	return 0;
}

RowToControlEventsRange
AutomationPattern::control_events_range (int rowi, const Evoral::Parameter& param) const
{
	return param_to_row_to_ces.find (param)->second.equal_range (rowi);
}

std::string
AutomationPattern::get_name (const Evoral::Parameter& param) const
{
	if (!param) {
		return "";
	}

	ParamNameMap::const_iterator it = param_to_name.find (param);
	if (it != param_to_name.end ()) {
		return it->second;
	}

	return "";
}

AutomationListPtr
AutomationPattern::get_alist (const Evoral::Parameter& param)
{
	if (AutomationControlPtr actl = get_actl (param)) {
		return actl->alist ();
	}

	return 0;
}

const AutomationListPtr
AutomationPattern::get_alist (const Evoral::Parameter& param) const
{
	if (const AutomationControlPtr actl = get_actl (param)) {
		return actl->alist ();
	}

	return 0;
}

bool
AutomationPattern::is_displayable (int row, const Evoral::Parameter& param) const
{
	ParamToRowToControlEvents::const_iterator pit = param_to_row_to_ces.find (param);
	return pit == param_to_row_to_ces.end () || is_displayable (row, pit->second);
}

bool
AutomationPattern::is_displayable (int row, const RowToControlEvents& r2ces)
{
	return r2ces.count (row) <= 1;
}

AutomationListIt
AutomationPattern::get_alist_iterator (int rowi, const Evoral::Parameter& param)
{
	Temporal::timepos_t when = get_control_event (rowi, param)->when;
	AutomationListPtr alist = get_alist (param);
	const Evoral::ControlEvent cp (when, 0.0);
	AutomationListIt it = std::lower_bound(alist->begin(), alist->end(), &cp, Evoral::ControlList::time_comparator);
	if (it != alist->end() && (*it)->when == when) {
		return it;
	}
	return alist->end();
}

Evoral::ControlEvent*
AutomationPattern::get_control_event (int rowi, const Evoral::Parameter& param)
{
	ParamToRowToControlEvents::iterator it = param_to_row_to_ces.find (param);
	if (it == param_to_row_to_ces.end ()) {
		return 0;
	}

	RowToControlEvents::iterator ces_it = it->second.find (rowi);
	if (ces_it != it->second.end ()) {
		return &ces_it->second;
	}

	return 0;
}

const Evoral::ControlEvent*
AutomationPattern::get_control_event (int rowi, const Evoral::Parameter& param) const
{
	ParamToRowToControlEvents::const_iterator it = param_to_row_to_ces.find (param);
	if (it == param_to_row_to_ces.end ()) {
		return 0;
	}

	RowToControlEvents::const_iterator ces_it = it->second.find (rowi);
	if (ces_it != it->second.end ()) {
		return &ces_it->second;
	}

	return 0;
}

std::vector<Temporal::BBT_Time>
AutomationPattern::get_automation_bbt_seq (int rowi, const Evoral::Parameter& param) const
{
	std::vector<Temporal::BBT_Time> bbt_seq;
	RowToControlEventsRange rng = control_events_range (rowi, param);
	for (; rng.first != rng.second; rng.first++) {
		bbt_seq.push_back (get_automation_bbt (param, rng.first));
	}
	return bbt_seq;
}

std::pair<double, bool>
AutomationPattern::get_automation_value (int rowi, const Evoral::Parameter& param) const
{
	if (const Evoral::ControlEvent* ce = get_control_event (rowi, param)) {
		return std::make_pair (ce->value, true);
	}

	return std::make_pair (0.0, false);
}

double
AutomationPattern::get_automation_value (RowToControlEvents::const_iterator it) const
{
	return it->second.value;
}

std::vector<double>
AutomationPattern::get_automation_value_seq (int rowi, const Evoral::Parameter& param) const
{
	std::vector<double> value_seq;
	RowToControlEventsRange rng = control_events_range (rowi, param);
	for (; rng.first != rng.second; rng.first++) {
		value_seq.push_back (get_automation_value (rng.first));
	}
	return value_seq;
}

double
AutomationPattern::get_automation_interpolation_value (int rowi, const Evoral::Parameter& param) const
{
	double inter_auto_val = 0;
	if (const AutomationListPtr alist = get_alist (param)) {
		// We need to use ControlList::rt_safe_eval instead of ControlList::eval,
		// otherwise the lock inside eval interferes with the lock inside
		// ControlList::erase. Though if mark_dirty is called outside of the
		// scope of the WriteLock in ControlList::erase and such, then eval can
		// be used.  Get corresponding beats and samples
		Temporal::timepos_t awhen = Temporal::timepos_t(sample_at_row (rowi));
		bool ok;
		inter_auto_val = alist->rt_safe_eval (awhen, ok);
	}
	return inter_auto_val;
}

void
AutomationPattern::set_automation_value (double val, int rowi, const Evoral::Parameter& param, int delay)
{
	// Fetch automation control and list
	AutomationControlPtr actl = get_actl (param);
	if (!actl) {
		return;
	}
	AutomationListPtr alist = actl->alist ();

	// Clamp val to its range
	val = TrackerUtils::clamp (val, actl->lower (), actl->upper ());

	// Find control event at rowi, if any
	Evoral::ControlEvent* ce = get_control_event (rowi, param);

	// If no existing value, insert one
	if (!ce) {
		Temporal::Beats row_relative_beats = region_relative_beats_at_row (rowi, delay);
		Temporal::samplepos_t row_sample = sample_at_row (rowi, delay);
		Temporal::timepos_t awhen = TrackerUtils::is_region_automation (param) ? Temporal::timepos_t (row_relative_beats) : Temporal::timepos_t (row_sample);
		add_automation_point (alist, awhen, val);
		return;
	}

	// Change existing value
	Temporal::timepos_t awhen = ce->when;
	modify_automation_point (alist, get_alist_iterator (rowi, param), awhen, val);
}

void
AutomationPattern::delete_automation_value (int rowi, const Evoral::Parameter& param)
{
	AutomationListPtr alist = get_alist (param);
	if (!alist) {
		return;
	}
	erase_automation_point (alist, get_alist_iterator (rowi, param));
}

std::pair<int, bool>
AutomationPattern::get_automation_delay (int rowi, const Evoral::Parameter& param) const
{
	return get_automation_delay (rowi, param, get_control_event (rowi, param));
}

std::pair<int, bool>
AutomationPattern::get_automation_delay (int rowi, const Evoral::Parameter& param, const Evoral::ControlEvent* ce) const
{
	if (ce) {
		Temporal::timepos_t awhen = ce->when;
		// TODO: we may want to overload MidiRegionAutomationPattern instead of
		// using TrackerUtils::is_region_automation.
		int delay = TrackerUtils::is_region_automation (param) ?
			region_relative_delay_ticks_at_row (awhen.beats (), rowi)
			: delay_ticks_at_row (awhen.samples (), rowi);
		return std::make_pair (delay, true);
	}
	return std::make_pair (0, false);
}

int
AutomationPattern::get_automation_delay (const Evoral::Parameter& param, RowToControlEvents::const_iterator it) const
{
	int row_idx = it->first;
	const Evoral::ControlEvent* ce = &it->second;
	return get_automation_delay (row_idx, param, ce).first;
}

std::vector<int>
AutomationPattern::get_automation_delay_seq (int rowi, const Evoral::Parameter& param) const
{
	std::vector<int> delay_seq;
	RowToControlEventsRange rng = control_events_range (rowi, param);
	for (; rng.first != rng.second; rng.first++) {
		delay_seq.push_back (get_automation_delay (param, rng.first));
	}
	return delay_seq;
}

void
AutomationPattern::set_automation_delay (int delay, int rowi, const Evoral::Parameter& param)
{
	// Check if within acceptable boundaries
	if (delay < delay_ticks_min () || delay_ticks_max () < delay) {
		return;
	}

	Temporal::Beats row_relative_beats = region_relative_beats_at_row (rowi, delay);
	int row_sample = sample_at_row (rowi, delay);

	// Make sure alist is defined
	AutomationListPtr alist = get_alist (param);
	if (!alist) {
		return;
	}

	// Make sure a value is defined
	Evoral::ControlEvent* ce = get_control_event (rowi, param);
	if (!ce) {
		return;
	}

	// Change existing delay
	Temporal::timepos_t awhen = TrackerUtils::is_region_automation (param) ?
		Temporal::timepos_t (row_relative_beats < start_beats ? start_beats : row_relative_beats)
		: Temporal::timepos_t (row_sample);
	modify_automation_point (alist, get_alist_iterator (rowi, param), awhen, ce->value);
}

Temporal::BBT_Time
AutomationPattern::get_automation_bbt (const Evoral::Parameter& param, RowToControlEvents::const_iterator it) const
{
	int row_idx = it->first;
	int delay = get_automation_delay (param, it);
	return bbt_at_row (row_idx, delay);
}

void
AutomationPattern::add_automation_point (AutomationListPtr alist, Temporal::timepos_t when, double val)
{
	// Save state for undo
	XMLNode& before = alist->get_state ();
	if (alist->editor_add (when, val, false)) {
		XMLNode& after = alist->get_state ();
		register_automation_undo (alist, _("add automation event"), before, after);
	}
}

void
AutomationPattern::modify_automation_point (AutomationListPtr alist, AutomationListIt it, Temporal::timepos_t when, double val)
{
	// Save state for undo
	XMLNode& before = alist->get_state ();
	alist->modify (it, when, val);
	XMLNode& after = alist->get_state ();
	register_automation_undo (alist, _("change automation event"), before, after);
}

void
AutomationPattern::erase_automation_point (AutomationListPtr alist, AutomationListIt it)
{
	// Save state for undo
	XMLNode& before = alist->get_state ();
	alist->erase (it);
	XMLNode& after = alist->get_state ();
	register_automation_undo (alist, _("delete automation event"), before, after);
}

void
AutomationPattern::register_automation_undo (AutomationListPtr alist, const std::string& opname, XMLNode& before, XMLNode& after)
{
	tracker_editor.public_editor.begin_reversible_command (opname);
	tracker_editor.session->add_command (new MementoCommand<ARDOUR::AutomationList> (*alist.get (), &before, &after));
	tracker_editor.public_editor.commit_reversible_command ();
	tracker_editor.session->set_dirty ();
}

RowToControlEvents::const_iterator
AutomationPattern::find_prev (RowToControlEvents::const_iterator it) const
{
	return --it;
}

RowToControlEvents::const_iterator
AutomationPattern::find_next (RowToControlEvents::const_iterator it) const
{
	return ++it;
}

RowToControlEvents::const_iterator
AutomationPattern::find_prev (int row, const RowToControlEvents& r2ces) const
{
	RowToControlEvents::const_reverse_iterator rit =
		std::reverse_iterator<RowToControlEvents::const_iterator> (r2ces.lower_bound (row));
	while (rit != r2ces.rend () && rit->first == row) { ++rit; };
	return rit != r2ces.rend () ? lattest (r2ces.equal_range (rit->first)) : rit.base ();
}

RowToControlEvents::const_iterator
AutomationPattern::find_next (int row, const RowToControlEvents& r2ces) const
{
	RowToControlEvents::const_iterator it = r2ces.upper_bound (row);
	while (it != r2ces.end () && it->first == row) { ++it; };
	return it != r2ces.end () ? earliest (r2ces.equal_range (it->first)) : it;
}

std::pair<int, int>
AutomationPattern::prev_next_range (RowToControlEvents::const_iterator it, const RowToControlEvents& row2ces) const
{
	RowToControlEvents::const_iterator p_it = find_prev (it);
	RowToControlEvents::const_iterator n_it = find_next (it);
	int p_row = p_it != row2ces.end () ? p_it->first : 0;
	int n_row = n_it != row2ces.end () ? n_it->first : nrows - 1;
	p_row = std::min (p_row + 1, it->first);
	n_row = std::max (n_row - 1, it->first);
	return std::make_pair (p_row, n_row);
}

std::pair<int, int>
AutomationPattern::prev_next_range (int row, const RowToControlEvents& row2ces) const
{
	RowToControlEvents::const_iterator p_it = find_prev (row, row2ces);
	RowToControlEvents::const_iterator n_it = find_next (row, row2ces);
	int p_row = p_it != row2ces.end () ? p_it->first : 0;
	int n_row = n_it != row2ces.end () ? n_it->first : nrows - 1;
	p_row = std::min (p_row + 1, row);
	n_row = std::max (n_row - 1, row);
	return std::make_pair (p_row, n_row);
}

RowToControlEvents::const_iterator
AutomationPattern::earliest (const RowToControlEventsRange& rng) const
{
	RowToControlEvents::const_iterator res_it = rng.first;
	RowToControlEvents::const_iterator it = res_it;
	++it;
	for (; it != rng.second; ++it) {
		if (res_it->second.when < it->second.when) {
			res_it = it;
		}
	}
	return res_it;
}

RowToControlEvents::const_iterator
AutomationPattern::lattest (const RowToControlEventsRange& rng) const
{
	RowToControlEvents::const_iterator res_it = rng.first;
	RowToControlEvents::const_iterator it = res_it;
	++it;
	for (; it != rng.second; ++it) {
		if (it->second.when < res_it->second.when) {
			res_it = it;
		}
	}
	return res_it;
}

void
AutomationPattern::set_param_enabled (const Evoral::Parameter& param, bool enabled)
{
	param_to_enabled[param] = enabled;
}

bool
AutomationPattern::is_param_enabled (const Evoral::Parameter& param) const
{
	return param_to_enabled[param];
}

ParameterSet
AutomationPattern::get_enabled_parameters () const
{
	ParameterSet eps;
	for (const auto& pte : param_to_enabled) {
		if (pte.second) {
			eps.insert(pte.first);
		}
	}
	return eps;
}

double
AutomationPattern::lower (const Evoral::Parameter& param) const
{
	const AutomationControlPtr actl = get_actl (param);
	if (!actl) {
		return -1e16;             // TODO: replace by something better
	}
	return actl->lower ();
}

double
AutomationPattern::upper (const Evoral::Parameter& param) const
{
	const AutomationControlPtr actl = get_actl (param);
	if (!actl) {
		return 1e16;             // TODO: replace by something better
	}
	return actl->upper ();
}

std::string
AutomationPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "AutomationPattern[" << this << "]";
	return ss.str ();
}

std::string
AutomationPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePattern::to_string (indent);

	std::string header = indent + self_to_string () + " ";
	std::string indent_l1 = indent + "  ";
	std::string indent_l2 = indent_l1 + "  ";
	ss << std::endl << header << "param_to_row_to_ces:";
	for (ParamToRowToControlEvents::const_iterator it = param_to_row_to_ces.begin (); it != param_to_row_to_ces.end (); ++it) {
		ss << std::endl << indent_l1 << "param[" << it->first << "]:";
		for (RowToControlEvents::const_iterator r2ces_it = it->second.begin (); r2ces_it != it->second.end (); ++r2ces_it) {
			ss << std::endl << indent_l2 << "row = " << r2ces_it->first
				<< ", ces[" << &r2ces_it->second << "] = (when=" << r2ces_it->second.when << ", value=" << r2ces_it->second.value << ")";
		}
	}
	ss << std::endl << header << "param_to_actl:";
	for (ParamAutomationControlMap::const_iterator it = param_to_actl.begin (); it != param_to_actl.end (); ++it) {
		const Evoral::Parameter& param = it->first;
		ss << std::endl << indent_l1 << "param[" << param << "] name = " << get_name (param) << ", actl = " << it->second;
	}
	ss << std::endl << header << "param_to_enabled:";
	for (ParamEnabledMap::const_iterator it = param_to_enabled.begin (); it != param_to_enabled.end (); ++it) {
		const Evoral::Parameter& param = it->first;
		ss << std::endl << indent_l1 << "param[" << param << "] enabled = " << it->second;
	}

	return ss.str ();
}
