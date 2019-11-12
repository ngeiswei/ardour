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

using namespace Tracker;

TrackPattern::TrackPattern (TrackerEditor& te,
                            TrackPtr trk,
                            const RegionSeq& regions)
	: AutomationPattern (te,
	                     TrackerUtils::get_position (regions),
	                     0,
	                     TrackerUtils::get_length (regions),
	                     TrackerUtils::get_first_sample (regions),
	                     TrackerUtils::get_last_sample (regions))
	, track (trk)
{
}

TrackPattern::TrackPattern (TrackerEditor& te,
                            TrackPtr trk,
                            Temporal::samplepos_t pos,
                            Temporal::samplecnt_t len,
                            Temporal::samplepos_t fst,
                            Temporal::samplepos_t lst)
	: AutomationPattern (te, pos, 0, len, fst, lst)
	, track (trk)
{
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
TrackPattern::region_relative_beats (uint32_t rowi, size_t mri, int32_t delay) const
{
	return Temporal::Beats ();
}

int64_t
TrackPattern::region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mri) const
{
	return 0;
}

bool
TrackPattern::is_auto_displayable (uint32_t rowi, size_t mri, const Evoral::Parameter& param) const
{
	return is_displayable (rowi, param);
}

size_t
TrackPattern::get_automation_list_count (uint32_t rowi, size_t mri, const Evoral::Parameter& param) const
{
	return param_to_row_to_ali.find (param)->second.count (rowi);
}

Evoral::ControlEvent*
TrackPattern::get_automation_control_event (uint32_t rowi, size_t mri, const Evoral::Parameter& param) const
{
	ParamToRowToAutomationListIt::const_iterator pit = param_to_row_to_ali.find (param);
	if (pit == param_to_row_to_ali.end())
		return 0;

	RowToAutomationListIt::const_iterator r2al_it = pit->second.find (rowi);
	if (r2al_it == pit->second.end())
		return 0;

	return *r2al_it->second;
}

bool
TrackPattern::is_region_defined (int rowi) const
{
	return false;
}

int
TrackPattern::to_rrri (uint32_t rowi, size_t mri) const
{
	return (int)rowi;
}

int
TrackPattern::to_rrri (uint32_t rowi) const
{
	return (int)rowi;
}

int
TrackPattern::to_mri (uint32_t rowi) const
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
TrackPattern::get_automation_value (size_t rowi, size_t mri, const Evoral::Parameter& param)
{
	return AutomationPattern::get_automation_value (rowi, param);
}

void
TrackPattern::set_automation_value (double val, size_t rowi, size_t mri, const Evoral::Parameter& param, int delay)
{
	AutomationPattern::set_automation_value (val, rowi, param, delay);
}

void
TrackPattern::delete_automation_value (size_t rowi, size_t mri, const Evoral::Parameter& param)
{
	AutomationPattern::delete_automation_value (rowi, param);
}

std::pair<int, bool>
TrackPattern::get_automation_delay (size_t rowi, size_t mri, const Evoral::Parameter& param)
{
	return AutomationPattern::get_automation_delay (rowi, param);
}

void
TrackPattern::set_automation_delay (int delay, size_t rowi, size_t mri, const Evoral::Parameter& param)
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
