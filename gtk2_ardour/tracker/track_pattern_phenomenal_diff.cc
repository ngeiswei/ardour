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

#include "track_pattern_phenomenal_diff.h"

#include <iostream>
#include <sstream>

#include "midi_track_pattern_phenomenal_diff.h"
#include "audio_track_pattern_phenomenal_diff.h"

using namespace Tracker;

bool
TrackPatternPhenomenalDiff::empty() const
{
	if (is_midi_track_phenomenal_diff()) {
		return midi_track_phenomenal_diff()->empty();
	} else if (is_audio_track_phenomenal_diff()) {
		return audio_track_phenomenal_diff()->empty();
	} else {
		std::cout << "Not implemented" << std::endl;
		return false;
	}
}

std::string
TrackPatternPhenomenalDiff::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePatternPhenomenalDiff::to_string(indent) << std::endl;
	if (is_midi_track_phenomenal_diff()) {
		ss << midi_track_phenomenal_diff()->to_string(indent);
	} else if (is_audio_track_phenomenal_diff()) {
		ss << audio_track_phenomenal_diff()->to_string(indent);
	} else {
		std::cout << "Not implemented" << std::endl;
	}
	return ss.str();
}

bool
TrackPatternPhenomenalDiff::is_midi_track_phenomenal_diff() const
{
	return dynamic_cast<const MidiTrackPatternPhenomenalDiff*>(this);
}

bool
TrackPatternPhenomenalDiff::is_audio_track_phenomenal_diff() const
{
	return dynamic_cast<const AudioTrackPatternPhenomenalDiff*>(this);
}

const MidiTrackPatternPhenomenalDiff*
TrackPatternPhenomenalDiff::midi_track_phenomenal_diff() const
{
	return dynamic_cast<const MidiTrackPatternPhenomenalDiff*>(this);
}

const AudioTrackPatternPhenomenalDiff*
TrackPatternPhenomenalDiff::audio_track_phenomenal_diff() const
{
	return dynamic_cast<const AudioTrackPatternPhenomenalDiff*>(this);
}

MidiTrackPatternPhenomenalDiff*
TrackPatternPhenomenalDiff::midi_track_phenomenal_diff()
{
	return dynamic_cast<MidiTrackPatternPhenomenalDiff*>(this);
}

AudioTrackPatternPhenomenalDiff*
TrackPatternPhenomenalDiff::audio_track_phenomenal_diff()
{
	return dynamic_cast<AudioTrackPatternPhenomenalDiff*>(this);
}
