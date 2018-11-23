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

#include "midi_region_view.h"
#include "audio_region_view.h"

#include "tracker_editor.h"

using namespace Tracker;

MultiTrackPattern::MultiTrackPattern (TrackerEditor& te)
	: BasePattern(te,
	              TrackerUtils::get_position(te.region_selection),
	              0,
	              TrackerUtils::get_length(te.region_selection),
	              TrackerUtils::get_first_sample(te.region_selection),
	              TrackerUtils::get_last_sample(te.region_selection))
	, earliest_tp (0)
	, global_nrows (0)
{
}

MultiTrackPattern::~MultiTrackPattern ()
{
	for (std::vector<TrackPattern*>::iterator it = tps.begin(); it != tps.end(); ++it)
		delete *it;
}

MultiTrackPattern::PhenomenalDiff
MultiTrackPattern::phenomenal_diff(const MultiTrackPattern& prev) const
{
	MultiTrackPattern::PhenomenalDiff pd;

	std::cout << "MultiTrackPattern::phenomenal_diff()" << std::endl;

	pd.global = prev.global_nrows != global_nrows
		|| prev.tps.size() != tps.size();

	std::cout << "global = " << pd.global << std::endl;

	if (pd.global)
		return pd;

	// No globa difference, let's look on a per track basis
	for (size_t mti = 0; mti < tps.size(); mti++) {
		bool diff = prev.row_offset[mti] != row_offset[mti]
			|| prev.nrows[mti] != nrows[mti]
			|| prev.tps[mti]->phenomenal_diff(tps[mti]);
		pd.tps.push_back(diff);

		std::cout << "pd.tps[" << mti << "] = " << diff << std::endl;
	}
	return pd;
}

void
MultiTrackPattern::setup ()
{
	setup_regions_per_track ();
	setup_track_patterns ();
	setup_row_offset ();
}

void
MultiTrackPattern::setup_regions_per_track ()
{
	// Associate track to its regions
	for (RegionSelection::const_iterator it = tracker_editor.region_selection.begin(); it != tracker_editor.region_selection.end(); ++it) {
		boost::shared_ptr<ARDOUR::Region> region = (*it)->region();
		boost::shared_ptr<ARDOUR::Track> track = dynamic_cast<RouteTimeAxisView&>((*it)->get_time_axis_view()).track();
		regions_per_track[track].push_back(region);
	}
	// Chronologically sort regions per track 
	for (TrackRegionsMap::iterator it = regions_per_track.begin(); it != regions_per_track.end(); it++)
		std::sort(it->second.begin(), it->second.end(), region_position_less());
}

void
MultiTrackPattern::setup_track_patterns()
{
	for (TrackRegionsMap::const_iterator it = regions_per_track.begin(); it != regions_per_track.end(); it++) {
		boost::shared_ptr<ARDOUR::MidiTrack> midi_track = boost::dynamic_pointer_cast<ARDOUR::MidiTrack>(it->first);
		if (midi_track) {
			MidiTrackPattern* mtp = new MidiTrackPattern(tracker_editor, it->first, it->second,
			                                             position, length, first_sample, last_sample);
			tps.push_back(mtp);
		}
		boost::shared_ptr<ARDOUR::AudioTrack> audio_track = boost::dynamic_pointer_cast<ARDOUR::AudioTrack>(it->first);
		if (audio_track) {
			AudioTrackPattern* atp = new AudioTrackPattern(tracker_editor, it->first, it->second,
			                                               position, length, first_sample, last_sample);
			tps.push_back(atp);
		}
	}
}

void
MultiTrackPattern::setup_row_offset()
{
	for (size_t mti = 0; mti < tps.size(); mti++) {
		row_offset.push_back(0);
		nrows.push_back(0);
	}
}

void
MultiTrackPattern::update ()
{
	set_row_range ();
	update_rows_per_beat ();
	update_content ();
	update_earliest_mtp ();
	update_global_nrows ();
}

