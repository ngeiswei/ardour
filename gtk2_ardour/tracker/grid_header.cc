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
	for (size_t mti = 0; mti < tracker_editor.grid.pattern.tps.size (); mti++) {
		int track_width = tracker_editor.grid.get_track_width (mti);
		set_track_header_size (mti, track_width);
	}
}
