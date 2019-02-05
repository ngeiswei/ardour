/*
    Copyright (C) 2018 Nil Geisweiller

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

#ifndef __ardour_tracker_multi_track_pattern_h_
#define __ardour_tracker_multi_track_pattern_h_

#include "ardour/midi_model.h"
#include "ardour/stripable.h"
#include "ardour/automation_list.h"
#include "ardour/track.h"

#include "tracker_utils.h"
#include "note_pattern.h"
#include "track_pattern.h"
#include "multi_track_pattern_phenomenal_diff.h"

namespace Tracker {

class TrackerEditor;

/**
 * Represent patterns of multiple tracks.
 */
class MultiTrackPattern : public BasePattern
{
public:
	MultiTrackPattern (TrackerEditor& te);
	~MultiTrackPattern ();

	// Phenomenal overload of operator=(), only need to copy what is necessary
	// for phenomenal_diff to correctly operate.
	MultiTrackPattern& operator=(const MultiTrackPattern& other);

	MultiTrackPatternPhenomenalDiff phenomenal_diff(const MultiTrackPattern& prev) const;
	
	void setup ();
	void setup_positions ();
	void setup_regions_per_track ();
	void setup_track_patterns ();
	void setup_row_offset ();

	void update ();
	void update_rows_per_beat ();
	void update_content ();
	void update_earliest_mtp ();  // earliest midi track
	void update_global_nrows ();  // row_offset, nrows, global_nrows

	// Return true iff the row at mti belongs to a region
	bool is_region_defined (uint32_t rowi, size_t mti) const;

	// Return true iff the row at mti is defined for param
	bool is_automation_defined (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const;

	// Like beats_at_row but the beats is calculated in reference to the
	// region's position
	Temporal::Beats region_relative_beats (uint32_t rowi, size_t mti, size_t mri, int32_t delay=0) const;

	// Like delay_ticks but the event_time is relative to the region position of a given mti
	int64_t region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mti, size_t mri) const;

	// Return an event's delay in a certain row and mti in ticks
	int64_t delay_ticks (samplepos_t when, uint32_t rowi, size_t mti) const;
	
	// Return the sample at the corresponding row index and delay in relative
	// ticks
	uint32_t sample_at_row_at_mti (uint32_t rowi, size_t mti, int32_t delay=0) const;

	size_t off_notes_count (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const;
	size_t on_notes_count (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const;
	bool is_note_displayable (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const;
	NoteTypePtr off_note (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const;
	NoteTypePtr on_note (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const;

	bool is_auto_displayable (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const;

	typedef ARDOUR::AutomationList::iterator AutomationListIt;
	size_t get_automation_list_count (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const;
	Evoral::ControlEvent* get_automation_control_event (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const;

	NoteTypePtr find_prev_note(uint32_t rowi, size_t mti, size_t mri, int cgi) const;
	NoteTypePtr find_next_note(uint32_t rowi, size_t mti, size_t mri, int cgi) const;

	// Return the Beats of the note off as far as it can go (i.e. the next on
	// note or the end of the region.)
	Temporal::Beats next_off(uint32_t rowi, size_t mti, size_t mri, int cgi) const;

	// Return the row index relative to the start of pattern at mti.
	int to_rri (uint32_t rowi, size_t mti) const;

	// Return the row index relative to the region pattern at mri of mti track
	int to_rrri (uint32_t rowi, size_t mti, size_t mri) const;
	int to_rrri (uint32_t rowi, size_t mti) const;

	// Given the row index, calculate the corresponding midi region index. This
	// can only work assuming that regions do not overlap in time. If no such
	// mri is defined, then return -1.
	int to_mri (uint32_t rowi, size_t mti) const;

	// Insert the automation control(s) associated to param at mti (and connect
	// it to the grid for changes)
	void insert(size_t mti, const Evoral::Parameter& param);

	// Return the midi model at mti and mri
	boost::shared_ptr<ARDOUR::MidiModel> midi_model (size_t mti, size_t mri);

	// Return the midi region at mti and mri
	boost::shared_ptr<ARDOUR::MidiRegion> midi_region (size_t mti, size_t mri);
	
	// Return the note pattern at mti and mri
	NotePattern& note_pattern (size_t mti, size_t mri);

	// Apply given command at mti
	void apply_command (size_t mti, size_t mri, ARDOUR::MidiModel::NoteDiffCommand* cmd);

	boost::shared_ptr<ARDOUR::AutomationList> get_alist (int mti, int mri, const Evoral::Parameter& param);
	
	// Return a pair with the automation value and whether it is defined or not
	std::pair<double, bool> get_automation_value (size_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param);

	// Set the automation value val at rowi, mti and mri for param
	void set_automation_value (double val, int rowi, int mti, int mri, const Evoral::Parameter& param, int delay);

	// Delete automation value at rowi, mti and mri for param
	void delete_automation_value (int rowi, int mti, int mri, const Evoral::Parameter& param);

	// Get a pair with automation delay in tick at rowi of param as first
	// element and whether it is defined as second element. Return (0, false) if
	// undefined.
	std::pair<int, bool> get_automation_delay (size_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param);

	// Set the automation delay in tick at rowi, mri and mri for param
	void set_automation_delay (int delay, size_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param);

	virtual std::string self_to_string() const;
	virtual std::string to_string(const std::string& indent = std::string()) const;
	
	// Mapping track to regions
	typedef std::map<boost::shared_ptr<ARDOUR::Track>, std::vector<boost::shared_ptr<ARDOUR::Region> >, ARDOUR::Stripable::Sorter> TrackRegionsMap;
	TrackRegionsMap regions_per_track;

	// Pattern per track
	std::vector<TrackPattern*> tps;

	// Row index offset and number of valid rows per mti
	size_t                       earliest_mti;
	TrackPattern*                earliest_tp;
	std::vector<uint32_t>        row_offset;
	std::vector<uint32_t>        nrows;
	uint32_t                     global_nrows;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_multi_track_pattern_h_ */
