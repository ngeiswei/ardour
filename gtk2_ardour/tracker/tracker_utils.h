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

#ifndef __ardour_tracker_tracker_utils_h_
#define __ardour_tracker_tracker_utils_h_

#include <cmath>
#include <cstdlib>
#include <regex>
#include <set>
#include <string>

#include <boost/lexical_cast.hpp>

#include "pbd/id.h"
#include "ardour/midi_track.h"
#include "ardour/audio_track.h"
#include "ardour/midi_region.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/types.h"
#include "evoral/Note.h"
#include "evoral/Parameter.h"
#include "temporal/bbt_time.h"

#include "region_selection.h"

namespace Tracker {

typedef std::shared_ptr<ARDOUR::Track> TrackPtr;
typedef std::shared_ptr<ARDOUR::MidiTrack> MidiTrackPtr;
typedef std::shared_ptr<ARDOUR::AudioTrack> AudioTrackPtr;
typedef std::shared_ptr<ARDOUR::Region> RegionPtr;
typedef std::shared_ptr<ARDOUR::MidiRegion> MidiRegionPtr;
typedef std::shared_ptr<ARDOUR::MidiModel> MidiModelPtr;
typedef Evoral::Note<Temporal::Beats> NoteType;
typedef std::shared_ptr<NoteType> NotePtr;
typedef std::shared_ptr<ARDOUR::AutomationControl> AutomationControlPtr;
typedef std::shared_ptr<ARDOUR::AutomationList> AutomationListPtr;
typedef std::shared_ptr<ARDOUR::Processor> ProcessorPtr;

typedef std::vector<RegionPtr> RegionSeq;
typedef std::vector<MidiRegionPtr> MidiRegionSeq;
typedef std::map<TrackPtr, RegionSeq, ARDOUR::Stripable::Sorter> TrackRegionsMap;
typedef std::map<Evoral::Parameter, AutomationControlPtr> ParamAutomationControlMap;
typedef std::pair<Evoral::Parameter, AutomationControlPtr> ParamAutomationControlPair;

// Allows to identify a parameter of a processor of a certain ID
typedef std::pair<PBD::ID, Evoral::Parameter> IDParameter;
typedef std::set<IDParameter> IDParameterSet;

typedef ARDOUR::AutomationList::iterator AutomationListIt;
typedef std::multimap<int, AutomationListIt> RowToAutomationListIt;
typedef std::pair<RowToAutomationListIt::const_iterator, RowToAutomationListIt::const_iterator> RowToAutomationListItRange;

typedef std::multimap<int, Evoral::ControlEvent> RowToControlEvents;
typedef std::pair<RowToControlEvents::const_iterator, RowToControlEvents::const_iterator> RowToControlEventsRange;

typedef std::multimap<int, NotePtr> RowToNotes;
typedef std::pair<RowToNotes::const_iterator, RowToNotes::const_iterator> RowToNotesRange;

typedef std::set<Evoral::Parameter> ParameterSet;
typedef ParameterSet::iterator ParameterSetIt;
typedef ParameterSet::const_iterator ParameterSetConstIt;

/**
 * Less than operator for regions, order according to position, earlier comes first
 */
struct region_position_less
{
	bool operator () (RegionPtr lhs, RegionPtr rhs);
};

/**
* Like Sequence::EarlierNoteComparator, but in case the two notes have the
* same time, then other attributes are used to determine their order, so
* that the ordering relationship is strict rather than partial. This is
* currently used by the Midi Pattern Editor.
*
* Only channel and pitch attributes are used. For now it is assumed (perhaps
* wrongly) that simultaneous notes cannot exist on the same midi region if
* these 2 attributes are equal.
*/
struct EarlierNoteStrictComparator {
	inline bool operator()(const std::shared_ptr<const NoteType> a,
	                       const std::shared_ptr<const NoteType> b) const {
		return a->time() < b->time()
			|| (a->time() == b->time()
			    && (a->channel() < b->channel()
			        || (a->channel() == b->channel()
			            && a->note() < b->note())));
	}
};

/**
 * Strictly ordered multiset of notes
 */
typedef std::multiset<NotePtr, EarlierNoteStrictComparator> StrictNotes;

class TrackerUtils
{
public:
	/**
	 * Return true iff param pertains to pan control
	 */
	static bool is_pan_type (const Evoral::Parameter& param);
	static std::set<ARDOUR::AutomationType> get_pan_param_types ();

	/**
	 * Return true iff the given param is for region automation (as opposed to
	 * track automation).
	 */
	static bool is_region_automation (const Evoral::Parameter& param);

	/**
	 * Test if element is in container
	 */
	template<typename C>
	static bool is_in (const typename C::value_type& element, const C& container)
	{
		return container.find (element) != container.end ();
	}

	/**
	 * Test if key is in map
	 */
	template<typename M>
	static bool is_key_in (const typename M::key_type& key, const M& map)
	{
		return map.find (key) != map.end ();
	}

