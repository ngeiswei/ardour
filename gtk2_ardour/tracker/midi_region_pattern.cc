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
#include "ardour/midi_source.h"

#include "midi_region_pattern.h"

using namespace Tracker;

MidiRegionPattern::MidiRegionPattern (TrackerEditor& te,
                                      boost::shared_ptr<ARDOUR::MidiTrack> mt,
                                      boost::shared_ptr<ARDOUR::MidiRegion> region)
	: BasePattern(te, region)
	, np(te, region)
	, rap(te, mt, region)
	, midi_model(region->midi_source(0)->model())
	, midi_region(region)
{
}

MidiRegionPattern::~MidiRegionPattern ()
{
}

MidiRegionPattern&
MidiRegionPattern::operator=(const MidiRegionPattern& other)
{
	std::cout << "MidiRegionPattern[" << this << "]::operator(" << &other << ")" << std::endl;

	BasePattern::operator=(other);
	np = other.np;
	rap = other.rap;
	midi_model = other.midi_model;
	midi_region = other.midi_region;
	
	return *this;
}

MidiRegionPattern::PhenomenalDiff
MidiRegionPattern::phenomenal_diff(const MidiRegionPattern& prev) const
{
	std::cout << "MidiRegionPattern::phenomenal_diff" << std::endl;

	NotePattern::PhenomenalDiff diff = np.phenomenal_diff(prev.np);
	// TODO: take care of rap

	std::cout << "MidiRegionPattern::phenomenal_diff diff = " << diff << std::endl;

	return diff;
}

void
MidiRegionPattern::set_rows_per_beat(uint16_t rpb)
{
	BasePattern::set_rows_per_beat(rpb);
	np.set_rows_per_beat(rpb);
	rap.set_rows_per_beat(rpb);
}

void
MidiRegionPattern::set_row_range()
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

void
MidiRegionPattern::update()
{
	set_row_range();
	np.update();
	rap.update();
}

void
MidiRegionPattern::insert(const Evoral::Parameter& param)
{
	rap.insert(param);
}

std::string
MidiRegionPattern::self_to_string() const
{
	std::stringstream ss;
	ss << "MidiRegionPattern[" << this << "]";
	return ss.str();
}

std::string
MidiRegionPattern::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePattern::to_string(indent);

	std::string header = indent + self_to_string() + " ";
	ss << header << "np:" << std::endl << np.to_string(indent + "  ");
	ss << header << "rap:" << std::endl << rap.to_string(indent + "  ");
	ss << header << "midi_model = " << midi_model.get() << std::endl;
	ss << header << "midi_region = " << midi_region.get() << std::endl;

	return ss.str();
}
