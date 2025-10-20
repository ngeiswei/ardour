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
                          RegionPtr region)
	: tracker_editor (te)
	, position (region->position ())
	, start (region->start ())
	, length (region->length ())
	, end (region->end ())
	, nt_last (region->nt_last ())
	, rows_per_beat (0)
	, nrows (0)
	, enabled (true)
	, selected (false)
	, _ticks_per_row (0)
	, _session (tracker_editor.session)
{
}

BasePattern::BasePattern (TrackerEditor& te,
                          Temporal::timepos_t pos,
                          Temporal::timepos_t sta,
                          Temporal::timecnt_t len,
                          Temporal::timepos_t ed,
                          Temporal::timepos_t ntl)
	: tracker_editor (te)
	, position (pos)
	, start (sta)
	, length (len)
	, end (ed)
	, nt_last (ntl)
	, rows_per_beat (0)
	, nrows (0)
	, enabled (true)
	, _ticks_per_row (0)
	, _session (tracker_editor.session)
{
}

BasePattern::~BasePattern ()
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
	start = other.position;
	length = other.length;
	end = other.end;
	nt_last = other.nt_last;

	position_beats = other.position_beats;
	global_end_beats = other.global_end_beats;
	start_beats = other.start_beats;
	end_beats = other.end_beats;
	length_beats = other.length_beats;

	rows_per_beat = other.rows_per_beat;

	beats_per_row = other.beats_per_row;

	position_row_beats = other.position_row_beats;
	end_row_beats = other.end_row_beats;

	nrows = other.nrows;

	enabled = other.enabled;

	_ticks_per_row = other._ticks_per_row;

	return *this;
}

bool
BasePattern::operator< (const BasePattern& other) const
{
	return position < other.position;
}

bool
BasePattern::row_lt (int row1, int row2) const
{
	if (row1 != INVALID_ROW and row2 != INVALID_ROW) {
		return row1 < row2;
	}
	return true;
}

bool
BasePattern::row_gte (int row1, int row2) const
{
	if (row1 != INVALID_ROW and row2 != INVALID_ROW) {
		return row1 >= row2;
	}
	return true;
}

void
BasePattern::repair_ranked_row (int ranked_row[3]) const
{
	if (ranked_row[1] < 0 && ranked_row[2] >= 0) {
		ranked_row[1] = ranked_row[2];
		ranked_row[2] = INVALID_ROW;
	}
	if (ranked_row[0] < 0 && ranked_row[1] >= 0) {
		ranked_row[0] = ranked_row[1];
		ranked_row[1] = INVALID_ROW;
	}
}

void
BasePattern::set_rows_per_beat (uint16_t rpb)
{
	rows_per_beat = rpb;
	// TODO: deal with rpb == 0 which would mean one row per bar
	beats_per_row = Temporal::Beats::from_double (1.0 / rows_per_beat);
	_ticks_per_row = Temporal::ticks_per_beat / rows_per_beat;
}

Temporal::Beats
BasePattern::find_position_row_beats () const
{
	return position_beats.round_to_multiple (beats_per_row);
}

Temporal::Beats
BasePattern::find_end_row_beats () const
{
	// TODO: it could be round_up_to_multiple or round_down_to_multiple
	return global_end_beats.round_to_multiple (beats_per_row);
}

int
BasePattern::find_nrows () const
{
	return Temporal::DoubleableBeats (end_row_beats - position_row_beats).to_double () * rows_per_beat;
}

void
BasePattern::set_row_range ()
{
	position_beats = position.beats ();
	global_end_beats = end.beats ();
	length_beats = global_end_beats - position_beats;
	start_beats = start.beats ();
	end_beats = start_beats + length_beats;
	position_row_beats = find_position_row_beats ();
	end_row_beats = find_end_row_beats ();
	nrows = find_nrows ();
}

