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

#ifndef __ardour_tracker_subgrid_selector_h_
#define __ardour_tracker_subgrid_selector_h_

#include <map>
#include <string>

namespace Tracker {

class TrackerEditor;

/**
 * Class holding the copied subgrid
 */
class Register
{
public:
	// Width and height of the register
	unsigned width;
	unsigned height;

	// The first unsigned is the column index (relative to the left border of
	// the selection only considering visible columns in the grid).  The second
	// unsigned is the row index (relative to the top border of the selection).
	// The mapped string is the verbatim content of the cell at this relative
	// location.
	typedef std::map<unsigned, std::string> ColumnData;
	typedef std::map<unsigned, ColumnData> Data;
	Data data;

	// Clear the register
	void clear ();

	// Check if the register is empty
	bool empty () const;
};

/**
 * Class handling the selected rectangle overlaying the tracker grid.
 */
class SubgridSelector
{
public:
	// CTor
	SubgridSelector (TrackerEditor& te);

	// Set/unset the selection location
	void set_source (int row_idx, int col_idx);
	void set_destination (int row_idx, int col_idx);
	void unset ();

	// Check if there is a selection
	bool has_source () const;
	bool has_destination () const;

	// Cut, copy or paste
	void cut ();
	void clear_subgrid ();
	void copy ();
	void paste ();
	void paste_overlay ();

	// Set top and bottom rows, left and right cols based on source and
	// destination rows and cols.
	void set_rectangle ();

	// Whether a selection is defined
	bool has_selection () const;

	// Whether a previous selection was defined
	bool has_prev_selection () const;

	std::string to_string (std::string indent="") const;

	TrackerEditor& tracker_editor;

	// Source cell location (first selected cell). Negative means undefined.
	int src_row_idx;
	int src_col_idx;

	// Destination cell location (last selected cell). Negative means undefined.
	int dst_row_idx;
	int dst_col_idx;

	// Top and bottom row index included,
	//
	// top_row_idx <= bottom_row_idx
	//
	// Negative means undefined.
	int top_row_idx;
	int bottom_row_idx;

	// Left and right col index included,
	//
	// left_col_idx <= right_col_idx
	//
	// Negative means undefined.
	int left_col_idx;
	int right_col_idx;

	// Like above but for previous selection (used for calculating
	// selection difference for removing selection)
	int prev_top_row_idx;
	int prev_bottom_row_idx;
	int prev_left_col_idx;
	int prev_right_col_idx;

	// Hold the copied data
	Register reg;
};

} // ~namespace tracker

#endif /* __ardour_tracker_subgrid_selector_h_ */
