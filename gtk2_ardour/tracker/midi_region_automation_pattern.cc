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

#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"

#include "midi_region_automation_pattern.h"

using namespace std;
using namespace ARDOUR;
using namespace Tracker;

/////////////////////////////////
// MidiRegionAutomationPattern //
/////////////////////////////////

MidiRegionAutomationPattern::MidiRegionAutomationPattern (TrackerEditor& te,
                                                          MidiTrackPtr mt,
                                                          MidiRegionPtr mr,
                                                          bool connect)
	: AutomationPattern (te, mr, connect)
	, midi_track (mt)
	, midi_model (mr->model ())
	, midi_region(mr)
{
	setup_automation_controls ();
}

MidiRegionAutomationPattern&
MidiRegionAutomationPattern::operator= (const MidiRegionAutomationPattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	AutomationPattern::operator= (other);
	midi_track = other.midi_track;
	midi_model = other.midi_model;

	return *this;
}

MidiRegionAutomationPatternPhenomenalDiff
MidiRegionAutomationPattern::phenomenal_diff (const MidiRegionAutomationPattern& prev) const
{
	MidiRegionAutomationPatternPhenomenalDiff diff;
	if (!prev.enabled && !enabled) {
		return diff;
	}

	diff.ap_diff = AutomationPattern::phenomenal_diff (prev);
	return diff;
}

void MidiRegionAutomationPattern::setup_automation_controls ()
{
	const set<Evoral::Parameter> midi_params = midi_track->midi_playlist ()->contained_automation ();
	for (set<Evoral::Parameter>::const_iterator i = midi_params.begin (); i != midi_params.end (); ++i) {
		AutomationPattern::insert_actl (midi_model->automation_control (*i, true), midi_track->describe_parameter (*i));
	}
}

void MidiRegionAutomationPattern::insert (const Evoral::Parameter& param)
{
	AutomationPattern::insert_actl (midi_model->automation_control (param, true), midi_track->describe_parameter (param));
}

Temporal::Beats
MidiRegionAutomationPattern::event2beats (const Evoral::Parameter& param, const Evoral::ControlEvent* event) const
{
	Temporal::Beats relative_beats (event->when.beats ());
	return midi_region->source_beats_to_absolute_beats (relative_beats);
}

double
MidiRegionAutomationPattern::get_automation_interpolation_value (int rowi, const Evoral::Parameter& param) const
{
	double inter_auto_val = 0;
	if (const AutomationListPtr alist = get_alist (param)) {
		// We need to use ControlList::rt_safe_eval instead of ControlList::eval,
		// otherwise the lock inside eval interferes with the lock inside
		// ControlList::erase. Though if mark_dirty is called outside of the
		// scope of the WriteLock in ControlList::erase and such, then eval can
		// be used.  Get corresponding beats and samples
		Temporal::timepos_t awhen = Temporal::timepos_t(region_relative_beats_at_row (rowi));
		bool ok;
		inter_auto_val = alist->rt_safe_eval (awhen, ok);
	}
	return inter_auto_val;
}

std::string
MidiRegionAutomationPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "MidiRegionAutomationPattern[" << this << "]";
	return ss.str ();
}

std::string
MidiRegionAutomationPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << AutomationPattern::to_string (indent) << std::endl;

	std::string header = indent + self_to_string () + " ";
	ss << header << "midi_track = " << midi_track << std::endl;
	ss << header << "midi_model = " << midi_model;

	return ss.str ();
}
