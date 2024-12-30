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

#ifndef __ardour_tracker_rows_phenomenal_diff_h_
#define __ardour_tracker_rows_phenomenal_diff_h_

#include <set>

#include "base_pattern_phenomenal_diff.h"

namespace Tracker {

struct RowsPhenomenalDiff : public BasePatternPhenomenalDiff
{
	std::set<int> rows;

	bool empty () const;
	std::string to_string (const std::string& indent = std::string ()) const;
};

}

#endif /* __ardour_tracker_rows_phenomenal_diff_h_ */
