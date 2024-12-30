/*
 * Copyright (C) 2018-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include <sstream>

#include "midi_track_pattern_phenomenal_diff.h"

using namespace Tracker;

bool
MidiTrackPatternPhenomenalDiff::empty () const
{
	return !full && mri2mrp_diff.empty () && TrackPatternPhenomenalDiff::empty ();
}

std::string
MidiTrackPatternPhenomenalDiff::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << indent << "mri2mrp_diff:" << std::endl;
	for (const Mri2MidiRegionPatternDiff::value_type& mrimrp_diff : mri2mrp_diff) {
		ss << indent + "  " << "midi_region_pattern_diff[" << mrimrp_diff.first << "]:" << std::endl
		   << mrimrp_diff.second.to_string (indent + "    ") << std::endl;
	}
	ss << indent << "TrackPatternPhenomenalDiff:" << std::endl;
	ss << TrackPatternPhenomenalDiff::to_string (indent + "  ");
	return ss.str ();
}
