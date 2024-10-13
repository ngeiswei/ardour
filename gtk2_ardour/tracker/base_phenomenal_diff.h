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

#ifndef __ardour_tracker_base_phenomenal_diff_h_
#define __ardour_tracker_base_phenomenal_diff_h_

#include <string>

namespace Tracker {

/**
 * Base class for storing the phenomenal difference between 2 patterns.
 *
 * The phenomenal difference is the visible difference once a pattern is
 * rendered on the grid. This class allows to only modify the cells that may
 * show a difference, thus avoid an exhausitve rendering of all cells of the
 * grid, which is too slow when the grid is large.
 */

class BasePhenomenalDiff {
public:
	BasePhenomenalDiff();

	bool full;
	bool empty() const;
	std::string to_string(const std::string& indent = std::string()) const;
};

} // ~namespace tracker

#endif /* __ardour_tracker_base_pattern_h_ */
