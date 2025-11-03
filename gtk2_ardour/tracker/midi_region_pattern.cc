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

#include "ardour/midi_region.h"
#include "ardour/session_playlists.h"

#include "midi_region_pattern.h"
#include "tracker_editor.h"

using namespace Tracker;

MidiRegionPattern::MidiRegionPattern (TrackerEditor& te,
                                      MidiTrackPtr mt,
                                      MidiRegionPtr region,
                                      bool connect)
	: BasePattern (te, region)
	, mnp (te, region)
	, mrap (te, mt, region, connect)
	, midi_track (mt)
	, midi_model (region->model ())
	, midi_region (region)
{
	if (connect)
		tracker_editor.connect_midi_region (midi_region);
}

MidiRegionPattern::~MidiRegionPattern ()
{
}

MidiRegionPattern&
MidiRegionPattern::operator= (const MidiRegionPattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	BasePattern::operator= (other);
	mnp = other.mnp;
	mrap = other.mrap;
	midi_model = other.midi_model;
	midi_region = other.midi_region;
	selected = other.selected;

	return *this;
}

MidiRegionPatternPhenomenalDiff
MidiRegionPattern::phenomenal_diff (const MidiRegionPattern& prev) const
{
	MidiRegionPatternPhenomenalDiff diff;
	if (!prev.enabled && !enabled) {
		return diff;
	}

	diff.mnp_diff = mnp.phenomenal_diff (prev.mnp);
	diff.mrap_diff = mrap.phenomenal_diff (prev.mrap);

	return diff;
}

void
MidiRegionPattern::set_rows_per_beat (uint16_t rpb, bool rfs)
{
	BasePattern::set_rows_per_beat (rpb, rfs);
	mnp.set_rows_per_beat (rpb, rfs);
	mrap.set_rows_per_beat (rpb, rfs);
}

void
MidiRegionPattern::update_enabled ()
{
	set_enabled (selected && is_region_visible ());
}

void
MidiRegionPattern::update_position_etc ()
{
	position = midi_region->position ();
	start = midi_region->start ();
	length = midi_region->length ();
	end = midi_region->end ();
	nt_last = midi_region->nt_last ();
	mnp.position = position;
	mnp.start = start;
	mnp.length = length;
	mnp.end = end;
	mnp.nt_last = nt_last;
	mrap.position = position;
	mrap.start = start;
	mrap.length = length;
	mrap.end = end;
	mrap.nt_last = nt_last;
}

void
MidiRegionPattern::set_row_range ()
{
	BasePattern::set_row_range ();
	mnp.set_row_range ();
	mrap.set_row_range ();

	// Make sure that midi and automation regions start at the same sample
	assert (sample_at_row (0) == mnp.sample_at_row (0));
	assert (sample_at_row (0) == mrap.sample_at_row (0));

	// Make sure all patterns have the same number of rows
	assert (nrows == mnp.nrows);
	assert (nrows == mrap.nrows);
}

void
MidiRegionPattern::update ()
{
	update_enabled ();
	update_position_etc ();
	set_row_range ();
	mnp.update ();
	mrap.update ();
}

bool
MidiRegionPattern::is_region_visible () const
{
	return 0 < midi_track->playlist ()->region_use_count (midi_region);
}

void
MidiRegionPattern::insert (const Evoral::Parameter& param)
{
	mrap.insert (param);
}

void
MidiRegionPattern::set_enabled (bool e)
{
	enabled = e;
	mnp.set_enabled(e);
	mrap.set_enabled(e);
}

std::string
MidiRegionPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "MidiRegionPattern[" << this << "]";
	return ss.str ();
}

std::string
MidiRegionPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePattern::to_string (indent) << std::endl;

	std::string header = indent + self_to_string () + " ";
	ss << header << "mnp:" << std::endl << mnp.to_string (indent + "  ") << std::endl;
	ss << header << "mrap:" << std::endl << mrap.to_string (indent + "  ") << std::endl;
	ss << header << "midi_model = " << midi_model.get () << std::endl;
	ss << header << "midi_region = " << midi_region.get ();

	return ss.str ();
}
