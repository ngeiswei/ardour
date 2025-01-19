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

#include "audio_region_view.h"
#include "midi_region_view.h"

#include "audio_track_pattern_phenomenal_diff.h"
#include "midi_track_pattern_phenomenal_diff.h"
#include "pattern.h"
#include "tracker_editor.h"

using namespace Tracker;

Pattern::Pattern (TrackerEditor& te, bool connect)
	: BasePattern (te,
	               TrackerUtils::get_position (te.region_selection),
	               Temporal::timepos_t (),
	               TrackerUtils::get_length (te.region_selection),
	               TrackerUtils::get_end (te.region_selection),
	               TrackerUtils::get_nt_last (te.region_selection))
	, earliest_mti (0)
	, earliest_tp (0)
	, global_nrows (0)
	, _connect (connect)
{
}

Pattern::~Pattern ()
{
	for (std::vector<TrackPattern*>::iterator it = tps.begin (); it != tps.end (); ++it) {
		delete *it;
	}
}

Pattern&
Pattern::operator= (const Pattern& other)
{
	if (!other.enabled) {
		enabled = false;
		return *this;
	}

	// BasePattern
	BasePattern::operator= (other);

	// Pattern
	assert (tps.size () == other.tps.size ());
	for (size_t mti = 0; mti < tps.size (); mti++) {
		if (tps[mti]->is_midi_track_pattern ()) {
			assert (other.tps[mti]->is_midi_track_pattern ());
			tps[mti]->midi_track_pattern ()->operator= (*other.tps[mti]->midi_track_pattern ());
		} else if (tps[mti]->is_audio_track_pattern ()) {
			assert (other.tps[mti]->is_audio_track_pattern ());
			tps[mti]->audio_track_pattern ()->operator= (*other.tps[mti]->audio_track_pattern ());
		} else {
			std::cout << "Not implemented" << std::endl;
		}
	}
	earliest_mti = other.earliest_mti;
	earliest_tp = other.tps[earliest_mti];
	row_offset = other.row_offset;
	tracks_nrows = other.tracks_nrows;
	global_nrows = other.global_nrows;

	return *this;
}

PatternPhenomenalDiff
Pattern::phenomenal_diff (const Pattern& prev) const
{
	PatternPhenomenalDiff diff;
	if (!prev.enabled && !enabled) {
		return diff;
	}

	diff.full = prev.enabled != enabled
		|| prev.global_nrows != global_nrows
		|| prev.tps.size () != tps.size ()
		|| prev.position_beats != position_beats
		|| prev.global_end_beats != global_end_beats;

	if (diff.full) {
		return diff;
	}

	// No global difference, let's look on a per track basis
	for (size_t mti = 0; mti < tps.size (); mti++) {
		// TODO: take care of the memory leak
		TrackPatternPhenomenalDiff* tp_diff = tps[mti]->phenomenal_diff_ptr (prev.tps[mti]);
		if (!tp_diff->empty ()) {
			diff.mti2tp_diff[mti] = tp_diff;
		}
	}
	return diff;
}

void
Pattern::setup ()
{
	setup_region_views_per_track ();
	setup_regions_per_track ();
	setup_track_patterns ();
	setup_row_offset ();
}

void
Pattern::setup_region_views_per_track ()
{
	// Associate track to its region selections
	for (RegionSelection::const_iterator it = tracker_editor.region_selection.begin (); it != tracker_editor.region_selection.end (); ++it) {
		TrackPtr track = dynamic_cast<RouteTimeAxisView&> ((*it)->get_time_axis_view ()).track ();
		std::vector<RegionView*>& region_views = region_views_per_track[track];
		if (std::find (region_views.begin (), region_views.end (), *it) == region_views.end ()) {
			region_views.push_back (*it);
		}
	}
}

void
Pattern::setup_regions_per_track ()
{
	regions_per_track.clear ();
	// Associate track to its regions
	for (RegionSelection::const_iterator it = tracker_editor.region_selection.begin (); it != tracker_editor.region_selection.end (); ++it) {
		RegionPtr region = (*it)->region ();
		TrackPtr track = dynamic_cast<RouteTimeAxisView&> ((*it)->get_time_axis_view ()).track ();
		regions_per_track[track].push_back (region);
	}
}

