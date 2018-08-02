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

MidiTrackPattern::MidiTrackPattern (ARDOUR::Session* session,
                                    const std::vector<boost::shared_ptr<ARDOUR::MidiRegion> >& regions)
	: BasePattern(session, regions.front()) // NEXT TODO: support multiple regions
	, mrp(session, regions.front())
	, tap(session, regions.front())
{
}

MidiTrackPattern::~MidiTrackPattern ()
{
}

void MidiTrackPattern::set_rows_per_beat(uint16_t rpb)
{
	BasePattern::set_rows_per_beat(rpb);
	mrp.set_rows_per_beat(rpb);
	tap.set_rows_per_beat(rpb);
}

void MidiTrackPattern::set_row_range()
{
	BasePattern::set_row_range();
	mrp.set_row_range();
	tap.set_row_range();

	// Make sure that midi and automation regions start at the same sample
	assert (sample_at_row(0) == mrp.sample_at_row(0));
	assert (sample_at_row(0) == tap.sample_at_row(0));

	// Make sure all patterns have the same number of rows
	assert (nrows == mrp.nrows);
	assert (nrows == tap.nrows);
}

void MidiTrackPattern::update()
{
	set_row_range();
	mrp.update();
	tap.update();
}

bool MidiTrackPattern::is_defined (uint32_t rowi) const
{
	// NEXT TODO: return true iff there is an existing region at this rowi
	return true;
}
