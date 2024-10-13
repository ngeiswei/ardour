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

#include "audio_track_pattern.h"
#include "audio_track_pattern_phenomenal_diff.h"
#include "grid.h"
#include "tracker_utils.h"

using namespace Tracker;

AudioTrackPattern::AudioTrackPattern (TrackerEditor& te,
                                      TrackPtr trk,
                                      const RegionSeq& regions,
                                      Temporal::samplepos_t position,
                                      Temporal::samplecnt_t length,
                                      Temporal::samplepos_t first_sample,
                                      Temporal::samplepos_t last_sample,
                                      bool connect)
	: TrackAutomationPattern (te, trk, position, length, first_sample, last_sample, connect)
{
}

AudioTrackPattern::~AudioTrackPattern ()
{
}

AudioTrackPatternPhenomenalDiff
AudioTrackPattern::phenomenal_diff (const AudioTrackPattern& prev) const
{
	// VT: implement
	return AudioTrackPatternPhenomenalDiff ();
}
