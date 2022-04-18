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
#include "midi_track_pattern.h"
#include "midi_track_pattern_phenomenal_diff.h"
#include "track_pattern.h"
#include "tracker_utils.h"
#include "tracker_editor.h"

using namespace Tracker;

// NEXT.9
//
// We need to think about how to have TrackPattern implement
//
// void insert (const Evoral::Parameter& param)
//
// Likely, it shouldn't implement it but rather implement
//
// insert(param, processor) or such.  Though in the mean time it could try to
// be backward compatible and implement insert(param), TBD.
//
// Note: the problem has been raised by the following compile error:
//
// ../gtk2_ardour/tracker/multi_track_pattern.cc: In member function ‘void Tracker::MultiTrackPattern::insert(int, const Evoral::Parameter&)’:
// ../gtk2_ardour/tracker/multi_track_pattern.cc:480:19: error: ‘class Tracker::TrackPattern’ has no member named ‘insert’
//   480 |         tps[mti]->insert (param);
//       |                   ^~~~~~
//
// NEXT.8
//
// ../gtk2_ardour/tracker/track_pattern.cc: In constructor ‘Tracker::TrackPattern::TrackPattern(Tracker::TrackerEditor&, Tracker::TrackPtr, const RegionSeq&, bool)’:
// ../gtk2_ardour/tracker/track_pattern.cc:33:11: error: type ‘Tracker::AutomationPattern’ is not a direct base of ‘Tracker::TrackPattern’
//    33 |         : AutomationPattern (te,
//       |           ^~~~~~~~~~~~~~~~~
// ../gtk2_ardour/tracker/track_pattern.cc:40:21: error: no matching function for call to ‘Tracker::BasePattern::BasePattern()’
//    40 |         , track (trk)
//       |                     ^
// In file included from ../gtk2_ardour/tracker/track_pattern.h:27,
//                  from ../gtk2_ardour/tracker/track_automation_pattern.h:24,
//                  from ../gtk2_ardour/tracker/audio_track_pattern.h:25,
//                  from ../gtk2_ardour/tracker/track_pattern.cc:19:
// ../gtk2_ardour/tracker/base_pattern.h:58:9: note: candidate: ‘Tracker::BasePattern::BasePattern(Tracker::TrackerEditor&, Temporal::timepos_t, Temporal::timepos_t, Temporal::timepos_t, Temporal::timepos_t, Temporal::timepos_t)’
//    58 |         BasePattern (TrackerEditor& te,
//       |         ^~~~~~~~~~~
TrackPattern::TrackPattern (TrackerEditor& te,
                            TrackPtr trk,
                            const RegionSeq& regions,
                            bool connect)
	: BasePattern (te,
	               TrackerUtils::get_position_sample (regions),
	               0,
	               TrackerUtils::get_length_sample (regions),
	               TrackerUtils::get_first_sample (regions),
	               TrackerUtils::get_last_sample (regions))
	, track (trk)
	, track_automation_pattern (te,
	                            track,
	                            TrackerUtils::get_position_sample (regions),
	                            TrackerUtils::get_length_sample (regions),
	                            TrackerUtils::get_first_sample (regions),
	                            TrackerUtils::get_last_sample (regions),
	                            connect)
{
	if (connect)
		tracker_editor.connect_track (track);
}

TrackPattern::TrackPattern (TrackerEditor& te,
                            TrackPtr trk,
                            Temporal::samplepos_t pos,
                            Temporal::samplecnt_t len,
                            Temporal::samplepos_t fst,
                            Temporal::samplepos_t lst,
                            bool connect)
	: BasePattern (te, pos, 0, len, fst, lst)
	, track (trk)
	, track_automation_pattern (te, pos, 0, len, fst, lst, connect)
{
	if (connect)
		tracker_editor.connect_track (track);
}

TrackPattern::~TrackPattern ()
{
}

void
TrackPattern::setup (const RegionSeq&)
{
	// Do nothing since there are no region by default
}

TrackPattern&
TrackPattern::operator= (const TrackPattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	AutomationPattern::operator= (other);
	track = other.track;

	return *this;
}

