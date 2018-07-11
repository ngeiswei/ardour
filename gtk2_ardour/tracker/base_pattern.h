/*
    Copyright (C) 2016-2017 Nil Geisweiller

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

#ifndef __ardour_tracker_base_pattern_h_
#define __ardour_tracker_base_pattern_h_

#include "temporal/beats.h"
#include "temporal/types.h"

#include "ardour/session_handle.h"
#include "ardour/beats_samples_converter.h"

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

class BasePattern {
public:
	BasePattern(ARDOUR::Session* session,
	            boost::shared_ptr<ARDOUR::Region> region);

	static const uint32_t UNDEFINED_ROW = -1;

	// Set the number of rows per beat. 0 means 1 row per bar (TODO: not fully
	// supported). After changing that you probably need to update the pattern,
	// see below.
	void set_rows_per_beat(uint16_t rpb);

	// Build or rebuild the pattern
	virtual void update() = 0;

	// Find the beats corresponding to the first row
	Temporal::Beats find_position_row_beats() const;

	// Find the beats corresponding to the end row (not visible)
	Temporal::Beats find_end_row_beats() const;

	// Find the number of rows of the region
	uint32_t find_nrows() const;

	// Set position_row_beats, end_row_beats and nrows
	void set_row_range();

	// Return the sample at the corresponding row index and delay in relative
	// ticks
	samplepos_t sample_at_row(uint32_t rowi, int32_t delay=0) const;

	// Return the beats at the corresponding row index and delay in relative
	// ticks
	Temporal::Beats beats_at_row (uint32_t rowi, int32_t delay=0) const;

	// Like beats_at_row but the beats is calculated in reference to the
	// region's position
	Temporal::Beats region_relative_beats_at_row (uint32_t rowi, int32_t delay=0) const;

	// Return the row index corresponding to the given beats, assuming the
	// minimum allowed delay is -_ticks_per_row/2 and the maximum allowed delay
	// is _ticks_per_row/2.
	uint32_t row_at_beats(const Temporal::Beats& beats) const;

	// Return the number rows from contained between 2 beats. Return double just
	// in case fraction of rows are used.
	double row_distance(const Temporal::Beats& from, const Temporal::Beats& to) const;

	// Like row_at_beats but use sample instead of beats
	uint32_t row_at_sample(samplepos_t sample) const;

	// Return the row index assuming the beats is allowed to have the minimum
	// negative delay (1 - _ticks_per_row).
	uint32_t row_at_beats_min_delay(const Temporal::Beats& beats) const;

	// Like row_at_beats_min_delay but use sample instead of beats
	uint32_t row_at_sample_min_delay(samplepos_t sample) const;

	// Return the row index assuming the beats is allowed to have the maximum
	// positive delay (_ticks_per_row - 1).
	uint32_t row_at_beats_max_delay(const Temporal::Beats& beats) const;

	// Like row_at_beats_max_delay but use sample instead of beats
	uint32_t row_at_sample_max_delay(samplepos_t sample) const;

	// Return an event's delay in a certain row in ticks
	int64_t delay_ticks(const Temporal::Beats& event_time, uint32_t rowi) const;

	// Like delay_ticks above but uses sample instead of beats
	int64_t delay_ticks(samplepos_t sample, uint32_t rowi) const;

	// Like delay_ticks but the event_time is relative to the region position
	int64_t region_relative_delay_ticks(const Temporal::Beats& event_time, uint32_t rowi) const;

	// Return the minimum and maximum number ticks allowed for delay
	int32_t delay_ticks_min() const;
	int32_t delay_ticks_max() const;

	// Beats corresponding to the region's position, start from the source, end
	// and length in beats.
	Temporal::Beats position_beats;
	Temporal::Beats start_beats;
	Temporal::Beats end_beats;
	Temporal::Beats length_beats;

	// Number of rows per beat. 0 means one row per bar (TODO not fully
	// supported).
	uint8_t rows_per_beat;

	// Determined by the number of rows per beat
	Temporal::Beats beats_per_row;

	// Beats corresponding to the location and end row
	Temporal::Beats position_row_beats;
	Temporal::Beats end_row_beats;

	// Number of rows of that region (given the choosen resolution)
	uint32_t nrows;

protected:
	uint32_t _ticks_per_row;		// number of ticks per rows
	ARDOUR::Session* _session;
	boost::shared_ptr<ARDOUR::Region> _region;
	ARDOUR::BeatsSamplesConverter _conv;

	// Make sure a given row is clamped to be in [0, nrows)
	uint32_t clamp(double row) const;
};

#endif /* __ardour_tracker_base_pattern_h_ */
