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
	, time_header (te)
	, track_header_hadjustment (0, 0, 0)
	, track_header_vadjustment (0, 0, 0)
	, track_header_viewport (track_header_hadjustment, track_header_vadjustment)
{
	time_header.show ();
	pack_start (time_header, Gtk::PACK_SHRINK);

	// Vertical line separating the time column from the track columns.
	Gtk::VSeparator* time_separator = Gtk::manage (new Gtk::VSeparator ());
	time_separator->show ();
	pack_start (*time_separator, false, false);

	track_headers_hbox.show ();
	track_header_viewport.add (track_headers_hbox);
	track_header_viewport.set_shadow_type (Gtk::SHADOW_NONE);
	track_header_viewport.show ();
	pack_start (track_header_viewport, Gtk::PACK_EXPAND_WIDGET);

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

	// Mirror the grid scroller's horizontal scroll position on the
	// track-header viewport so they always stay aligned.
	hadjustment_connection = tracker_editor.scroller.get_hadjustment ()->signal_value_changed ().connect (
		sigc::mem_fun (*this, &GridHeader::sync_hadjustment));
	sync_hadjustment ();
}

void
GridHeader::sync_hadjustment ()
{
	double v = tracker_editor.scroller.get_hadjustment ()->get_value ();
	if (track_header_hadjustment.get_value () != v) {
		track_header_hadjustment.set_value (v);
	}
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
			track_headers_hbox.pack_start (*th, Gtk::PACK_SHRINK);
		}
	}
}

GridHeader::~GridHeader ()
{
	hadjustment_connection.disconnect ();
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
	set_spacing (0);
	track_headers_hbox.set_spacing (track_separator_width);
	set_time_header_size (time_width);

	// Align each track header and its right separator column to the track's
	// content width in a single pass, so the header and the grid columns are
	// always the same width.  Computing the target from the content width
	// (excluding the right separator) avoids reading a stale right-separator
	// width, which previously left the headers misaligned after a resize.
	for (size_t mti = 0; mti < tracker_editor.grid.pattern.tps.size (); mti++) {
		int content_width = tracker_editor.grid.get_track_width_without_right (mti);
		int min_width = track_headers[mti]->get_min_width ();
		int target = std::max (content_width + LEFT_RIGHT_SEPARATOR_WIDTH, min_width);
		tracker_editor.grid.set_right_separator_width (mti, target - content_width);
		set_track_header_size (mti, target);
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
