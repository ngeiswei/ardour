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

#include "audio_track_pattern.h"

#include "tracker_utils.h"
#include "grid.h"

using namespace Tracker;

AudioTrackPattern::AudioTrackPattern (TrackerEditor& te,
                                      boost::shared_ptr<ARDOUR::Track> trk,
                                      const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions,
                                      Temporal::samplepos_t position,
                                      Temporal::samplecnt_t length,
                                      Temporal::samplepos_t first_sample,
                                      Temporal::samplepos_t last_sample)
	: TrackAutomationPattern(te, trk, position, length, first_sample, last_sample)
{
}

AudioTrackPattern::~AudioTrackPattern ()
{
}

bool
AudioTrackPattern::PhenomenalDiff::empty() const
{
	return !full;
}

std::string
AudioTrackPattern::PhenomenalDiff::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << TrackPattern::PhenomenalDiff::to_string(indent) << std::endl;
	return ss.str();
}

AudioTrackPattern::PhenomenalDiff
AudioTrackPattern::phenomenal_diff(const AudioTrackPattern& prev) const
{
	// VT: implement
	return PhenomenalDiff();
}
