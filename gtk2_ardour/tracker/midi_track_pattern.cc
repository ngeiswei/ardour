/*
 * Copyright (C) 2017-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include "ardour/midi_region.h"

#include "midi_region_view.h"
#include "region_view.h"

#include "grid.h"
#include "midi_track_pattern.h"
#include "midi_track_pattern_phenomenal_diff.h"
#include "tracker_editor.h"
#include "tracker_utils.h"

using namespace Tracker;

MidiTrackPattern::MidiTrackPattern (TrackerEditor& te,
                                    TrackPtr trk,
                                    const std::vector<RegionView*>& region_views,
                                    const RegionSeq& regions,
                                    Temporal::samplepos_t position,
                                    Temporal::samplecnt_t length,
                                    Temporal::samplepos_t first_sample,
                                    Temporal::samplepos_t last_sample,
                                    bool connect)
	: TrackPattern (te, trk, position, length, first_sample, last_sample, connect)
	, midi_track (boost::static_pointer_cast<ARDOUR::MidiTrack> (trk))
	, rvs (region_views)
	, row_offset (regions.size (), 0)
{
	setup (regions);
}

MidiTrackPattern::~MidiTrackPattern ()
{
	for (size_t i = 0; i < mrps.size(); i++)
		delete mrps[i];
}

void
MidiTrackPattern::setup (const RegionSeq& regions)
{
	// All regions are assumed to be deselected at this point
	
	// Add new regions or re-enable existing ones
	for (size_t mri = 0; mri < regions.size (); mri++) {
		MidiRegionPtr midi_region = boost::static_pointer_cast<ARDOUR::MidiRegion> (regions[mri]);
		MidiRegionPattern* mrp = find_midi_region_pattern (midi_region);
		if (mrp) {
			mrp->set_selected(true);
		} else {
			MidiRegionPattern* new_mrp = new MidiRegionPattern(tracker_editor, midi_track, midi_region, /*_connect NEXT.6*/true);
			new_mrp->set_selected(true);
			mrps.push_back (new_mrp);
		}
	}

	// Chronologically sort regions per track
	struct region_less { bool operator()(const MidiRegionPattern* l, const MidiRegionPattern* r) { return *l < *r; }};
	std::sort (mrps.begin (), mrps.end (), region_less());

	// Enable track
	enabled = true;
}

MidiTrackPattern&
MidiTrackPattern::operator= (const MidiTrackPattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	TrackPattern::operator= (other);
	midi_track = other.midi_track;
	assert (mrps.size () == other.mrps.size ());
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		*mrps[mri] = *other.mrps[mri];
	}
	assert (row_offset.size () == other.row_offset.size ());
	for (size_t mri = 0; mri < row_offset.size (); mri++) {
		row_offset[mri] = other.row_offset[mri];
	}

	return *this;
}

MidiTrackPatternPhenomenalDiff
MidiTrackPattern::phenomenal_diff (const MidiTrackPattern& prev) const
{
	MidiTrackPatternPhenomenalDiff diff;
	if (!prev.enabled && !enabled) {
		return diff;
	}

	diff.full = prev.enabled != enabled
		|| prev.row_offset != row_offset
		|| prev.mrps.size () != mrps.size ();

	if (diff.full) {
		return diff;
	}

	// No global difference, let's look on a per region basis
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		MidiRegionPatternPhenomenalDiff mrp_diff = mrps[mri]->phenomenal_diff (*prev.mrps[mri]);
		if (!mrp_diff.empty ()) {
			diff.mri2mrp_diff[mri] = mrp_diff;
		}
	}
	diff.auto_diff = track_automation_pattern.phenomenal_diff (prev.track_automation_pattern);

	return diff;
}

std::string
MidiTrackPattern::get_name (const Evoral::Parameter& param) const
{
	if (TrackerUtils::is_region_automation (param)) {
		return midi_track->describe_parameter (param);
	}
	return track_automation_pattern.get_name (param);
}

void
MidiTrackPattern::set_param_enabled (const Evoral::Parameter& param, bool enabled)
{
	if (TrackerUtils::is_region_automation (param)) {
		for (size_t mri = 0; mri < mrps.size (); mri++) {
			mrps[mri]->rap.set_param_enabled (param, enabled);
		}
		if (enabled) {
			enabled_region_params.insert (param);
		} else {
			enabled_region_params.erase (param);
		}
	}
	else {
		track_automation_pattern.set_param_enabled (param, enabled);
	}
}

bool
MidiTrackPattern::is_param_enabled (const Evoral::Parameter& param) const
{
	if (TrackerUtils::is_region_automation (param)) {
		// If param is enabled in the first region we can assume it is in all
		// region
		return mrps[0]->rap.is_param_enabled (param);
	}
	return track_automation_pattern.is_param_enabled (param);
}

