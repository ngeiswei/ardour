/*
    Copyright (C) 2017-2018 Nil Geisweiller

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

#ifndef __ardour_tracker_audio_track_pattern_h_
#define __ardour_tracker_audio_track_pattern_h_

#include "ardour/audio_track.h"
#include "ardour/audioregion.h"

#include "track_automation_pattern.h"

namespace Tracker {

/**
 * Represent audio track pattern. Just track automation for now.
 */
class AudioTrackPattern : public TrackAutomationPattern {
public:
	AudioTrackPattern(TrackerEditor& te,
	                  boost::shared_ptr<ARDOUR::Track> track,
	                  const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions,
	                  Temporal::samplepos_t position,
	                  Temporal::samplecnt_t length,
	                  Temporal::samplepos_t first_sample,
	                  Temporal::samplepos_t last_sample);
	virtual ~AudioTrackPattern ();

	// Represent the differences that may impact grid rendering. For now only a
	// boolean to tell whether the patterns differ.
	typedef bool PhenomenalDiff;

	PhenomenalDiff phenomenal_diff(const AudioTrackPattern& prev) const;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_midi_track_pattern_h_ */
