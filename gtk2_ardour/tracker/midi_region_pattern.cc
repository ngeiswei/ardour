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

#include "ardour/midi_region.h"

#include "midi_region_pattern.h"

MidiRegionPattern::MidiRegionPattern (ARDOUR::Session* session,
                                      boost::shared_ptr<ARDOUR::MidiRegion> region)
	: BasePattern(session, region)
	, np(session, region)
	, rap(session, region)
{
}

MidiRegionPattern::~MidiRegionPattern ()
{
}

void MidiRegionPattern::set_rows_per_beat(uint16_t rpb)
{
	BasePattern::set_rows_per_beat(rpb);
	np.set_rows_per_beat(rpb);
	rap.set_rows_per_beat(rpb);
}

void MidiRegionPattern::set_row_range()
{
	BasePattern::set_row_range();
	np.set_row_range();
	rap.set_row_range();

	// Make sure that midi and automation regions start at the same sample
	assert (sample_at_row(0) == np.sample_at_row(0));
	assert (sample_at_row(0) == rap.sample_at_row(0));

	// Make sure all patterns have the same number of rows
	assert (nrows == np.nrows);
	assert (nrows == rap.nrows);
}

void MidiRegionPattern::update()
{
	set_row_range();
	np.update();
	rap.update();
}
