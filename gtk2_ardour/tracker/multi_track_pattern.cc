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

#include "midi_region_view.h"

MultiTrackPattern::MultiTrackPattern (TrackerEditor& te)
	: tracker_editor (te)
	, earliest_mtp (NULL)
	, global_nrows (0)
{}

MultiTrackPattern::~MultiTrackPattern ()
{
	for (std::vector<MidiTrackPattern*>::iterator it = mtps.begin(); it != mtps.end(); ++it)
		delete *it;
}

void
MultiTrackPattern::setup_regions_per_track ()
{
	for (RegionSelection::const_iterator it = tracker_editor.region_selection.begin(); it != tracker_editor.region_selection.end(); ++it) {
		const MidiRegionView* mrv = dynamic_cast<const MidiRegionView*>(*it);
		if (mrv) {                // Make sure it is midi region
			boost::shared_ptr<ARDOUR::MidiRegion> midi_region = mrv->midi_region();
			boost::shared_ptr<ARDOUR::MidiTrack> midi_track = mrv->midi_view()->midi_track();
			regions_per_track[midi_track].push_back(midi_region);
		}
	}
	for (TrackRegionsMap::iterator it = regions_per_track.begin(); it != regions_per_track.end(); it++)
	{
		std::sort(it->second.begin(), it->second.end(), region_position_less());
	}
}

