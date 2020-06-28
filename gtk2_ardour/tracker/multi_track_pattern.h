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

#ifndef __ardour_tracker_multi_track_pattern_h_
#define __ardour_tracker_multi_track_pattern_h_

#include "ardour/automation_list.h"
#include "ardour/midi_model.h"
#include "ardour/stripable.h"
#include "ardour/track.h"

#include "region_view.h"

#include "midi_region_pattern.h"
#include "multi_track_pattern_phenomenal_diff.h"
#include "note_pattern.h"
#include "track_pattern.h"
#include "tracker_utils.h"

namespace Tracker {

class TrackerEditor;

/**
 * Represent patterns of multiple tracks.
 */
class MultiTrackPattern : public BasePattern
{
public:
	// Create patterns for selected regions. If connect is true then connect
	// them to various signals to trigger grid redisplay.
	MultiTrackPattern (TrackerEditor& te, bool connect);
	~MultiTrackPattern ();

	// Phenomenal overload of operator= (), only need to copy what is necessary
	// for phenomenal_diff to correctly operate.
	MultiTrackPattern& operator= (const MultiTrackPattern& other);

	MultiTrackPatternPhenomenalDiff phenomenal_diff (const MultiTrackPattern& prev) const;

	void setup ();
	void setup_positions ();
	void setup_region_views_per_track ();
	void setup_regions_per_track ();
	void setup_track_patterns ();
	void add_track_pattern (TrackPtr, const RegionSeq&);
	void setup_row_offset ();

	void update ();
	void update_position_etc ();
	void update_rows_per_beat ();
	void update_content ();
	void update_earliest_mtp ();  // earliest midi track
	void update_global_nrows ();  // row_offset, nrows, global_nrows

	// Return true iff the row at mti belongs to a region
	bool is_region_defined (uint32_t rowi, size_t mti) const;

	// Return true iff the row at mti is defined for param
	// Warning: if param is undefined, then the behavior is unknown
	bool is_automation_defined (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const;

	// Like beats_at_row but the beats is calculated in reference to the
	// region's position
	Temporal::Beats region_relative_beats (uint32_t rowi, size_t mti, int mri, int32_t delay=0) const;

	// Like delay_ticks but the event_time is relative to the region position of a given mti
	int64_t region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mti, int mri) const;

	// Return an event's delay in a certain row and mti in ticks
	int64_t delay_ticks (samplepos_t when, uint32_t rowi, size_t mti) const;
	
	// Return the sample at the corresponding row index and delay in relative
	// ticks
	uint32_t sample_at_row_at_mti (uint32_t rowi, size_t mti, int32_t delay=0) const;

	size_t off_notes_count (uint32_t rowi, size_t mti, int mri, size_t cgi) const;
	size_t on_notes_count (uint32_t rowi, size_t mti, int mri, size_t cgi) const;
	bool is_note_displayable (uint32_t rowi, size_t mti, int mri, size_t cgi) const;
	NotePtr off_note (uint32_t rowi, size_t mti, int mri, size_t cgi) const;
	NotePtr on_note (uint32_t rowi, size_t mti, int mri, size_t cgi) const;

	bool is_auto_displayable (uint32_t rowi, size_t mti, int mri, const Evoral::Parameter& param) const;

	typedef ARDOUR::AutomationList::iterator AutomationListIt;
	size_t get_automation_list_count (uint32_t rowi, size_t mti, int mri, const Evoral::Parameter& param) const;
	Evoral::ControlEvent* get_automation_control_event (uint32_t rowi, size_t mti, int mri, const Evoral::Parameter& param) const;

	NotePtr find_prev_on_note (uint32_t rowi, size_t mti, int mri, int cgi) const;
	NotePtr find_next_on_note (uint32_t rowi, size_t mti, int mri, int cgi) const;

	// Return the Beats of the next note on (resp. off) or the end of the region
	// if none
	Temporal::Beats next_on_note (uint32_t rowi, size_t mti, int mri, int cgi) const;
	Temporal::Beats next_off_note (uint32_t rowi, size_t mti, int mri, int cgi) const;

	// Return the row index relative to the start of pattern at mti.
	int to_rri (uint32_t rowi, size_t mti) const;

	// Return the row index relative to the region pattern at mri of mti track
	int to_rrri (uint32_t rowi, size_t mti, int mri) const;
	int to_rrri (uint32_t rowi, size_t mti) const;

	// Given the row index, calculate the corresponding midi region index. This
	// can only work assuming that regions do not overlap in time. If no such
	// mri is defined, then return -1.
	int to_mri (uint32_t rowi, size_t mti) const;

	// Insert the automation control (s) associated to param at mti (and connect
	// it to the grid for changes)
	void insert (size_t mti, const Evoral::Parameter& param);

	// Return the midi model at mti and mri
	MidiModelPtr midi_model (size_t mti, int mri);

	// Return the midi region at mti and mri
	MidiRegionPtr midi_region (size_t mti, int mri);
	
	// Return the note pattern at mti and mri
	NotePattern& note_pattern (size_t mti, int mri);

	// Return the midi region pattern at mti and mri
	MidiRegionPattern& midi_region_pattern (size_t mti, int mri);
	const MidiRegionPattern& midi_region_pattern (size_t mti, int mri) const;

	// Apply given command at mti
	void apply_command (size_t mti, int mri, ARDOUR::MidiModel::NoteDiffCommand* cmd);

	AutomationListPtr get_alist (int mti, int mri, const Evoral::Parameter& param);
	const AutomationListPtr get_alist (int mti, int mri, const Evoral::Parameter& param) const;
	
	// Return a pair with the automation value and whether it is defined or not
	std::pair<double, bool> get_automation_value (size_t rowi, size_t mti, int mri, const Evoral::Parameter& param) const;

	// Set the automation value val at rowi, mti and mri for param
	void set_automation_value (double val, int rowi, int mti, int mri, const Evoral::Parameter& param, int delay);

	// Delete automation value at rowi, mti and mri for param
	void delete_automation_value (int rowi, int mti, int mri, const Evoral::Parameter& param);

	// Get a pair with automation delay in tick at rowi of param as first
	// element and whether it is defined as second element. Return (0, false) if
	// undefined.
	std::pair<int, bool> get_automation_delay (size_t rowi, size_t mti, int mri, const Evoral::Parameter& param) const;

	// Set the automation delay in tick at rowi, mri and mri for param
	void set_automation_delay (int delay, size_t rowi, size_t mti, int mri, const Evoral::Parameter& param);

	// Return the track pattern assicuated to track, or 0 if it doesn't exist
	TrackPattern* find_track_pattern (TrackPtr track);

	// Enable/disable pattern and all its track patterns
	void set_enabled (bool e);

	// Select/deselect all track patterns
	void set_selected (bool s);

	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	// Mapping track to region views
	typedef std::map<TrackPtr, std::vector<RegionView*>, ARDOUR::Stripable::Sorter> TrackRegionViewsMap;
	TrackRegionViewsMap region_views_per_track;
	
	// Mapping track to regions
	typedef std::map<TrackPtr, RegionSeq, ARDOUR::Stripable::Sorter> TrackRegionsMap;
	TrackRegionsMap regions_per_track;

	// Pattern per track
	std::vector<TrackPattern*> tps;

	// Row index offset and number of valid rows per mti
	size_t                       earliest_mti;
	TrackPattern*                earliest_tp;
	std::vector<uint32_t>        row_offset;
	std::vector<uint32_t>        tracks_nrows;
	uint32_t                     global_nrows;

private:
	bool _connect;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_multi_track_pattern_h_ */