	/**
	 * Clamp x to be within [l, u], that is return max (l, min (u, x))
	 *
	 * TODO: can probably use automation_controller.cc clamp function
	 */
	template<typename Num>
	static Num clamp (Num x, Num l, Num u)
	{
		return std::max (l, std::min (u, x));
	}

	/**
	 * Give a string representing a floating point number, remove all zeros
	 * after a point, and trailing point if any. For instance
	 *
	 * "1.0000"
	 *
	 * returns
	 *
	 * "1"
	 */
	static std::string rm_point_zeros (const std::string& str);

	/**
	 * Convert number to string without scientific notation.
	 *
	 * TODO: for int, precision is not necessary.
	 */
	static std::string num_to_string (int n, int base=10, int precision=2);
	static std::string num_to_string (double n, int base=10, int precision=2);

	/**
	 * Convert tring to number.
	 */
	template<typename Num>
	static Num string_to_num (const std::string& str, int base=10)
	{
		const std::string sign = "(\\+|-)?";
		const std::string digit = base == 16 ? "[0-9a-fA-F]" : "[0-9]";
		const std::string digits = "(" + digit + "*)";
		const std::string point = "\\.";
		const std::string number = sign + digits + "(\\." + digits + ")?";
		const std::regex regex(number);
		std::smatch match;
		if (std::regex_search(str, match, regex)) {
			const std::string msign = match[1];
			const std::string minteger = match[2];
			const std::string mfractional = match[4];
			int sign = msign == "-" ? -1 : 1;
			int integer = minteger.empty () ? 0 : std::stoi (minteger, nullptr, base);
			int fractional = mfractional.empty () ? 0 : std::stoi (mfractional, nullptr, base);
			return (Num)(sign * (integer + fractional / (double)std::pow(base, mfractional.size())));
		} else {
			return 0;
		}
	}

	static char digit_to_char (int digit, int base=10);
	static int char_to_digit (char c, int base=10);

	/**
	 * Given a string representing a float, return the position of the point (a
	 * dot in US local). If no such point exist then return the string size,
	 * assuming that the point is implicitly after the last character.
	 */
	static size_t point_position (const std::string& str);

	/**
	 * Given a string representing a number, find a pair l, u representing the
	 * position range [l, u] of that number. For instance
	 *
	 * position_range ("123.0") = <-1, 2>
	 * position_range ("123") = <0, 2>
	 * position_range ("0123") = <0, 3>
	 * position_range ("-0123") = <0, 3>
	 */
	static std::pair<int, int> position_range (const std::string& str);

	/**
	 * Return true iff the string encodes a number in the given base.
	 */
	template <typename Num>
	static bool is_number (const std::string& str, int base=10)
	{
		const std::string sign = "(\\+|-)?";
		const std::string digit = base == 16 ? "[0-9a-fA-F]" : "[0-9]";
		const std::string digits = "(" + digit + "*)";
		const std::string point = "\\.";
		const std::string number = sign + digits + "(\\." + digits + ")?";
		const std::regex regex(number);
		return std::regex_match(str, regex);
	}

	/**
	 * Return true iff the number encoded in str is negative. That is if it
	 * starts with '-'.
	 */
	static bool is_negative (const std::string& str);

	/**
	 * Return true if the number has an hexadecimal prefix. For instance
	 *
	 * "A" -> false
	 * "-A" -> false
	 * "0xA" -> true
	 * "-0xA" -> true
	 */
	static bool has_hex_prefix (const std::string& str);

	/**
	 * Append hex prefix to a hexadecimal number if missing. For instance
	 *
	 * "A" -> "0xA"
	 * "-A" -> "-0xA"
	 * "0xA" -> "0xA"
	 * "-0xA" -> "-0xA"
	 */
	static std::string append_hex_prefix(const std::string& str);

	/**
	 * Given the string corresponding to a number and a position in the
	 * numerical system, pad the string so that it contains extra zeros so that
	 * the string covers the position. For instance
	 *
	 * pad ("123", -1) = "123.0"
	 * pad ("123", 0) = "123"
	 * pad ("123", 3) = "0123"
	 * pad ("-123, 3) = "-0123"
	 */
	static std::string pad (const std::string& str, int position);

	/**
	 * Like pad but only supports non negative numbers
	 */
	static std::string non_negative_pad (const std::string& str, int position);

	/**
	 * Performs the inverse operation of pad, that is remove the unnecessary
	 * leading (and perhaps trailing as well) zeros.
	 */
	static std::string int_unpad (const std::string& str, int base=10);
	static std::string float_unpad (const std::string& str, int base=10, int precision=2);