void
MultiTrackPattern::setup ()
{
	setup_regions_per_track ();

	// TODO: it would be better if it followed the order on the piano roll view!
	for (TrackRegionsMap::const_iterator it = regions_per_track.begin(); it != regions_per_track.end(); it++) {
		MidiTrackPattern* mtp = new MidiTrackPattern(tracker_editor, it->first, it->second);
		mtps.push_back(mtp);
	}

	for (size_t mti = 0; mti < mtps.size(); mti++) {
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
	for (size_t mti = 0; mti < mtps.size(); mti++) {
		MidiTrackPattern* mtp = mtps[mti];
		mtp->set_rows_per_beat(tracker_editor.main_toolbar.rows_per_beat);
	}
}

void
MultiTrackPattern::update_content ()
{
	for (size_t mti = 0; mti < mtps.size(); mti++) {
		MidiTrackPattern* mtp = mtps[mti];
		mtp->update();
	}
}

void
MultiTrackPattern::update_earliest_mtp ()
{
	Temporal::Beats min_position_beats = std::numeric_limits<Temporal::Beats>::max();
	for (size_t mti = 0; mti < mtps.size(); mti++) {
		MidiTrackPattern* mtp = mtps[mti];

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
	for (size_t mti = 0; mti < mtps.size(); mti++) {
		MidiTrackPattern* mtp = mtps[mti];
		row_offset[mti] = (int32_t)mtp->row_distance(earliest_mtp->position_row_beats, mtp->position_row_beats);
		std::cout << "row_offset[" << mti << "] = " << row_offset[mti] << std::endl;
		nrows[mti] = mtp->nrows;
		std::cout << "nrows[" << mti << "] = " << nrows[mti] << std::endl;
		global_nrows = std::max(global_nrows, row_offset[mti] + nrows[mti]);
	}
	std::cout << "global_nrows = " << global_nrows << std::endl;
}

bool
MultiTrackPattern::is_defined (uint32_t rowi, size_t mti) const
{
	return mtps[mti]->is_defined (to_rri (rowi, mti));
}

Temporal::Beats
MultiTrackPattern::region_relative_beats_at_row (uint32_t rowi, size_t mti, int32_t delay) const
{
	return mtps[mti]->region_relative_beats_at_row(to_rri(rowi, mti), delay);
}

int64_t
MultiTrackPattern::region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mti) const
{
	return mtps[mti]->region_relative_delay_ticks(event_time, to_rri(rowi, mti));
}

int64_t
MultiTrackPattern::delay_ticks (samplepos_t when, uint32_t rowi, size_t mti) const
{
	return mtps[mti]->delay_ticks(when, to_rri(rowi, mti));
}

uint32_t
MultiTrackPattern::sample_at_row (uint32_t rowi, size_t mti, int32_t delay) const
{
	return mtps[mti]->sample_at_row(to_rri(rowi, mti), delay);
}

size_t
MultiTrackPattern::off_notes_count (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	return mtps[mti]->mrps[mri].np.off_notes[cgi].count(to_rrri(rowi, mti, mri));
}

size_t
MultiTrackPattern::on_notes_count (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	return mtps[mti]->mrps[mri].np.on_notes[cgi].count(to_rrri(rowi, mti, mri));
}

bool
MultiTrackPattern::is_note_displayable (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	return mtps[mti]->mrps[mri].np.is_displayable (to_rrri(rowi, mti, mri), cgi);
}

NoteTypePtr
MultiTrackPattern::off_note (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	NotePattern::RowToNotes::const_iterator i_off = mtps[mti]->mrps[mri].np.off_notes[cgi].find(to_rrri(rowi, mti, mri));
	if (i_off != mtps[mti]->mrps[mri].np.off_notes[cgi].end())
		return i_off->second;
	return NULL;
}

NoteTypePtr
MultiTrackPattern::on_note (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	NotePattern::RowToNotes::const_iterator i_on = mtps[mti]->mrps[mri].np.on_notes[cgi].find(to_rrri(rowi, mti, mri));
	if (i_on != mtps[mti]->mrps[mri].np.on_notes[cgi].end())
		return i_on->second;
	return NULL;
}

bool
MultiTrackPattern::is_auto_displayable (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		mtps[mti]->mrps[mri].rap.is_displayable (to_rrri(rowi, mti, mri), param)
		: mtps[mti]->tap.is_displayable (to_rri(rowi, mti), param);
}

// // TODO: do you really need that?
// AutomationPattern*
// MultiTrackPattern::get_automation_pattern (size_t mti, const Evoral::Parameter& param)
// {
// 	if (!param)
// 		return NULL;
// 	return TrackerUtils::is_region_automation (param) ? (AutomationPattern*)&(mtps[mti]->mrps[mri].rap) : (AutomationPattern*)&(mtps[mti]->tap);
// }

// // TODO: do you really need that?
// const AutomationPattern*
// MultiTrackPattern::get_automation_pattern (size_t mti, const Evoral::Parameter& param) const
// {
// 	if (!param)
// 		return NULL;
// 	return TrackerUtils::is_region_automation (param) ? (const AutomationPattern*)&(mtps[mti]->mrp.rap) : (const AutomationPattern*)&(mtps[mti]->tap);
// }

size_t
MultiTrackPattern::get_automation_list_count (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		mtps[mti]->mrps[mri].rap.automations.find(param)->second.count(to_rrri(rowi, mti, mri))
		: mtps[mti]->tap.automations.find(param)->second.count(to_rri(rowi, mti));
}

Evoral::ControlEvent*
MultiTrackPattern::get_automation_control_event (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ?
		*mtps[mti]->mrps[mri].rap.automations.find(param)->second.find(to_rrri(rowi, mti, mri))->second
		: *mtps[mti]->tap.automations.find(param)->second.find(to_rri(rowi, mti))->second;
}

NoteTypePtr
MultiTrackPattern::find_prev_note(uint32_t rowi, size_t mti, size_t mri, int cgi) const
{
	return mtps[mti]->mrps[mri].np.find_prev(to_rrri(rowi, mti, mri), cgi);
}

NoteTypePtr
MultiTrackPattern::find_next_note(uint32_t rowi, size_t mti, size_t mri, int cgi) const
{
	return mtps[mti]->mrps[mri].np.find_next(to_rrri(rowi, mti, mri), cgi);
}

Temporal::Beats
MultiTrackPattern::next_off(uint32_t rowi, size_t mti, size_t mri, int cgi) const
{
	return mtps[mti]->mrps[mri].np.next_off(to_rrri(rowi, mti, mri), cgi);
}

int
MultiTrackPattern::to_rri (uint32_t rowi, size_t mti) const
{
	return (int)rowi - row_offset[mti];
}

int
MultiTrackPattern::to_rrri(uint32_t rowi, size_t mti, size_t mri) const
{
	return (int)mtps[mti]->to_rrri(to_rri(rowi, mti), mri);
}

int
MultiTrackPattern::to_rrri(uint32_t rowi, size_t mti) const
{
	return (int)mtps[mti]->to_rrri(to_rri(rowi, mti));
}

int
MultiTrackPattern::to_mri(uint32_t rowi, size_t mti) const
{
	return mtps[mti]->to_mri(to_rri(rowi, mti));
}

void
MultiTrackPattern::insert(size_t mti, const Evoral::Parameter& param)
{
	mtps[mti]->insert(param);
}

boost::shared_ptr<ARDOUR::MidiModel>
MultiTrackPattern::midi_model (size_t mti, size_t mri)
{
	return mtps[mti]->mrps[mri].midi_model;
}

NotePattern&
MultiTrackPattern::note_pattern (size_t mti, size_t mri)
{
	return mtps[mti]->mrps[mri].np;
}

void
MultiTrackPattern::apply_command (size_t mti, size_t mri, ARDOUR::MidiModel::NoteDiffCommand* cmd)
{
	midi_model(mti, mri)->apply_command (tracker_editor.session, cmd);
}

std::pair<double, bool>
MultiTrackPattern::get_automation_value (size_t rowi, size_t mti, const Evoral::Parameter& param)
{
	return mtps[mti]->get_automation_value (to_rri(rowi, mti), param);
}

void
MultiTrackPattern::set_automation_value (double val, int rowi, int mti, const Evoral::Parameter& param, int delay)
{
	return mtps[mti]->set_automation_value (val, to_rri(rowi, mti), param, delay);
}