void
MultiTrackPattern::update_rows_per_beat ()
{
	uint16_t rpb = tracker_editor.main_toolbar.rows_per_beat;
	BasePattern::set_rows_per_beat(rpb);
	for (size_t mti = 0; mti < tps.size(); mti++) {
		TrackPattern* tp = tps[mti];
		tp->set_rows_per_beat(rpb);
	}
}

void
MultiTrackPattern::update_content ()
{
	for (size_t mti = 0; mti < tps.size(); mti++) {
		TrackPattern* tp = tps[mti];
		tp->update();
	}
}

void
MultiTrackPattern::update_earliest_mtp ()
{
	Temporal::Beats min_position_beats = std::numeric_limits<Temporal::Beats>::max();
	for (size_t mti = 0; mti < tps.size(); mti++) {
		TrackPattern* tp = tps[mti];

		// Get min position beat
		if (tp->position_row_beats < min_position_beats) {
			min_position_beats = tp->position_row_beats;
			earliest_tp = tp;
		}
	}
}

void
MultiTrackPattern::update_global_nrows ()
{
	global_nrows = 0;
	for (size_t mti = 0; mti < tps.size(); mti++) {
		TrackPattern* tp = tps[mti];
		row_offset[mti] = (int32_t)tp->row_distance(earliest_tp->position_row_beats, tp->position_row_beats);
		nrows[mti] = tp->nrows;
		global_nrows = std::max(global_nrows, row_offset[mti] + nrows[mti]);
	}
}

bool
MultiTrackPattern::is_region_defined (uint32_t rowi, size_t mti) const
{
	return tps[mti]->is_region_defined (to_rri (rowi, mti));
}

bool
MultiTrackPattern::is_automation_defined (uint32_t rowi, size_t mti, const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param) ? is_region_defined (rowi, mti) : tps[mti]->is_defined (to_rri (rowi, mti));
}

Temporal::Beats
MultiTrackPattern::region_relative_beats (uint32_t rowi, size_t mti, size_t mri, int32_t delay) const
{
	return tps[mti]->region_relative_beats(to_rri(rowi, mti), mri, delay);
}

int64_t
MultiTrackPattern::region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mti, size_t mri) const
{
	return tps[mti]->region_relative_delay_ticks (event_time, to_rri(rowi, mti), mri);
}

int64_t
MultiTrackPattern::delay_ticks (samplepos_t when, uint32_t rowi, size_t mti) const
{
	return tps[mti]->delay_ticks_at_row (when, to_rri(rowi, mti));
}

uint32_t
MultiTrackPattern::sample_at_row_at_mti (uint32_t rowi, size_t mti, int32_t delay) const
{
	return tps[mti]->sample_at_row(to_rri(rowi, mti), delay);
}

size_t
MultiTrackPattern::off_notes_count (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern();
	return mtp->mrps[mri].np.off_notes[cgi].count(to_rrri(rowi, mti, mri));
}

size_t
MultiTrackPattern::on_notes_count (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern();
	return mtp->mrps[mri].np.on_notes[cgi].count(to_rrri(rowi, mti, mri));
}

bool
MultiTrackPattern::is_note_displayable (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern();
	return mtp->mrps[mri].np.is_displayable (to_rrri(rowi, mti, mri), cgi);
}

NoteTypePtr
MultiTrackPattern::off_note (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern();
	NotePattern::RowToNotes::const_iterator i_off = mtp->mrps[mri].np.off_notes[cgi].find(to_rrri(rowi, mti, mri));
	if (i_off != mtp->mrps[mri].np.off_notes[cgi].end())
		return i_off->second;
	return 0;
}

NoteTypePtr
MultiTrackPattern::on_note (uint32_t rowi, size_t mti, size_t mri, size_t cgi) const
{
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern();
	NotePattern::RowToNotes::const_iterator i_on = mtp->mrps[mri].np.on_notes[cgi].find(to_rrri(rowi, mti, mri));
	if (i_on != mtp->mrps[mri].np.on_notes[cgi].end())
		return i_on->second;
	return 0;
}