void
Pattern::setup_track_patterns ()
{
	// Disable and deselect all existing tracks
	set_enabled (false);
	set_selected (false);

	// Add new track or re-enable selected ones
	for (TrackRegionsMap::const_iterator it = regions_per_track.begin (); it != regions_per_track.end (); ++it) {
		TrackPtr track = it->first;
		TrackPattern* tp = find_track_pattern (track);
		if (tp) {
			tp->setup (it->second);
		} else {
			add_track_pattern (track, it->second);
		}
	}

	enabled = !regions_per_track.empty();
}

void
Pattern::add_track_pattern (TrackPtr track, const RegionSeq& regions)
{
	MidiTrackPtr midi_track = std::dynamic_pointer_cast<ARDOUR::MidiTrack> (track);
	if (midi_track) {
		// TODO: fix memory leak
		MidiTrackPattern* mtp = new MidiTrackPattern (tracker_editor, track, region_views_per_track[midi_track], regions,
		                                              position, length, end, nt_last, _connect);
		tps.push_back (mtp);
	}
	// NEXT.2: re-enable
	// AudioTrackPtr audio_track = std::dynamic_pointer_cast<ARDOUR::AudioTrack> (track);
	// if (audio_track) {
	// 	AudioTrackPattern* atp = new AudioTrackPattern (tracker_editor, track, regions,
	// 	                                                position_sample, length_sample, first_sample, last_sample, _connect);
	// 	tps.push_back (atp);
	// }
}

void
Pattern::setup_row_offset ()
{
	row_offset.resize (tps.size ());
	tracks_nrows.resize (tps.size ());
}

void
Pattern::update ()
{
	update_position_etc ();
	update_rows_per_beat ();
	set_row_range ();
	update_content ();
	update_earliest_mtp ();
	update_global_nrows ();
}

void
Pattern::update_position_etc ()
{
	position = TrackerUtils::get_position (regions_per_track);
	length = TrackerUtils::get_length (regions_per_track);
	end = TrackerUtils::get_end (regions_per_track);
	nt_last = TrackerUtils::get_nt_last (regions_per_track);
	for (size_t mti = 0; mti < tps.size (); mti++) {
		TrackPattern* tp = tps[mti];
		tp->position = position;
		tp->length = length;
		tp->end = end;
		tp->nt_last = nt_last;
	}
}

void
Pattern::update_rows_per_beat ()
{
	uint16_t rpb = tracker_editor.main_toolbar.rows_per_beat;
	BasePattern::set_rows_per_beat (rpb);
	for (size_t mti = 0; mti < tps.size (); mti++) {
		tps[mti]->set_rows_per_beat (rpb);
	}
}

void
Pattern::update_content ()
{
	for (size_t mti = 0; mti < tps.size (); mti++) {
		TrackPattern* tp = tps[mti];
		tp->update ();
	}

	// In case some tracks have been disabled, disable their track headers as
	// well.
	//
	// TODO: maybe optimize
	tracker_editor.grid_header->setup_track_headers ();
}

void
Pattern::update_earliest_mtp ()
{
	Temporal::Beats min_position_beats = std::numeric_limits<Temporal::Beats>::max ();
	for (size_t mti = 0; mti < tps.size (); mti++) {
		TrackPattern* tp = tps[mti];

		// Get min position beat
		if (tp->position_row_beats < min_position_beats) {
			min_position_beats = tp->position_row_beats;
			earliest_mti = mti;
			earliest_tp = tp;
		}
	}
}

void
Pattern::update_global_nrows ()
{
	global_nrows = 0;
	for (size_t mti = 0; mti < tps.size (); mti++) {
		TrackPattern* tp = tps[mti];
		row_offset[mti] = (int)tp->row_distance (earliest_tp->position_row_beats, tp->position_row_beats);
		tracks_nrows[mti] = tp->nrows;
		global_nrows = std::max (global_nrows, row_offset[mti] + tracks_nrows[mti]);
	}
}

