/*
    Copyright (C) 2019 Nil Geisweiller

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

#include "note_pattern_phenomenal_diff.h"

#include <sstream>

using namespace Tracker;

bool
NoteColPhenomenalDiff::empty() const
{
	return !full && rows.empty();
}

std::string
NoteColPhenomenalDiff::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePatternPhenomenalDiff::to_string(indent) << std::endl
		<< indent << "rows:" << std::endl
	   << indent << "size = " << rows.size()
	   << std::endl << indent << "  rows=";
	for (std::set<size_t>::const_iterator it = rows.begin(); it != rows.end(); it++) {
		if (it != rows.begin())
			ss << ",";
		ss << *it;
	}
	return ss.str();
}

bool
NotePatternPhenomenalDiff::empty() const
{
	return !full && cgi2nc_diff.empty();
}

std::string
NotePatternPhenomenalDiff::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePatternPhenomenalDiff::to_string(indent) << std::endl
		<< indent << "cgi2nc_diff:" << std::endl
	   << indent << "size = " << cgi2nc_diff.size();
	for (Cgi2NoteColPhenomenalDiff::const_iterator it = cgi2nc_diff.begin(); it != cgi2nc_diff.end(); it++) {
		ss << std::endl << indent << "  " << "(cgi=" << it->first << ", rows=";
		const NoteColPhenomenalDiff& nc_diff = it->second;
		const std::set<size_t>& rows = nc_diff.rows;
		for (std::set<size_t>::const_iterator row_it = rows.begin(); row_it != rows.end(); row_it++) {
			if (row_it != rows.begin())
				ss << ",";
			ss << *row_it;
		}
		ss << ")";
	}
	return ss.str();
}
