/*
    Copyright (C) 2015-2018 Nil Geisweiller

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

#include "tracker_utils.h"

#include "ardour/parameter_types.h"

#include "midi_region_view.h"
#include "audio_region_view.h"

using namespace Tracker;

bool region_position_less::operator()(boost::shared_ptr<ARDOUR::Region> lhs, boost::shared_ptr<ARDOUR::Region> rhs)
{
	return lhs->position() < rhs->position();
}

bool
TrackerUtils::is_pan_type (const Evoral::Parameter& param)
{
	static std::set<ARDOUR::AutomationType> pan_param_types = get_pan_param_types();
	return pan_param_types.find((ARDOUR::AutomationType)param.type()) != pan_param_types.end();
}

std::set<ARDOUR::AutomationType>
TrackerUtils::get_pan_param_types()
{
	std::set<ARDOUR::AutomationType> pan_param_types;
	pan_param_types.insert(ARDOUR::PanAzimuthAutomation);
	pan_param_types.insert(ARDOUR::PanElevationAutomation);
	pan_param_types.insert(ARDOUR::PanWidthAutomation);
	pan_param_types.insert(ARDOUR::PanFrontBackAutomation);
	pan_param_types.insert(ARDOUR::PanLFEAutomation);
	return pan_param_types;
}

bool
TrackerUtils::is_region_automation (const Evoral::Parameter& param)
{
	return ARDOUR::parameter_is_midi((ARDOUR::AutomationType)param.type());
}

char
TrackerUtils::digit_to_char(int digit, int base)
{
	return num_to_string(digit, base)[0];
}

int
TrackerUtils::char_to_digit(char c, int base)
{
	std::string s;
	s.push_back(c);
	return string_to_num<int>(std::string(0, c));
}

std::pair<int, int>
TrackerUtils::position_range (const std::string& str)
{
	size_t sepos = str.find('.'); // TODO support other locals
	int l = 0, u = 0;
	if (sepos == std::string::npos) {
		u = (int)str.size() - 1;
	} else {
		l = (int)sepos - (int)str.size() + 1;
		u = (int)sepos - 1;
	}
	return std::pair<int, int>(l, u);
}

std::string
TrackerUtils::pad (const std::string& str, int position)
{
	std::pair<int, int> pr = position_range (str);
	if (position < pr.first) {
		int diff = pr.first - position;
		return str + (pr.first == 0 ? "." : "") + std::string(diff, '0');
	}
	if (pr.second < position) {
		int diff = position - pr.second;
		return std::string(diff, '0') + str;
	}
	return str;
}

size_t
TrackerUtils::locate (const std::string& str, int position)
{
	std::pair<int, int> pr = position_range (str);
	if (pr.first <= position && position <= pr.second) {
		if (0 <= position) {
			return pr.second - position;
		} else {
			return str.size() + pr.first - position - 1;
		}
	}
	return std::string::npos;
}

uint8_t
TrackerUtils::pitch (uint8_t semitones, int octave)
{
	return (uint8_t)(octave + 1) * 12 + semitones;
}

uint8_t
TrackerUtils::parse_pitch (std::string text, int default_octave)
{
	// Add default octave, if the octave digit is missing
	if (!text.empty() && !std::isdigit(*text.rbegin()))
		text += to_string(default_octave);

	// Parse the note per se
	return ARDOUR::ParameterDescriptor::midi_note_num(text);
}

std::vector<boost::shared_ptr<ARDOUR::Region> >
TrackerUtils::get_sorted_regions(const RegionSelection& region_selection)
{
	std::vector<boost::shared_ptr<ARDOUR::Region> > regions;
	for (RegionSelection::const_iterator it = region_selection.begin(); it != region_selection.end(); ++it) {
		boost::shared_ptr<ARDOUR::Region> region = (*it)->region();
		regions.push_back(region);
	}
	std::sort(regions.begin(), regions.end(), region_position_less());
	return regions;
}

Temporal::samplepos_t
TrackerUtils::get_position(const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions)
{
	return regions.front()->position();
}

Temporal::samplepos_t
TrackerUtils::get_position(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
{
	return regions.front()->position();
}

Temporal::samplepos_t
TrackerUtils::get_position(const RegionSelection& region_selection)
{
	return get_position(get_sorted_regions(region_selection));
}

Temporal::samplecnt_t
TrackerUtils::get_length(const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions)
{
	return regions.back()->last_sample() + 1 - regions.front()->first_sample();
}

Temporal::samplecnt_t
TrackerUtils::get_length(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
{
	return regions.back()->last_sample() + 1 - regions.front()->first_sample();
}

Temporal::samplecnt_t
TrackerUtils::get_length(const RegionSelection& region_selection)
{
	return get_length(get_sorted_regions(region_selection));
}

Temporal::samplepos_t
TrackerUtils::get_first_sample(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
{
	return regions.front()->first_sample();
}

Temporal::samplepos_t
TrackerUtils::get_first_sample(const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions)
{
	return regions.front()->first_sample();
}

Temporal::samplepos_t
TrackerUtils::get_first_sample(const RegionSelection& region_selection)
{
	return get_first_sample(get_sorted_regions(region_selection));
}

Temporal::samplepos_t
TrackerUtils::get_last_sample(const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions)
{
	return regions.back()->last_sample();
}

Temporal::samplepos_t
TrackerUtils::get_last_sample(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
{
	return regions.back()->last_sample();
}

Temporal::samplepos_t
TrackerUtils::get_last_sample(const RegionSelection& region_selection)
{
	return get_last_sample(get_sorted_regions(region_selection));
}

bool
TrackerUtils::is_on_equal(NoteTypePtr ln, NoteTypePtr rn)
{
	return ln->time() == rn->time()
		&& ln->note() == rn->note()
		&& ln->velocity() == rn->velocity()
		&& ln->channel() == rn->channel();
}

bool
TrackerUtils::is_off_equal(NoteTypePtr ln, NoteTypePtr rn)
{
	return ln->end_time() == rn->end_time();
}