AutomationListPtr
MidiTrackPattern::get_alist_at_mri (int mri, const Evoral::Parameter& param)
{
	// NEXT.11: have both MainAutomationPattern and ProcessorAutomationPattern
	// inherit from TrackAutomationPattern and then have them be attributes of
	// TrackPattern as it is today.  If so, it is unclear what to do with
	// methods such as get_alist.  Should they be in TrackPattern, or hiden
	// inside MainAutomationPattern and ProcessorAutomationPattern?  Probably
	// the latter, and in this case some may even go to TrackAutomationPattern.
	if (TrackerUtils::is_region_automation (param)) {
		return mrps[mri]->rap.get_alist (param);
	} else {
		return track_automation_pattern.get_alist (param);
	}
}

const AutomationListPtr
MidiTrackPattern::get_alist_at_mri (int mri, const Evoral::Parameter& param) const
{
	if (TrackerUtils::is_region_automation (param)) {
		return mrps[mri]->rap.get_alist (param);
	} else {
		return track_automation_pattern.get_alist (param);
	}
}

void
MidiTrackPattern::insert (const Evoral::Parameter& param)
{
	if (TrackerUtils::is_region_automation (param)) {
		for (size_t mri = 0; mri < mrps.size (); mri++) {
			mrps[mri]->insert (param);
		}
	} else {
		track_automation_pattern.insert (param);
	}
}

void
MidiTrackPattern::set_rows_per_beat (uint16_t rpb)
{
	BasePattern::set_rows_per_beat (rpb);
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		mrps[mri]->set_rows_per_beat (rpb);
	}
	track_automation_pattern.set_rows_per_beat (rpb);
}

void
MidiTrackPattern::set_row_range ()
{
	BasePattern::set_row_range ();
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		mrps[mri]->set_row_range ();
	}
}

void
MidiTrackPattern::update ()
{
	// Update midi regions
	set_row_range ();
	update_midi_regions ();

	// Update whether the track is enabled (after midi regions to take into
	// account whether some are enabled)
	update_enabled ();

	// Set number of note tracks to its common max and re-update
	set_ntracks (get_ntracks ());
	update_midi_regions ();

	// Update track automation pattern
	track_automation_pattern.update ();

	update_row_offset ();
}

void
MidiTrackPattern::update_midi_regions ()
{
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		mrps[mri]->update ();
	}
}

void
MidiTrackPattern::update_enabled ()
{
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		if (mrps[mri]->enabled) {
			enabled = true;
			return;
		}
	}
	enabled = false;
}

void
MidiTrackPattern::update_row_offset ()
{
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		row_offset[mri] = mrps[mri]->row_distance (position_row_beats, mrps[mri]->position_row_beats);
	}
}

bool
MidiTrackPattern::is_region_defined (int rowi) const
{
	return -1 < to_mri (rowi);
}

int
MidiTrackPattern::to_rrri (int rowi, int mri) const
{
	return rowi - row_offset[mri];
}

int
MidiTrackPattern::to_rrri (int rowi) const
{
	return to_rrri (rowi, to_mri (rowi));
}

int
MidiTrackPattern::to_mri (int rowi) const
{
	// Disgard is out of midi track pattern range
	if (!BasePattern::is_defined (rowi)) {
		return -1;
	}

	// Check if rowi points to an existing region
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		if (mrps[mri]->is_defined (to_rrri (rowi, mri))) {
			return mri;
		}
	}

	// Within range but no region defined there
	return -1;
}

uint16_t
MidiTrackPattern::get_ntracks () const
{
	uint16_t ntracks = 0;
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		ntracks = std::max (ntracks, mrps[mri]->np.ntracks);
	}

	if (ntracks > MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK) {
		// TODO: use Ardour's logger instead of stdout
		std::cout << "Warning: Number of note tracks needed for "
		          << "the tracker interface is too high, "
		          << "some notes might be discarded" << std::endl;
		ntracks = MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK;
	}

	return ntracks;
}

void
MidiTrackPattern::set_ntracks (uint16_t n)
{
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		mrps[mri]->np.set_ntracks (n);
	}
}

void
MidiTrackPattern::inc_ntracks ()
{
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		mrps[mri]->np.inc_ntracks ();
	}
}

void
MidiTrackPattern::dec_ntracks ()
{
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		mrps[mri]->np.dec_ntracks ();
	}
}

uint16_t
MidiTrackPattern::get_nreqtracks () const
{
	uint16_t nreqtracks = 0;
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		nreqtracks = std::max (nreqtracks, mrps[mri]->np.nreqtracks);
	}
	return nreqtracks;
}

