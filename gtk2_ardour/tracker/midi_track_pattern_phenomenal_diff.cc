/*
    Copyright (C) 2018 Nil Geisweiller

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

#include <sstream>

#include "midi_track_pattern_phenomenal_diff.h"

using namespace Tracker;

bool
MidiTrackPatternPhenomenalDiff::empty() const
{
	return !full && mri2mrp_diff.empty() && auto_diff.empty();
}

std::string
MidiTrackPatternPhenomenalDiff::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << indent << "mri2mrp_diff:" << std::endl;
	for (Mri2MidiRegionPatternDiff::const_iterator it = mri2mrp_diff.begin(); it != mri2mrp_diff.end(); ++it) {
		ss << indent + "  " << "midi_region_pattern_diff[" << it->first << "]:" << std::endl
		   << it->second.to_string(indent + "    ") << std::endl;
	}
	ss << indent << "auto_diff:" << std::endl;
	ss << auto_diff.to_string(indent + "  ");
	return ss.str();
}