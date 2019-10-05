/*
 * Copyright (C) 2016-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include <cmath>
#include <map>

#include "ardour/beats_samples_converter.h"
#include "ardour/region.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "base_pattern.h"
#include "tracker_editor.h"

using namespace Tracker;

/////////////////
// BasePattern //
/////////////////

BasePattern::BasePattern (TrackerEditor& te,
                          boost::shared_ptr<ARDOUR::Region> region)
	: tracker_editor (te)
	, position (region->position ())
	, start (region->start ())
	, length (region->length ())
	, first_sample (region->first_sample ())
	, last_sample (region->last_sample ())
	, rows_per_beat (0)
	, nrows (0)
	, enabled (true)
	, _ticks_per_row (0)
	, _session (tracker_editor.session)
	, _conv (_session->tempo_map (), 0)
{
}

BasePattern::BasePattern (TrackerEditor& te,
                          Temporal::samplepos_t pos,
                          Temporal::samplepos_t sta,
                          Temporal::samplecnt_t len,
                          Temporal::samplepos_t fir,
                          Temporal::samplepos_t las)
	: tracker_editor (te)
	, position (pos)
	, start (sta)
	, length (len)
	, first_sample (fir)
	, last_sample (las)
	, rows_per_beat (0)
	, nrows (0)
	, enabled (true)
	, _ticks_per_row (0)
	, _session (tracker_editor.session)
	, _conv (tracker_editor.session->tempo_map (), 0)
{
}

BasePattern&
BasePattern::operator= (const BasePattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	position = other.position;
	start = other.start;
	length = other.length;
	first_sample = other.first_sample;
	last_sample = other.last_sample;

	position_beats = other.position_beats;
	start_beats = other.start_beats;
	end_beats = other.end_beats;
	length_beats = other.length_beats;

	rows_per_beat = other.rows_per_beat;

	beats_per_row = other.beats_per_row;

	position_row_beats = other.position_row_beats;
	end_row_beats = other.end_row_beats;

	nrows = other.nrows;

	_ticks_per_row = other._ticks_per_row;

	return *this;
}

bool
BasePattern::operator< (const BasePattern& other) const
{
	return position < other.position;
}

void
BasePattern::set_rows_per_beat (uint16_t rpb)
{
	rows_per_beat = rpb;
	// TODO: deal with rpb == 0 which would mean one row per bar
	beats_per_row = Temporal::Beats (1.0 / rows_per_beat);
	_ticks_per_row = Timecode::BBT_Time::ticks_per_beat/rows_per_beat;
}

Temporal::Beats
BasePattern::find_position_row_beats () const
{	
	return position_beats.snap_to (beats_per_row);
}

Temporal::Beats
BasePattern::find_end_row_beats () const
{
	return end_beats.snap_to (beats_per_row);
}

uint32_t
BasePattern::find_nrows () const
{
	return (end_row_beats - position_row_beats).to_double () * rows_per_beat;
}

void
BasePattern::set_row_range ()
{
	position_beats = _conv.from (position);
	start_beats = _conv.from (start);
	end_beats = _conv.from (last_sample + 1);
	length_beats = end_beats - position_beats;
	position_row_beats = find_position_row_beats ();
	end_row_beats = find_end_row_beats ();
	nrows = find_nrows ();
}

Temporal::samplepos_t
BasePattern::sample_at_row (uint32_t rowi, int32_t delay) const
{
	return _conv.to (beats_at_row (rowi, delay));
}

Temporal::Beats
BasePattern::beats_at_row (uint32_t rowi, int32_t delay) const
{
	Temporal::Beats result = position_row_beats + (rowi*1.0) / rows_per_beat;
	result += Temporal::Beats::ticks (delay);
	return result;
}

Temporal::Beats
BasePattern::region_relative_beats_at_row (uint32_t rowi, int32_t delay) const
{
	return beats_at_row (rowi, delay) - position_beats + start_beats;
}

uint32_t
BasePattern::row_at_beats (const Temporal::Beats& beats) const
{
	return clamp (row_distance (position_row_beats, beats));
}

double
BasePattern::row_distance (const Temporal::Beats& from, const Temporal::Beats& to) const
{
	Temporal::Beats half_row (0.5/rows_per_beat);
	return (to - from + half_row).to_double () * rows_per_beat;
}

uint32_t
BasePattern::row_at_sample (Temporal::samplepos_t sample) const
{
	return row_at_beats (_conv.from (sample));
}

uint32_t
BasePattern::row_at_beats_min_delay (const Temporal::Beats& beats) const
{
	Temporal::Beats tpr_minus_1 = Temporal::Beats::ticks (_ticks_per_row - 1);
	return clamp ((beats - position_row_beats + tpr_minus_1).to_double () * rows_per_beat);
}

uint32_t
BasePattern::row_at_sample_min_delay (Temporal::samplepos_t sample) const
{
	return row_at_beats_min_delay (_conv.from (sample));
}

uint32_t
BasePattern::row_at_beats_max_delay (const Temporal::Beats& beats) const
{
	return clamp ((beats - position_row_beats).to_double () * rows_per_beat);
}

uint32_t
BasePattern::row_at_sample_max_delay (Temporal::samplepos_t sample) const
{
	return row_at_beats_max_delay (_conv.from (sample));
}

int64_t
BasePattern::delay_ticks_at_row (const Temporal::Beats& event_time, uint32_t rowi) const
{
	return (event_time - beats_at_row (rowi)).to_ticks ();
}

int64_t
BasePattern::delay_ticks_at_row (Temporal::samplepos_t sample, uint32_t rowi) const
{
	return delay_ticks_at_row (_conv.from (sample), rowi);
}

int64_t
BasePattern::region_relative_delay_ticks_at_row (const Temporal::Beats& event_time, uint32_t rowi) const
{
	return delay_ticks_at_row (event_time + position_beats - start_beats, rowi);
}

int32_t
BasePattern::delay_ticks_min () const
{
	return 1 - (int32_t)_ticks_per_row;
}

int32_t
BasePattern::delay_ticks_max () const
{
	return (int32_t)_ticks_per_row - 1;
}

bool
BasePattern::is_defined (int row_idx) const
{
	return 0 <= row_idx && row_idx < (int)nrows;
}

uint32_t
BasePattern::clamp (double row) const
{
	return std::min (std::max (0.0, row), nrows - 1.0);
}

std::string
BasePattern::self_to_string () const
{
	std::stringstream ss;
	ss << "BasePattern[" << this << "]";
	return ss.str ();
}

std::string
BasePattern::to_string (const std::string& indent) const
{
	std::string header = indent + self_to_string () + " ";
	std::stringstream ss;
	ss << header << "position = " << position << std::endl;
	ss << header << "start = " << start << std::endl;
	ss << header << "length = " << length << std::endl;
	ss << header << "first_sample = " << first_sample << std::endl;
	ss << header << "last_sample = " << last_sample << std::endl;
	ss << header << "position_beats = " << position_beats << std::endl;
	ss << header << "start_beats = " << start_beats << std::endl;
	ss << header << "end_beats = " << end_beats << std::endl;
	ss << header << "length_beats = " << length_beats << std::endl;
	ss << header << "rows_per_beat = " << (int)rows_per_beat << std::endl;
	ss << header << "beats_per_row = " << beats_per_row << std::endl;
	ss << header << "position_row_beats = " << position_row_beats << std::endl;
	ss << header << "end_row_beats = " << end_row_beats << std::endl;
	ss << header << "nrows = " << nrows << std::endl;
	ss << header << "enabled = " << enabled << std::endl;
	ss << header << "_ticks_per_row = " << _ticks_per_row;

	return ss.str ();
}