bool
Pattern::is_region_defined (int rowi, int mti) const
{
	return tps[mti]->is_region_defined (to_rri (rowi, mti));
}

bool
Pattern::is_automation_defined (int rowi, int mti, const IDParameter& id_param) const
{
	const Evoral::Parameter& param = id_param.second;
	return TrackerUtils::is_region_automation (param) ? is_region_defined (rowi, mti) : tps[mti]->is_defined (to_rri (rowi, mti));
}

Temporal::Beats
Pattern::region_relative_beats (int rowi, int mti, int mri, int delay) const
{
	return tps[mti]->region_relative_beats (to_rri (rowi, mti), mri, delay);
}

int64_t
Pattern::region_relative_delay_ticks (const Temporal::Beats& event_time, int rowi, int mti, int mri) const
{
	return tps[mti]->region_relative_delay_ticks (event_time, to_rri (rowi, mti), mri);
}

int64_t
Pattern::delay_ticks (samplepos_t when, int rowi, int mti) const
{
	return tps[mti]->delay_ticks_at_row (when, to_rri (rowi, mti));
}

int
Pattern::sample_at_row_at_mti (int rowi, int mti, int delay) const
{
	return tps[mti]->sample_at_row (to_rri (rowi, mti), delay);
}

size_t
Pattern::off_notes_count (int rowi, int mti, int mri, int cgi) const
{
	if (rowi < 0 or mti < 0 or mri < 0 or cgi < 0)
		return 0;

	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	return mtp->mrps[mri]->mnp.off_notes[cgi].count (to_rrri (rowi, mti, mri));
}

size_t
Pattern::on_notes_count (int rowi, int mti, int mri, int cgi) const
{
	if (rowi < 0 or mti < 0 or mri < 0 or cgi < 0)
		return 0;

	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	return mtp->mrps[mri]->mnp.on_notes[cgi].count (to_rrri (rowi, mti, mri));
}

bool
Pattern::is_note_displayable (int rowi, int mti, int mri, int cgi) const
{
	if (rowi < 0 or mti < 0 or mri < 0 or cgi < 0)
		return 0;

	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	return mtp->mrps[mri]->mnp.is_displayable (to_rrri (rowi, mti, mri), cgi);
}

NotePtr
Pattern::off_note (int rowi, int mti, int mri, int cgi) const
{
	if ((int)tps.size () <= mti)
		return 0;
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	if (!mtp or (int)mtp->mrps.size () <= mri)
		return 0;
	const MidiRegionPattern& mrp = *mtp->mrps[mri];
	if ((int)mrp.mnp.off_notes.size () <= cgi)
		return 0;
	const RowToNotes& rtn = mrp.mnp.off_notes[cgi];
	RowToNotes::const_iterator i_off = rtn.find (to_rrri (rowi, mti, mri));
	if (i_off != rtn.end ()) {
		return i_off->second;
	}
	return 0;
}

NotePtr
Pattern::on_note (int rowi, int mti, int mri, int cgi) const
{
	if (mti < 0 && (int)tps.size () <= mti)
		return 0;
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	if (!mtp or (int)mtp->mrps.size () <= mri)
		return 0;
	const MidiRegionPattern& mrp = *mtp->mrps[mri];
	if ((int)mrp.mnp.on_notes.size () <= cgi)
		return 0;
	const RowToNotes& rtn = mrp.mnp.on_notes[cgi];
	RowToNotes::const_iterator i_on = rtn.find (to_rrri (rowi, mti, mri));
	if (i_on != rtn.end ()) {
		return i_on->second;
	}
	return 0;
}

RowToNotesRange
Pattern::off_notes_range (int row_idx, int mti, int mri, int cgi) const
{
	if (mti < 0 && (int)tps.size () <= mti)
		return RowToNotesRange ();
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	if (!mtp or (int)mtp->mrps.size () <= mri)
		return RowToNotesRange ();
	const MidiRegionPattern& mrp = *mtp->mrps[mri];
	if ((int)mrp.mnp.off_notes.size () <= cgi)
		return RowToNotesRange ();
	const RowToNotes& rtn = mrp.mnp.off_notes[cgi];
	return rtn.equal_range (to_rrri (row_idx, mti, mri));
}

