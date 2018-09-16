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

using namespace std;
using namespace ARDOUR;
using Timecode::BBT_Time;

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

void AutomationPattern::update()
{
	set_row_range();

	automations.clear();

	for (AutomationControlSet::const_iterator actrl = _automation_controls.begin(); actrl != _automation_controls.end(); ++actrl) {
		boost::shared_ptr<AutomationList> al = (*actrl)->alist();
		const Evoral::Parameter& param = (*actrl)->parameter();
		// Build automation pattern
		for (AutomationList::iterator it = al->begin(); it != al->end(); ++it) {
			uint32_t row = event2row(param, *it);
			if (row != INVALID_ROW)
				automations[param].insert(RowToAutomationIt::value_type(row, it));
		}
	}
}

void AutomationPattern::insert(boost::shared_ptr<ARDOUR::AutomationControl> actrl)
{
	std::pair<AutomationControlSet::iterator, bool> result = _automation_controls.insert(actrl);
	// VT: re-enable connecting automation
	// if (result.second)
	// 	tracker_editor.connect_automation(actrl);
}

size_t AutomationPattern::get_asize (const Evoral::Parameter& param) const
{
	return get_alist(param)->size();
}

boost::shared_ptr<ARDOUR::AutomationControl> AutomationPattern::get_actrl(const Evoral::Parameter& param)
{
	if (!param)
		return NULL;

	for (AutomationControlSet::iterator actrl = _automation_controls.begin(); actrl != _automation_controls.end(); ++actrl)
		if (param == (*actrl)->parameter())
			return *actrl;

	return NULL;
}

const boost::shared_ptr<ARDOUR::AutomationControl> AutomationPattern::get_actrl(const Evoral::Parameter& param) const
{
	if (!param)
		return NULL;

	for (AutomationControlSet::const_iterator actrl = _automation_controls.begin(); actrl != _automation_controls.end(); ++actrl)
		if (param == (*actrl)->parameter())
			return *actrl;

	return NULL;
}

boost::shared_ptr<AutomationList>
AutomationPattern::get_alist (const Evoral::Parameter& param)
{
	if (boost::shared_ptr<ARDOUR::AutomationControl> actrl = get_actrl(param))
		return actrl->alist();

	return NULL;
}

const boost::shared_ptr<AutomationList>
AutomationPattern::get_alist (const Evoral::Parameter& param) const
{
	if (const boost::shared_ptr<ARDOUR::AutomationControl> actrl = get_actrl(param))
		return actrl->alist();

	return NULL;
}

bool AutomationPattern::is_displayable(uint32_t row, const Evoral::Parameter& param) const
{
	return automations.find(param) == automations.end()
		|| automations.find(param)->second.count(row) <= 1;
}

AutomationPattern::AutomationListIt
AutomationPattern::get_alist_iterator (size_t rowi, const Evoral::Parameter& param)
{
	return automations.find(param)->second.find(rowi)->second;
}

Evoral::ControlEvent*
AutomationPattern::get_control_event (size_t rowi, const Evoral::Parameter& param)
{
	std::map<Evoral::Parameter, RowToAutomationIt>::iterator it = automations.find(param);
	if (it == automations.end())
		return NULL;

	AutomationPattern::RowToAutomationIt::iterator auto_it = it->second.find(rowi);
	if (auto_it != it->second.end())
		return *auto_it->second;

	return NULL;
}

const Evoral::ControlEvent*
AutomationPattern::get_control_event (size_t rowi, const Evoral::Parameter& param) const
{
	std::map<Evoral::Parameter, RowToAutomationIt>::const_iterator it = automations.find(param);
	if (it == automations.end())
		return NULL;

	AutomationPattern::RowToAutomationIt::const_iterator auto_it = it->second.find(rowi);
	if (auto_it != it->second.end())
		return *auto_it->second;

	return NULL;
}

std::pair<double, bool>
AutomationPattern::get_automation_value (size_t rowi, const Evoral::Parameter& param)
{
	if (const Evoral::ControlEvent* ce = get_control_event (rowi, param))
		return std::make_pair(ce->value, true);

	return std::make_pair(0.0, false);	
}

void
AutomationPattern::set_automation_value (double val, size_t rowi, const Evoral::Parameter& param, int delay)
{
	// Fetch automation control and list
	boost::shared_ptr<ARDOUR::AutomationControl> actrl = get_actrl(param);
	if (!actrl)
		return;
	boost::shared_ptr<AutomationList> alist = actrl->alist();

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
	boost::shared_ptr<AutomationList> alist = get_alist (param);
	if (!alist)
		return;

		// Save state for undo
	XMLNode& before = alist->get_state ();

	alist->erase (get_alist_iterator (rowi, param));
	XMLNode& after = alist->get_state ();
	tracker_editor.grid.register_automation_undo (alist, _("delete automation event"), before, after);
}

std::pair<int, bool>
AutomationPattern::get_automation_delay (int rowi, const Evoral::Parameter& param)
{
	if (Evoral::ControlEvent* ce = get_control_event(rowi, param)) {
		double awhen = ce->when;
		int delay = TrackerUtils::is_region_automation (param) ?
			region_relative_delay_ticks(Temporal::Beats(awhen), rowi)
			: delay_ticks((samplepos_t)awhen, rowi);
		return std::make_pair(delay, true);
	}
	return std::make_pair(0, false);
}

void
AutomationPattern::set_automation_delay (int delay, int rowi, const Evoral::Parameter& param)
{
	// Check if within acceptable boundaries
	if (delay < delay_ticks_min() || delay_ticks_max() < delay)
		return;

	Temporal::Beats row_relative_beats = region_relative_beats_at_row(rowi, delay);
	uint32_t row_sample = sample_at_row(rowi, delay);

	// Make sure alist is defined
	boost::shared_ptr<AutomationList> alist = get_alist (param);
	if (!alist)
		return;

	// Make sure a value is defined
	Evoral::ControlEvent* ce = get_control_event (rowi, param);
	if (!ce)
		return;

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
