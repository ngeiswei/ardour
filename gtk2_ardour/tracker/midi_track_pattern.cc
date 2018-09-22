/*
    Copyright (C) 2017 Nil Geisweiller

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

#include "ardour/midi_region.h"

#include "tracker_utils.h"
#include "midi_track_pattern.h"
#include "tracker_grid.h"

using namespace Tracker;

MidiTrackPattern::MidiTrackPattern (TrackerEditor& te,
                                    boost::shared_ptr<ARDOUR::Track> trk,
                                    const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions)
	: TrackPattern(te, trk, regions)
	, midi_track(boost::static_pointer_cast<ARDOUR::MidiTrack>(trk))
	, tap(te,
	      midi_track,
	      TrackerUtils::get_position(regions),
	      TrackerUtils::get_length(regions),
	      TrackerUtils::get_first_sample(regions),
	      TrackerUtils::get_last_sample(regions))
	, row_offset(regions.size(), 0)
{
	for (size_t i = 0; i < regions.size(); i++)
		mrps.push_back(MidiRegionPattern(te, midi_track, boost::static_pointer_cast<ARDOUR::MidiRegion>(regions[i])));
}

MidiTrackPattern::~MidiTrackPattern ()
{
}

boost::shared_ptr<ARDOUR::AutomationList>
MidiTrackPattern::get_alist(int mri, const Evoral::Parameter& param)
{
	if (TrackerUtils::is_region_automation (param))
		return mrps[mri].rap.get_alist(param);
	else
		return tap.get_alist(param);
}

void
MidiTrackPattern::insert(const Evoral::Parameter& param)
{
	if (TrackerUtils::is_region_automation (param))
		for (size_t i = 0; i < mrps.size(); i++)
			mrps[i].insert(param);
	else
		tap.insert(param);
}

void
MidiTrackPattern::set_rows_per_beat(uint16_t rpb)
{
	BasePattern::set_rows_per_beat(rpb);
	for (size_t i = 0; i < mrps.size(); i++)
		mrps[i].set_rows_per_beat(rpb);
	tap.set_rows_per_beat(rpb);
}

void
MidiTrackPattern::set_row_range()
{
	BasePattern::set_row_range();
	for (size_t i = 0; i < mrps.size(); i++)
		mrps[i].set_row_range();
	tap.set_row_range();
}

void
MidiTrackPattern::update()
{
	// Update midi regions
	set_row_range();
	update_midi_regions();

	// Set number of note tracks to its common max and re-update
	set_ntracks (get_ntracks());
	update_midi_regions();

	// Update track automation pattern
	tap.update();

	update_row_offset ();
}

void
MidiTrackPattern::update_midi_regions()
{
	for (size_t mri = 0; mri < mrps.size(); mri++)
		mrps[mri].update();
}

void
MidiTrackPattern::update_row_offset()
{
	for (size_t mri = 0; mri < mrps.size(); mri++)
		row_offset[mri] = mrps[mri].row_distance(position_row_beats, mrps[mri].position_row_beats);
}

bool
MidiTrackPattern::is_defined (int rowi) const
{
	return -1 < to_mri (rowi);
}

int
MidiTrackPattern::to_rrri (uint32_t rowi, size_t mri) const
{
	return (int)rowi - (int)row_offset[mri];
}

int
MidiTrackPattern::to_rrri (uint32_t rowi) const
{
	return (int)rowi - (int)row_offset[to_mri(rowi)];
}

int
MidiTrackPattern::to_mri (uint32_t rowi) const
{
	// Disgard is out of midi track pattern range
	if (!BasePattern::is_defined (rowi))
		return -1;

	// Check if rowi points to an existing region
	for (size_t mri = 0; mri < mrps.size(); mri++)
		if (mrps[mri].is_defined(to_rrri((uint32_t)rowi, mri)))
			return mri;

	// Within range but no region defined there
	return -1;
}

uint16_t
MidiTrackPattern::get_ntracks () const
{
	uint16_t ntracks = 0;
	for (size_t mri = 0; mri < mrps.size(); mri++)
		ntracks = std::max(ntracks, mrps[mri].np.ntracks);

	if (ntracks > MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK) {
		// TODO: use Ardour's logger instead of stdout
		std::cout << "Warning: Number of note tracks needed for "
		          << "the tracker interface is too high, "
		          << "some notes might be discarded" << std::endl;
		ntracks = MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK;
	}

	return ntracks;
}

void
MidiTrackPattern::set_ntracks (uint16_t n)
{
	for (size_t i = 0; i < mrps.size(); i++)
		mrps[i].np.set_ntracks(n);
}

void
MidiTrackPattern::inc_ntracks ()
{
	for (size_t i = 0; i < mrps.size(); i++)
		mrps[i].np.inc_ntracks();
}

void
MidiTrackPattern::dec_ntracks ()
{
	for (size_t i = 0; i < mrps.size(); i++)
		mrps[i].np.dec_ntracks();
}

uint16_t
MidiTrackPattern::get_nreqtracks () const
{
	uint16_t nreqtracks = 0;
	for (size_t mri = 0; mri < mrps.size(); mri++)
		nreqtracks = std::max(nreqtracks, mrps[mri].np.nreqtracks);
	return nreqtracks;
}

bool
MidiTrackPattern::is_empty (const Evoral::Parameter& param) const
{
	if (TrackerUtils::is_region_automation (param)) {
		for (size_t mri = 0; mri < mrps.size(); mri++)
			if (!mrps[mri].rap.is_empty(param))
				return false;
		return true;
	}
	return tap.is_empty(param);
}

std::pair<double, bool>
MidiTrackPattern::get_automation_value (size_t rowi, size_t mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri].rap.get_automation_value (to_rrri(rowi, mri), param)
		: tap.get_automation_value (rowi, param);
}

void
MidiTrackPattern::set_automation_value (double val, size_t rowi, size_t mri, const Evoral::Parameter& param, int delay)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri].rap.set_automation_value (val, to_rrri(rowi, mri), param, delay)
		: tap.set_automation_value (val, rowi, param, delay);
}

void
MidiTrackPattern::delete_automation_value (size_t rowi, size_t mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri].rap.delete_automation_value (to_rrri(rowi, mri), param)
		: tap.delete_automation_value (rowi, param);
}

std::pair<int, bool>
MidiTrackPattern::get_automation_delay (int rowi, int mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri].rap.get_automation_delay (to_rrri(rowi, mri), param)
		: tap.get_automation_delay (rowi, param);
}

void
MidiTrackPattern::set_automation_delay (int delay, int rowi, int mri, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[mri].rap.set_automation_delay (delay, to_rrri(rowi, mri), param)
		: tap.set_automation_delay (delay, rowi, param);
}

Temporal::Beats
MidiTrackPattern::region_relative_beats_at_row (uint32_t rowi, size_t mri, int32_t delay) const
{
	return mrps[mri].region_relative_beats_at_row (to_rrri(rowi, mri), delay);
}

int64_t
MidiTrackPattern::region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mri) const
{
	return mrps[mri].region_relative_delay_ticks(event_time, to_rrri(rowi, mri));
}
