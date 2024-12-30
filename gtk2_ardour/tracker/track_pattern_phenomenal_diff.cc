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
#include <sstream>

#include "audio_track_pattern_phenomenal_diff.h"
#include "midi_track_pattern_phenomenal_diff.h"
#include "track_pattern_phenomenal_diff.h"

using namespace Tracker;

bool
TrackPatternPhenomenalDiff::empty () const
{
	return taap_diff.empty ();
}

std::string
TrackPatternPhenomenalDiff::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePatternPhenomenalDiff::to_string (indent) << std::endl;
	ss << indent << "taap_diff:" << std::endl;
	ss << taap_diff.to_string (indent + "  ");
	return ss.str ();
}

const MidiTrackPatternPhenomenalDiff*
TrackPatternPhenomenalDiff::midi_track_pattern_phenomenal_diff () const
{
	return dynamic_cast<const MidiTrackPatternPhenomenalDiff*> (this);
}

const AudioTrackPatternPhenomenalDiff*
TrackPatternPhenomenalDiff::audio_track_pattern_phenomenal_diff () const
{
	return dynamic_cast<const AudioTrackPatternPhenomenalDiff*> (this);
}

MidiTrackPatternPhenomenalDiff*
TrackPatternPhenomenalDiff::midi_track_pattern_phenomenal_diff ()
{
	return dynamic_cast<MidiTrackPatternPhenomenalDiff*> (this);
}

AudioTrackPatternPhenomenalDiff*
TrackPatternPhenomenalDiff::audio_track_pattern_phenomenal_diff ()
{
	return dynamic_cast<AudioTrackPatternPhenomenalDiff*> (this);
}
