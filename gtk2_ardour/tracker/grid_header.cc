/*
    Copyright (C) 2018 Nil Geisweiller

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "grid_header.h"
#include "tracker_editor.h"

using namespace Tracker;

GridHeader::GridHeader(TrackerEditor& te)
	: tracker_editor(te)
{
	time_header.show();
	add (time_header);
	for (size_t mti = 0; mti < te.grid.pattern.tps.size(); mti++) {
		TrackPattern* tp = te.grid.pattern.tps[mti];
		TrackHeader* th = new TrackHeader (te, tp, mti);
		track_headers.push_back(th);
		add (*th);
	}
	show();
}

GridHeader::~GridHeader()
{
	for (std::vector<TrackHeader*>::iterator it = track_headers.begin(); it != track_headers.end(); ++it)
		delete *it;
}

void
GridHeader::set_time_header_size(int width, int height)
{
	time_header.set_size_request(width, height);
}

void
GridHeader::set_track_header_size(size_t mti, int width, int height)
{
	track_headers[mti]->set_size_request(width, height);
}

void
GridHeader::align()
{
	set_time_header_size(tracker_editor.grid.get_time_width());
	for (size_t mti = 0; mti < tracker_editor.grid.pattern.tps.size(); mti++)
		set_track_header_size(mti, tracker_editor.grid.get_track_width(mti));
}
