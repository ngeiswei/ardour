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

#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_playlist.h"

#include "region_automation_pattern.h"

using namespace std;
using namespace ARDOUR;
using namespace Tracker;

/////////////////////////////
// RegionAutomationPattern //
/////////////////////////////

RegionAutomationPattern::RegionAutomationPattern(TrackerEditor& te,
                                                 boost::shared_ptr<MidiTrack> mt,
                                                 boost::shared_ptr<MidiRegion> region)
	: AutomationPattern(te, region)
	, midi_track(mt)
	, midi_model(region->midi_source(0)->model())
{
	setup_automation_controls ();
}

RegionAutomationPattern&
RegionAutomationPattern::operator=(const RegionAutomationPattern& other)
{
	AutomationPattern::operator=(other);
	midi_track = other.midi_track;
	midi_model = other.midi_model;

	return *this;
}

RegionAutomationPatternPhenomenalDiff
RegionAutomationPattern::phenomenal_diff(const RegionAutomationPattern& other) const
{
	RegionAutomationPatternPhenomenalDiff diff;
	diff.ap_diff = AutomationPattern::phenomenal_diff(other);
	return diff;
}

void RegionAutomationPattern::setup_automation_controls ()
{
	const set<Evoral::Parameter> midi_params = midi_track->midi_playlist()->contained_automation();
	for (set<Evoral::Parameter>::const_iterator i = midi_params.begin(); i != midi_params.end(); ++i)
		AutomationPattern::insert(midi_model->automation_control(*i));
}

void RegionAutomationPattern::insert(const Evoral::Parameter& param)
{
	AutomationPattern::insert(midi_model->automation_control(param, true));
}

uint32_t
RegionAutomationPattern::event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event)
{
	Temporal::Beats relative_beats(event->when);

	if (relative_beats < start_beats || start_beats + length_beats <= relative_beats)
		return INVALID_ROW;

	Temporal::Beats beats(relative_beats + position_beats - start_beats);
	uint32_t row = row_at_beats(beats);
	if (automations[param].count(row) != 0)
		row = row_at_beats_min_delay(beats);
	return row;
}

std::string
RegionAutomationPattern::self_to_string() const
{
	std::stringstream ss;
	ss << "RegionAutomationPattern[" << this << "]";
	return ss.str();
}

std::string
RegionAutomationPattern::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << AutomationPattern::to_string(indent);

	std::string header = indent + self_to_string() + " ";
	ss << header << "midi_track = " << midi_track << std::endl;
	ss << header << "midi_model = " << midi_model;

	return ss.str();
}
