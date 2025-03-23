/*
 * Copyright (C) 2015-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include <cstdlib>

#include "ardour/parameter_types.h"

#include "audio_region_view.h"
#include "midi_region_view.h"
#include "tracker_utils.h"

using namespace Tracker;

bool region_position_less::operator () (RegionPtr lhs, RegionPtr rhs)
{
	return lhs->position () < rhs->position ();
}

bool
TrackerUtils::is_pan_type (const Evoral::Parameter& param)
{
	static std::set<ARDOUR::AutomationType> pan_param_types = get_pan_param_types ();
	return pan_param_types.find ((ARDOUR::AutomationType)param.type ()) != pan_param_types.end ();
}

std::set<ARDOUR::AutomationType>
TrackerUtils::get_pan_param_types ()
{
	std::set<ARDOUR::AutomationType> pan_param_types;
	pan_param_types.insert (ARDOUR::PanAzimuthAutomation);
	pan_param_types.insert (ARDOUR::PanElevationAutomation);
	pan_param_types.insert (ARDOUR::PanWidthAutomation);
	pan_param_types.insert (ARDOUR::PanFrontBackAutomation);
	pan_param_types.insert (ARDOUR::PanLFEAutomation);
	return pan_param_types;
}

bool
TrackerUtils::is_region_automation (const Evoral::Parameter& param)
{
	return ARDOUR::parameter_is_midi ((ARDOUR::AutomationType)param.type ());
}

std::string
TrackerUtils::rm_point_zeros (const std::string& str)
{
	std::string::size_type point_pos = str.rfind ('.');
	if (point_pos == std::string::npos) {
		return str;
	}
	std::string::size_type zero_pos = str.size ();
	while (point_pos < zero_pos) {
		if (str[zero_pos - 1] == '0') {
			zero_pos--;
		} else {
			break;
		}
	}
	return str.substr (0, point_pos + 1 == zero_pos ? point_pos : zero_pos);
}

std::string
TrackerUtils::num_to_string (int n, int base, int precision)
{
	std::stringstream ss;
	if (base == 16) {
		ss << std::uppercase << std::hex;
	}
	if (n < 0) {
		ss << "-";
	}
	ss << std::abs(n);
	return ss.str();
}

// TODO: bug if precision is above 6
std::string
TrackerUtils::num_to_string (double n, int base, int precision)
{
	std::stringstream ss;
	if (base == 10) {
		ss << std::fixed << std::setprecision (precision) << n;
	} else {
		int numerator = std::round(n * std::pow(base, precision));
		std::string numerator_str = num_to_string (numerator, base);
		int pos = (int)numerator_str.size () - precision;
		ss << insert_point (numerator_str, pos);
	}
	return rm_point_zeros (ss.str ());
}

char
TrackerUtils::digit_to_char (int digit, int base)
{
	return num_to_string (digit, base)[0];
}

int
TrackerUtils::char_to_digit (char c, int base)
{
	std::string s;
	s.push_back (c);
	return string_to_num<int> (std::string (0, c), base);
}

size_t
TrackerUtils::point_position (const std::string& str)
{
	size_t point_pos = str.find (".");
	return point_pos == std::string::npos ? str.size () : point_pos;
}

std::pair<int, int>
TrackerUtils::position_range (const std::string& str)
{
	size_t sepos = str.find ('.'); // TODO support other locals
	int l = 0, u = 0;
	if (sepos == std::string::npos) {
		u = (int)str.size () - 1;
	} else {
		l = (int)sepos - (int)str.size () + 1;
		u = (int)sepos - 1;
	}
	return std::pair<int, int> (l, u);
}

bool
TrackerUtils::is_negative (const std::string& str)
{
	if (str.empty ())
		return false;
	return str[0] == '-';
}

bool
TrackerUtils::has_hex_prefix (const std::string& str)
{
	if (is_negative (str)) {
		if (str.size () < 3) {
			return false;
		}
		return (str[1] == '0') && ((str[2] == 'x') || (str[2] == 'X'));
	} else {
		if (str.size () < 2) {
			return false;
		}
		return (str[0] == '0') && ((str[1] == 'x') || (str[1] == 'X'));
	}
}

std::string
TrackerUtils::append_hex_prefix(const std::string& str)
{
	if (has_hex_prefix (str)) {
		return str;
	}
	std::string new_str = str;
	new_str.insert(is_negative (str) ? 1 : 0, "0x");
	return new_str;
}

std::string
TrackerUtils::pad (const std::string& str, int position)
{
	bool is_neg = is_negative(str);
	std::string ng_str =  is_neg ? str.substr (1) : str;
	return std::string (is_neg ? "-" : "") + non_negative_pad (ng_str, position);
}

std::string
TrackerUtils::non_negative_pad (const std::string& str, int position)
{
	std::pair<int, int> pr = position_range (str);
	if (position < pr.first) {
		int diff = pr.first - position;
		return str + (pr.first == 0 ? "." : "") + std::string (diff, '0');
	}
	if (pr.second < position) {
		int diff = position - pr.second;
		return std::string (diff, '0') + str;
	}
	return str;
}

std::string
TrackerUtils::int_unpad (const std::string& str, int base)
{
	return num_to_string (string_to_num<int> (str, base), base);
}

std::string
TrackerUtils::float_unpad (const std::string& str, int base, int precision)
{
	return num_to_string (string_to_num<float> (str, base), base, precision);
}

std::string
TrackerUtils::insert_point (const std::string& str, int position)
{
	std::string res = str;
	if (position <= 0) {
		res.insert(0, std::abs(position), '0');
		res.insert(0, "0.");
	} else if (position < (int)str.size()) {
		res.insert(position, ".");
	}
	return res;
}

size_t
TrackerUtils::locate (const std::string& str, int position)
{
	std::pair<int, int> pr = position_range (str);
	if (pr.first <= position && position <= pr.second) {
		if (0 <= position) {
			return pr.second - position;
		} else {
			return str.size () + pr.first - position - 1;
		}
	}
	return std::string::npos;
}

uint8_t
TrackerUtils::pitch (uint8_t semitones, int octave)
{
	return (uint8_t) octave * 12 + semitones;
}

uint8_t
TrackerUtils::parse_pitch (std::string text, int default_octave)
{
	// Add default octave, if the octave digit is missing
	if (!text.empty () && !std::isdigit (*text.rbegin ())) {
		text += to_string (default_octave);
	}

	// Parse the note per se
	return ARDOUR::ParameterDescriptor::midi_note_num (text);
}

RegionSeq
TrackerUtils::get_sorted_regions (const RegionSelection& region_selection)
{
	RegionSeq regions;
	for (RegionSelection::const_iterator it = region_selection.begin (); it != region_selection.end (); ++it) {
		RegionPtr region = (*it)->region ();
		regions.push_back (region);
	}
	std::sort (regions.begin (), regions.end (), region_position_less ());
	return regions;
}

Temporal::timepos_t
TrackerUtils::get_position (const RegionSeq& regions)
{
	if (regions.empty ())
		return Temporal::timepos_t ();

	size_t i = 0;
	Temporal::timepos_t position = regions[i++]->position ();
	for (; i < regions.size (); i++) {
		position = std::min (position, regions[i]->position ());
	}
	return position;
}

Temporal::timepos_t
TrackerUtils::get_position (const MidiRegionSeq& regions)
{
	if (regions.empty ())
		return Temporal::timepos_t ();

	size_t i = 0;
	Temporal::timepos_t position = regions[i++]->position ();
	for (; i < regions.size (); i++) {
		position = std::min (position, regions[i]->position ());
	}
	return position;
}

Temporal::timepos_t
TrackerUtils::get_position (const RegionSelection& region_selection)
{
	return region_selection.start_time ();
}

Temporal::timepos_t
TrackerUtils::get_position (const TrackRegionsMap& regions_per_track)
{
	if (regions_per_track.empty())
		return Temporal::timepos_t ();

	TrackRegionsMap::const_iterator it = regions_per_track.begin();
	Temporal::timepos_t position = get_position (it->second);
	for (; it != regions_per_track.end(); ++it) {
		position = std::min (position, get_position (it->second));
	}
	return position;
}

Temporal::timecnt_t
TrackerUtils::get_length (const RegionSeq& regions)
{
	return get_position (regions).distance (get_end (regions));
}

Temporal::timecnt_t
TrackerUtils::get_length (const MidiRegionSeq& regions)
{
	return get_position (regions).distance (get_end (regions));
}

Temporal::timecnt_t
TrackerUtils::get_length (const RegionSelection& region_selection)
{
	return get_position (region_selection).distance (get_end (region_selection));
}

Temporal::timecnt_t
TrackerUtils::get_length (const TrackRegionsMap& regions_per_track)
{
	return get_position (regions_per_track).distance (get_end (regions_per_track));
}

Temporal::timepos_t
TrackerUtils::get_end (const MidiRegionSeq& regions)
{
	if (regions.empty())
		return Temporal::timepos_t ();

	size_t i = 0;
	Temporal::timepos_t end = regions[i++]->end ();
	for (; i < regions.size (); i++) {
		end = std::max (end, regions[i]->end ());
	}
	return end;
}

Temporal::timepos_t
TrackerUtils::get_end (const RegionSeq& regions)
{
	if (regions.empty())
		return Temporal::timepos_t ();

	size_t i = 0;
	Temporal::timepos_t end = regions[i++]->end ();
	for (; i < regions.size (); i++) {
		end = std::max (end, regions[i]->end ());
	}
	return end;
}

Temporal::timepos_t
TrackerUtils::get_end (const RegionSelection& region_selection)
{
	if (region_selection.empty())
		return Temporal::timepos_t ();

	RegionSelection::const_iterator it = region_selection.begin ();
	Temporal::timepos_t end = (*it)->region ()->end ();
	++it;
	for (; it != region_selection.end (); ++it) {
		end = std::max (end, (*it)->region ()->end ());
	}
	return end;
}

Temporal::timepos_t
TrackerUtils::get_end (const TrackRegionsMap& regions_per_track)
{
	if (regions_per_track.empty())
		return Temporal::timepos_t ();

	TrackRegionsMap::const_iterator it = regions_per_track.begin();
	Temporal::timepos_t end = get_end (it->second);
	for (; it != regions_per_track.end(); ++it) {
		end = std::max (end, get_end (it->second));
	}
	return end;
}

Temporal::timepos_t
TrackerUtils::get_nt_last (const RegionSeq& regions)
{
	if (regions.empty())
		return Temporal::timepos_t ();

	size_t i = 0;
	Temporal::timepos_t nt_last = regions[i++]->nt_last ();
	for (; i < regions.size (); i++) {
		nt_last = std::max (nt_last, regions[i]->nt_last ());
	}
	return nt_last;
}

Temporal::timepos_t
TrackerUtils::get_nt_last (const MidiRegionSeq& regions)
{
	if (regions.empty())
		return Temporal::timepos_t ();

	size_t i = 0;
	Temporal::timepos_t nt_last = regions[i++]->nt_last ();
	for (; i < regions.size (); i++) {
		nt_last = std::max (nt_last, regions[i]->nt_last ());
	}
	return nt_last;
}

Temporal::timepos_t
TrackerUtils::get_nt_last (const RegionSelection& region_selection)
{
	return region_selection.end_time ().decrement ();
}

Temporal::timepos_t
TrackerUtils::get_nt_last (const TrackRegionsMap& regions_per_track)
{
	if (regions_per_track.empty())
		return Temporal::timepos_t ();

	TrackRegionsMap::const_iterator it = regions_per_track.begin();
	Temporal::timepos_t nt_last = get_nt_last (it->second);
	for (; it != regions_per_track.end(); ++it) {
		nt_last = std::max (nt_last, get_nt_last (it->second));
	}
	return nt_last;
}

bool
TrackerUtils::is_on_equal (NotePtr ln, NotePtr rn)
{
	return ln->time () == rn->time ()
		&& ln->note () == rn->note ()
		&& ln->velocity () == rn->velocity ()
		&& ln->channel () == rn->channel ();
}

bool
TrackerUtils::is_off_equal (NotePtr ln, NotePtr rn)
{
	return ln->end_time () == rn->end_time ();
}

bool
TrackerUtils::off_meets_on (NotePtr off_note, NotePtr on_note)
{
	return off_note->end_time () == on_note->time ();
}

bool
TrackerUtils::is_equal (const Evoral::ControlEvent& lce, const Evoral::ControlEvent& rce)
{
	return lce.when == rce.when && lce.value == rce.value;
}

std::ostream&
TrackerUtils::hex_print_padded (std::ostream& o, const Temporal::BBT_Time& bbt)
{
	o << std::setfill ('0') << std::right << std::uppercase << std::hex
	  << std::setw (3) << bbt.bars << "|"
	  << std::setw (1) << bbt.beats << "|"
	  << std::setw (3) << bbt.ticks;

	return o;
}

std::string
TrackerUtils::bbt_to_string (const Temporal::BBT_Time& bbt, int base)
{
	std::stringstream ss;
	if (base == 16) {
		TrackerUtils::hex_print_padded (ss, bbt);
	} else {
		bbt.print_padded (ss);
	}
	return ss.str ();
}

std::string
TrackerUtils::channel_to_string (uint8_t ch, int base)
{
	return num_to_string (ch + (base == 16 ? 0 : 1), base);
}

uint8_t
TrackerUtils::string_to_channel (std::string str, int base)
{
	return string_to_num<int> (str, base) - (base == 16 ? 0 : 1);
}

std::string
TrackerUtils::underline (const std::string& str)
{
	return std::string("<u>") + str + "</u>";
}

std::string
TrackerUtils::bold (const std::string& str)
{
	return std::string("<b>") + str + "</b>";
}

std::string
TrackerUtils::id_param_to_string (const IDParameter& id_param)
{
	std::stringstream ss;
	ss << "(id=" << id_param.first.to_s () << ", param=" << id_param.second << ")";
	return ss.str ();
}

std::string
TrackerUtils::color_to_string (const Gtkmm2ext::Color& color)
{
	std::stringstream ss;
	ss << std::hex;
	ss << "#" << std::setw (6) << std::setfill ('0') << (color >> 8);
	return ss.str();
}

IDParameter
TrackerUtils::defaultIDParameter ()
{
	return std::make_pair (PBD::ID (0), Evoral::Parameter (0));
}
