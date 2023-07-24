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

#ifndef __ardour_tracker_base_pattern_h_
#define __ardour_tracker_base_pattern_h_

#include "temporal/beats.h"
#include "temporal/types.h"

#include "widgets/ardour_dropdown.h"

#include "ardour/session_handle.h"

#include "ardour_window.h"
#include "editing.h"

#include "tracker_utils.h"

namespace ARDOUR {
	class Region;
	class Session;
};

namespace Tracker {

class TrackerEditor;

/**
 * Shared methods for storing and handling data for the midi, audio and
 * automation pattern editor.
 */

class BasePattern {
public:
	BasePattern (TrackerEditor& te,
	             RegionPtr region);
	BasePattern (TrackerEditor& te,
	             Temporal::timepos_t position,
	             Temporal::timepos_t start, // TODO: maybe should only be present for RegionPattern
	             Temporal::timecnt_t length,
	             Temporal::timepos_t end,
	             Temporal::timepos_t nt_last);
	virtual ~BasePattern ();

	// A negative row index (i.e. max value) is considered invalid
	static const int INVALID_ROW = -1;

	// Phenomenal overload of operator= (), only need to copy what is necessary
	// for phenomenal_diff to correctly operate.
	BasePattern& operator= (const BasePattern& other);

	// Return true iff this->position < other.position
	// WARNING: not a total order.
	bool operator< (const BasePattern& other) const;

	// Set the number of rows per beat. 0 means 1 row per bar (TODO: not fully
	// supported). After changing that you probably need to update the pattern,
	// i.e. call update ().
	//
	// It is virtual because sophisticated inherited pattern classes
	// may want to overwrite it to trickle down set_rows_per_beat to
	// their subpatterns.
	virtual void set_rows_per_beat (uint16_t rpb);

	// Build or rebuild the pattern
	virtual void update () = 0;

	// Find the beats corresponding to the first row
	Temporal::Beats find_position_row_beats () const;

	// Find the beats corresponding to the end row (not visible)
	Temporal::Beats find_end_row_beats () const;

	// Find the number of rows of the region.  Requires rows_per_beat
	// to be set (which can be done with set_rows_per_beat).
	int find_nrows () const;

	// Set position_row_beats, end_row_beats and nrows.  Requires
	// rows_per_beat to be set (which can be done with
	// set_rows_per_beat).
	virtual void set_row_range ();

	// Return the sample at the corresponding row index and delay in relative
	// ticks
	Temporal::samplepos_t sample_at_row (int rowi, int delay=0) const;

	// Return the beats at the given row index and delay in relative ticks
	Temporal::Beats beats_at_row (int rowi, int delay=0) const;

	// Return BBT (bar beat tick) at the given row index and delay in relative ticks
	Temporal::BBT_Time bbt_at_row (int rowi, int delay=0) const;

	// Like beats_at_row but the beats is calculated in reference to the
	// region's position
	Temporal::Beats region_relative_beats_at_row (int rowi, int delay=0) const;

	// Return the row index corresponding to the given beats, assuming the
	// minimum allowed delay is -_ticks_per_row/2 and the maximum allowed delay
	// is _ticks_per_row/2.
	int row_at_beats (const Temporal::Beats& beats) const;

	// Return the number rows from contained between 2 beats. Return double just
	// in case fraction of rows are used.
	double row_distance (const Temporal::Beats& from, const Temporal::Beats& to) const;

	// Like row_at_beats but use timepos_t instead of Beats
	int row_at_time (Temporal::timepos_t pos) const;

	// Return the row index assuming the beats is allowed to have the minimum
	// negative delay (1 - _ticks_per_row).
	int row_at_beats_min_delay (const Temporal::Beats& beats) const;

	// Like row_at_beats_min_delay but use timepos_t instead of beats
	int row_at_time_min_delay (Temporal::timepos_t pos) const;

	// Return the row index assuming the beats is allowed to have the maximum
	// positive delay (_ticks_per_row - 1).
	int row_at_beats_max_delay (const Temporal::Beats& beats) const;

	// Like row_at_beats_max_delay but use timepos_t instead of beats
	int row_at_time_max_delay (Temporal::timepos_t pos) const;

	// Return an event's delay in a certain row in ticks
	int64_t delay_ticks_at_row (const Temporal::Beats& event_time, int rowi) const;

	// Like delay_ticks above but uses sample instead of beats
	int64_t delay_ticks_at_row (Temporal::samplepos_t sample, int rowi) const;

	// Like delay_ticks but the event_time is relative to the region position
	int64_t region_relative_delay_ticks_at_row (const Temporal::Beats& event_time, int rowi) const;

	// Return the minimum and maximum number ticks allowed for delay
	int delay_ticks_min () const;
	int delay_ticks_max () const;

	// Return true iff the row index is within the defined range of
	// rows. Specifially between 0 and nrow-1
	virtual bool is_defined (int row_idx) const;

	// Enable/disable pattern
	virtual void set_enabled (bool e);

	// Select/deselect pattern
	virtual void set_selected (bool s);

	// For representing pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	// Reference to main tracker editor
	TrackerEditor& tracker_editor;

	// Time position corresponding to typical region
	Temporal::timepos_t position;
	Temporal::timepos_t start;
	Temporal::timecnt_t length;
	Temporal::timepos_t end;
	Temporal::timepos_t nt_last;

	// Beats of the region's position relative to ardour time line, and end
	// relative to ardour time line as well, thus called global.
	Temporal::Beats position_beats;
	Temporal::Beats global_end_beats;

	// Start and end beats relative to the source.
	Temporal::Beats start_beats;
	Temporal::Beats end_beats;

	// Length in beats of the visible region.
	Temporal::Beats length_beats;

	// Number of rows per beat. 0 means one row per bar (TODO not fully
	// supported).
	uint8_t rows_per_beat;

	// Determined by the number of rows per beat
	Temporal::Beats beats_per_row;

	// Beats corresponding to the absolute position and end row. Note that end
	// is absolute, not relative to source, unlike end_beats.
	Temporal::Beats position_row_beats;
	Temporal::Beats end_row_beats;

	// Number of rows of that region (given the choosen resolution)
	int nrows;

	// If pattern disabled, do not display or update it
	bool enabled;

	// Whether the pattern is selected for display
	bool selected;

protected:
	// number of ticks per rows
	int _ticks_per_row;

	ARDOUR::Session* _session;

	// Make sure a given row is clamped to be in [0, nrows)
	int clamp (double row) const;
};

} // ~namespace tracker

#endif /* __ardour_tracker_base_pattern_h_ */