bool
MultiTrackPattern::is_auto_displayable (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const
{
	return tps[mti]->is_auto_displayable (to_rri(rowi, mti), mri, param);
}

size_t
MultiTrackPattern::get_automation_list_count (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const
{
	return tps[mti]->get_automation_list_count (to_rri(rowi, mti), mri, param);
}

Evoral::ControlEvent*
MultiTrackPattern::get_automation_control_event (uint32_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param) const
{
	return tps[mti]->get_automation_control_event (to_rri(rowi, mti), mri, param);
}

NoteTypePtr
MultiTrackPattern::find_prev_note(uint32_t rowi, size_t mti, size_t mri, int cgi) const
{
	return tps[mti]->midi_track_pattern()->mrps[mri].np.find_prev(to_rrri(rowi, mti, mri), cgi);
}

NoteTypePtr
MultiTrackPattern::find_next_note(uint32_t rowi, size_t mti, size_t mri, int cgi) const
{
	return tps[mti]->midi_track_pattern()->mrps[mri].np.find_next(to_rrri(rowi, mti, mri), cgi);
}

Temporal::Beats
MultiTrackPattern::next_off(uint32_t rowi, size_t mti, size_t mri, int cgi) const
{
	return tps[mti]->midi_track_pattern()->mrps[mri].np.next_off(to_rrri(rowi, mti, mri), cgi);
}

int
MultiTrackPattern::to_rri (uint32_t rowi, size_t mti) const
{
	return (int)rowi - row_offset[mti];
}

int
MultiTrackPattern::to_rrri(uint32_t rowi, size_t mti, size_t mri) const
{
	return (int)tps[mti]->to_rrri(to_rri(rowi, mti), mri);
}

int
MultiTrackPattern::to_rrri(uint32_t rowi, size_t mti) const
{
	return (int)tps[mti]->to_rrri(to_rri(rowi, mti));
}

int
MultiTrackPattern::to_mri(uint32_t rowi, size_t mti) const
{
	return tps[mti]->to_mri(to_rri(rowi, mti));
}

void
MultiTrackPattern::insert(size_t mti, const Evoral::Parameter& param)
{
	tps[mti]->insert(param);
}

boost::shared_ptr<ARDOUR::MidiModel>
MultiTrackPattern::midi_model (size_t mti, size_t mri)
{
	return tps[mti]->midi_track_pattern()->mrps[mri].midi_model;
}

boost::shared_ptr<ARDOUR::MidiRegion>
MultiTrackPattern::midi_region (size_t mti, size_t mri)
{
	return tps[mti]->midi_track_pattern()->mrps[mri].midi_region;
}

NotePattern&
MultiTrackPattern::note_pattern (size_t mti, size_t mri)
{
	return tps[mti]->midi_track_pattern()->mrps[mri].np;
}

void
MultiTrackPattern::apply_command (size_t mti, size_t mri, ARDOUR::MidiModel::NoteDiffCommand* cmd)
{
	midi_model(mti, mri)->apply_command (tracker_editor.session, cmd);
}

boost::shared_ptr<ARDOUR::AutomationList>
MultiTrackPattern::get_alist (int mti, int mri, const Evoral::Parameter& param)
{
	return tps[mti]->get_alist_at_mri (mri, param);
}

std::pair<double, bool>
MultiTrackPattern::get_automation_value (size_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param)
{
	return tps[mti]->get_automation_value (to_rri(rowi, mti), mri, param);
}

void
MultiTrackPattern::set_automation_value (double val, int rowi, int mti, int mri, const Evoral::Parameter& param, int delay)
{
	return tps[mti]->set_automation_value (val, to_rri(rowi, mti), mri, param, delay);
}

void
MultiTrackPattern::delete_automation_value (int rowi, int mti, int mri, const Evoral::Parameter& param)
{
	return tps[mti]->delete_automation_value (to_rri(rowi, mti), mri, param);
}

std::pair<int, bool>
MultiTrackPattern::get_automation_delay (size_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param)
{
	return tps[mti]->get_automation_delay (to_rri(rowi, mti), mri, param);
}

void
MultiTrackPattern::set_automation_delay (int delay, size_t rowi, size_t mti, size_t mri, const Evoral::Parameter& param)
{
	return tps[mti]->set_automation_delay (delay, to_rri(rowi, mti), mri, param);
}
