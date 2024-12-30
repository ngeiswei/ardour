/*
 * Copyright (C) 2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include "pattern_phenomenal_diff.h"

using namespace Tracker;

bool
PatternPhenomenalDiff::empty () const
{
	return !full && mti2tp_diff.empty ();
}

std::string
PatternPhenomenalDiff::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePatternPhenomenalDiff::to_string (indent) << std::endl;
	ss << indent << "mti2tp_diff:";
	for (Mti2TrackPatternDiff::const_iterator it = mti2tp_diff.begin (); it != mti2tp_diff.end (); ++it) {
		int mti = it->first;
		TrackPatternPhenomenalDiff* tp_diff = it->second;
		ss << std::endl << indent + "  " << "track_pattern_diff[" << mti << "]:" << std::endl;
		ss << tp_diff->to_string (indent + "    ");
	}
	return ss.str ();
}
