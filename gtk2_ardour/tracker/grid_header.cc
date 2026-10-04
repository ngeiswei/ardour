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

#include "pbd/i18n.h"

#include <glibmm/main.h>

#include "grid_header.h"
#include "tracker_editor.h"

using namespace Tracker;

GridHeader::GridHeader (TrackerEditor& te)
	: tracker_editor (te)
{
	time_header.show ();
	pack_start (time_header, Gtk::PACK_SHRINK);
	setup_track_headers ();
	show ();

	// Re-align the headers whenever the Grid (TreeView) has been
	// laid out with a new size, e.g. after the user resizes the
	// tracker window. Without this, the headers would stay at their
	// previous size_request while the columns reflow, leaving them
	// misaligned until the next redisplay. align_deferred() coalesces
	// successive calls, so even a stream of size-allocate signals
	// (e.g. during a continuous resize drag) results in a single
	// alignment once the layout has settled.
	grid_size_allocate_connection = tracker_editor.grid.signal_size_allocate ().connect (
		[this] (Gtk::Allocation&) { align_deferred (); });
}

void
GridHeader::setup_track_headers ()
{
	for (size_t mti = 0; mti < tracker_editor.grid.pattern.tps.size (); mti++) {
		if (mti < track_headers.size ()) {
			if (tracker_editor.grid.pattern.tps[mti]->enabled) {
				track_headers[mti]->show ();
			} else {
				track_headers[mti]->hide ();
			}
		} else {
			TrackPattern* tp = tracker_editor.grid.pattern.tps[mti];
			TrackHeader* th = new TrackHeader (tracker_editor, tp, mti);
			track_headers.push_back (th);
			pack_start (*th, Gtk::PACK_SHRINK);
		}
	}
}

GridHeader::~GridHeader ()
{
	grid_size_allocate_connection.disconnect ();
	align_deferred_connection.disconnect ();
	for (std::vector<TrackHeader*>::iterator it = track_headers.begin (); it != track_headers.end (); ++it) {
		delete *it;
	}
}

void
GridHeader::set_time_header_size (int width, int height)
{
	time_header.set_size_request (width, height);
}

void
GridHeader::set_track_header_size (int mti, int width, int height)
{
	width = std::max (width, track_headers[mti]->get_min_width ());
	track_headers[mti]->set_size_request (width, height);
}

void
GridHeader::align ()
{
	int track_separator_width = tracker_editor.grid.get_track_separator_width ();
	int time_width = tracker_editor.grid.get_time_width ();
	set_spacing (track_separator_width);
	int diff = time_width - track_separator_width;
	if (diff < 0) {
		diff = -1;
	}
	set_time_header_size (diff);

	// Re-adjust the track-color (right separator) columns against the
	// post-layout column widths.  This runs deferred (from align_deferred),
	// so it avoids the transient 0-width state seen right after a schema
	// rebuild, which previously made these separators over-wide.
	tracker_editor.grid.align_left_right_separators ();

	for (size_t mti = 0; mti < tracker_editor.grid.pattern.tps.size (); mti++) {
		int track_width = tracker_editor.grid.get_track_width (mti);
		set_track_header_size (mti, track_width);
	}
}

void
GridHeader::align_deferred ()
{
	// Coalesce: if a deferred alignment is already pending, do nothing.
	if (align_deferred_connection.connected ()) {
		return;
	}
	align_deferred_connection = Glib::signal_idle ().connect (
		[this] () {
			// Returning false disconnects the idle handler, so the
			// next align_deferred() schedules a fresh one.
			align ();
			return false;
		});
}
