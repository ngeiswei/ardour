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

TrackPattern::TrackPattern (TrackerEditor& te,
                            TrackPtr trk,
                            Temporal::timepos_t pos,
                            Temporal::timecnt_t len,
                            Temporal::timepos_t ed,
                            Temporal::timepos_t ntl,
                            bool connect)
	: BasePattern (te, pos, Temporal::timepos_t (), len, ed, ntl)
	, track (trk)
	, track_all_automations_pattern (te, trk, pos, len, ed, ntl, connect)
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

	track_all_automations_pattern.operator= (other.track_all_automations_pattern);
	track = other.track;

	return *this;
}

MidiTrackPtr
TrackPattern::midi_track ()
{
	return std::dynamic_pointer_cast<ARDOUR::MidiTrack> (track);
}

AudioTrackPtr
TrackPattern::audio_track ()
{
	return std::dynamic_pointer_cast<ARDOUR::AudioTrack> (track);
}

void
TrackPattern::set_rows_per_beat (uint16_t rpb)
{
	BasePattern::set_rows_per_beat (rpb);
	track_all_automations_pattern.set_rows_per_beat (rpb);
}

void
TrackPattern::set_row_range ()
{
	BasePattern::set_row_range ();
	track_all_automations_pattern.set_row_range ();
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

void
TrackPattern::update ()
{
	track_all_automations_pattern.update ();
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
TrackPattern::is_automation_displayable (int rowi, int mri, const IDParameter& id_param) const
{
	return track_all_automations_pattern.is_displayable (rowi, id_param);
}

size_t
TrackPattern::control_events_count (int rowi, int mri, const IDParameter& id_param) const
{
	return track_all_automations_pattern.control_events_count (rowi, id_param);
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

void
TrackPattern::insert (const IDParameter& id_param)
{
	track_all_automations_pattern.insert (id_param);
}

bool
TrackPattern::is_empty (const IDParameter& id_param) const
{
	return track_all_automations_pattern.is_empty (id_param);
}

std::vector<Temporal::BBT_Time>
TrackPattern::get_automation_bbt_seq (int rowi, int mri, const IDParameter& id_param) const
{
	return track_all_automations_pattern.get_automation_bbt_seq (rowi, id_param);
}

std::pair<double, bool>
TrackPattern::get_automation_value (int rowi, int mri, const IDParameter& id_param) const
{
	return track_all_automations_pattern.get_automation_value (rowi, id_param);
}

std::vector<double>
TrackPattern::get_automation_value_seq (int rowi, int mri, const IDParameter& id_param) const
{
	return track_all_automations_pattern.get_automation_value_seq (rowi, id_param);
}

double
TrackPattern::get_automation_interpolation_value (int rowi, int mri, const IDParameter& id_param) const
{
	return track_all_automations_pattern.get_automation_interpolation_value (rowi, id_param);
}

void
TrackPattern::set_automation_value (double val, int rowi, int mri, const IDParameter& id_param, int delay)
{
	track_all_automations_pattern.set_automation_value (val, rowi, id_param, delay);
}

void
TrackPattern::delete_automation_value (int rowi, int mri, const IDParameter& id_param)
{
	track_all_automations_pattern.delete_automation_value (rowi, id_param);
}

std::pair<int, bool>
TrackPattern::get_automation_delay (int rowi, int mri, const IDParameter& id_param) const
{
	return track_all_automations_pattern.get_automation_delay (rowi, id_param);
}

std::vector<int>
TrackPattern::get_automation_delay_seq (int rowi, int mri, const IDParameter& id_param) const
{
	return track_all_automations_pattern.get_automation_delay_seq (rowi, id_param);
}

void
TrackPattern::set_automation_delay (int delay, int rowi, int mri, const IDParameter& id_param)
{
	track_all_automations_pattern.set_automation_delay (delay, rowi, id_param);
}

std::string
TrackPattern::get_name (const IDParameter& id_param) const
{
	return track_all_automations_pattern.get_name (id_param);
}

void
TrackPattern::set_param_enabled (const IDParameter& id_param, bool enabled)
{
	track_all_automations_pattern.set_param_enabled (id_param, enabled);
}

bool
TrackPattern::is_param_enabled (const IDParameter& id_param) const
{
	return track_all_automations_pattern.is_param_enabled (id_param);
}

IDParameterSet
TrackPattern::get_enabled_parameters (int mri) const
{
	return track_all_automations_pattern.get_enabled_parameters ();
}

double
TrackPattern::lower (int rowi, const IDParameter& id_param) const
{
	return track_all_automations_pattern.lower (id_param);
}

double
TrackPattern::upper (int rowi, const IDParameter& id_param) const
{
	return track_all_automations_pattern.upper (id_param);
}

std::string
TrackPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "TrackPattern[" << this << "]";
	return ss.str ();
}

std::string
TrackPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePattern::to_string (indent);

	std::string header = indent + self_to_string () + " ";
	std::string indent2 = indent + "  ";

	// Print track pointer address
	ss << std::endl << header << "track = " << track;

	// Print track automation pattern
	ss << std::endl << header << "track_automation_pattern:";
	ss << std::endl << track_all_automations_pattern.to_string(indent2);

	return ss.str ();
}
