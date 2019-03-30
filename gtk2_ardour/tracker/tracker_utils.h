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

#ifndef __ardour_tracker_tracker_utils_h_
#define __ardour_tracker_tracker_utils_h_

#include <set>
#include <cmath>
#include <boost/lexical_cast.hpp>

#include "evoral/Parameter.hpp"
#include "evoral/Note.hpp"
#include "ardour/types.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/midi_region.h"

#include "region_selection.h"

namespace Tracker {

typedef Evoral::Note<Temporal::Beats> NoteType;
typedef boost::shared_ptr<NoteType> NoteTypePtr;

/**
 * Less than operator for regions, order according to position, earlier comes first
 */
struct region_position_less
{
	bool operator()(boost::shared_ptr<ARDOUR::Region> lhs, boost::shared_ptr<ARDOUR::Region> rhs);
};

class TrackerUtils
{
public:
	/**
	 * Return true iff param pertains to pan control
	 */
	static bool is_pan_type (const Evoral::Parameter& param);
	static std::set<ARDOUR::AutomationType> get_pan_param_types();

	/**
	 * Return true iff the given param is for region automation (as opposed to
	 * track automation).
	 */
	static bool is_region_automation (const Evoral::Parameter& param);

	/**
	 * Test if element is in container
	 */
	template<typename C>
	static bool is_in(const typename C::value_type& element, const C& container)
	{
		return container.find(element) != container.end();
	}

	/**
	 * Test if key is in map
	 */
	template<typename M>
	static bool is_key_in(const typename M::key_type& key, const M& map)
	{
		return map.find(key) != map.end();
	}

	/**
	 * Clamp x to be within [l, u], that is return max(l, min(u, x))
	 *
	 * TODO: can probably use automation_controller.cc clamp function
	 */
	template<typename Num>
	static Num clamp(Num x, Num l, Num u)
	{
		return std::max(l, std::min(u, x));
	}

	template<typename Num>
	static std::string num_to_string(Num n, int base=10)
	{
		std::stringstream ss;
		ss << n;
		return ss.str();
	}

	template<typename Num>
	static Num string_to_num(const std::string& str, int base=10)
	{
		return boost::lexical_cast<Num>(str);
	}

	static char digit_to_char(int digit, int base=10);
	static int char_to_digit(char c, int base=10);

	/**
	 * Given a string representing a number, find a pair l, u representing the
	 * position range [l, u] of that number. For instance
	 *
	 * position_range ("123.0") = <-1, 2>
	 * position_range ("123") = <0, 2>
	 * position_range ("0123") = <0, 3>
	 */
	static std::pair<int, int> position_range (const std::string& str);

	/**
	 * Given the string corresponding to a number and a position in the
	 * numerical system, pad the string so that it contains extra zeros so that
	 * the string covers the position. For instance
	 *
	 * pad ("123", -1) = "123.0"
	 * pad ("123", 0) = "123"
	 * pad ("123", 3) = "0123"
	 */
	static std::string pad (const std::string& str, int position);

	/**
	 * Given the string corresponding to a number and a position in the
	 * numerical system, return the location of the character in the string
	 * corresponding to the given position. Return npos if undefined. For
	 * instance
	 *
	 * locate ("123.15", -1) = 4
	 * locate ("123.1", -1) = 4
	 * locate ("123", 0) = 2
	 * locate ("0123", 3) = 0
	 * locate ("0123", 4) = npos
	 */
	static size_t locate (const std::string& str, int position);

	/**
	 * Like change_digit but also changes the sign, if digit is negative then
	 * return a negative val, if base <= digit then return a positive val.
	 */
	template <typename Num>
	static Num change_digit_or_sign (Num val, int digit, int position, int base=10)
	{
		if (0 <= digit && digit < base)
			return change_digit (val, digit, position);

		if ((digit < 0 && 0 < val) || (base <= digit && val < 0))
			return -val;

		return val;;
	}

	/**
	 * Given a value, a digit and position, replace the digit of that value at
	 * the given position by the given digit. For instance
	 *
	 * change_digit (val=100, digit=5, position=-2) = 100.05
	 * change_digit (val=100, digit=5, position=-1) = 100.5
	 * change_digit (val=100, digit=5, position=0) = 105
	 * change_digit (val=100, digit=5, position=1) = 150
	 * change_digit (val=100, digit=5, position=2) = 500
	 * change_digit (val=100, digit=5, position=3) = 5100
	 */
	template <typename Num>
	static Num change_digit (Num val, int digit, int position, int base=10)
	{
		std::string val_str = change_digit (num_to_string (val, base), digit, position, base);
		return string_to_num<Num> (val_str, base);
	}
	static std::string change_digit (const std::string& val_str, int digit, int position, int base=10)
	{
		std::string padded_val_str = pad (val_str, position);
		size_t str_pos = locate (padded_val_str, position);
		padded_val_str[str_pos] = digit_to_char(digit);
		return padded_val_str;
	}

	// Make it up for the lack of C++11 support
	template<typename T> static std::string to_string(const T& v)
	{
		std::stringstream ss;
		ss << v;
		return ss.str();
	}

	// Calculate the midi note pitch given the octave and the number of
	// semitones within this octave
	static uint8_t pitch (uint8_t semitones, int octave);

	// Get the pitch of a note given its textual description. If the octave
	// number is missing then the default one is used.
	static uint8_t parse_pitch (std::string text, int default_octave);

	// Return a sequence of regions sorted by position
	static std::vector<boost::shared_ptr<ARDOUR::Region> > get_sorted_regions(const RegionSelection& region_selection);

	// Given a list of chronologically ordered, non overlapping regions, return
	// the position of the earliest one
	static Temporal::samplepos_t get_position(const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions);
	static Temporal::samplepos_t get_position(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions);
	static Temporal::samplepos_t get_position(const RegionSelection& region_selection);

	// Given a list of chronologically ordered, non overlapping regions, return
	// the length between the very first position of the position very last + 1
	static Temporal::samplecnt_t get_length(const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions);
	static Temporal::samplecnt_t get_length(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions);
	static Temporal::samplepos_t get_length(const RegionSelection& region_selection);

	// Given a list of chronologically ordered, non overlapping regions, return
	// the very first sample (it looks identical to get_regions_position
	static Temporal::samplepos_t get_first_sample(const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions);
	static Temporal::samplepos_t get_first_sample(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions);
	static Temporal::samplepos_t get_first_sample(const RegionSelection& region_selection);

	// Given a list of chronologically ordered, non overlapping regions, return
	// the position of the last sample.
	static Temporal::samplepos_t get_last_sample(const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions);
	static Temporal::samplepos_t get_last_sample(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions);
	static Temporal::samplepos_t get_last_sample(const RegionSelection& region_selection);

	// Compare if two notes have the same on note attributes
	static bool is_on_equal(NoteTypePtr ln, NoteTypePtr rn);

	// Compare if two notes have the same off note attributes
	static bool is_off_equal(NoteTypePtr ln, NoteTypePtr rn);

	// Compare if two control events have the same attributes
	static bool is_equal(const Evoral::ControlEvent& lce, const Evoral::ControlEvent& rce);
};

} // ~namespace tracker

#endif /* __ardour_tracker_tracker_utils_h_ */
