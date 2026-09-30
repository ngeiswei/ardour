/*
 * Copyright (C) 2018-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_grid_header_h_
#define __ardour_tracker_grid_header_h_

#include <vector>

#include <sigc++/connection.h>

#include "time_header.h"
#include "track_header.h"

namespace Tracker {

/**
 * Widget to place on top of Grid, contains general tools on top of the
 * Time column such as resolution, number of steps, etc, and per track tools
 * such as -+ note tracks, Note, Channel, Velocity, Delay and Automation.
 *
 * Attempt to be aligned with the columns.
 */
class GridHeader : public Gtk::HBox
{
public:
	explicit GridHeader (TrackerEditor& te);
	~GridHeader ();

	void setup_track_headers ();

	void set_time_header_size (int width=-1, int height=-1);
	void set_track_header_size (int mti, int width=-1, int height=-1);

	/**
	 * Align the time and track headers with the time and track columns.
	 */
	void align ();

	/**
	 * Schedule an alignment to occur once GTK has had a chance to lay
	 * out the Grid (and its columns) after the columns visibility or
	 * count has changed. Without this deferral, querying the columns'
	 * widths from align() would return stale values because the tree
	 * view's size allocation has not yet been processed, leaving the
	 * headers misaligned with the columns until the next redisplay.
	 *
	 * Subsequent calls before the deferred alignment runs are
	 * coalesced into a single align().
	 */
	void align_deferred ();

	TrackerEditor& tracker_editor;

	TimeHeader time_header;
	std::vector<TrackHeader*> track_headers;

private:
	// Connection for a pending deferred alignment. Disconnects itself
	// once it fires so the next align_deferred() schedules a fresh one.
	sigc::connection align_deferred_connection;

	// Connection from the Grid's size-allocate signal, used to
	// re-align the headers after the tree view has been laid out
	// (e.g. after the user resizes the tracker window, which would
	// otherwise leave the headers misaligned with the columns until
	// the next redisplay).
	sigc::connection grid_size_allocate_connection;
};

} // ~namespace tracker

#endif /* __ardour_tracker_grid_header_h_ */
