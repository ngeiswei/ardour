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

#include "midi_notes_pattern_phenomenal_diff.h"

using namespace Tracker;

bool
MidiNotesPatternPhenomenalDiff::empty () const
{
	return !full && cgi2rows_diff.empty ();
}

std::string
MidiNotesPatternPhenomenalDiff::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePatternPhenomenalDiff::to_string (indent) << std::endl
		<< indent << "cgi2rows_diff:" << std::endl
	   << indent << "size = " << cgi2rows_diff.size ();
	for (Cgi2RowsPhenomenalDiff::const_iterator it = cgi2rows_diff.begin (); it != cgi2rows_diff.end (); ++it) {
		ss << std::endl << indent << "  " << " (cgi=" << it->first << ", rows=";
		const RowsPhenomenalDiff& rows_diff = it->second;
		const std::set<int>& rows = rows_diff.rows;
		for (std::set<int>::const_iterator row_it = rows.begin (); row_it != rows.end (); ++row_it) {
			if (row_it != rows.begin ()) {
				ss << ",";
			}
			ss << *row_it;
		}
		ss << ")";
	}
	return ss.str ();
}
