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

void Register::clear ()
{
	width = 0;
	height = 0;
	data.clear ();
}

bool Register::empty () const
{
	return width == 0;
}

SubgridSelector::SubgridSelector (TrackerEditor& te)
	: tracker_editor (te)
	, src_row_idx (BasePattern::INVALID_ROW)
	, src_col_idx (BasePattern::INVALID_COL)
	, dst_row_idx (BasePattern::INVALID_ROW)
	, dst_col_idx (BasePattern::INVALID_COL)
	, top_row_idx (BasePattern::INVALID_ROW)
	, bottom_row_idx (BasePattern::INVALID_ROW)
	, left_col_idx (BasePattern::INVALID_COL)
	, right_col_idx (BasePattern::INVALID_COL)
	, prev_top_row_idx (BasePattern::INVALID_ROW)
	, prev_bottom_row_idx (BasePattern::INVALID_ROW)
	, prev_left_col_idx (BasePattern::INVALID_COL)
	, prev_right_col_idx (BasePattern::INVALID_COL)
{
}

void
SubgridSelector::set_source (int row_idx, int col_idx)
{
	src_row_idx = row_idx;
	src_col_idx = col_idx;
	dst_row_idx = BasePattern::INVALID_ROW;
	dst_col_idx = BasePattern::INVALID_COL;

	set_rectangle ();
}

void
SubgridSelector::set_destination (int row_idx, int col_idx)
{
	if (!has_source ())
		set_source (row_idx, col_idx);

	dst_row_idx = row_idx;
	dst_col_idx = col_idx;

	set_rectangle ();
}

void
SubgridSelector::unset ()
{
	src_row_idx = BasePattern::INVALID_ROW;
	src_col_idx = BasePattern::INVALID_COL;
	dst_row_idx = BasePattern::INVALID_ROW;
	dst_col_idx = BasePattern::INVALID_COL;

	set_rectangle ();
}

bool
SubgridSelector::has_source () const
{
	return 0 <= src_row_idx && 0 <= src_col_idx;
}

bool
SubgridSelector::has_destination () const
{
	return 0 <= dst_row_idx && 0 <= dst_col_idx;
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
	if (reg.empty ())
		return;

	// Maximum number of columns and rows in the pattern
	int global_ncols = tracker_editor.grid.get_columns ().size ();
	int global_nrows = tracker_editor.grid.pattern.global_nrows;
	int top_row_idx = this->top_row_idx; // Mask attribute in case it changes
	int bottom_row_idx = std::min (global_nrows, (int)reg.height + top_row_idx) - 1;

	// Loop over columns, skipping uneditable or invisible ones
	int col_idx = left_col_idx;
	for (unsigned rgtr_col_idx = 0; rgtr_col_idx < reg.width; rgtr_col_idx++, col_idx++) {
		// Column does not hold data or is not visible, skip it
		while (!tracker_editor.grid.is_editable (col_idx) or
				 !tracker_editor.grid.is_visible (col_idx)) {
			col_idx++;
			if (global_ncols <= col_idx) break; // Avoid horizontal overflow
		}
		if (global_ncols <= col_idx) break; // Avoid horizontal overflow

		// Paste empty cells
		for (int row_idx = top_row_idx; row_idx <= bottom_row_idx; row_idx++) {
			tracker_editor.grid.set_cell_content(row_idx, col_idx, "");
		}
	}
}

void
SubgridSelector::copy ()
{
	if (not has_selection ())
		return;

	// Clear register
	reg.clear ();

	// Set height
	int top_row_idx = this->top_row_idx; // Mask attribute in case it changes
	reg.height = (bottom_row_idx - top_row_idx) + 1;

	// Fill register
	unsigned rgtr_col_idx = 0;
	for (int col_idx = left_col_idx; col_idx <= right_col_idx; col_idx++) {
		// Column does not hold data, skip it
		if (!tracker_editor.grid.is_editable (col_idx)) {
			continue;
		}

		// Column is not visible, skip it
		if (!tracker_editor.grid.is_visible (col_idx)) {
			continue;
		}

		// Fill register column
		std::map<unsigned, std::string> rgtr_col;
		unsigned rgtr_row_idx = 0;
		for (int row_idx = top_row_idx; row_idx <= bottom_row_idx; row_idx++) {
			// Cell does not hold data, skip it
			if (!tracker_editor.grid.is_cell_defined (row_idx, col_idx)) {
				rgtr_row_idx++;
				continue;
			}
			std::string content = tracker_editor.grid.get_cell_content (row_idx, col_idx);
			if (content.empty ()) {
				rgtr_row_idx++;
				continue;
			}
			rgtr_col[rgtr_row_idx] = content;
			rgtr_row_idx++;
		}
		reg.data[rgtr_col_idx] = rgtr_col;
		rgtr_col_idx++;
	}
	reg.width = rgtr_col_idx;
}

