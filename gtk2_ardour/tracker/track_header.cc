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

#include <iostream>

#include "audio_track_toolbar.h"
#include "midi_track_toolbar.h"
#include "track_header.h"

using namespace Tracker;

TrackHeader::TrackHeader (TrackerEditor& te, TrackPattern* tp, int mti)
{
	// Instantiate TrackToolbar
	if (tp->is_midi_track_pattern ()) {
		track_toolbar = new MidiTrackToolbar (te, *tp->midi_track_pattern (), mti);
	} else if (tp->is_audio_track_pattern ()) {
		track_toolbar = new AudioTrackToolbar (te, *tp->audio_track_pattern (), mti);
	} else {
		std::cerr << "[ERROR] TrackHeader::TrackHeader track type not implemented" << std::endl;
	}

	// Set frame title
	set_label (tp->track->name ());
	set_label_align (0.5);

	// Add toolbar to frame body
	add (*track_toolbar);

	show ();
}

TrackHeader::~TrackHeader ()
{
	delete track_toolbar;
}

int
TrackHeader::get_min_width () const
{
	int width = 2*get_border_width () + track_toolbar->get_min_width () + 4;
	return width;
}