	/**
	 * Given a string corresponding to an integer, insert a point '.' before the
	 * given position, padding with zeros when necessary. If position is the
	 * size of the string or more, then do nothing. For instance
	 *
	 * insert_point ("123", -1) = 0.0123
	 * insert_point ("123", 0) = 0.123
	 * insert_point ("123", 1) = 1.23
	 * insert_point ("123", 2) = 12.3
	 * insert_point ("123", 3) = 123
	 */
	static std::string insert_point (const std::string& str, int position);

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
	static Num change_digit_or_sign (Num val, int digit, int position, int base=10, int precision=2)
	{
		if (0 <= digit && digit < base) {
			return change_digit (val, digit, position, base, precision);
		}

		if ((digit < 0 && 0 < val) || (base <= digit && val < 0)) {
			return -val;
		}

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
	static Num change_digit (Num val, int digit, int position, int base=10, int precision=2)
	{
		std::string val_str = change_digit (num_to_string (val, base, precision), digit, position, base);
		return string_to_num<Num> (val_str, base);
	}
	static std::string change_digit (const std::string& val_str, int digit, int position, int base=10)
	{
		std::string padded_val_str = pad (val_str, position);
		size_t str_pos = locate (padded_val_str, position);
		padded_val_str[str_pos] = digit_to_char (digit, base);
		return padded_val_str;
	}

	// Make it up for the lack of C++11 support
	template<typename T> static std::string to_string (const T& v)
	{
		std::stringstream ss;
		ss << v;
		return ss.str ();
	}

	// Calculate the midi note pitch given the octave and the number of
	// semitones within this octave
	static uint8_t pitch (uint8_t semitones, int octave);

	// Get the pitch of a note given its textual description. If the octave
	// number is missing then the default one is used.
	static uint8_t parse_pitch (std::string text, int default_octave);

	// Return a sequence of regions sorted by position
	static RegionSeq get_sorted_regions (const RegionSelection& region_selection);

	// Given a list of chronologically ordered, non overlapping regions, return
	// the position of the earliest one.  If empty then return 0.
	static Temporal::timepos_t get_position (const RegionSeq& regions);
	static Temporal::timepos_t get_position (const MidiRegionSeq& regions);
	static Temporal::timepos_t get_position (const RegionSelection& region_selection);
	static Temporal::timepos_t get_position (const TrackRegionsMap& regions_per_track);

	// Given a list of chronologically ordered, non overlapping regions, return
	// the length between the very first position of the position very last + 1.
	// If empty then return 0.
	static Temporal::timecnt_t get_length (const RegionSeq& regions);
	static Temporal::timecnt_t get_length (const MidiRegionSeq& regions);
	static Temporal::timecnt_t get_length (const RegionSelection& region_selection);
	static Temporal::timecnt_t get_length (const TrackRegionsMap& regions_per_track);

	// Given a list of chronologically ordered, non overlapping regions, return
	// the very first sample (it looks identical to get_regions_position).  If
	// empty then return 0.
	static Temporal::timepos_t get_end (const RegionSeq& regions);
	static Temporal::timepos_t get_end (const MidiRegionSeq& regions);
	static Temporal::timepos_t get_end (const RegionSelection& region_selection);
	static Temporal::timepos_t get_end (const TrackRegionsMap& regions_per_track);

	// Given a list of chronologically ordered, non overlapping regions, return
	// the position of the last sample.  If empty then return 0.
	static Temporal::timepos_t get_nt_last (const RegionSeq& regions);
	static Temporal::timepos_t get_nt_last (const MidiRegionSeq& regions);
	static Temporal::timepos_t get_nt_last (const RegionSelection& region_selection);
	static Temporal::timepos_t get_nt_last (const TrackRegionsMap& regions_per_track);

	// Compare if two notes have the same on note attributes
	static bool is_on_equal (NotePtr ln, NotePtr rn);

	// Compare if two notes have the same off note attributes
	static bool is_off_equal (NotePtr ln, NotePtr rn);

	// Return true iff off_note ends exactly where on_note begings
	static bool off_meets_on (NotePtr off_note, NotePtr on_note);

	// Compare if two control events have the same attributes
	static bool is_equal (const Evoral::ControlEvent& lce, const Evoral::ControlEvent& rce);

	// Like print_padded in bbt_time.h but for hexadecimal
	static std::ostream& hex_print_padded (std::ostream& o, const Temporal::BBT_Time& bbt);

	// Render a BBT, padding if necessary taking into account whether it is
	// rendered in decimal or hexadecimal.
	static std::string bbt_to_string (const Temporal::BBT_Time& bbt, int base=10);

	// Convert channel to string, taking into account the base, starting from 0
	// if hex, and from 1 otherwise.
	static std::string channel_to_string (uint8_t ch, int base=10);

	// Convert channel string representation to channel value, taking into
	// account the base, starting from 0 if hex, and from 1 otherwise.
	static uint8_t string_to_channel (std::string str, int base=10);

	// Wrap <u> </u> around the givem string
	static std::string underline (const std::string& str);

	// Wrap <b> </b> around the givem string
	static std::string bold (const std::string& str);

	static std::string id_param_to_string (const IDParameter& id_param);

	// Convert Gtkmm2ext::Color to its string representation
	static std::string color_to_string (const Gtkmm2ext::Color& color);

	// Return initial IDParameter object with no useful information in it
	static IDParameter defaultIDParameter ();
};

} // ~namespace tracker

#endif /* __ardour_tracker_tracker_utils_h_ */
