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

#include "ardour/session.h"

#include "audio_track_pattern.h"
#include "audio_track_pattern_phenomenal_diff.h"
#include "midi_track_pattern.h"
#include "midi_track_pattern_phenomenal_diff.h"
#include "pattern.h"

using namespace Tracker;

Pattern::Pattern (TrackerContext& ctx, const TrackRegionsMap& rpt, bool connect)
	: BasePattern (ctx,
	               TrackerUtils::get_position (rpt),
	               Temporal::timepos_t (),
	               TrackerUtils::get_length (rpt),
	               TrackerUtils::get_end (rpt),
	               TrackerUtils::get_nt_last (rpt))
	, regions_per_track (rpt)
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
Pattern::setup (const TrackRegionsMap& rpt)
{
	setup_regions_per_track (rpt);
	setup_track_patterns ();
	setup_row_offset ();
}

void
Pattern::setup_regions_per_track (const TrackRegionsMap& rpt)
{
	// Note: cannot use operator= on the map as Stripable::Sorter is not
	// assignable (const member)
	regions_per_track.clear ();
	for (TrackRegionsMap::const_iterator it = rpt.begin (); it != rpt.end (); ++it) {
		regions_per_track[it->first] = it->second;
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
		MidiTrackPattern* mtp = new MidiTrackPattern (context, track, regions,
		                                              position, length, end, nt_last, _connect);
		tps.push_back (mtp);
	}
	AudioTrackPtr audio_track = std::dynamic_pointer_cast<ARDOUR::AudioTrack> (track);
	if (audio_track) {
		// Only track automation (main and processor) is supported for audio
		// tracks for now, not audio region content. See AudioTrackPattern.
		AudioTrackPattern* atp = new AudioTrackPattern (context, track, regions,
		                                                position, length, end, nt_last, _connect);
		tps.push_back (atp);
	}
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
	set_rows_per_beat (rows_per_beat, false);
	set_row_range ();
	update_content ();
	update_earliest_mtp ();
	update_global_nrows ();
}

void
Pattern::update_track (int mti)
{
	// Content edit: region positions and the global time range are unchanged,
	// but recomputing them is cheap and keeps things in sync in case a region
	// was moved.  Only the given track's content is rebuilt.
	update_position_etc ();
	set_rows_per_beat (rows_per_beat, false);
	tps[mti]->update ();
	update_earliest_mtp ();
	update_global_nrows ();
}

void
Pattern::update_track_automations (int mti)
{
	// Automation edit: only the automation patterns change, the notes and the
	// global time range are unaffected.
	tps[mti]->update_automations ();
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
Pattern::set_rows_per_beat (uint16_t rpb, bool rfs)
{
	BasePattern::set_rows_per_beat (rpb, rfs);
	for (size_t mti = 0; mti < tps.size (); mti++) {
		tps[mti]->set_rows_per_beat (rpb, rfs);
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
	context.track_headers_changed ();
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
		return nullptr;
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	if (!mtp or (int)mtp->mrps.size () <= mri)
		return nullptr;
	const MidiRegionPattern& mrp = *mtp->mrps[mri];
	if ((int)mrp.mnp.off_notes.size () <= cgi)
		return nullptr;
	const RowToNotes& rtn = mrp.mnp.off_notes[cgi];
	RowToNotes::const_iterator i_off = rtn.find (to_rrri (rowi, mti, mri));
	if (i_off != rtn.end ()) {
		return i_off->second;
	}
	return nullptr;
}

NotePtr
Pattern::on_note (int rowi, int mti, int mri, int cgi) const
{
	if (mti < 0 && (int)tps.size () <= mti)
		return nullptr;
	const MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
	if (!mtp or (int)mtp->mrps.size () <= mri)
		return nullptr;
	const MidiRegionPattern& mrp = *mtp->mrps[mri];
	if ((int)mrp.mnp.on_notes.size () <= cgi)
		return nullptr;
	const RowToNotes& rtn = mrp.mnp.on_notes[cgi];
	RowToNotes::const_iterator i_on = rtn.find (to_rrri (rowi, mti, mri));
	if (i_on != rtn.end ()) {
		return i_on->second;
	}
	return nullptr;
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
	if (!cmd) {
		return;
	}

	midi_model (mti, mri)->apply_diff_command_as_commit (context.get_session (), cmd);

	// Capture the diff after applying: applying the command may add side
	// effect removals (notes that could not be re-added).  The command is now
	// owned by the session undo history, so it is still valid here.
	ARDOUR::MidiModel::NoteDiffCommand::NoteList added = cmd->added_notes ();
	ARDOUR::MidiModel::NoteDiffCommand::NoteList removed = cmd->removed_notes ();
	ARDOUR::MidiModel::NoteDiffCommand::ChangeList changes = cmd->changes ();

	// Update the note packing of the edited region incrementally instead of
	// re-reading the whole model (the expensive path).
	if (tps[mti]->is_midi_track_pattern ()) {
		MidiTrackPattern* mtp = tps[mti]->midi_track_pattern ();
		if (mri >= 0 && mri < (int)mtp->mrps.size ()) {
			mtp->mrps[mri]->mnp.apply_note_diff (added, removed, changes);
		}
	}
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
	return nullptr;
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