Temporal::samplepos_t
BasePattern::sample_at_row (int rowi, int delay) const
{
	return Temporal::timepos_t (beats_at_row (rowi, delay)).samples ();
}

Temporal::Beats
BasePattern::beats_at_row (int rowi, int delay) const
{
	Temporal::Beats result = position_row_beats + Temporal::Beats::from_double((rowi*1.0) / rows_per_beat);
	result += Temporal::Beats::ticks (delay);
	return result;
}

Temporal::BBT_Time
BasePattern::bbt_at_row (int rowi, int delay) const
{
	Temporal::Beats beats = beats_at_row (rowi, delay);
	return Temporal::TempoMap::use()->bbt_at (beats);
}

Temporal::Beats
BasePattern::region_relative_beats_at_row (int rowi, int delay) const
{
	return beats_at_row (rowi, delay) - position_beats + start_beats;
}

int
BasePattern::row_at_beats (const Temporal::Beats& beats) const
{
	if (position_row_beats <= beats and beats <= end_row_beats) {
		return row_distance (position_row_beats, beats);
	}
	return INVALID_ROW;
}

double
BasePattern::row_distance (const Temporal::Beats& from, const Temporal::Beats& to) const
{
	Temporal::Beats half_row = Temporal::Beats::from_double(0.5 / rows_per_beat);
	return Temporal::DoubleableBeats (to - from + half_row).to_double () * rows_per_beat;
}

int
BasePattern::row_at_time (Temporal::timepos_t pos) const
{
	return row_at_beats (pos.beats ());
}

int
BasePattern::row_at_beats_min_delay (const Temporal::Beats& beats) const
{
	if (position_row_beats <= beats and beats <= end_row_beats) {
		Temporal::Beats tpr_minus_1 = Temporal::Beats::ticks (_ticks_per_row - 1);
		return Temporal::DoubleableBeats (beats - position_row_beats + tpr_minus_1).to_double () * rows_per_beat;
	}
	return INVALID_ROW;
}

int
BasePattern::row_at_time_min_delay (Temporal::timepos_t pos) const
{
	return row_at_beats_min_delay (pos.beats ());
}

int
BasePattern::row_at_beats_max_delay (const Temporal::Beats& beats) const
{
	if (position_row_beats <= beats and beats <= end_row_beats) {
		return Temporal::DoubleableBeats(beats - position_row_beats).to_double () * rows_per_beat;
	}
	return INVALID_ROW;
}

int
BasePattern::row_at_time_max_delay (Temporal::timepos_t pos) const
{
	return row_at_beats_max_delay (pos.beats ());
}

int64_t
BasePattern::delay_ticks_at_row (const Temporal::Beats& event_time, int rowi) const
{
	return (event_time - beats_at_row (rowi)).to_ticks ();
}

int64_t
BasePattern::delay_ticks_at_row (Temporal::samplepos_t sample, int rowi) const
{
	return delay_ticks_at_row (Temporal::timepos_t (sample).beats (), rowi);
}

int64_t
BasePattern::region_relative_delay_ticks_at_row (const Temporal::Beats& event_time, int rowi) const
{
	return delay_ticks_at_row (event_time + position_beats - start_beats, rowi);
}

int
BasePattern::delay_ticks_min () const
{
	return 1 - _ticks_per_row;
}

int
BasePattern::delay_ticks_max () const
{
	return _ticks_per_row - 1;
}

bool
BasePattern::is_defined (int row_idx) const
{
	return enabled && 0 <= row_idx && row_idx < nrows;
}

void
BasePattern::set_enabled (bool e)
{
	enabled = e;
}

void
BasePattern::set_selected (bool s)
{
	selected = s;
}

int
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
	ss << header << "end = " << end << std::endl;
	ss << header << "nt_last = " << nt_last << std::endl;
	ss << header << "position_beats = " << position_beats << std::endl;
	ss << header << "global_end_beats = " << global_end_beats << std::endl;
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
