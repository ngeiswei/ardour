/*
    Copyleft (C) 2016 Nil Geisweiller

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

#ifndef __ardour_gtk2_pattern_h_
#define __ardour_gtk2_pattern_h_

#include "evoral/types.hpp"
#include "evoral/Beats.hpp"

#include "ardour/session_handle.h"
#include "ardour/beats_frames_converter.h"

#include "widgets/ardour_dropdown.h"
#include "ardour_window.h"
#include "editing.h"

namespace ARDOUR {
	class Region;
	class Session;
};

/**
 * Shared methods for storing and handling data for the midi, audio and
 * automation pattern editor.
 */
class Pattern {
public:
	Pattern(ARDOUR::Session* session,
	        boost::shared_ptr<ARDOUR::Region> region);

	// Set the number of rows per beat. 0 means 1 row per bar (TODO: not fully
	// supported). After changing that you probably need to update the pattern,
	// see below.
	void set_rows_per_beat(uint16_t rpb);

	// Build or rebuild the pattern
	virtual void update_pattern() = 0;

	// Find the beats corresponding to the first row
	Evoral::Beats find_start_row_beats();

	// Find the beats corresponding to the end row (not visible)
	Evoral::Beats find_end_row_beats();

	// Find the number of rows of the region
	uint32_t find_nrows();

	// Set start_row_beats, end_row_beats and nrows
	void set_row_range();

	// Return the frame at the corresponding row index and delay in relative
	// ticks
	framepos_t frame_at_row(uint32_t irow, int32_t delay=0);

	// Return the beats at the corresponding row index and delay in relative
	// ticks
	Evoral::Beats beats_at_row(uint32_t irow, int32_t delay=0);

	// Like beats_at_row but the beats is calculated in reference to the region
	Evoral::Beats region_relative_beats_at_row(uint32_t irow, int32_t delay=0);

	// Return the row index corresponding to the given beats, assuming the
	// minimum allowed delay is -_ticks_per_row/2 and the maximum allowed delay
	// is _ticks_per_row/2.
	uint32_t row_at_beats(Evoral::Beats beats);

	// Like row_at_beats but use frame instead of beats
	uint32_t row_at_frame(framepos_t frame);

	// Return the row index assuming the beats is allowed to have the minimum
	// negative delay (1 - _ticks_per_row).
	uint32_t row_at_beats_min_delay(Evoral::Beats beats);

	// Like row_at_beats_min_delay but use frame instead of beats
	uint32_t row_at_frame_min_delay(framepos_t frame);

	// Return the row index assuming the beats is allowed to have the maximum
	// positive delay (_ticks_per_row - 1).
	uint32_t row_at_beats_max_delay(Evoral::Beats beats);

	// Like row_at_beats_max_delay but use frame instead of beats
	uint32_t row_at_frame_max_delay(framepos_t frame);

	// Return an event's delay in a certain row in ticks
	int64_t delay_ticks(const Evoral::Beats& event_time, uint32_t irow);

	// Like delay_ticks above but uses frame instead of beats
	int64_t delay_ticks(framepos_t frame, uint32_t irow);

	// Like delay_ticks but the event_time is relative to the region position
	int64_t region_relative_delay_ticks(const Evoral::Beats& event_time, uint32_t irow);

	// Return the minimum and maximum number ticks allowed for delay
	int32_t delay_ticks_min() const;
	int32_t delay_ticks_max() const;

	// Beats corresponding to the region's start, end and length frames
	Evoral::Beats start_beats;
	Evoral::Beats end_beats;
	Evoral::Beats length_beats;

	// Number of rows per beat. 0 means one row per bar (TODO not fully
	// supported).
	uint8_t rows_per_beat;

	// Determined by the number of rows per beat
	Evoral::Beats beats_per_row;

	// Beats corresponding to the start and end row
	Evoral::Beats start_row_beats;
	Evoral::Beats end_row_beats;

	// Number of rows of that region (given the choosen resolution)
	uint32_t nrows;

private:
	uint32_t _ticks_per_row;		// number of ticks per rows
	ARDOUR::Session* _session;
	boost::shared_ptr<ARDOUR::Region> _region;
	ARDOUR::BeatsFramesConverter _conv;

	// Make sure a given row is clamped to be in [0, nrows)
	uint32_t clamp(double row) const;
};

#endif /* __ardour_gtk2_pattern_h_ */
