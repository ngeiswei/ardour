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

MidiTrackPattern::MidiTrackPattern (const TrackerEditor& te,
                                    boost::shared_ptr<ARDOUR::MidiTrack> mt,
                                    const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
	: BasePattern(te,
	              get_regions_position(regions),
	              0,
	              get_regions_length(regions),
	              get_regions_first_sample(regions),
	              get_regions_last_sample(regions))
	, midi_track(mt)
	, tap(session,
	      midi_track,
	      get_regions_position(regions),
	      get_regions_length(regions),
	      get_regions_first_sample(regions),
	      get_regions_last_sample(regions))
	, row_offset(regions.size(), 0)
{
	for (size_t i = 0; i < regions.size(); i++)
		mrps.push_back(MidiRegionPattern(te, regions[i]));
}

MidiTrackPattern::~MidiTrackPattern ()
{
}

// NEXT TODO: do I need that?
boost::shared_ptr<ARDOUR::AutomationControl> MidiTrackPattern::get_actl(const Evoral::Parameter& param)
{
	// NEXT TODO: support regions somehow
	return tap.get_actl(param);
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
	// Disgard is out of midi track pattern range
	if (!BasePattern::is_defined (rowi))
		return false;

	// Check if rowi points to an existing region
	for (size_t mri = 0; i < mrps.size(); i++)
		if (mrps[i].is_defined(to_rrri((uint32_t)rowi)))
			return true;

	// Within range but no region defined there
	return false;
}

int MidiTrackPattern::to_rrri(uint32_t rowi, size_t mri) const
{
	return (int)rowi - (int)row_offset[mri];
}