bool
MidiTrackPattern::is_empty (const Evoral::Parameter& param) const
{
	if (TrackerUtils::is_region_automation (param)) {
		for (size_t mri = 0; mri < mrps.size (); mri++) {
			if (!mrps[mri]->rap.is_empty (param)) {
				return false;
			}
		}
		return true;
	}
	return track_automation_pattern.is_empty (param);
}

std::pair<double, bool>
MidiTrackPattern::get_automation_value (int rowi, int mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.get_automation_value (to_rrri (rowi, mri), param)
		: track_automation_pattern.get_automation_value (rowi, param);
}

void
MidiTrackPattern::set_automation_value (double val, int rowi, int mri, const Evoral::Parameter& param, int delay)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.set_automation_value (val, to_rrri (rowi, mri), param, delay)
		: track_automation_pattern.set_automation_value (val, rowi, param, delay);
}

void
MidiTrackPattern::delete_automation_value (int rowi, int mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.delete_automation_value (to_rrri (rowi, mri), param)
		: track_automation_pattern.delete_automation_value (rowi, param);
}

std::pair<int, bool>
MidiTrackPattern::get_automation_delay (int rowi, int mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.get_automation_delay (to_rrri (rowi, mri), param)
		: track_automation_pattern.get_automation_delay (rowi, param);
}

void
MidiTrackPattern::set_automation_delay (int delay, int rowi, int mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.set_automation_delay (delay, to_rrri (rowi, mri), param)
		: track_automation_pattern.set_automation_delay (delay, rowi, param);
}

Temporal::Beats
MidiTrackPattern::region_relative_beats (int rowi, int mri, int delay) const
{
	return mrps[mri]->region_relative_beats_at_row (to_rrri (rowi, mri), delay);
}

int64_t
MidiTrackPattern::region_relative_delay_ticks (const Temporal::Beats& event_time, int rowi, int mri) const
{
	return mrps[mri]->region_relative_delay_ticks_at_row (event_time, to_rrri (rowi, mri));
}

bool
MidiTrackPattern::is_auto_displayable (int rowi, int mri, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.is_displayable (to_rrri (rowi, mri), param)
		: track_automation_pattern.is_displayable (rowi, param);
}

size_t
MidiTrackPattern::control_events_count (int rowi, int mri, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.control_events_count (to_rrri (rowi, mri), param)
		: track_automation_pattern.control_events_count (rowi, param);
}


RowToControlEventsRange
MidiTrackPattern::control_events_range (int rowi, int mri, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.control_events_range (to_rrri (rowi, mri), param)
		: track_automation_pattern.control_events_range (rowi, param);
}

Evoral::ControlEvent*
MidiTrackPattern::get_automation_control_event (int rowi, int mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.get_control_event (to_rrri (rowi, mri), param)
		: track_automation_pattern.get_control_event (rowi, param);
}

const Evoral::ControlEvent*
MidiTrackPattern::get_automation_control_event (int rowi, int mri, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri]->rap.get_control_event (to_rrri (rowi, mri), param)
		: track_automation_pattern.get_control_event (rowi, param);
}

MidiRegionPattern*
MidiTrackPattern::find_midi_region_pattern (MidiRegionPtr midi_region)
{
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		if (mrps[mri]->midi_region == midi_region) {
			return mrps[mri];
		}
	}
	return 0;
}

boost::shared_ptr<MIDI::Name::MasterDeviceNames>
MidiTrackPattern::get_device_names ()
{
	return midi_track->instrument_info ().master_device_names ();
}

double
MidiTrackPattern::lower (int rowi, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[to_mri (rowi)]->rap.lower (param)
		: track_automation_pattern.lower (param);
}

double
MidiTrackPattern::upper (int rowi, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[to_mri (rowi)]->rap.upper (param)
		: track_automation_pattern.upper (param);
}

void
MidiTrackPattern::set_enabled (bool e)
{
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		mrps[mri]->set_enabled (e);
	}
	enabled = e;
}

void
MidiTrackPattern::set_selected (bool s)
{
	// Select/deselect all regions
	for (size_t mri = 0; mri < mrps.size (); mri++) {
		mrps[mri]->set_selected(s);
	}
}

std::string
MidiTrackPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "MidiTrackPattern[" << this << "]";
	return ss.str ();
}

std::string
MidiTrackPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << track_automation_pattern.to_string (indent) << std::endl;

	std::string header = indent + self_to_string () + " ";
	ss << header << "midi_track = " << midi_track.get ();
	for (size_t mri = 0; mri != mrps.size (); mri++) {
		ss << std::endl << header << "mrps[" << mri << "]:" << std::endl
		   << mrps[mri]->to_string (indent + "  ");
	}
	for (size_t mri = 0; mri != row_offset.size (); mri++) {
		ss << std::endl << header << "row_offset[" << mri << "] = " << row_offset[mri];
	}

	return ss.str ();
}
