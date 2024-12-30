/*
 * Copyright (C) 2017-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_audio_track_pattern_h_
#define __ardour_tracker_audio_track_pattern_h_

#include "ardour/audio_track.h"
#include "ardour/audioregion.h"

#include "track_pattern.h"
#include "audio_track_pattern_phenomenal_diff.h"

namespace Tracker {

/**
 * Represent audio track pattern. Just track automation for now.
 */
class AudioTrackPattern : public TrackPattern {
public:
	AudioTrackPattern (TrackerEditor& te,
	                   TrackPtr track,
	                   const RegionSeq& regions,
	                   Temporal::timepos_t position,
	                   Temporal::timecnt_t length,
	                   Temporal::timepos_t end,
	                   Temporal::timepos_t nt_last,
	                   bool connect);
	virtual ~AudioTrackPattern ();

	// TODO: for now do not worry about memory leaking, create a new
	// PhenomenalDiff object at every call
	TrackPatternPhenomenalDiff* phenomenal_diff_ptr (const TrackPattern* prev) const;

	AudioTrackPatternPhenomenalDiff phenomenal_diff (const AudioTrackPattern& prev) const;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_midi_track_pattern_h_ */
