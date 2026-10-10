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

#include "midi_region_pattern_phenomenal_diff.h"

using namespace Tracker;

bool
MidiRegionPatternPhenomenalDiff::empty () const
{
	return !full && mnp_diff.empty () && mrap_diff.empty ();
}

std::string
MidiRegionPatternPhenomenalDiff::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePatternPhenomenalDiff::to_string (indent) << std::endl
	   << indent << "mnp_diff:" << std::endl
	   << mnp_diff.to_string (indent + "  ") << std::endl
	   << indent << "mrap_diff:" << std::endl
	   << mrap_diff.to_string (indent + "  ");
	return ss.str ();
}