TrackPatternPhenomenalDiff*
TrackPattern::phenomenal_diff_ptr (const TrackPattern* prev) const
{
	TrackPatternPhenomenalDiff* diff_ptr;
	if (is_midi_track_pattern ()) {
		const MidiTrackPattern* mtp = midi_track_pattern ();
		const MidiTrackPattern* prev_mtp = prev->midi_track_pattern ();
		MidiTrackPatternPhenomenalDiff mtp_diff = mtp->phenomenal_diff (*prev_mtp);
		diff_ptr = new MidiTrackPatternPhenomenalDiff (mtp_diff);
	} else if (is_audio_track_pattern ()) {
		const AudioTrackPattern* atp = audio_track_pattern ();
		const AudioTrackPattern* prev_atp = prev->audio_track_pattern ();
		AudioTrackPatternPhenomenalDiff atp_diff = atp->phenomenal_diff (*prev_atp);
		diff_ptr = new AudioTrackPatternPhenomenalDiff (atp_diff);
	} else {
		std::cout << "Not implemented" << std::endl;
		diff_ptr = 0;
	}
	return diff_ptr;
}

MidiTrackPtr
TrackPattern::midi_track ()
{
	return boost::dynamic_pointer_cast<ARDOUR::MidiTrack> (track);
}

AudioTrackPtr
TrackPattern::audio_track ()
{
	return boost::dynamic_pointer_cast<ARDOUR::AudioTrack> (track);
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
	return dynamic_cast<const MidiTrackPattern*> (this);
}

const AudioTrackPattern*
TrackPattern::audio_track_pattern () const
{
	return dynamic_cast<const AudioTrackPattern*> (this);
}

MidiTrackPattern*
TrackPattern::midi_track_pattern ()
{
	return dynamic_cast<MidiTrackPattern*> (this);
}

AudioTrackPattern*
TrackPattern::audio_track_pattern ()
{
	return dynamic_cast<AudioTrackPattern*> (this);
}

Temporal::Beats
TrackPattern::region_relative_beats (int rowi, int mri, int delay) const
{
	return Temporal::Beats ();
}

int64_t
TrackPattern::region_relative_delay_ticks (const Temporal::Beats& event_time, int rowi, int mri) const
{
	return 0;
}

bool
TrackPattern::is_auto_displayable (int rowi, int mri, const Evoral::Parameter& param) const
{
	// VERY VERY NEXT: look for main or processors with such param and
	// call its corresponding is_displayable method.  Maybe using
	// what_can_be_automated ().
	return is_displayable (rowi, param);
}

size_t
TrackPattern::control_events_count (int rowi, int mri, const Evoral::Parameter& param) const
{
	return AutomationPattern::control_events_count (rowi, param);
}

RowToControlEventsRange
TrackPattern::control_events_range (int rowi, int mri, const Evoral::Parameter& param) const
{
	return AutomationPattern::control_events_range (rowi, param);
}

Evoral::ControlEvent*
TrackPattern::get_automation_control_event (int rowi, int mri, const Evoral::Parameter& param)
{
	return get_control_event (rowi, param);
}

const Evoral::ControlEvent*
TrackPattern::get_automation_control_event (int rowi, int mri, const Evoral::Parameter& param) const
{
	return get_control_event (rowi, param);
}

bool
TrackPattern::is_region_defined (int rowi) const
{
	return false;
}

int
TrackPattern::to_rrri (int rowi, int mri) const
{
	return rowi;
}

int
TrackPattern::to_rrri (int rowi) const
{
	return rowi;
}

int
TrackPattern::to_mri (int rowi) const
{
	return -1;
}

AutomationListPtr
TrackPattern::get_alist_at_mri (int mri, const Evoral::Parameter& param)
{
	return AutomationPattern::get_alist (param);
}

const AutomationListPtr
TrackPattern::get_alist_at_mri (int mri, const Evoral::Parameter& param) const
{
	return AutomationPattern::get_alist (param);
}

std::pair<double, bool>
TrackPattern::get_automation_value (int rowi, int mri, const Evoral::Parameter& param)
{
	return AutomationPattern::get_automation_value (rowi, param);
}

void
TrackPattern::set_automation_value (double val, int rowi, int mri, const Evoral::Parameter& param, int delay)
{
	AutomationPattern::set_automation_value (val, rowi, param, delay);
}

void
TrackPattern::delete_automation_value (int rowi, int mri, const Evoral::Parameter& param)
{
	AutomationPattern::delete_automation_value (rowi, param);
}

std::pair<int, bool>
TrackPattern::get_automation_delay (int rowi, int mri, const Evoral::Parameter& param)
{
	return AutomationPattern::get_automation_delay (rowi, param);
}

void
TrackPattern::set_automation_delay (int delay, int rowi, int mri, const Evoral::Parameter& param)
{
	AutomationPattern::set_automation_delay (delay, rowi, param);
}

double
TrackPattern::lower (int rowi, const Evoral::Parameter& param) const
{
	return AutomationPattern::lower (param);
}

double
TrackPattern::upper (int rowi, const Evoral::Parameter& param) const
{
	return AutomationPattern::upper (param);
}
