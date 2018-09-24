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

#include "tracker_utils.h"
#include "track_pattern.h"
#include "midi_track_pattern.h"
#include "audio_track_pattern.h"

using namespace Tracker;

TrackPattern::TrackPattern (TrackerEditor& te,
                            boost::shared_ptr<ARDOUR::Track> trk,
                            const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions)
	: AutomationPattern(te,
	                    TrackerUtils::get_position(regions),
	                    0,
	                    TrackerUtils::get_length(regions),
	                    TrackerUtils::get_first_sample(regions),
	                    TrackerUtils::get_last_sample(regions))
	, track(trk)
{
}

TrackPattern::TrackPattern (TrackerEditor& te,
                            boost::shared_ptr<ARDOUR::Track> trk,
                            Temporal::samplepos_t pos,
                            Temporal::samplecnt_t len,
                            Temporal::samplepos_t fst,
                            Temporal::samplepos_t lst)
	: AutomationPattern(te, pos, 0, len, fst, lst)
	, track(trk)
{
}

TrackPattern::~TrackPattern ()
{
}

boost::shared_ptr<ARDOUR::MidiTrack>
TrackPattern::midi_track ()
{
	return boost::dynamic_pointer_cast<ARDOUR::MidiTrack>(track);
}

boost::shared_ptr<ARDOUR::AudioTrack>
TrackPattern::audio_track ()
{
	return boost::dynamic_pointer_cast<ARDOUR::AudioTrack>(track);
}

bool
TrackPattern::is_midi_track_pattern () const
{
	return (bool)midi_track_pattern ();
}

bool
TrackPattern::is_audio_track_pattern () const
{
	return (bool)audio_track_pattern ();
}

const MidiTrackPattern*
TrackPattern::midi_track_pattern () const
{
	return dynamic_cast<const MidiTrackPattern*>(this);
}

const AudioTrackPattern*
TrackPattern::audio_track_pattern () const
{
	return dynamic_cast<const AudioTrackPattern*>(this);
}

MidiTrackPattern*
TrackPattern::midi_track_pattern ()
{
	return dynamic_cast<MidiTrackPattern*>(this);
}

AudioTrackPattern*
TrackPattern::audio_track_pattern ()
{
	return dynamic_cast<AudioTrackPattern*>(this);
}

Temporal::Beats
TrackPattern::region_relative_beats (uint32_t rowi, size_t mri, int32_t delay) const
{
	return Temporal::Beats();
}

int64_t TrackPattern::region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mri) const
{
	return 0;
}
