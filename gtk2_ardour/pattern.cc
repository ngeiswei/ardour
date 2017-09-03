/*
    Copyright (C) 2016 Nil Geisweiller

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

#include <cmath>
#include <map>

#include "ardour/beats_frames_converter.h"
#include "ardour/session.h"
#include "ardour/region.h"
#include "ardour/tempo.h"

#include "pattern.h"

using namespace std;
using namespace ARDOUR;
using Timecode::BBT_Time;

/////////////
// Pattern //
/////////////

Pattern::Pattern(ARDOUR::Session* session,
                 boost::shared_ptr<ARDOUR::Region> region)
	: _session(session), _region(region),
	  _conv(_session->tempo_map(), _region->position())
{
	start_beats = _conv.from (_region->first_frame());
	end_beats = _conv.from (_region->last_frame() + 1);
}

void Pattern::set_rows_per_beat(uint16_t rpb)
{
	rows_per_beat = rpb;
	// TODO: deal with rpb == 0 which would mean one row per bar
	beats_per_row = Evoral::Beats(1.0 / rows_per_beat);
	_ticks_per_row = BBT_Time::ticks_per_beat/rows_per_beat;
}

Evoral::Beats Pattern::find_start_row_beats()
{	
	return start_beats.snap_to (beats_per_row);
}

Evoral::Beats Pattern::find_end_row_beats()
{
	return end_beats.snap_to (beats_per_row);
}

uint32_t Pattern::find_nrows()
{
	return (end_row_beats - start_row_beats).to_double() * rows_per_beat;
}

void Pattern::set_row_range()
{
	start_row_beats = find_start_row_beats();
	end_row_beats = find_end_row_beats();
	nrows = find_nrows();
}

framepos_t Pattern::frame_at_row(uint32_t irow, int32_t delay)
{
	return _conv.to (beats_at_row(irow, delay));
}

Evoral::Beats Pattern::beats_at_row(uint32_t irow, int32_t delay)
{
	Evoral::Beats result = start_row_beats + (irow*1.0) / rows_per_beat;
	result += Evoral::Beats::relative_ticks(delay);
	return result;
}

Evoral::Beats Pattern::region_relative_beats_at_row(uint32_t irow, int32_t delay)
{
	return beats_at_row(irow, delay) - start_beats;
}

uint32_t Pattern::row_at_beats(Evoral::Beats beats)
{
	Evoral::Beats half_row(0.5/rows_per_beat);
	return clamp((beats - start_row_beats + half_row).to_double() * rows_per_beat);
}

uint32_t Pattern::row_at_frame(framepos_t frame)
{
	return row_at_beats (_conv.from (frame));
}

uint32_t Pattern::row_at_beats_min_delay(Evoral::Beats beats)
{
	Evoral::Beats tpr_minus_1 = Evoral::Beats::ticks(_ticks_per_row - 1);
	return clamp((beats - start_row_beats + tpr_minus_1).to_double() * rows_per_beat);
}

uint32_t Pattern::row_at_frame_min_delay(framepos_t frame)
{
	return row_at_beats_min_delay(_conv.from (frame));
}

uint32_t Pattern::row_at_beats_max_delay(Evoral::Beats beats)
{
	return clamp((beats - start_row_beats).to_double() * rows_per_beat);
}

uint32_t Pattern::row_at_frame_max_delay(framepos_t frame)
{
	return row_at_beats_max_delay (_conv.from (frame));
}

int64_t Pattern::delay_ticks(const Evoral::Beats& event_time, uint32_t irow)
{
	return (event_time - beats_at_row(irow)).to_relative_ticks();
}

int64_t Pattern::delay_ticks(framepos_t frame, uint32_t irow)
{
	return delay_ticks(_conv.from (frame), irow);
}

int64_t Pattern::region_relative_delay_ticks(const Evoral::Beats& event_time, uint32_t irow)
{
	return delay_ticks(event_time + start_beats, irow);
}

int32_t Pattern::delay_ticks_min() const
{
	return 1 - (int32_t)_ticks_per_row;
}

int32_t Pattern::delay_ticks_max() const
{
	return (int32_t)_ticks_per_row - 1;
}

uint32_t Pattern::clamp(double row) const
{
	return std::min(std::max(0.0, row), nrows - 1.0);
}
