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

#include "multi_track_pattern.h"

#include "tracker_editor.h"

MultiTrackPattern::MultiTrackPattern (const TrackerEditor& te)
	: tracker_editor (te)
	, earliest_mtp (NULL)
	, global_nrows (0)
{}

MultiTrackPattern::~MultiTrackPattern ()
{
}

void
MultiTrackPattern::setup (std::vector<MidiTrackPattern*>& midi_track_patterns)
{
	mtps = &midi_track_patterns;

	for (size_t mti = 0; mti < mtps->size(); mti++) {
		row_offset.push_back(0);
		nrows.push_back(0);
	}
}

void
MultiTrackPattern::update ()
{
	update_rows_per_beat ();
	update_content ();
	update_earliest_mtp ();
	update_global_nrows ();
}

void
MultiTrackPattern::update_rows_per_beat ()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		MidiTrackPattern* mtp = (*mtps)[mti];
		mtp->set_rows_per_beat(tracker_editor.main_toolbar.rows_per_beat);
	}
}

void
MultiTrackPattern::update_content ()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		MidiTrackPattern* mtp = (*mtps)[mti];
		mtp->update();
	}
}

void
MultiTrackPattern::update_earliest_mtp ()
{
	Temporal::Beats min_position_beats = std::numeric_limits<Temporal::Beats>::max();
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		MidiTrackPattern* mtp = (*mtps)[mti];

		std::cout << "position_row_beats[" << mti << "] = "
		          << mtp->position_row_beats << std::endl;

		// Get min position beat
		if (mtp->position_row_beats < min_position_beats) {
			min_position_beats = mtp->position_row_beats;
			earliest_mtp = mtp;
		}
	}
	std::cout << "min_position_beats = " << min_position_beats << std::endl;
}

void
MultiTrackPattern::update_global_nrows ()
{
	global_nrows = 0;
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		MidiTrackPattern* mtp = (*mtps)[mti];
		row_offset[mti] = (int32_t)mtp->row_distance(earliest_mtp->position_row_beats, mtp->position_row_beats);
		std::cout << "row_offset[" << mti << "] = " << row_offset[mti] << std::endl;
		nrows[mti] = mtp->nrows;
		std::cout << "nrows[" << mti << "] = " << nrows[mti] << std::endl;
		global_nrows = std::max(global_nrows, row_offset[mti] + nrows[mti]);
	}
	std::cout << "global_nrows = " << global_nrows << std::endl;
}

bool
MultiTrackPattern::is_defined (uint32_t row_idx, size_t mti)
{
	// TODO
	return true;
}

Temporal::Beats
MultiTrackPattern::region_relative_beats_at_row (uint32_t rowi, size_t mti, int32_t delay) const
{
	return (*mtps)[mti]->region_relative_beats_at_row(rowi2rri(rowi, mti), delay);
}

int64_t
MultiTrackPattern::region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mti) const
{
	return (*mtps)[mti]->region_relative_delay_ticks(event_time, rowi2rri(rowi, mti));
}

int64_t
MultiTrackPattern::delay_ticks (samplepos_t when, uint32_t rowi, size_t mti) const
{
	return (*mtps)[mti]->delay_ticks(when, rowi2rri(rowi, mti));
}

uint32_t
MultiTrackPattern::sample_at_row (uint32_t rowi, size_t mti, int32_t delay) const
{
	return (*mtps)[mti]->sample_at_row(rowi2rri(rowi, mti), delay);
}

size_t
MultiTrackPattern::off_notes_count (uint32_t rowi, size_t mti, size_t cgi) const
{
	return (*mtps)[mti]->np.off_notes[cgi].count(rowi2rri(rowi, mti));
}

size_t
MultiTrackPattern::on_notes_count (uint32_t rowi, size_t mti, size_t cgi) const
{
	return (*mtps)[mti]->np.on_notes[cgi].count(rowi2rri(rowi, mti));
}

bool
MultiTrackPattern::is_note_displayable (uint32_t rowi, size_t mti, size_t cgi) const
{
	return (*mtps)[mti]->np.is_displayable (rowi2rri(rowi, mti), cgi);
}

NoteTypePtr
MultiTrackPattern::off_note (uint32_t rowi, size_t mti, size_t cgi) const
{
	NotePattern::RowToNotes::const_iterator i_off = (*mtps)[mti]->np.off_notes[cgi].find(rowi2rri(rowi, mti));
	if (i_off != (*mtps)[mti]->np.off_notes[cgi].end())
		return i_off->second;
	return NULL;
}

NoteTypePtr
MultiTrackPattern::on_note (uint32_t rowi, size_t mti, size_t cgi) const
{
	NotePattern::RowToNotes::const_iterator i_on = (*mtps)[mti]->np.on_notes[cgi].find(rowi2rri(rowi, mti));
	if (i_on != (*mtps)[mti]->np.on_notes[cgi].end())
		return i_on->second;
	return NULL;
}

bool
MultiTrackPattern::is_auto_displayable (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const
{
	return get_automation_pattern (mti, param)->is_displayable (rowi2rri(rowi, mti), param);
}

AutomationPattern*
MultiTrackPattern::get_automation_pattern (size_t mti, const Evoral::Parameter& param)
{
	if (!param)
		return NULL;
	return TrackerUtils::is_region_automation (param) ? (AutomationPattern*)&((*mtps)[mti]->rap) : (AutomationPattern*)&((*mtps)[mti]->tap);
}

const AutomationPattern*
MultiTrackPattern::get_automation_pattern (size_t mti, const Evoral::Parameter& param) const
{
	if (!param)
		return NULL;
	return TrackerUtils::is_region_automation (param) ? (const AutomationPattern*)&((*mtps)[mti]->rap) : (const AutomationPattern*)&((*mtps)[mti]->tap);
}

size_t
MultiTrackPattern::get_automation_list_count (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const
{
	const AutomationPattern* ap = get_automation_pattern (mti, param);
	return ap->automations.find(param)->second.count(rowi2rri(rowi, mti));
}

Evoral::ControlEvent*
MultiTrackPattern::get_automation_control_event (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const
{
	const AutomationPattern* ap = get_automation_pattern (mti, param);
	return *(ap->automations.find(param)->second.find(rowi2rri(rowi, mti))->second);
}

int
MultiTrackPattern::rowi2rri (uint32_t rowi, size_t mti) const
{
	return (int)rowi - row_offset[mti];
}
