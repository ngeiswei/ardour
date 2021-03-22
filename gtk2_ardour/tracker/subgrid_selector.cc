/*
 * Copyright (C) 2020 Nil Geisweiller <ngeiswei@gmail.com>
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

#include "subgrid_selector.h"

#include <iostream>
#include <sstream>
#include <algorithm>

#include "tracker_editor.h"

using namespace Tracker;

SubgridSelector::SubgridSelector (TrackerEditor& te)
	: tracker_editor (te)
	, src_row_idx (-1)
	, src_col_idx (-1)
	, dst_row_idx (-1)
	, dst_col_idx (-1)
	, top_row_idx (-1)
	, bottom_row_idx (-1)
	, left_col_idx (-1)
	, right_col_idx (-1)
	, prev_top_row_idx (-1)
	, prev_bottom_row_idx (-1)
	, prev_left_col_idx (-1)
	, prev_right_col_idx (-1)
{
}

void
SubgridSelector::set_source (int row_idx, int col_idx)
{
	src_row_idx = row_idx;
	src_col_idx = col_idx;
	dst_row_idx = -1;
	dst_col_idx = -1;

	set_rectangle ();

	std::cout << "SubgridSelector::set_source (row_idx=" << row_idx << ", col_idx=" << col_idx << "):" << std::endl;
	std::cout << to_string ("  ") << std::endl;
}

void
SubgridSelector::set_destination (int row_idx, int col_idx)
{
	if (!has_source ())
		set_source (row_idx, col_idx);

	dst_row_idx = row_idx;
	dst_col_idx = col_idx;

	set_rectangle ();

	std::cout << "SubgridSelector::destination (row_idx=" << row_idx << ", col_idx=" << col_idx << "):" << std::endl;
	std::cout << to_string ("  ") << std::endl;
}

void
SubgridSelector::unset ()
{
	src_row_idx = -1;
	src_col_idx = -1;
	dst_row_idx = -1;
	dst_row_idx = -1;

	set_rectangle ();

	std::cout << "SubgridSelector::unset ():" << std::endl;
	std::cout << to_string ("  ") << std::endl;
}

bool
SubgridSelector::has_source () const
{
	return 0 <= src_row_idx and 0 <= src_col_idx;
}

bool
SubgridSelector::has_destination () const
{
	std::cout << "SubgridSelector::has_destination () = " << (0 <= dst_row_idx and 0 <= dst_col_idx) << std::endl;
	return 0 <= dst_row_idx and 0 <= dst_col_idx;
}

void
SubgridSelector::cut ()
{
	copy ();
	clear_subgrid ();
}

void
SubgridSelector::clear_subgrid ()
{
	// NEXT: Remove all selected events from the grid
}

void
SubgridSelector::copy ()
{
	if (not has_selection ())
		return;

	// Clear register
	reg.clear ();

	// Fill register
	unsigned rgtr_col_idx = 0;
	unsigned rgtr_row_idx = 0;
	for (int col_idx = left_col_idx; col_idx <= right_col_idx; col_idx++) {
		// Column does not hold data, skip it
		if (!tracker_editor.grid.is_editable (col_idx)) {
			continue;
		}
		// Fill register column
		std::map<unsigned, std::string> rgtr_col;
		for (int row_idx = top_row_idx; row_idx <= bottom_row_idx; row_idx++) {
			// Cell does not hold data, skip it
			if (!tracker_editor.grid.is_cell_defined (row_idx, col_idx)) {
				continue;
			}
			std::string content = tracker_editor.grid.get_cell_content (row_idx, col_idx);
			if (content.empty ()) {
				continue;
			}
			rgtr_col[rgtr_row_idx] = content;
			rgtr_row_idx++;
		}
		reg[rgtr_col_idx] = rgtr_col;
		rgtr_col_idx++;
	}
}

void
SubgridSelector::paste ()
{
	// NEXT
}

void
SubgridSelector::paste_overlay ()
{
	// NEXT: like paste but does not delete the existing events when
	// possible
}

void
SubgridSelector::set_rectangle ()
{
	// Save previous rectangle
	prev_top_row_idx = top_row_idx;
	prev_bottom_row_idx = bottom_row_idx;
	prev_left_col_idx = left_col_idx;
	prev_right_col_idx = right_col_idx;

	// No source or destination set
	if (not has_source ()) {
		top_row_idx = -1;
		bottom_row_idx = -1;
		left_col_idx = -1;
		right_col_idx = -1;
		return;
	}

	// If there is a source but no destination then the rectangle is
	// just the source cell.
	if (not has_destination ()) {
		top_row_idx = src_row_idx;
		bottom_row_idx = src_row_idx;
		left_col_idx = src_col_idx;
		right_col_idx = src_col_idx;
		return;
	}

	// If there is both a source and destination then define the
	// rectangle.
	top_row_idx = std::min (src_row_idx, dst_row_idx);
	bottom_row_idx = std::max (src_row_idx, dst_row_idx);
	left_col_idx = std::min (src_col_idx, dst_col_idx);
	right_col_idx = std::max (src_col_idx, dst_col_idx);
}

bool
SubgridSelector::has_selection () const
{
	std::cout << "SubgridSelector::has_selection () = " << (0 <= top_row_idx) << std::endl;
	return 0 <= top_row_idx;
}

bool
SubgridSelector::has_prev_selection () const
{
	std::cout << "SubgridSelector::has_prev_selection () = " << (0 <= prev_top_row_idx) << std::endl;
	return 0 <= prev_top_row_idx;
}

std::string
SubgridSelector::to_string (std::string indent) const
{
	std::stringstream ss;
	ss << indent << "src_row_idx = " << src_row_idx << std::endl;
	ss << indent << "src_col_idx = " << src_col_idx << std::endl;
	ss << indent << "dst_row_idx = " << dst_row_idx << std::endl;
	ss << indent << "dst_col_idx = " << dst_col_idx << std::endl;
	ss << indent << "top_row_idx = " << top_row_idx << std::endl;
	ss << indent << "bottom_row_idx = " << bottom_row_idx << std::endl;
	ss << indent << "left_col_idx = " << left_col_idx << std::endl;
	ss << indent << "right_col_idx = " << right_col_idx << std::endl;
	return ss.str ();
}
