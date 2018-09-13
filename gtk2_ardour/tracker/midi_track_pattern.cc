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

#include "midi_track_pattern.h"
#include "tracker_grid.h"

Temporal::samplepos_t get_regions_position(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
{
	return regions.front()->position();
}

Temporal::samplecnt_t get_regions_length(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
{
	return regions.back()->position() - regions.front()->last_sample() + 1;
}

Temporal::samplepos_t get_regions_first_sample(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
{
	return regions.front()->first_sample();
}

Temporal::samplepos_t get_regions_last_sample(const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
{
	return regions.back()->last_sample();
}

MidiTrackPattern::MidiTrackPattern (TrackerEditor& te,
                                    boost::shared_ptr<ARDOUR::MidiTrack> mt,
                                    const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
	: BasePattern(te,
	              get_regions_position(regions),
	              0,
	              get_regions_length(regions),
	              get_regions_first_sample(regions),
	              get_regions_last_sample(regions))
	, midi_track(mt)
	, tap(te,
	      midi_track,
	      get_regions_position(regions),
	      get_regions_length(regions),
	      get_regions_first_sample(regions),
	      get_regions_last_sample(regions))
	, row_offset(regions.size(), 0)
{
	for (size_t i = 0; i < regions.size(); i++)
		mrps.push_back(MidiRegionPattern(te, midi_track, regions[i]));
}

MidiTrackPattern::~MidiTrackPattern ()
{
}

// NEXT TODO: do I need that?
boost::shared_ptr<ARDOUR::AutomationControl> MidiTrackPattern::get_actrl(const Evoral::Parameter& param)
{
	// NEXT TODO: support regions somehow
	return tap.get_actrl(param);
}

void MidiTrackPattern::insert(const Evoral::Parameter& param)
{
	if (TrackerUtils::is_region_automation (param))
		for (size_t i = 0; i < mrps.size(); i++)
			mrps[i].insert(param);
	else
		tap.insert(param);
}

void MidiTrackPattern::set_rows_per_beat(uint16_t rpb)
{
	BasePattern::set_rows_per_beat(rpb);
	for (size_t i = 0; i < mrps.size(); i++)
		mrps[i].set_rows_per_beat(rpb);
	tap.set_rows_per_beat(rpb);
}

void MidiTrackPattern::set_row_range()
{
	BasePattern::set_row_range();
	for (size_t i = 0; i < mrps.size(); i++)
		mrps[i].set_row_range();
	tap.set_row_range();
}

void MidiTrackPattern::update()
{
	set_row_range();
	for (size_t i = 0; i < mrps.size(); i++)
		mrps[i].update();
	tap.update();

	update_row_offset ();
}

void MidiTrackPattern::update_row_offset()
{
	for (size_t mri = 0; mri < mrps.size(); mri++)
		row_offset[mri] = mrps[mri].row_distance(position_row_beats, mrps[mri].position_row_beats);
}

bool MidiTrackPattern::is_defined (int rowi) const
{
	return -1 < to_mri (rowi);
}

int MidiTrackPattern::to_rrri (uint32_t rowi, size_t mri) const
{
	return (int)rowi - (int)row_offset[mri];
}

int MidiTrackPattern::to_rrri (uint32_t rowi) const
{
	return (int)rowi - (int)row_offset[to_mri(rowi)];
}

int MidiTrackPattern::to_mri (uint32_t rowi) const
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

uint16_t MidiTrackPattern::get_ntracks () const
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

size_t MidiTrackPattern::get_asize (const Evoral::Parameter& param) const
{
	if (TrackerUtils::is_region_automation (param)) {
		size_t asize = 0;
		for (size_t mri = 0; mri < mrps.size(); mri++)
			asize += mrps[mri].rap.get_asize(param);
		return asize;
	}
	return tap.get_asize(param);	
}

std::pair<double, bool>
MidiTrackPattern::get_automation_value (size_t rowi, const Evoral::Parameter& param)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[to_mri(rowi)].rap.get_automation_value (to_rrri(rowi), param)
		: tap.get_automation_value (rowi, param);
}

void
MidiTrackPattern::set_automation_value (double val, size_t rowi, const Evoral::Parameter& param, int delay)
{
	return TrackerUtils::is_region_automation (param) ?
		mrps[to_mri(rowi)].rap.set_automation_value (val, to_rrri(rowi), param, delay)
		: tap.set_automation_value (val, rowi, param, delay);
}