void
SubgridSelector::paste ()
{
	if (reg.empty ())
		return;

	// Maximum number of columns and rows in the pattern
	int global_ncols = tracker_editor.grid.get_columns ().size ();
	int global_nrows = tracker_editor.grid.pattern.global_nrows;
	int top_row_idx = this->top_row_idx; // Mask attribute in case it changes
	int bottom_row_idx = std::min (global_nrows, (int)reg.height + top_row_idx) - 1;

	// Loop over columns, skipping uneditable or invisible ones
	int col_idx = left_col_idx;
	Register::Data::const_iterator rgtr_col_it = reg.data.begin ();
	for (unsigned rgtr_col_idx = 0; rgtr_col_idx < reg.width; rgtr_col_idx++) {
		// Column does not hold data or is not visible, skip it
		while (!tracker_editor.grid.is_editable (col_idx) or
				 !tracker_editor.grid.is_visible (col_idx)) {
			col_idx++;
			if (global_ncols <= col_idx) break; // Avoid horizontal overflow
		}
		if (global_ncols <= col_idx) break; // Avoid horizontal overflow

		// Column has no record, paste empty cells
		if (rgtr_col_it == reg.data.end () or rgtr_col_idx < rgtr_col_it->first) {
			for (int row_idx = top_row_idx; row_idx <= bottom_row_idx; row_idx++) {
				tracker_editor.grid.set_cell_content(row_idx, col_idx, "");
			}
			col_idx++;
			if (global_ncols <= col_idx) break; // Avoid horizontal overflow
			continue;
		}

		// Paste the column
		const Register::ColumnData& col_data = rgtr_col_it->second;
		Register::ColumnData::const_iterator row_it = col_data.begin ();
		for (int row_idx = top_row_idx; row_idx <= bottom_row_idx; ++row_it, row_idx++) {
			// Paste empty cells between recorded values
			while (row_it == col_data.end () or row_idx < (int)row_it->first + top_row_idx) {
				tracker_editor.grid.set_cell_content(row_idx, col_idx, "");
				row_idx++;
				if (bottom_row_idx < row_idx) break; // Avoid vertical overflow
			}
			if (bottom_row_idx < row_idx) break; // Avoid vertical overflow
			tracker_editor.grid.set_cell_content(row_idx, col_idx, row_it->second);
		}
		col_idx++;
		if (global_ncols <= col_idx) break; // Avoid horizontal overflow
		++rgtr_col_it;
	}
}

void
SubgridSelector::paste_overlay ()
{
	if (reg.empty ())
		return;

	// Maximum number of columns and rows in the pattern
	int global_ncols = tracker_editor.grid.get_columns ().size ();
	int global_nrows = tracker_editor.grid.pattern.global_nrows;
	int top_row_idx = this->top_row_idx; // Mask attribute in case it changes

	// Calculate the max column with content to paste
	unsigned max_rgtr_col_idx = reg.data.rbegin ()->first;

	// Loop over columns, skipping uneditable or invisible ones
	int col_idx = left_col_idx;
	Register::Data::const_iterator rgtr_col_it = reg.data.begin ();
	for (unsigned rgtr_col_idx = 0; rgtr_col_idx <= max_rgtr_col_idx; rgtr_col_idx++) {
		// Column does not hold data or is not visible, skip it
		while (!tracker_editor.grid.is_editable (col_idx) or
				 !tracker_editor.grid.is_visible (col_idx)) {
			col_idx++;
			if (global_ncols <= col_idx) break; // Avoid horizontal overflow
		}
		if (global_ncols <= col_idx) break; // Avoid horizontal overflow

		// Column has nothing to paste, skip it
		if (rgtr_col_idx < rgtr_col_it->first) {
			col_idx++;
			if (global_ncols <= col_idx) break; // Avoid horizontal overflow
			continue;
		}

		// Paste the column
		const Register::ColumnData& col_data = rgtr_col_it->second;
		Register::ColumnData::const_iterator row_it = col_data.begin ();
		for (; row_it != col_data.end (); ++row_it) {
			int row_idx = row_it->first + top_row_idx;
			if (global_nrows <= row_idx) break; // Avoid vertical overflow
			tracker_editor.grid.set_cell_content(row_idx, col_idx, row_it->second);
		}
		col_idx++;
		if (global_ncols <= col_idx) break; // Avoid horizontal overflow
		++rgtr_col_it;
	}
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
		top_row_idx = BasePattern::INVALID_ROW;
		bottom_row_idx = BasePattern::INVALID_ROW;
		left_col_idx = BasePattern::INVALID_COL;
		right_col_idx = BasePattern::INVALID_COL;
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
	return 0 <= top_row_idx;
}

bool
SubgridSelector::has_prev_selection () const
{
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
	ss << indent << "prev_top_row_idx = " << prev_top_row_idx << std::endl;
	ss << indent << "prev_bottom_row_idx = " << prev_bottom_row_idx << std::endl;
	ss << indent << "prev_left_col_idx = " << prev_left_col_idx << std::endl;
	ss << indent << "prev_right_col_idx = " << prev_right_col_idx << std::endl;

	// Register
	ss << indent << "Register:" << std::endl;
	ss << indent << "  " << "width = " << reg.width << std::endl;
	ss << indent << "  " << "height = " << reg.height << std::endl;
	ss << indent << "  " << "data:" << std::endl;
	Register::Data::const_iterator col_it = reg.data.begin ();
	for (; col_it != reg.data.end (); ++col_it) {
		unsigned col_idx = col_it->first;
		const Register::ColumnData& col_data = col_it->second;
		Register::ColumnData::const_iterator row_it = col_data.begin ();
		for (; row_it != col_data.end (); ++row_it) {
			unsigned row_idx = row_it->first;
			ss << indent << "    " << "reg[col_idx=" << col_idx << "][row_idx="
				<< row_idx << "] = " << row_it->second << std::endl;
		}
	}

	return ss.str ();
}
