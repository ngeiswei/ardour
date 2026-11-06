/*
 * Copyright (C) 2026 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_column_content_type_h_
#define __ardour_tracker_column_content_type_h_

namespace Tracker {

/**
 * Type of content carried by a grid column, and therefore by every cell
 * copied from it.  A note-name column (including the "===" note-off marker)
 * may only be pasted into another note-name column, and a numeric column
 * only into another numeric column.
 */
enum class ColumnContentType {
	EMPTY,   // no data / clear a destination cell
	NOTE,    // note name or the note-off marker "==="
	VALUE    // numeric value (channel, velocity, delay, automation, ...)
};

/**
 * Return true iff content of type content_type can be pasted into a column
 * of type column_type.  Empty content may clear any column.
 */
inline bool
column_content_type_compatible (ColumnContentType content_type, ColumnContentType column_type)
{
	return content_type == ColumnContentType::EMPTY ||
		(column_type != ColumnContentType::EMPTY && content_type == column_type);
}

inline const char*
column_content_type_to_string (ColumnContentType type)
{
	switch (type) {
	case ColumnContentType::EMPTY: return "empty";
	case ColumnContentType::NOTE:  return "note";
	case ColumnContentType::VALUE: return "value";
	}
	return "";
}

} // ~namespace Tracker

#endif /* __ardour_tracker_column_content_type_h_ */