RowToNotesRange
Pattern::on_notes_range (int row_idx, int mti, int mri, int cgi) const
{
	if (mti < 0 && (int)tps.size () <= mti)
		return RowToNotesRange ();
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	if (!mtp or (int)mtp->mrps.size () <= mri)
		return RowToNotesRange ();
	const MidiRegionPattern& mrp = *mtp->mrps[mri];
	if ((int)mrp.mnp.on_notes.size () <= cgi)
		return RowToNotesRange ();
	const RowToNotes& rtn = mrp.mnp.on_notes[cgi];
	return rtn.equal_range (to_rrri (row_idx, mti, mri));
}

bool
Pattern::is_automation_displayable (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return tps[mti]->is_automation_displayable (to_rri (rowi, mti), mri, id_param);
}

size_t
Pattern::control_events_count (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return tps[mti]->control_events_count (to_rri (rowi, mti), mri, id_param);
}

NotePtr
Pattern::find_prev_on_note (int rowi, int mti, int mri, int cgi) const
{
	return midi_region_pattern (mti, mri).mnp.find_prev_on (to_rrri (rowi, mti, mri), cgi);
}

NotePtr
Pattern::find_next_on_note (int rowi, int mti, int mri, int cgi) const
{
	return midi_region_pattern (mti, mri).mnp.find_next_on (to_rrri (rowi, mti, mri), cgi);
}

Temporal::Beats
Pattern::next_on_note_beats (int rowi, int mti, int mri, int cgi) const
{
	return midi_region_pattern (mti, mri).mnp.next_on_beats (to_rrri (rowi, mti, mri), cgi);
}

Temporal::Beats
Pattern::next_off_note_beats (int rowi, int mti, int mri, int cgi) const
{
	return midi_region_pattern (mti, mri).mnp.next_off_beats (to_rrri (rowi, mti, mri), cgi);
}

int
Pattern::to_rri (int rowi, int mti) const
{
	return rowi - row_offset[mti];
}

int
Pattern::to_rrri (int rowi, int mti, int mri) const
{
	return tps[mti]->to_rrri (to_rri (rowi, mti), mri);
}

int
Pattern::to_rrri (int rowi, int mti) const
{
	return tps[mti]->to_rrri (to_rri (rowi, mti));
}

int
Pattern::to_mri (int rowi, int mti) const
{
	return tps[mti]->to_mri (to_rri (rowi, mti));
}

void
Pattern::insert (int mti, const PBD::ID& id, const Evoral::Parameter& param)
{
	const IDParameter id_param(id, param);
	insert (mti, id_param);
}

void
Pattern::insert (int mti, const IDParameter& id_param)
{
	tps[mti]->insert (id_param);
}

MidiModelPtr
Pattern::midi_model (int mti, int mri)
{
	return midi_region_pattern (mti, mri).midi_model;
}

MidiRegionPtr
Pattern::midi_region (int mti, int mri)
{
	return midi_region_pattern (mti, mri).midi_region;
}

MidiNotesPattern&
Pattern::midi_notes_pattern (int mti, int mri)
{
	return midi_region_pattern (mti, mri).mnp;
}

const MidiNotesPattern&
Pattern::midi_notes_pattern (int mti, int mri) const
{
	return midi_region_pattern (mti, mri).mnp;
}

MidiRegionPattern&
Pattern::midi_region_pattern (int mti, int mri)
{
	return *tps[mti]->midi_track_pattern ()->mrps[mri];
}

const MidiRegionPattern&
Pattern::midi_region_pattern (int mti, int mri) const
{
	return *tps[mti]->midi_track_pattern ()->mrps[mri];
}

