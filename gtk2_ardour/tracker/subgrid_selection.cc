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

#include "subgrid_selection.h"

using namespace Tracker;

SubgridSelection::SubgridSelection()
	: src_row_idx(-1)
	, src_col_idx(-1)
	, dst_row_idx(-1)
	, dst_col_idx(-1)
	, top_row_idx(-1)
	, bottom_row_idx(-1)
	, left_col_idx(-1)
	, right_col_idx(-1)
{
}

void SubgridSelection::set_source(int row_idx, int col_idx)
{
	src_row_idx = row_idx;
	src_col_idx = col_idx;
	dst_row_idx = -1;
	dst_col_idx = -1;
}

void SubgridSelection::set_destination(int row_idx, int col_idx)
{
	if (!is_source_set())
		set_source(row_idx, col_idx);

	dst_row_idx = row_idx;
	dst_col_idx = col_idx;
}

void SubgridSelection::unset()
{
	src_row_idx = -1;
	src_col_idx = -1;
	dst_row_idx = -1;
	dst_row_idx = -1;
}

bool SubgridSelection::is_source_set() const
{
	return 0 <= src_row_idx and 0 <= src_col_idx;
}

bool SubgridSelection::is_destination_set() const
{
	return 0 <= dst_row_idx and 0 <= dst_col_idx;
}

void SubgridSelection::cut()
{
	// NEXT
}

void SubgridSelection::copy()
{
	// NEXT
}

void SubgridSelection::paste()
{
	// NEXT
}
