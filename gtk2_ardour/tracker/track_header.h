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

#ifndef __ardour_tracker_track_header_h_
#define __ardour_tracker_track_header_h_

#include <ytkmm/frame.h>

#include "track_pattern.h"
#include "track_toolbar.h"

namespace Tracker {

class TrackerEditor;

class TrackHeader : public Gtk::Frame
{
public:
	TrackHeader (TrackerEditor& te, TrackPattern* tp, int mti);
	~TrackHeader ();

	int get_min_width () const;

	TrackToolbar* track_toolbar;
};

} // ~namespace tracker

#endif /* __ardour_tracker_super_header_h_ */