void
Pattern::apply_command (int mti, int mri, ARDOUR::MidiModel::NoteDiffCommand* cmd)
{
	midi_model (mti, mri)->apply_diff_command_as_commit (tracker_editor.session, cmd);
}

std::string
Pattern::get_name (int mti, const IDParameter& id_param) const
{
	return tps[mti]->get_name (id_param);
}

void
Pattern::set_param_enabled (int mti, const IDParameter& id_param, bool enabled)
{
	tps[mti]->set_param_enabled (id_param, enabled);
}

bool
Pattern::is_param_enabled (int mti, const IDParameter& id_param) const
{
	return tps[mti]->is_param_enabled (id_param);
}

IDParameterSet
Pattern::get_enabled_parameters (int mti, int mri) const
{
	return tps[mti]->get_enabled_parameters (mri);
}

std::vector<Temporal::BBT_Time>
Pattern::get_automation_bbt_seq (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return tps[mti]->get_automation_bbt_seq (to_rri (rowi, mti), mri, id_param);
}

std::pair<double, bool>
Pattern::get_automation_value (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return tps[mti]->get_automation_value (to_rri (rowi, mti), mri, id_param);
}

std::vector<double>
Pattern::get_automation_value_seq (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return tps[mti]->get_automation_value_seq (to_rri (rowi, mti), mri, id_param);
}

double
Pattern::get_automation_interpolation_value (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return tps[mti]->get_automation_interpolation_value (to_rri (rowi, mti), mri, id_param);
}

void
Pattern::set_automation_value (double val, int rowi, int mti, int mri, const IDParameter& id_param, int delay)
{
	return tps[mti]->set_automation_value (val, to_rri (rowi, mti), mri, id_param, delay);
}

void
Pattern::delete_automation_value (int rowi, int mti, int mri, const IDParameter& id_param)
{
	return tps[mti]->delete_automation_value (to_rri (rowi, mti), mri, id_param);
}

std::pair<int, bool>
Pattern::get_automation_delay (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return tps[mti]->get_automation_delay (to_rri (rowi, mti), mri, id_param);
}

std::vector<int>
Pattern::get_automation_delay_seq (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return tps[mti]->get_automation_delay_seq (to_rri (rowi, mti), mri, id_param);
}

void
Pattern::set_automation_delay (int delay, int rowi, int mti, int mri, const IDParameter& id_param)
{
	return tps[mti]->set_automation_delay (delay, to_rri (rowi, mti), mri, id_param);
}

TrackPattern*
Pattern::find_track_pattern (TrackPtr track)
{
	for (size_t mti = 0; mti < tps.size (); mti++) {
		if (tps[mti]->track == track) {
			return tps[mti];
		}
	}
	return 0;
}

void
Pattern::set_enabled (bool e)
{
	for (size_t mti = 0; mti < tps.size (); mti++) {
		tps[mti]->set_enabled (e);
	}
	enabled = e;
}

void
Pattern::set_selected (bool s)
{
	for (size_t mti = 0; mti < tps.size (); mti++) {
		tps[mti]->set_selected (s);
	}
}

std::string
Pattern::self_to_string () const
{
	std::stringstream ss;
	ss << "Pattern[" << this << "]";
	return ss.str ();
}

std::string
Pattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePattern::to_string (indent) << std::endl;

	std::string header = indent + self_to_string () + " ";
	for (size_t mti = 0; mti != tps.size (); mti++) {
		ss << header << "tps[" << mti << "]:" << std::endl
		   << tps[mti]->to_string (indent + "  ") << std::endl;
	}

	ss << header << "earliest_mti = " << earliest_mti << std::endl;
	ss << header << "earliest_tp = " << earliest_tp << std::endl;
	for (size_t i = 0; i != row_offset.size (); i++) {
		ss << header << "row_offset[" << i << "] = " << row_offset[i] << std::endl;
	}
	for (size_t i = 0; i != tracks_nrows.size (); i++) {
		ss << header << "tracks_nrows[" << i << "] = " << tracks_nrows[i] << std::endl;
	}
	ss << header << "global_nrows = " << global_nrows;

	return ss.str ();
}
