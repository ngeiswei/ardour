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

namespace Tracker {

/**
 * Class handling the selected rectangle overlaying the tracker grid.
 */
class SubgridSelection
{
public:
	// CTor
	SubgridSelection();

	// Set/unset the selection coordinates
	void set_source(int row_idx, int col_idx);
	void set_destination(int row_idx, int col_idx);
	void unset();

	// Check if there is a selection
	bool is_set() const;
	bool is_source_set() const;
	bool is_destination_set() const;

	// Cut, copy or paste
	void cut();
	void copy();
	void paste();

private:
	// Source cell coordonates (first selected cell). Negative means
	// undefined.
	int src_row_idx;
	int src_col_idx;

	// Destination cell coordonates (last selected cell). Negative
	// means undefined.
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
};

} // ~namespace tracker
