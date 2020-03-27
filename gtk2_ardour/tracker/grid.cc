/*
 * Copyright (C) 2015-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include <cctype>
#include <cmath>
#include <iterator>
#include <map>

#include <boost/algorithm/string.hpp>

#include "pbd/file_utils.h"
#include "pbd/i18n.h"
#include "pbd/memento_command.h"

#include <pangomm/attributes.h>

#include <gtkmm/cellrenderercombo.h>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "evoral/Note.h"
#include "evoral/midi_util.h"

#include "ardour/amp.h"
#include "ardour/beats_samples_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/port_set.h"
#include "ardour/port.h"

#include "ardour_ui.h"
#include "axis_view.h"
#include "editor.h"
#include "midi_region_view.h"
#include "note_player.h"
#include "ui_config.h"

#include "midi_track_pattern_phenomenal_diff.h"
#include "track_toolbar.h"
#include "tracker_editor.h"
#include "tracker_utils.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;
using namespace Tracker;

const string Grid::note_off_str = "===";
const string Grid::undefined_str = "***";

Grid::Grid (TrackerEditor& te)
	: tracker_editor (te)
	, pattern (te, true /** connect **/)
	, prev_pattern (te, false)
	, current_beats (0)
	, current_path (1)
	, current_rowi (0)
	, current_col (0)
	, current_mti (0)
	, previous_mtp (0)
	, current_mtp (0)
	, current_mri (0)
	, current_cgi (0)
	, current_pos (0)
	, current_note_type (TrackerColumn::NOTE)
	, current_auto_type (TrackerColumn::AUTOMATION_SEPARATOR)
	, clock_pos (0)
	, edit_rowi (-1)
	, edit_col (-1)
	, edit_mti (-1)
	, edit_mtp (0)
	, edit_mri (-1)
	, edit_cgi (-1)
	, editing_editable (0)
	, last_keyval (GDK_VoidSymbol)
	, redisplay_grid_connect_call_enabled (true)
	, time_column (0)
{
}

Grid::~Grid ()
{
	delete time_column;
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		delete left_separator_columns[mti];
		delete region_name_columns[mti];
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
			delete note_columns[mti][cgi];
			delete channel_columns[mti][cgi];
			delete velocity_columns[mti][cgi];
			delete delay_columns[mti][cgi];
			delete note_separator_columns[mti][cgi];
		}
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {			
			delete automation_columns[mti][cgi];
			delete automation_delay_columns[mti][cgi];
			delete automation_separator_columns[mti][cgi];
		}
		delete right_separator_columns[mti];
		delete track_separator_columns[mti];
	}
}

Grid::GridModelColumns::GridModelColumns ()
{
	// The background color differs when the row is on beats and
	// bars. This is to keep track of it.
	add (_background_color);
	add (_family);
	add (_time_background_color);
	add (time);
	for (size_t mti /* multi track index */ = 0; mti < MAX_NUMBER_OF_TRACKS; mti++) {
		add (_left_right_separator_background_color[mti]);
		add (left_separator[mti]);
		add (region_name[mti]);
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
			add (note_name[mti][cgi]);
			add (_note_background_color[mti][cgi]);
			add (_note_foreground_color[mti][cgi]);
			add (channel[mti][cgi]);
			add (_channel_background_color[mti][cgi]);
			add (_channel_foreground_color[mti][cgi]);
			add (_channel_attributes[mti][cgi]);
			add (velocity[mti][cgi]);
			add (_velocity_background_color[mti][cgi]);
			add (_velocity_foreground_color[mti][cgi]);
			add (_velocity_attributes[mti][cgi]);
			add (delay[mti][cgi]);
			add (_delay_background_color[mti][cgi]);
			add (_delay_foreground_color[mti][cgi]);
			add (_delay_attributes[mti][cgi]);
			add (_empty);
		}
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
			add (automation[mti][cgi]);
			add (_automation_background_color[mti][cgi]);
			add (_automation_foreground_color[mti][cgi]);
			add (_automation_attributes[mti][cgi]);
			add (automation_delay[mti][cgi]);
			add (_automation_delay_background_color[mti][cgi]);
			add (_automation_delay_foreground_color[mti][cgi]);
			add (_automation_delay_attributes[mti][cgi]);
			add (_empty);
		}
		add (right_separator[mti]);
		add (track_separator[mti]);
	}
}

size_t
Grid::select_available_automation_column (size_t mti)
{
	// Find the next available column
	if (available_automation_columns[mti].empty ()) {
		cout << "Warning: no more available automation column" << endl;
		return 0;
	}
	set<size_t>::iterator it = available_automation_columns[mti].begin ();
	size_t column = *it;
	available_automation_columns[mti].erase (it);

	return column;
}

size_t
Grid::add_main_automation_column (size_t mti, const Evoral::Parameter& param)
{
	// Select the next available column
	size_t column = select_available_automation_column (mti);
	if (column == 0) {
		return column;
	}

	// Associate that column to the parameter
	col2params[mti].insert (IndexParamBimap::value_type (column, param)); // TODO: better put this knowledge in an inherited column

	// Set the column title
	get_column (column)->set_title (get_name (mti, param));

	return column;
}

size_t
Grid::add_midi_automation_column (size_t mti, const Evoral::Parameter& param)
{
	// Insert the corresponding automation control (and connect to the grid if
	// not already there)
	pattern.insert (mti, param);

	// Select the next available column
	size_t column = select_available_automation_column (mti);
	if (column == 0) {
		return column;
	}

	// Associate that column to the parameter
	col2params[mti].insert (IndexParamBimap::value_type (column, param));

	// Set the column title
	get_column (column)->set_title (get_name (mti, param));

	return column;
}

/**
 * Add an AutomationTimeAxisView to display automation for a processor's
 * parameter.
 */
void
Grid::add_processor_automation_column (size_t mti, ProcessorPtr processor, const Evoral::Parameter& param)
{
	ProcessorAutomationNode* pauno;

	if ((pauno = tracker_editor.grid_header->track_headers[mti]->track_toolbar->find_processor_automation_node (processor, param)) == 0) {
		/* session state may never have been saved with new plugin */
		error << _("programming error: ")
		      << string_compose (X_("processor automation column for %1:%2/%3/%4 not registered with track!"),
		                         processor->name (), param.type (), (int) param.channel (), param.id () )
		      << endmsg;
		abort (); /*NOTREACHED*/
		return;
	}

	if (pauno->column) {
		return;
	}

	// Find the next available column
	pauno->column = select_available_automation_column (mti);
	if (!pauno->column) {
		return;
	}

	// Associate that column to the parameter
	col2params[mti].insert (IndexParamBimap::value_type (pauno->column, param));

	// Set the column title
	get_column (pauno->column)->set_title (get_name (mti, param));
}

void
Grid::change_all_channel_tracks_visibility (size_t mti, bool yn, const Evoral::Parameter& param)
{
	if (!pattern.tps[mti]->is_midi_track_pattern ()) {
		return;
	}

	const uint16_t selected_channels = pattern.tps[mti]->midi_track ()->get_playback_channel_mask ();

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (param.type (), chn, param.id ());
			CheckMenuItem* menu = tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->automation_child_menu_item (fully_qualified_param);

			if (menu) {
				menu->set_active (yn);
			}
		}
	}
}

/** Toggle an automation column for a fully-specified Parameter (type,channel,id)
 *  Will add column if necessary.
 */
void
Grid::update_automation_column_visibility (size_t mti, const Evoral::Parameter& param)
{
	if (!pattern.tps[mti]->is_midi_track_pattern ()) {
		return;
	}

	// Find menu item associated to this parameter
	CheckMenuItem* mitem = tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->automation_child_menu_item (param);
	assert (mitem);
	const bool showit = mitem->get_active ();

	// Find the column associated to this parameter, assign one if necessary
	IndexParamBimap::right_const_iterator it = col2params[mti].right.find (param);
	size_t column = (it == col2params[mti].right.end ()) || (it->second == 0) ?
		add_midi_automation_column (mti, param) : it->second;

	// Still no column available, skip
	if (column == 0) {
		return;
	}

	set_automation_column_visible (mti, param, column, showit);

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

bool
Grid::is_automation_visible (size_t mti, const Evoral::Parameter& param) const
{
	IndexParamBimap::right_const_iterator it = col2params[mti].right.find (param);
	return it != col2params[mti].right.end () &&
		visible_automation_columns.find (it->second) != visible_automation_columns.end ();
}

void
Grid::set_automation_column_visible (size_t mti, const Evoral::Parameter& param, size_t column, bool showit)
{
	if (showit) {
		visible_automation_columns.insert (column);
	} else {
		visible_automation_columns.erase (column);
	}
	set_enabled (mti, param, showit);
}

bool
Grid::has_visible_automation (size_t mti) const
{
	for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
		size_t col = automation_colnum (mti, cgi);
		bool visible = TrackerUtils::is_in (col, visible_automation_columns);
		if (visible) {
			return true;
		}
	}
	return false;
}

bool
Grid::is_gain_visible (size_t mti) const
{
	return visible_automation_columns.find (gain_columns[mti])
		!= visible_automation_columns.end ();
};

bool
Grid::is_trim_visible (size_t mti) const
{
	return visible_automation_columns.find (trim_columns[mti])
		!= visible_automation_columns.end ();
};

bool
Grid::is_mute_visible (size_t mti) const
{
	return visible_automation_columns.find (mute_columns[mti])
		!= visible_automation_columns.end ();
};

bool
Grid::is_pan_visible (size_t mti) const
{
	bool visible = !pan_columns[mti].empty ();
	for (vector<size_t>::const_iterator it = pan_columns[mti].begin (); it != pan_columns[mti].end (); ++it) {
		visible = visible_automation_columns.find (*it) != visible_automation_columns.end ();
		if (!visible) {
			break;
		}
	}
	return visible;
};

void
Grid::update_gain_column_visibility (size_t mti)
{
	const bool showit = tracker_editor.grid_header->track_headers[mti]->track_toolbar->gain_automation_item->get_active ();

	if (gain_columns[mti] == 0) {
		gain_columns[mti] = add_main_automation_column (mti, Evoral::Parameter (GainAutomation));
	}

	// Still no column available, abort
	if (gain_columns[mti] == 0) {
		return;
	}

	set_automation_column_visible (mti, Evoral::Parameter (GainAutomation), gain_columns[mti], showit);

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

void
Grid::update_trim_column_visibility (size_t mti)
{
	const bool showit = tracker_editor.grid_header->track_headers[mti]->track_toolbar->trim_automation_item->get_active ();

	if (trim_columns[mti] == 0) {
		trim_columns[mti] = add_main_automation_column (mti, Evoral::Parameter (TrimAutomation));
	}

	// Still no column available, abort
	if (trim_columns[mti] == 0) {
		return;
	}

	set_automation_column_visible (mti, Evoral::Parameter (TrimAutomation), trim_columns[mti], showit);

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

void
Grid::update_mute_column_visibility (size_t mti)
{
	const bool showit = tracker_editor.grid_header->track_headers[mti]->track_toolbar->mute_automation_item->get_active ();

	if (mute_columns[mti] == 0) {
		mute_columns[mti] = add_main_automation_column (mti, Evoral::Parameter (MuteAutomation));
	}

	// Still no column available, abort
	if (mute_columns[mti] == 0) {
		return;
	}

	set_automation_column_visible (mti, Evoral::Parameter (MuteAutomation), mute_columns[mti], showit);

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

void
Grid::update_pan_columns_visibility (size_t mti)
{
	const bool showit = tracker_editor.grid_header->track_headers[mti]->track_toolbar->pan_automation_item->get_active ();

	if (pan_columns[mti].empty ()) {
		set<Evoral::Parameter> const & params = pattern.tps[mti]->track->panner ()->what_can_be_automated ();
		for (set<Evoral::Parameter>::const_iterator p = params.begin (); p != params.end (); ++p) {
			pan_columns[mti].push_back (add_main_automation_column (mti, *p));
		}
	}

	// Still no column available, abort
	if (pan_columns[mti].empty ()) {
		return;
	}

	for (vector<size_t>::const_iterator it = pan_columns[mti].begin (); it != pan_columns[mti].end (); ++it) {
		IndexParamBimap::left_const_iterator c2p_it = col2params[mti].left.find (*it);
		set_automation_column_visible (mti, c2p_it->second, *it, showit);
	}

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

void
Grid::redisplay_visible_note ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
			bool visible = pattern.tps[mti]->enabled
				&& pattern.tps[mti]->is_midi_track_pattern ()
				&& cgi < pattern.tps[mti]->midi_track_pattern ()->get_ntracks ()
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->visible_note;
			get_column (note_colnum (mti, cgi))->set_visible (visible);
		}
		if (tracker_editor.grid_header->track_headers[mti]->track_toolbar->is_midi_track_toolbar ()) {
			tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->update_visible_note_button ();
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();

	// Align track toolbar
	tracker_editor.grid_header->align ();
}

int
Grid::mti_col_offset (size_t mti) const
{
	return 1 /* time */
		+ mti * (1 /* left separator */ + 1 /* region name */ +
		         MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK * NUMBER_OF_COL_PER_NOTE_TRACK
		         + MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK * NUMBER_OF_COL_PER_AUTOMATION_TRACK
		         + 1 /* right separator */ + 1 /* track separator */);
}

int
Grid::left_separator_colnum (size_t mti) const
{
	return mti_col_offset (mti) + 1 /* left separator */;
}

void
Grid::redisplay_visible_left_separator (size_t mti) const
{
	left_separator_columns[mti]->set_visible (pattern.tps[mti]->enabled);
}

int
Grid::region_name_colnum (size_t mti) const
{
	return left_separator_colnum (mti) + 1 /* region name */;
}

int
Grid::note_colnum (size_t mti, size_t cgi) const
{
	return region_name_colnum (mti) + cgi * NUMBER_OF_COL_PER_NOTE_TRACK;
}

void
Grid::redisplay_visible_channel ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
			bool visible = pattern.tps[mti]->enabled
				&& pattern.tps[mti]->is_midi_track_pattern ()
				&& cgi < pattern.tps[mti]->midi_track_pattern ()->get_ntracks ()
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->visible_note
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->visible_channel;
			get_column (note_channel_colnum (mti, cgi))->set_visible (visible);
		}
		if (tracker_editor.grid_header->track_headers[mti]->track_toolbar->is_midi_track_toolbar ()) {
			tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->update_visible_channel_button ();
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();

	// Align track toolbar
	tracker_editor.grid_header->align ();
}

int
Grid::note_channel_colnum (size_t mti, size_t cgi) const
{
	return note_colnum (mti, cgi) + 1 /* channel */;
}

void
Grid::redisplay_visible_velocity ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
			bool visible = pattern.tps[mti]->enabled
				&& pattern.tps[mti]->is_midi_track_pattern ()
				&& cgi < pattern.tps[mti]->midi_track_pattern ()->get_ntracks ()
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->visible_note
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->visible_velocity;
			get_column (note_velocity_colnum (mti, cgi))->set_visible (visible);
		}
		if (tracker_editor.grid_header->track_headers[mti]->track_toolbar->is_midi_track_toolbar ()) {
			tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->update_visible_velocity_button ();
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();

	// Align track toolbar
	tracker_editor.grid_header->align ();
}

int
Grid::note_velocity_colnum (size_t mti, size_t cgi) const
{
	return note_channel_colnum (mti, cgi) + 1 /* velocity */;
}

void
Grid::redisplay_visible_delay ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
			bool visible = pattern.tps[mti]->enabled
				&& pattern.tps[mti]->is_midi_track_pattern ()
				&& cgi < pattern.tps[mti]->midi_track_pattern ()->get_ntracks ()
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->visible_note
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->visible_delay;
			get_column (note_delay_colnum (mti, cgi))->set_visible (visible);
		}
		tracker_editor.grid_header->track_headers[mti]->track_toolbar->update_visible_delay_button ();
	}
	// Do the same for automations
	redisplay_visible_automation_delay ();

	// Keep the window width to its minimum
	tracker_editor.resize_width ();

	// Align track toolbar
	tracker_editor.grid_header->align ();
}

int
Grid::note_delay_colnum (size_t mti, size_t cgi) const
{
	return note_velocity_colnum (mti, cgi) + 1 /* delay */;
}

void
Grid::redisplay_visible_note_separator ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
			bool visible = false;
			if (pattern.tps[mti]->enabled && pattern.tps[mti]->is_midi_track_pattern ()) {
				bool hva = has_visible_automation (mti);
				size_t ntracks = pattern.tps[mti]->midi_track_pattern ()->get_ntracks ();
				bool visible_note = tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->visible_note && 0 < ntracks;
				visible = visible_note && (cgi < ntracks - (hva ? 0 : 1));
			}
			get_column (note_separator_colnum (mti, cgi))->set_visible (visible);
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();
}

int
Grid::note_separator_colnum (size_t mti, size_t cgi) const
{
	return note_delay_colnum (mti, cgi) + 1 /* note group separator */;
}

void
Grid::redisplay_visible_automation ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
			size_t col = automation_colnum (mti, cgi);
			bool visible = pattern.tps[mti]->enabled && TrackerUtils::is_in (col, visible_automation_columns);
			get_column (col)->set_visible (visible);
		}
	}
	redisplay_visible_automation_delay ();

	// Keep the window width to its minimum
	tracker_editor.resize_width ();
}

size_t
Grid::automation_col_offset (size_t mti) const
{
	return mti_col_offset (mti) + 1 /* left separator */ + 1 /* region name */
		+ MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK * NUMBER_OF_COL_PER_NOTE_TRACK;
}

int
Grid::automation_colnum (size_t mti, size_t cgi) const
{
	return automation_col_offset (mti) + NUMBER_OF_COL_PER_AUTOMATION_TRACK * cgi;
}

void
Grid::redisplay_visible_automation_delay ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
			size_t col = automation_delay_colnum (mti, cgi);
			bool visible = pattern.tps[mti]->enabled
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->visible_delay
				&& TrackerUtils::is_in (col - 1, visible_automation_columns);
			get_column (col)->set_visible (visible);
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();

	// Align track toolbar
	tracker_editor.grid_header->align ();
}

int
Grid::automation_delay_colnum (size_t mti, size_t cgi) const
{
	return automation_colnum (mti, cgi) + 1 /* delay */;
}

void
Grid::redisplay_visible_automation_separator ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		size_t greatest_visible_col = 0;
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
			size_t col = automation_separator_colnum (mti, cgi);
			bool visible = pattern.tps[mti]->enabled && TrackerUtils::is_in (col - 2, visible_automation_columns);
			if (visible) {
				greatest_visible_col = std::max (greatest_visible_col, col);
			}
			get_column (col)->set_visible (visible);
		}
		if (0 < greatest_visible_col) {
			get_column (greatest_visible_col)->set_visible (false);
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();
}

int
Grid::automation_separator_colnum (size_t mti, size_t cgi) const
{
	return automation_delay_colnum (mti, cgi) + 1 /* automation group separator */;
}

int
Grid::right_separator_colnum (size_t mti) const
{
	return automation_col_offset (mti)
		+ MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK * NUMBER_OF_COL_PER_AUTOMATION_TRACK;
}

void
Grid::redisplay_visible_right_separator (size_t mti) const
{
	right_separator_columns[mti]->set_visible (pattern.tps[mti]->enabled);
}

void
Grid::init_columns ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		if (mti < left_separator_columns.size ()) {
			continue;
		}

		left_separator_columns.push_back (0);
		region_name_columns.push_back (0);
		note_columns.push_back (std::vector<NoteColumn*> (MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK, 0));
		channel_columns.push_back (std::vector<ChannelColumn*> (MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK, 0));
		velocity_columns.push_back (std::vector<VelocityColumn*> (MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK, 0));
		delay_columns.push_back (std::vector<DelayColumn*> (MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK, 0));
		note_separator_columns.push_back (std::vector<TreeViewColumn*> (MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK, 0));
		automation_columns.push_back (std::vector<AutomationColumn*> (MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK, 0));
		automation_delay_columns.push_back (std::vector<AutomationDelayColumn*> (MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK, 0));
		automation_separator_columns.push_back (std::vector<TreeViewColumn*> (MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK, 0));
		right_separator_columns.push_back (0);
		track_separator_columns.push_back (0);
	}
}

void
Grid::init_model ()
{
	if (!tracker_editor._first) {
		return;
	}

	model = ListStore::create (columns);
	set_model (model);
}

int
Grid::first_defined_col ()
{
	vector<TreeViewColumn*> cols = (vector<TreeViewColumn*>)get_columns ();

	for (int i = 0; i < (int)cols.size (); i++) {
		if (cols[i]->get_visible () && is_editable (cols[i]) && is_defined (current_path, cols[i])) {
			return i;
		}
	}

	return -1;
}

void
Grid::setup ()
{
	pattern.setup ();

	// Setup previous pattern in order to instantiate its tracks in
	// memory. However prev_pattern will never be updated, and only serve as
	// buffer to calculate phenomenal differences with the current (updated)
	// pattern.
	prev_pattern.setup ();

	init_columns ();
	init_model ();
	setup_time_column ();
	setup_data_columns ();
	connect_events ();
	setup_tree_view ();

	show ();
}

void
Grid::read_colors ()
{
	gtk_bases_color = UIConfiguration::instance ().color_str ("gtk_bases");
	beat_background_color = UIConfiguration::instance ().color_str ("tracker editor: beat background");
	bar_background_color = UIConfiguration::instance ().color_str ("tracker editor: bar background");
	background_color = UIConfiguration::instance ().color_str ("tracker editor: background");
	active_foreground_color = UIConfiguration::instance ().color_str ("tracker editor: active foreground");
	passive_foreground_color = UIConfiguration::instance ().color_str ("tracker editor: passive foreground");
	cursor_color = UIConfiguration::instance ().color_str ("tracker editor: cursor");
	current_row_color = UIConfiguration::instance ().color_str ("tracker editor: current row");
	current_edit_row_color = UIConfiguration::instance ().color_str ("tracker editor: current edit row");
}

void
Grid::redisplay_global_columns ()
{
	// If no global changes, then skip
	if (!_phenomenal_diff.full) {
		return;
	}

	// Set Time column, row background color, font
	TreeModel::Children::iterator row_it = model->children ().begin ();
	for (uint32_t rowi = 0; rowi < pattern.global_nrows; rowi++) {
		// Get existing row, or create one if it does exist
		if (row_it == model->children ().end ()) {
			row_it = model->append ();
		}
		TreeModel::Row row = *row_it++;

		Temporal::Beats row_beats = pattern.earliest_tp->beats_at_row (rowi);
		uint32_t row_sample = pattern.earliest_tp->sample_at_row (rowi);

		// Time
		Timecode::BBT_Time row_bbt;
		tracker_editor.session->bbt_time (row_sample, row_bbt);
		stringstream ss;
		print_padded (ss, row_bbt);
		row[columns.time] = ss.str ();

		// If the row is on a bar, beat or otherwise, the color differs
		bool is_row_beat = row_beats == row_beats.round_up_to_beat ();
		bool is_row_bar = row_bbt.beats == 1;
		string row_background_color = (is_row_beat ? (is_row_bar ? bar_background_color : beat_background_color) : background_color);
		row[columns._background_color] = row_background_color;

		// Set font family to Monospace (for region name for now)
		row[columns._family] = "Monospace";

		row[columns._time_background_color] = row_background_color;
	}
}

void
Grid::redisplay_grid ()
{
	// std::cout << "Grid::redisplay_grid ()" << std::endl;

	if (editing_editable) {
		return;
	}

	if (!tracker_editor.session) {
		return;
	}

	// In case the resolution (lines per beat) has changed
	// TODO: support multi track multi region
	tracker_editor.main_toolbar.delay_spinner.get_adjustment ()->set_lower (pattern.tps.front ()->delay_ticks_min ());
	tracker_editor.main_toolbar.delay_spinner.get_adjustment ()->set_upper (pattern.tps.front ()->delay_ticks_max ());

	// Load colors from config
	read_colors ();

	// Update pattern settings and content
	pattern.update ();

	// After update, compare pattern and prev_pattern to come up with a list of
	// differences to display. For now only worry about redisplaying the
	// changed mti.
	_phenomenal_diff = pattern.phenomenal_diff (prev_pattern);

	// std::cout << "Grid::redisplay_grid _phenomenal_diff:" << std::endl
	//           << _phenomenal_diff.to_string () << std::endl;

	// Set time column, row background colors and font
	redisplay_global_columns ();

	// Redisplay cell contents
	if (_phenomenal_diff.full) {
		for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
			redisplay_track (mti);
		}
	} else {
		for (MultiTrackPatternPhenomenalDiff::Mti2TrackPatternDiff::const_iterator it = _phenomenal_diff.mti2tp_diff.begin (); it != _phenomenal_diff.mti2tp_diff.end (); ++it) {
			redisplay_track (it->first, it->second);
		}
	}

	// Redisplay current row and cursor
	// TODO: optimize
	uint32_t current_rowi_from_beats = pattern.row_at_beats (current_beats);
	TreeModel::Children::iterator row_it = model->children ().begin ();
	for (uint32_t rowi = 0; rowi < pattern.global_nrows; rowi++) {

		// Get row
		TreeModel::Row row = *row_it++;

		// Used to draw the background of the current row and cursor
		if (rowi == current_rowi_from_beats) {
			if (current_col <= 0) {
				current_col = first_defined_col ();
				current_mti = get_mti (get_column (current_col));
			}
			current_rowi = current_rowi_from_beats;
			current_row = row;
			redisplay_current_row_background ();
			redisplay_current_cursor ();
		}
	}

	// Remove unused rows
	for (; row_it != model->children ().end ();) {
		row_it = model->erase (row_it);
	}

	set_model (model);

	// In case tracks have been added or removed
	redisplay_visible_note ();
	redisplay_visible_channel ();
	redisplay_visible_velocity ();
	redisplay_visible_delay ();
	redisplay_visible_note_separator ();
	redisplay_visible_automation ();
	redisplay_visible_automation_separator ();

	redisplay_left_right_separator_columns ();
	tracker_editor.grid_header->align ();

	// Save the state of pattern to prev_pattern. We may need to do a deep copy
	// into prev_pattern to make sure that the note pointers do capture a
	// different, if it exists.

	// std::cout << "pattern:" << std::endl
	//           << pattern.to_string ("  ") << std::endl;
	// std::cout << "prev_pattern:" << std::endl
	//           << prev_pattern.to_string ("  ") << std::endl;

	prev_pattern = pattern;
}

void
Grid::redisplay_grid_direct_call ()
{
	// std::cout << "Grid::redisplay_grid_direct_call [begin]" << std::endl;
	redisplay_grid ();
	// std::cout << "Grid::redisplay_grid_direct_call [end]" << std::endl;
}

void
Grid::redisplay_grid_connect_call ()
{
	// std::cout << "Grid::redisplay_grid_connect_call [begin]" << std::endl;
	if (redisplay_grid_connect_call_enabled) {
		redisplay_grid ();
	}
	// std::cout << "Grid::redisplay_grid_connect_call [end]" << std::endl;
}

void
Grid::redisplay_undefined_notes (TreeModel::Row& row, size_t mti)
{
	if (!pattern.tps[mti]->is_midi_track_pattern ()) {
		return;
	}

	// Number of column groups
	size_t ntracks = pattern.tps[mti]->midi_track_pattern ()->get_ntracks ();
	for (size_t cgi = 0; cgi < ntracks; cgi++) {
		redisplay_undefined_note (row, mti, cgi);
	}
}

void
Grid::redisplay_undefined_note (TreeModel::Row& row, size_t mti, size_t cgi)
{
	// cgi stands from column group index
	row[columns.note_name[mti][cgi]] = "";
	row[columns.channel[mti][cgi]] = "";
	row[columns.velocity[mti][cgi]] = "";
	row[columns.delay[mti][cgi]] = "";

	// TODO: replace gtk_bases_color by a custom one
	row[columns._note_background_color[mti][cgi]] = gtk_bases_color;
	row[columns._channel_background_color[mti][cgi]] = gtk_bases_color;
	row[columns._velocity_background_color[mti][cgi]] = gtk_bases_color;
	row[columns._delay_background_color[mti][cgi]] = gtk_bases_color;
}

void
Grid::redisplay_undefined_automations (TreeModel::Row& row, size_t rowi, size_t mti)
{
	if (!pattern.tps[mti]->is_midi_track_pattern ()) {
		return;
	}

	size_t mri = 0;              // consider the first one, all parameters
										  // should be the same in all other regions
	MidiRegionPattern& mrp = pattern.midi_region_pattern (mti, mri);
	AutomationPattern& ap = mrp.rap;
	for (AutomationPattern::ParamEnabledMap::const_iterator it = ap.param_to_enabled.begin (); it != ap.param_to_enabled.end (); ++it) {
		Evoral::Parameter param = it->first;
		if (it->second) {
			int cgi = get_cgi (mti, param);
			redisplay_undefined_automation (row, mti, cgi);
		}
	}
}

void
Grid::redisplay_track_separator (size_t mti)
{
	track_separator_columns[mti]->set_visible (pattern.tps[mti]->enabled);
}

void
Grid::redisplay_undefined_region_name (TreeModel::Row& row, size_t mti)
{
	row[columns.region_name[mti]] = "";
}

void
Grid::redisplay_left_right_separator_columns ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		redisplay_left_right_separator_columns (mti);
	}
}

void
Grid::redisplay_left_right_separator_columns (size_t mti)
{
	TreeModel::Children::iterator row_it = model->children ().begin ();
	for (uint32_t rowi = 0; rowi < pattern.global_nrows; rowi++) {
		TreeModel::Row row = *row_it++;
		redisplay_left_right_separator (row, mti);
	}
}

void
Grid::redisplay_left_right_separator (TreeModel::Row& row, size_t mti)
{
	// TODO: fix synchronization with track toolbar

	// Display track color for background
	row[columns._left_right_separator_background_color[mti]] =
		gdk_color_to_string (tracker_editor.public_editor.time_axis_view_from_stripable (pattern.tps[mti]->track)->color ().gobj ());

	// Align with track toolbar
	int track_header_width = tracker_editor.grid_header->track_headers[mti]->get_min_width ();
	int track_width = get_track_width (mti);
	int track_width_without_right = track_width - get_right_separator_width (mti);
	if (track_width_without_right + LEFT_RIGHT_SEPARATOR_WIDTH < track_header_width) {
		int diff = track_header_width - track_width_without_right;
		right_separator_columns[mti]->set_min_width (diff);
		right_separator_columns[mti]->set_max_width (diff);
	} else if (track_header_width < track_width_without_right + LEFT_RIGHT_SEPARATOR_WIDTH) {
		right_separator_columns[mti]->set_min_width (LEFT_RIGHT_SEPARATOR_WIDTH);
		right_separator_columns[mti]->set_max_width (LEFT_RIGHT_SEPARATOR_WIDTH);
	}
}

void
Grid::redisplay_undefined_automation (Gtk::TreeModel::Row& row, size_t mti, size_t cgi)
{
	// Set empty forground
	row[columns.automation[mti][cgi]] = "";
	row[columns.automation_delay[mti][cgi]] = "";

	// Set undefined background color
	row[columns._automation_background_color[mti][cgi]] = gtk_bases_color;
	row[columns._automation_delay_background_color[mti][cgi]] = gtk_bases_color;
}

// VT: remove when no longer useful
void
Grid::redisplay_automations (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri)
{
	// Render automation pattern
	for (int cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {

		// Display undefined
		if (!is_automation_defined (rowi, mti, cgi)) {
			redisplay_undefined_automation (row, mti, cgi);
			continue;
		}

		// Display defined
		Evoral::Parameter param = get_param (mti, cgi);
		size_t auto_count = pattern.get_automation_list_count (rowi, mti, mri, param);

		// Fill background colors
		redisplay_auto_background (row, mti, cgi);

		// Fill default blank foreground text and color
		redisplay_blank_auto_foreground (row, mti, cgi);

		if (auto_count > 0) {
			redisplay_automation (row, rowi, mti, mri, cgi, param);
		} else {
			redisplay_auto_interpolation (row, rowi, mti, mri, cgi, param);
		}
	}
}

void
Grid::redisplay_note_background (TreeModel::Row& row, size_t mti, size_t cgi)
{
	string row_background_color = row[columns._background_color];
	row[columns._note_background_color[mti][cgi]] = row_background_color;
	row[columns._channel_background_color[mti][cgi]] = row_background_color;
	row[columns._velocity_background_color[mti][cgi]] = row_background_color;
	row[columns._delay_background_color[mti][cgi]] = row_background_color;
}

void
Grid::redisplay_current_row_background ()
{
	string color = tracker_editor.main_toolbar.step_edit ? current_edit_row_color : current_row_color;
	redisplay_row_background_color (current_row, current_rowi, color);
}

void
Grid::redisplay_current_note_cursor (TreeModel::Row& row, size_t mti, size_t cgi)
{
	string color = cursor_color;

	switch (current_note_type) {
	case TrackerColumn::NOTE:
		row[columns._note_background_color[mti][cgi]] = color;
		break;
	case TrackerColumn::CHANNEL:
		row[columns._channel_background_color[mti][cgi]] = color;
		break;
	case TrackerColumn::VELOCITY:
		row[columns._velocity_background_color[mti][cgi]] = color;
		break;
	case TrackerColumn::DELAY:
		row[columns._delay_background_color[mti][cgi]] = color;
		break;
	default:
		cerr << "Grid::redisplay_current_note_cursor: Implementation Error!" << endl;
	}
}

void
Grid::redisplay_blank_note_foreground (TreeModel::Row& row, size_t mti, size_t cgi)
{
	// Fill with blank
	row[columns.note_name[mti][cgi]] = mk_blank (4);
	row[columns.channel[mti][cgi]] = mk_blank (2);
	row[columns.velocity[mti][cgi]] = mk_blank (3);
	row[columns.delay[mti][cgi]] = mk_blank (DELAY_DIGITS);

	// Grey out infoless cells
	row[columns._note_foreground_color[mti][cgi]] = passive_foreground_color;
	row[columns._channel_foreground_color[mti][cgi]] = passive_foreground_color;
	row[columns._velocity_foreground_color[mti][cgi]] = passive_foreground_color;
	row[columns._delay_foreground_color[mti][cgi]] = passive_foreground_color;
}

void
Grid::redisplay_auto_background (TreeModel::Row& row, size_t mti, size_t cgi)
{
	string row_background_color = row[columns._background_color];
	row[columns._automation_background_color[mti][cgi]] = row_background_color;
	row[columns._automation_delay_background_color[mti][cgi]] = row_background_color;
}

void
Grid::redisplay_note_foreground (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi)
{
	if (pattern.is_note_displayable (rowi, mti, mri, cgi)) {
		// Notes off
		NotePtr note = pattern.off_note (rowi, mti, mri, cgi);
		if (note) {
			row[columns.note_name[mti][cgi]] = note_off_str;
			row[columns._note_foreground_color[mti][cgi]] = active_foreground_color;
			int64_t delay = pattern.region_relative_delay_ticks (note->end_time (), rowi, mti, mri);
			if (delay != 0) {
				row[columns.delay[mti][cgi]] = TrackerUtils::num_to_string (delay);
				row[columns._delay_foreground_color[mti][cgi]] = active_foreground_color;
			}
		}

		// Notes on
		note = pattern.on_note (rowi, mti, mri, cgi);
		if (note) {
			row[columns.note_name[mti][cgi]] = ParameterDescriptor::midi_note_name (note->note ());
			row[columns._note_foreground_color[mti][cgi]] = active_foreground_color;
			row[columns.channel[mti][cgi]] = TrackerUtils::num_to_string (note->channel () + 1);
			row[columns._channel_foreground_color[mti][cgi]] = active_foreground_color;
			row[columns.velocity[mti][cgi]] = TrackerUtils::num_to_string ((int)note->velocity ());
			row[columns._velocity_foreground_color[mti][cgi]] = active_foreground_color;

			int64_t delay = pattern.region_relative_delay_ticks (note->time (), rowi, mti, mri);
			if (delay != 0) {
				row[columns.delay[mti][cgi]] = TrackerUtils::num_to_string (delay);
				row[columns._delay_foreground_color[mti][cgi]] = active_foreground_color;
			}
		}
	} else {
		// Too many notes, not displayable
		row[columns.note_name[mti][cgi]] = undefined_str;
		row[columns._note_foreground_color[mti][cgi]] = active_foreground_color;
	}
}

void
Grid::redisplay_current_auto_cursor (TreeModel::Row& row, size_t mti, size_t cgi)
{
	std::string color = cursor_color;

	switch (current_auto_type) {
	case TrackerColumn::AUTOMATION:
		row[columns._automation_background_color[mti][cgi]] = color;
		break;
	case TrackerColumn::AUTOMATION_DELAY:
		row[columns._automation_delay_background_color[mti][cgi]] = color;
		break;
	default:
		cerr << "Grid::redisplay_current_auto_cursor: Implementation Error!" << endl;
	}
}

void
Grid::redisplay_current_cursor ()
{
	if (current_auto_type == TrackerColumn::AUTOMATION_SEPARATOR) {
		redisplay_current_note_cursor (current_row, current_mti, current_cgi);
	} else {
		redisplay_current_auto_cursor (current_row, current_mti, current_cgi);
	}
}

void
Grid::redisplay_blank_auto_foreground (TreeModel::Row& row, size_t mti, size_t cgi)
{
	// Fill with blank
	row[columns.automation[mti][cgi]] = mk_blank (3);
	row[columns.automation_delay[mti][cgi]] = mk_blank (DELAY_DIGITS);

	// Fill default foreground color
	row[columns._automation_delay_foreground_color[mti][cgi]] = passive_foreground_color;
}

void
Grid::redisplay_automation (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi, const Evoral::Parameter& param)
{
	if (pattern.is_auto_displayable (rowi, mti, mri, param)) {
		Evoral::ControlEvent* ctl_event = pattern.get_automation_control_event (rowi, mti, mri, param);
		double aval = ctl_event->value;
		row[columns.automation[mti][cgi]] = TrackerUtils::num_to_string (aval);
		double awhen = ctl_event->when;
		int64_t delay = TrackerUtils::is_region_automation (param) ?
			pattern.region_relative_delay_ticks (Temporal::Beats (awhen), rowi, mti, mri)
			: pattern.delay_ticks ((samplepos_t)awhen, rowi, mti);
		if (delay != 0) {
			row[columns.automation_delay[mti][cgi]] = TrackerUtils::num_to_string (delay);
			row[columns._automation_delay_foreground_color[mti][cgi]] = active_foreground_color;
		}
	} else {
		row[columns.automation[mti][cgi]] = undefined_str;
	}
	row[columns._automation_foreground_color[mti][cgi]] = active_foreground_color;
}

void
Grid::redisplay_auto_interpolation (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi, const Evoral::Parameter& param)
{
	double inter_auto_val = get_automation_interpolation_value (rowi, mti, mri, param);
	if (is_int_param (param)) {
		row[columns.automation[mti][cgi]] = TrackerUtils::num_to_string (std::round (inter_auto_val));
	} else {
		row[columns.automation[mti][cgi]] = TrackerUtils::num_to_string (inter_auto_val);
	}
	row[columns._automation_foreground_color[mti][cgi]] = passive_foreground_color;
}

void
Grid::redisplay_cell_background (TreeModel::Row& row, size_t mti, size_t cgi)
{
	if (current_auto_type == TrackerColumn::AUTOMATION_SEPARATOR) {
		redisplay_note_background (row, mti, cgi);
	} else {
		redisplay_auto_background (row, mti, cgi);
	}
}

void
Grid::redisplay_row_background (Gtk::TreeModel::Row& row, uint32_t rowi)
{
	redisplay_row_background_color (row, rowi, row[columns._background_color]);
}

void
Grid::redisplay_row_background_color (Gtk::TreeModel::Row& row, uint32_t rowi, const std::string& color)
{
	row[columns._time_background_color] = color;
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		redisplay_row_mti_background_color (row, rowi, mti, color);
	}
}

void
Grid::redisplay_row_mti_background_color (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, const std::string& color)
{
	if (is_region_defined (rowi, mti)) {
		// Set current notes row background color
		redisplay_row_mti_notes_background_color (row, rowi, mti, color);

		// Set current region automations background color
		int mri = pattern.to_mri (rowi, mti);
		MidiRegionPattern& mrp = pattern.midi_region_pattern (mti, mri);
		redisplay_row_mti_automations_background_color (row, rowi, mti, mrp.rap, color);
	}

	// Set automation background color of the enabled track automations
	AutomationPattern& ap = *pattern.tps[mti];
	redisplay_row_mti_automations_background_color (row, rowi, mti, ap, color);
}

void
Grid::redisplay_row_mti_notes_background_color (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, const std::string& color)
{
	for (size_t cgi = 0; cgi < pattern.tps[mti]->midi_track_pattern ()->get_ntracks (); cgi++) {
		row[columns._note_background_color[mti][cgi]] = color;
		row[columns._channel_background_color[mti][cgi]] = color;
		row[columns._velocity_background_color[mti][cgi]] = color;
		row[columns._delay_background_color[mti][cgi]] = color;
	}
}

void
Grid::redisplay_row_mti_automations_background_color (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, const AutomationPattern& ap, const std::string& color)
{
	for (AutomationPattern::ParamEnabledMap::const_iterator it = ap.param_to_enabled.begin (); it != ap.param_to_enabled.end (); ++it) {
		Evoral::Parameter param = it->first;
		if (it->second) {
			int cgi = get_cgi (mti, param);
			row[columns._automation_background_color[mti][cgi]] = color;
			row[columns._automation_delay_background_color[mti][cgi]] = color;
		}
	}
}

void
Grid::redisplay_track (size_t mti, const TrackPatternPhenomenalDiff* tp_diff)
{
	if (!pattern.tps[mti]->enabled)
		return;

	if (pattern.tps[mti]->is_midi_track_pattern ()) {
		redisplay_midi_track (mti,
		                      *pattern.tps[mti]->midi_track_pattern (),
		                      tp_diff ? tp_diff->midi_track_pattern_phenomenal_diff () : 0);
	} else if (pattern.tps[mti]->is_audio_track_pattern ()) {
		redisplay_audio_track (mti,
		                       *pattern.tps[mti]->audio_track_pattern (),
		                       tp_diff ? tp_diff->audio_track_pattern_phenomenal_diff () : 0);
	} else {
		std::cout << "Not implemented" << std::endl;
	}
}

void
Grid::redisplay_midi_track (size_t mti, const MidiTrackPattern& mtp, const MidiTrackPatternPhenomenalDiff* mtp_diff)
{
	if (mtp_diff == 0 || mtp_diff->full) {
		redisplay_inter_midi_regions (mti);
		for (size_t mri = 0; mri < mtp.mrps.size (); mri++) {
			redisplay_midi_region (mti, mri, mtp.mrps[mri]);
		}
		redisplay_track_automations (mti, mtp);
	} else {
		// TODO: optimize redisplay_inter_midi_regions so that it only redisplay new inter midi regions
		redisplay_inter_midi_regions (mti);
		for (MidiTrackPatternPhenomenalDiff::Mri2MidiRegionPatternDiff::const_iterator it = mtp_diff->mri2mrp_diff.begin (); it != mtp_diff->mri2mrp_diff.end (); ++it) {
			size_t mri = it->first;
			redisplay_midi_region (mti, mri, mtp.mrps[mri], &it->second);
		}
		redisplay_track_automations (mti, mtp, &mtp_diff->auto_diff);
	}
}

void
Grid::redisplay_track_automations (size_t mti, const TrackAutomationPattern& tap, const AutomationPatternPhenomenalDiff* auto_diff)
{
	if (auto_diff == 0 || auto_diff->full) {
		for (AutomationPattern::ParamEnabledMap::const_iterator it = tap.param_to_enabled.begin (); it != tap.param_to_enabled.end (); ++it) {
			if (it->second) {
				redisplay_track_automation_param (mti, tap, it->first);
			}
		}
	} else {
		for (AutomationPatternPhenomenalDiff::Param2RowsPhenomenalDiff::const_iterator it = auto_diff->param2rows_diff.begin (); it != auto_diff->param2rows_diff.end (); ++it) {
			redisplay_track_automation_param (mti, tap, it->first, &it->second);
		}
	}
}

void
Grid::redisplay_track_automation_param (size_t mti, const TrackAutomationPattern& tap, const Evoral::Parameter& param, const RowsPhenomenalDiff* rows_diff)
{
	int cgi = get_cgi (mti, param);
	if (cgi < 0) {
		return;
	}

	if (rows_diff == 0 || rows_diff->full) {
		for (size_t rowi = 0; rowi < tap.nrows; rowi++) {
			redisplay_track_automation_param_row (mti, cgi, rowi, tap, param);
		}
	} else {
		for (std::set<size_t>::const_iterator it = rows_diff->rows.begin (); it != rows_diff->rows.end (); ++it) {
			redisplay_track_automation_param_row (mti, cgi, *it, tap, param);
		}
	}
}

void
Grid::redisplay_track_automation_param_row (size_t mti, size_t cgi, size_t rowi, const TrackAutomationPattern& tap, const Evoral::Parameter& param)
{
	// TODO: optimize!
	Gtk::TreeModel::Row row = get_row (rowi);
	int mri = -1;
	size_t auto_count = pattern.get_automation_list_count (rowi, mti, mri, param);

	// Fill background colors
	redisplay_auto_background (row, mti, cgi);

	// Fill default blank foreground text and color
	redisplay_blank_auto_foreground (row, mti, cgi);

	if (auto_count > 0) {
		redisplay_automation (row, rowi, mti, mri, cgi, param);
	} else {
		redisplay_auto_interpolation (row, rowi, mti, mri, cgi, param);
	}
}

void
Grid::redisplay_inter_midi_regions (size_t mti)
{
	TreeModel::Children::iterator row_it = model->children ().begin ();
	for (uint32_t rowi = 0; rowi < pattern.global_nrows; rowi++) {
		// Get row
		TreeModel::Row row = *row_it++;
		if (!is_region_defined (rowi, mti)) {
			redisplay_undefined_notes (row, mti);
			redisplay_undefined_automations (row, rowi, mti);
		}
	}
}

void
Grid::redisplay_midi_region (size_t mti, size_t mri, const MidiRegionPattern& mrp, const MidiRegionPatternPhenomenalDiff* mrp_diff)
{
	if (!pattern.midi_region_pattern (mti, mri).enabled)
		return;

	redisplay_region_notes (mti, mri, mrp.np, mrp_diff ? &mrp_diff->np_diff : 0);
	redisplay_region_automations (mti, mri, mrp.rap, mrp_diff ? &mrp_diff->rap_diff : 0);
}

void
Grid::redisplay_region_notes (size_t mti, size_t mri, const NotePattern& np, const NotePatternPhenomenalDiff* np_diff)
{
	if (np_diff == 0 || np_diff->full) {
		for (size_t cgi = 0; cgi < np.ntracks; cgi++) {
			redisplay_note_column (mti, mri, cgi, np);
		}
	} else {
		const NotePatternPhenomenalDiff::Cgi2RowsPhenomenalDiff& cgi2rows_diff = np_diff->cgi2rows_diff;
		for (NotePatternPhenomenalDiff::Cgi2RowsPhenomenalDiff::const_iterator it = cgi2rows_diff.begin (); it != cgi2rows_diff.end (); ++it) {
			redisplay_note_column (mti, mri, it->first, np, &it->second);
		}
	}
}

void
Grid::redisplay_region_automations (size_t mti, size_t mri, const RegionAutomationPattern& rap, const RegionAutomationPatternPhenomenalDiff* rap_diff)
{
	if (rap_diff == 0 || rap_diff->full || rap_diff->ap_diff.full) {
		for (AutomationPattern::ParamEnabledMap::const_iterator it = rap.param_to_enabled.begin (); it != rap.param_to_enabled.end (); ++it)
		{
			if (it->second) {
				redisplay_region_automation_param (mti, mri, rap, it->first);
			}
		}
	} else {
		const AutomationPatternPhenomenalDiff& ap_diff = rap_diff->ap_diff;
		for (AutomationPatternPhenomenalDiff::Param2RowsPhenomenalDiff::const_iterator it = ap_diff.param2rows_diff.begin (); it != ap_diff.param2rows_diff.end (); ++it)
		{
			redisplay_region_automation_param (mti, mri, rap, it->first, &it->second);
		}
	}
}

void
Grid::redisplay_region_automation_param (size_t mti, size_t mri, const RegionAutomationPattern& rap, const Evoral::Parameter& param, const RowsPhenomenalDiff* rows_diff)
{
	int cgi = get_cgi (mti, param);

	if (cgi < 0) {
		return;
	}

	size_t row_offset = get_row_offset (mti, mri);
	if (rows_diff == 0 || rows_diff->full) {
		for (size_t rowi = 0; rowi < rap.nrows; rowi++) {
			redisplay_region_automation_param_row (mti, mri, cgi, rowi + row_offset, rap, param);
		}
	} else {
		for (std::set<size_t>::const_iterator it = rows_diff->rows.begin (); it != rows_diff->rows.end (); ++it) {
			redisplay_region_automation_param_row (mti, mri, cgi, *it + row_offset, rap, param);
		}
	}
}

void
Grid::redisplay_region_automation_param_row (size_t mti, size_t mri, size_t cgi, size_t rowi, const RegionAutomationPattern& rap, const Evoral::Parameter& param)
{
	// TODO: optimize!
	Gtk::TreeModel::Row row = get_row (rowi);
	size_t auto_count = pattern.get_automation_list_count (rowi, mti, mri, param);

	// Fill background colors
	redisplay_auto_background (row, mti, cgi);

	// Fill default blank foreground text and color
	redisplay_blank_auto_foreground (row, mti, cgi);

	if (auto_count > 0) {
		redisplay_automation (row, rowi, mti, mri, cgi, param);
	} else {
		redisplay_auto_interpolation (row, rowi, mti, mri, cgi, param);
	}
}

void
Grid::redisplay_note_column (size_t mti, size_t mri, size_t cgi, const NotePattern& np, const RowsPhenomenalDiff* rows_diff)
{
	size_t row_offset = get_row_offset (mti, mri);
	if (rows_diff == 0 || rows_diff->full) {
		for (size_t rrrowi = 0; rrrowi < get_row_size (mti, mri); rrrowi++) {
			redisplay_note (mti, mri, cgi, row_offset + rrrowi, np);
		}
	} else {
		for (std::set<size_t>::const_iterator row_it = rows_diff->rows.begin (); row_it != rows_diff->rows.end (); ++row_it) {
			redisplay_note (mti, mri, cgi, row_offset + *row_it, np);
		}
	}
}

void
Grid::redisplay_note (size_t mti, size_t mri, size_t cgi, size_t rowi, const NotePattern& np)
{
	// TODO: optimize!
	Gtk::TreeModel::Row row = get_row (rowi);

	// Fill background colors
	// TODO: optimize, should only need to redisplay note bg once
	redisplay_note_background (row, mti, cgi);

	// Fill with blank foreground text and colors
	redisplay_blank_note_foreground (row, mti, cgi);

	// Display note
	size_t off_notes_count = pattern.off_notes_count (rowi, mti, mri, cgi);
	size_t on_notes_count = pattern.on_notes_count (rowi, mti, mri, cgi);
	if (0 < on_notes_count || 0 < off_notes_count) {
		redisplay_note_foreground (row, rowi, mti, mri, cgi);
	}
}

void
Grid::unset_underline_current_step_edit_cell ()
{
	if (current_auto_type == TrackerColumn::AUTOMATION_SEPARATOR) {
		unset_underline_current_step_edit_note_cell ();
	} else {
		unset_underline_current_step_edit_auto_cell ();
	}
}

void
Grid::unset_underline_current_step_edit_note_cell ()
{
	Gtk::TreeModel::Row& row = current_row;
	size_t mti = current_mti;
	size_t cgi = current_cgi;
	switch (current_note_type) {
	case TrackerColumn::NOTE:
		break;
	case TrackerColumn::CHANNEL: {
		std::string val_str = row[columns.channel[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		row[columns.channel[mti][cgi]] = TrackerUtils::int_unpad (val_str);
		row[columns._channel_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	case TrackerColumn::VELOCITY: {
		std::string val_str = row[columns.velocity[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		row[columns.velocity[mti][cgi]] = TrackerUtils::int_unpad (val_str);
		row[columns._velocity_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	case TrackerColumn::DELAY: {
		std::string val_str = row[columns.delay[mti][cgi]];
		if (is_blank (val_str)) {
			// For some unknown reason the attributes must be reset
			row[columns._delay_attributes[mti][cgi]] = Pango::AttrList ();
			break;
		}
		bool is_null = std::stoi (val_str) == 0;
		row[columns.delay[mti][cgi]] = is_null ? mk_blank (DELAY_DIGITS) : TrackerUtils::int_unpad (val_str);
		row[columns._delay_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	default:
		cerr << "Grid::redisplay_current_note_cursor: Implementation Error!" << endl;
	}
}

void
Grid::unset_underline_current_step_edit_auto_cell ()
{
	Gtk::TreeModel::Row& row = current_row;
	size_t mti = current_mti;
	size_t cgi = current_cgi;
	switch (current_auto_type) {
	case TrackerColumn::AUTOMATION: {
		std::string val_str = row[columns.automation[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		row[columns.automation[mti][cgi]] = TrackerUtils::float_unpad (val_str);
		row[columns._automation_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	case TrackerColumn::AUTOMATION_DELAY: {
		std::string val_str = row[columns.automation_delay[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		bool is_null = std::stoi (val_str) == 0;
		row[columns.automation_delay[mti][cgi]] = is_null ? mk_blank (DELAY_DIGITS) : TrackerUtils::int_unpad (val_str);
		row[columns._automation_delay_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	default:
		cerr << "Grid::redisplay_current_auto_cursor: Implementation Error!" << endl;
	}
}

void
Grid::set_underline_current_step_edit_cell ()
{
	if (tracker_editor.main_toolbar.step_edit) {
		if (current_auto_type == TrackerColumn::AUTOMATION_SEPARATOR) {
			set_underline_current_step_edit_note_cell ();
		} else {
			set_underline_current_step_edit_auto_cell ();
		}
	}
}

void
Grid::set_underline_current_step_edit_note_cell ()
{
	Gtk::TreeModel::Row& row = current_row;
	int rowi = current_rowi;
	int mti = current_mti;
	int cgi = current_cgi;
	switch (current_note_type) {
	case TrackerColumn::NOTE:
		break;
	case TrackerColumn::CHANNEL: {
		std::string val_str = row[columns.channel[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		set_current_pos (0, 1);
		std::pair<std::string, Pango::AttrList> ul = underlined_value (val_str);
		row[columns.channel[mti][cgi]] = ul.first;
		row[columns._channel_attributes[mti][cgi]] = ul.second;
		break;
	}
	case TrackerColumn::VELOCITY: {
		std::string val_str = row[columns.velocity[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		set_current_pos (0, 2);
		std::pair<std::string, Pango::AttrList> ul = underlined_value (val_str);
		row[columns.velocity[mti][cgi]] = ul.first;
		row[columns._velocity_attributes[mti][cgi]] = ul.second;
		break;
	}
	case TrackerColumn::DELAY: {
		std::string val_str = row[columns.delay[mti][cgi]];
		NotePtr note = get_note (rowi, mti, cgi);
		if (!note) {
			break;
		}
		if (is_blank (val_str)) {
			val_str = "0";
		}
		set_current_pos (0, 3);
		std::pair<std::string, Pango::AttrList> ul = underlined_value (val_str);
		row[columns.delay[mti][cgi]] = ul.first;
		row[columns._delay_attributes[mti][cgi]] = ul.second;
		break;
	}
	default:
		cerr << "Grid::redisplay_current_note_cursor: Implementation Error!" << endl;
	}
}

void
Grid::set_underline_current_step_edit_auto_cell ()
{
	Gtk::TreeModel::Row& row = current_row;
	int rowi = current_rowi;
	int mti = current_mti;
	int mri = current_mri;
	int cgi = current_cgi;
	switch (current_auto_type) {
	case TrackerColumn::AUTOMATION: {
		std::string val_str = row[columns.automation[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		Evoral::Parameter param = get_param (mti, cgi);
		double l = lower (current_row, mti, param);
		double u = upper (current_row, mti, param);
		double mlu = std::max (std::abs (l), std::abs (u));
		int min_pos = is_int_param (param) ? 0 : MainToolbar::min_position;
		int max_pos = std::floor (std::log10 (mlu));
		set_current_pos (min_pos, max_pos);
		std::pair<std::string, Pango::AttrList> ul = underlined_value (val_str);
		row[columns.automation[mti][cgi]] = ul.first;
		row[columns._automation_attributes[mti][cgi]] = ul.second;
		break;
	}
	case TrackerColumn::AUTOMATION_DELAY: {
		std::string val_str = row[columns.automation_delay[mti][cgi]];
		if (!has_automation_delay (rowi, mti, mri, cgi)) {
			break;
		}
		if (is_blank (val_str)) {
			val_str = "0";
		}
		set_current_pos (0, 3);
		std::pair<std::string, Pango::AttrList> ul = underlined_value (val_str);
		row[columns.automation_delay[mti][cgi]] = ul.first;
		row[columns._automation_delay_attributes[mti][cgi]] = ul.second;
		break;
	}
	default:
		cerr << "Grid::redisplay_current_auto_cursor: Implementation Error!" << endl;
	}
}

void
Grid::redisplay_audio_track (size_t mti, const AudioTrackPattern& atp, const AudioTrackPatternPhenomenalDiff* atp_diff)
{
	// VT: implement
}

int
Grid::get_time_width () const
{
	return time_column->get_width ();
}

int
Grid::get_track_width (size_t mti) const
{
	int width = 0;
	width += left_separator_columns[mti]->get_width ();
	if (region_name_columns[mti]->get_visible ()) {
		width += region_name_columns[mti]->get_width ();
	}
	for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
		if (note_columns[mti][cgi]->get_visible ()) {
			width += note_columns[mti][cgi]->get_width ();
		}
		if (channel_columns[mti][cgi]->get_visible ()) {
			width += channel_columns[mti][cgi]->get_width ();
		}
		if (velocity_columns[mti][cgi]->get_visible ()) {
			width += velocity_columns[mti][cgi]->get_width ();
		}
		if (delay_columns[mti][cgi]->get_visible ()) {
			width += delay_columns[mti][cgi]->get_width ();
		}
		if (note_separator_columns[mti][cgi]->get_visible ()) {
			width += note_separator_columns[mti][cgi]->get_width ();
		}
	}
	for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
		if (automation_columns[mti][cgi]->get_visible ()) {
			width += automation_columns[mti][cgi]->get_width ();
		}
		if (automation_delay_columns[mti][cgi]->get_visible ()) {
			width += automation_delay_columns[mti][cgi]->get_width ();
		}
		if (automation_separator_columns[mti][cgi]->get_visible ()) {
			width += automation_separator_columns[mti][cgi]->get_width ();
		}
	}
	width += right_separator_columns[mti]->get_width ();
	return width;
}

int
Grid::get_right_separator_width (size_t mti) const
{
	return right_separator_columns[mti]->get_width ();
}

int
Grid::get_track_separator_width () const
{
	return TRACK_SEPARATOR_WIDTH;
}

std::string
Grid::get_name (size_t mti, const Evoral::Parameter& param) const
{
	return pattern.tps[mti]->get_name (param);
}

void
Grid::set_enabled (size_t mti, const Evoral::Parameter& param, bool enabled)
{
	pattern.tps[mti]->set_enabled (param, enabled);
}

bool
Grid::is_enabled (size_t mti, const Evoral::Parameter& param) const
{
	return pattern.tps[mti]->is_enabled (param);
}

Pango::AttrList
Grid::char_underline (int ul_idx) const
{
	Pango::Attribute ul_attr = Pango::Attribute::create_attr_underline (Pango::UNDERLINE_LOW);
	ul_attr.set_start_index (ul_idx);
	ul_attr.set_end_index (ul_idx + 1);
	Pango::AttrList attributes;
	attributes.insert (ul_attr);
	return attributes;
}

std::pair<std::string, Pango::AttrList>
Grid::underlined_value (int val) const
{
	return underlined_value (TrackerUtils::num_to_string (val));
}

std::pair<std::string, Pango::AttrList>
Grid::underlined_value (float val) const
{
	return underlined_value (TrackerUtils::num_to_string (val));
}

std::pair<std::string, Pango::AttrList>
Grid::underlined_value (const std::string& val_str) const
{
	std::string padded_val_str = TrackerUtils::pad (val_str, current_pos);
	size_t point_pos = TrackerUtils::point_position (padded_val_str);
	int ul_idx = point_pos - current_pos - (current_pos < 0 ? 0 : 1);
	return make_pair (padded_val_str, char_underline (ul_idx));
}

bool
Grid::is_int_param (const Evoral::Parameter& param) const
{
	return TrackerUtils::is_region_automation (param);
}

/////////////////////
// Edit Pattern    //
/////////////////////

uint32_t
Grid::get_row_index (const string& path) const
{
	return get_row_index (TreeModel::Path (path));
}

uint32_t
Grid::get_row_index (const TreeModel::Path& path) const
{
	return path.front ();
}

uint32_t
Grid::get_row_offset (size_t mti, size_t mri) const
{
	return pattern.row_offset[mti] + pattern.tps[mti]->midi_track_pattern ()->row_offset[mri];
}

uint32_t
Grid::get_row_size (size_t mti, size_t mri) const
{
	return pattern.tps[mti]->midi_track_pattern ()->mrps[mri].nrows;
}

Gtk::TreeModel::Row
Grid::get_row (uint32_t row_idx) const
{
	TreeModel::Children::const_iterator row_it = model->children ().begin ();
	std::advance (row_it, (int)row_idx);
	return *row_it;
}

int
Grid::get_col_index (const TreeViewColumn* col)
{
	vector<TreeViewColumn*> cols = (vector<TreeViewColumn*>)get_columns ();

	for (int i = 0; i < (int)cols.size (); i++) {
		if (cols[i] == col) {
			return i;
		}
	}

	return -1;
}

NotePtr
Grid::get_on_note (const string& path, int mti, int cgi)
{
	return get_on_note (TreeModel::Path (path), mti, cgi);
}

NotePtr
Grid::get_on_note (const TreeModel::Path& path, int mti, int cgi)
{
	return get_on_note (get_row_index (path), mti, cgi);
}

NotePtr
Grid::get_on_note (int rowi, int mti, int cgi)
{
	return pattern.on_note (rowi, mti, pattern.to_mri (rowi, mti), cgi);
}

NotePtr
Grid::get_off_note (const string& path, int mti, int cgi)
{
	return get_off_note (TreeModel::Path (path), mti, cgi);
}

NotePtr
Grid::get_off_note (const TreeModel::Path& path, int mti, int cgi)
{
	return get_off_note (get_row_index (path), mti, cgi);
}

NotePtr
Grid::get_off_note (int rowi, int mti, int cgi)
{
	return pattern.off_note (rowi, mti, pattern.to_mri (rowi, mti), cgi);
}

NotePtr
Grid::get_note (int rowi, int mti, int cgi)
{
	return get_note (TreeModel::Path (1U, rowi), mti, cgi);
}

NotePtr
Grid::get_note (const string& path, int mti, int cgi)
{
	return get_note (TreeModel::Path (path), mti, cgi);
}

NotePtr
Grid::get_note (const TreeModel::Path& path, int mti, int cgi)
{
	NotePtr on_note = get_on_note (path, mti, cgi);
	if (on_note) {
		return on_note;
	}
	return get_off_note (path, mti, cgi);
}

void
Grid::editing_note_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = note_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_note_channel_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = note_channel_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_note_velocity_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = note_velocity_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_note_delay_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = note_delay_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_automation_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = automation_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_automation_delay_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = automation_delay_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_path = TreePath (path);
	edit_rowi = get_row_index (edit_path);
	edit_mti = mti;
	edit_mtp = pattern.tps[edit_mti];
	edit_mri = pattern.to_mri (edit_rowi, edit_mti);
	edit_cgi = cgi;
	editing_editable = ed;

	// For now set the current cursor to the edit cursor
	current_mti = mti;
}

void
Grid::clear_editables ()
{
	edit_path.clear ();
	edit_rowi = -1;
	edit_col = -1;
	edit_mti = -1;
	edit_mtp = 0;
	edit_mri = -1;
	edit_cgi = -1;
	editing_editable = 0;

	redisplay_grid_direct_call ();
}

void
Grid::editing_canceled ()
{
	clear_editables ();
}

uint8_t
Grid::parse_pitch (const string& text) const
{
	return TrackerUtils::parse_pitch (text, tracker_editor.main_toolbar.octave_spinner.get_value_as_int ());
}

void
Grid::note_edited (const string& path, const string& text)
{
	string norm_text = boost::erase_all_copy (text, " ");
	bool is_del = norm_text.empty ();
	bool is_off = !is_del && (norm_text[0] == note_off_str[0]);
	uint8_t pitch = parse_pitch (norm_text);
	bool is_on = pitch <= 127;

	// Can't edit ***
	if (!pattern.is_note_displayable (edit_rowi, edit_mti, edit_mri, edit_cgi)) {
		clear_editables ();
		return;
	}

	if (is_on) {
		set_on_note (pitch, edit_rowi, edit_mti, edit_mri, edit_cgi);
	} else if (is_off) {
		set_off_note (edit_rowi, edit_mti, edit_mri, edit_cgi);
	} else if (is_del) {
		delete_note (edit_rowi, edit_mti, edit_mri, edit_cgi);
	}

	clear_editables ();
}

std::pair<uint8_t, uint8_t>
Grid::set_on_note (uint8_t pitch, int rowi, int mti, int mri, int cgi)
{
	uint8_t channel = tracker_editor.main_toolbar.channel_spinner.get_value_as_int () - 1;
	uint8_t velocity = tracker_editor.main_toolbar.velocity_spinner.get_value_as_int ();
	return set_on_note (pitch, channel, velocity, rowi, mti, mri, cgi);
}

std::pair<uint8_t, uint8_t>
Grid::set_on_note (uint8_t pitch, uint8_t ch, uint8_t vel, int rowi, int mti, int mri, int cgi)
{
	uint8_t new_ch = ch;
	uint8_t new_vel = vel;

	// Abort if the new pitch is invalid
	if (127 < pitch) {
		return std::make_pair(0, 0);
	}

	NotePtr on_note = get_on_note (rowi, mti, cgi);
	NotePtr off_note = get_off_note (rowi, mti, cgi);

	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int ();

	MidiModel::NoteDiffCommand* cmd = 0;

	if (on_note) {
		// Change the pitch of the on note
		char const * opname = _("change note");
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->change (on_note, MidiModel::NoteDiffCommand::NoteNumber, pitch);
		if (tracker_editor.main_toolbar.overwrite_existing) {
			cmd->change (on_note, MidiModel::NoteDiffCommand::Channel, ch);
			cmd->change (on_note, MidiModel::NoteDiffCommand::Velocity, vel);
			// VVT: take care of delay
		} else {
			new_ch = on_note->channel ();
			new_vel = on_note->velocity ();
		}
	} else if (off_note) {
		// Replace off note by another (non-off) note. Calculate the start
		// time and length of the new on note.
		Temporal::Beats start = off_note->end_time ();
			// VVT: take care of delay
		Temporal::Beats end = pattern.next_off (rowi, mti, mri, cgi);
		Temporal::Beats length = end - start;
		// Build note using defaults
		NotePtr new_note (new NoteType (ch, start, length, pitch, vel));
		char const * opname = _("add note");
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->add (new_note);
		// Pre-emptively add the note in the pattern so that it knows in which
		// track it is supposed to be.
		pattern.note_pattern (mti, mri).add (cgi, new_note);
	} else {
		// Create a new on note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = pattern.region_relative_beats (rowi, mti, mri, delay);
		NotePtr prev_note = pattern.find_prev_note (rowi, mti, mri, cgi);
		Temporal::Beats prev_start;
		Temporal::Beats prev_end;
		if (prev_note) {
			prev_start = prev_note->time ();
			prev_end = prev_note->end_time ();
		}

		char const * opname = _("add note");
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		// Only update the length the previous note if the new on note
		// is shortening it.
		if (prev_note) {
			if (here <= prev_end) {
				Temporal::Beats new_length = here - prev_start;
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, new_length);
			}
		}

		// Create the new note using the defaults. Calculate the start
		// and length of the new note
		Temporal::Beats end = pattern.next_off (rowi, mti, mri, cgi);
		// If new note occur between the on note and off note of the previous
		// note, then use the off note of the previous note as off note of the
		// new note.
		if (prev_note && here < prev_end && prev_end < end)
			end = prev_end;
		Temporal::Beats length = end - here;
		NotePtr new_note (new NoteType (ch, here, length, pitch, vel));
		cmd->add (new_note);
		// Pre-emptively add the note in np so that it knows in which track it is
		// supposed to be.
		pattern.note_pattern (mti, mri).add (cgi, new_note);
	}

	// Apply note changes
	if (cmd) {
		apply_command (mti, mri, cmd);
	}

	return std::make_pair(new_ch, new_vel);
}

void
Grid::set_off_note (int rowi, int mti, int mri, int cgi)
{
	NotePtr on_note = get_on_note (rowi, mti, cgi);
	NotePtr off_note = get_off_note (rowi, mti, cgi);

	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int ();

	MidiModel::NoteDiffCommand* cmd = 0;

	if (on_note) {
		// Replace the on note by an off note, that is remove the on note
		char const * opname = _("delete note");
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is no off note, update the length of the preceding node
		// to match the new off note (smart off note).
		if (!off_note) {
			NotePtr prev_note = pattern.find_prev_note (rowi, mti, mri, cgi);
			if (prev_note) {
				Temporal::Beats length = on_note->time () - prev_note->time ();
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else {
		// Create a new off note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = pattern.region_relative_beats (rowi, mti, mri, delay);
		NotePtr prev_note = pattern.find_prev_note (rowi, mti, mri, cgi);
		Temporal::Beats prev_start;
		Temporal::Beats prev_end;
		if (prev_note) {
			prev_start = prev_note->time ();
			prev_end = prev_note->end_time ();
		}

		// Update the length the previous note to match the new off
		// note no matter what (smart off note).
		if (prev_note) {
			Temporal::Beats new_length = here - prev_start;
			char const * opname = _("resize note");
			cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
			cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, new_length);
		}
	}

	// Apply note changes
	if (cmd) {
		apply_command (mti, mri, cmd);
	}
}

void
Grid::delete_note (int rowi, int mti, int mri, int cgi)
{
	NotePtr on_note = get_on_note (rowi, mti, cgi);
	NotePtr off_note = get_off_note (rowi, mti, cgi);

	MidiModel::NoteDiffCommand* cmd = 0;

	if (on_note) {
		// Delete on note and change
		char const * opname = _("delete note");
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is an off note, update the length of the preceding note
		// to match the next note or the end of the region.
		if (off_note) {
			NotePtr prev_note = pattern.find_prev_note (rowi, mti, mri, cgi);
			if (prev_note) {
				// Calculate the length of the previous note
				Temporal::Beats start = prev_note->time ();
				Temporal::Beats end = pattern.next_off (rowi, mti, mri, cgi);
				Temporal::Beats length = end - start;
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else if (off_note) {
		// Update the length of the corresponding on note so the off note
		// matches the next note or the end of the region.
		Temporal::Beats start = off_note->time ();
		Temporal::Beats end = pattern.next_off (rowi, mti, mri, cgi);
		Temporal::Beats length = end - start;
		char const * opname = _("resize note");
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->change (off_note, MidiModel::NoteDiffCommand::Length, length);
	}

	// Apply note changes
	if (cmd) {
		apply_command (mti, mri, cmd);
	}
}

void
Grid::note_channel_edited (const string& path, const string& text)
{
	NotePtr note = get_on_note (path, edit_mti, edit_cgi);
	if (text.empty () || !note) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	if (!pattern.is_note_displayable (edit_rowi, edit_mti, edit_mri, edit_cgi)) {
		clear_editables ();
		return;
	}

	int  ch;
	if (sscanf (text.c_str (), "%d", &ch) == 1) {
		ch--;  // Adjust for zero-based counting
		set_note_channel (edit_mti, edit_mri, note, ch);
	}

	clear_editables ();
}

void
Grid::set_note_channel (int mti, int mri, NotePtr note, int ch)
{
	if (!note) {
		return;
	}

	if (0 <= ch && ch < 16 && ch != note->channel ()) {
		char const* opname = _("change note channel");

		// Define change command
		MidiModel::NoteDiffCommand* cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->change (note, MidiModel::NoteDiffCommand::Channel, ch);

		// Apply change command
		apply_command (mti, mri, cmd);
	}
}

void
Grid::note_velocity_edited (const string& path, const string& text)
{
	NotePtr note = get_on_note (path, edit_mti, edit_cgi);
	if (text.empty () || !note) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	if (!pattern.is_note_displayable (edit_rowi, edit_mti, edit_mri, edit_cgi)) {
		clear_editables ();
		return;
	}

	int  vel;
	// Parse the edited velocity and set the note velocity
	if (sscanf (text.c_str (), "%d", &vel) == 1) {
		set_note_velocity (edit_mti, edit_mri, note, vel);
	}

	clear_editables ();
}

void
Grid::set_note_velocity (int mti, int mri, NotePtr note, int vel)
{
	if (!note) {
		return;
	}

	// Change if within acceptable boundaries and different than the previous
	// velocity
	if (0 <= vel && vel <= 127 && vel != note->velocity ()) {
		char const* opname = _("change note velocity");

		// Define change command
		MidiModel::NoteDiffCommand* cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->change (note, MidiModel::NoteDiffCommand::Velocity, vel);

		// Apply change command
		apply_command (mti, mri, cmd);
	}
}

void
Grid::note_delay_edited (const string& path, const string& text)
{
	// Can't edit ***
	if (!pattern.is_note_displayable (edit_rowi, edit_mti, edit_mri, edit_cgi)) {
		clear_editables ();
		return;
	}

	// Parse the edited delay and set note delay
	int delay;
	if (!text.empty () && sscanf (text.c_str (), "%d", &delay) == 1) {
		set_note_delay (delay, edit_rowi, edit_mti, edit_mri, edit_cgi);
	}

	clear_editables ();
}

void
Grid::set_note_delay (int delay, int rowi, int mti, int mri, int cgi)
{
	NotePtr on_note = get_on_note (rowi, mti, cgi);
	NotePtr off_note = get_off_note (rowi, mti, cgi);
	if (!on_note && !off_note) {
		return;
	}

	char const* opname = _("change note delay");
	MidiModel::NoteDiffCommand* cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);

	// Get corresponding midi region pattern
	MidiRegionPattern& mrp = pattern.tps[mti]->midi_track_pattern ()->mrps[mri];

	// Check if within acceptable boundaries
	if (delay < mrp.delay_ticks_min () || mrp.delay_ticks_max () < delay) {
		return;
	}

	if (on_note) {
		// Modify the start time and length according to the new on note delay

		// Change start time according to new delay
		int delta = delay - pattern.region_relative_delay_ticks (on_note->time (), rowi, mti, mri);
		Temporal::Beats relative_beats = Temporal::Beats::ticks (delta);
		Temporal::Beats new_start = on_note->time () + relative_beats;
		// Make sure the new_start is still within the visible region
		if (new_start < mrp.start_beats) {
			new_start = mrp.start_beats;
			relative_beats = new_start - on_note->time ();
		}
		cmd->change (on_note, MidiModel::NoteDiffCommand::StartTime, new_start);

		// Adjust length so that the end time doesn't change
		Temporal::Beats new_length = on_note->length () - relative_beats;
		cmd->change (on_note, MidiModel::NoteDiffCommand::Length, new_length);

		if (off_note) {
			// There is an off note at the same row. Adjust its length as well.
			Temporal::Beats new_off_length = off_note->length () + relative_beats;
			cmd->change (off_note, MidiModel::NoteDiffCommand::Length, new_off_length);
		}
	}
	else if (off_note) {
		// There is only an off note. Modify its length accoding to the new off
		// note delay.
		int delta = delay - pattern.region_relative_delay_ticks (off_note->end_time (), rowi, mti, mri);
		Temporal::Beats relative_beats = Temporal::Beats::ticks (delta);
		Temporal::Beats new_length = off_note->length () + relative_beats;
		// Make sure the off note is after the on note
		if (new_length < Temporal::Beats::ticks (1)) {
			new_length = Temporal::Beats::ticks (1);
		}
		// Make sure the off note doesn't go beyong the limit of the region
		if (mrp.end_beats < off_note->time () + new_length) {
			new_length = mrp.end_beats - off_note->time ();
		}
		cmd->change (off_note, MidiModel::NoteDiffCommand::Length, new_length);
	}

	apply_command (mti, mri, cmd);
}

void Grid::play_note (int mti, uint8_t pitch)
{
	uint8_t ch = tracker_editor.main_toolbar.channel_spinner.get_value_as_int () - 1;
	uint8_t vel = tracker_editor.main_toolbar.velocity_spinner.get_value_as_int ();
	play_note (mti, pitch, ch, vel);
}

void Grid::play_note (int mti, uint8_t pitch, uint8_t ch, uint8_t vel)
{
	uint8_t event[3];
	event[0] = (MIDI_CMD_NOTE_ON | ch);
	event[1] = pitch;
	event[2] = vel;
	pattern.tps[mti]->midi_track ()->write_immediate_event (3, event);

	pressed_keys_pitch_to_channel[pitch] = ch;
}

void Grid::release_note (int mti, uint8_t pitch)
{
	uint8_t event[3];
	std::map<uint8_t, uint8_t>::iterator it = pressed_keys_pitch_to_channel.find(pitch);
	bool is_present = it != pressed_keys_pitch_to_channel.end();
	uint8_t ch = is_present ? it->second : (tracker_editor.main_toolbar.channel_spinner.get_value_as_int () - 1);
	event[0] = (MIDI_CMD_NOTE_OFF | ch);
	event[1] = pitch;
	event[2] = 0;
	pattern.tps[mti]->midi_track ()->write_immediate_event (3, event);

	if (is_present)
		pressed_keys_pitch_to_channel.erase (it);
}

void
Grid::set_current_cursor (const TreeModel::Path& path, TreeViewColumn* col, bool set_playhead)
{
	// Make sure the cell is defined
	if (!is_defined (path, col)) {
		return;
	}

	// Reset background color over the previous row
	redisplay_row_background (current_row, current_rowi);

	// Unset underline over previous cursor
	unset_underline_current_step_edit_cell ();

	// Remember the current mti to update the current track and the
	// step editor
	previous_mtp = current_mtp;

	// Update current row
	current_path = path;
	current_rowi = get_row_index (path);
	current_beats = pattern.beats_at_row (current_rowi);
	current_row = get_row (current_rowi);

	// Update current col
	current_col = get_col_index (col);

	// Update current mti, mtp, cgi and types
	current_mti = get_mti (col);
	current_mtp = pattern.tps[current_mti];
	current_mri = pattern.to_mri (current_rowi, current_mti);
	current_cgi = get_cgi (col);
	current_note_type = get_note_type (col);
	current_auto_type = get_auto_type (col);

	// Now display current row and cursor background colors
	redisplay_current_row_background ();
	redisplay_current_cursor ();

	// Set underline
	set_underline_current_step_edit_cell ();

	// Readjust scroller
	scroll_to_row (path);

	// Update selected track and step editor
	if (previous_mtp != current_mtp) {
		select_current_track();
		unset_step_editing_previous_track ();
		set_step_editing_current_track ();
	}

	// Update playhead accordingly
	if (set_playhead) {
		clock_pos = pattern.sample_at_row (current_rowi);
		skip_follow_playhead.push (current_rowi);
		tracker_editor.session->request_locate (clock_pos);
	}

	// TODO: remove that when no longer necessary
	// Align track toolbar
	tracker_editor.grid_header->align ();
}

void
Grid::set_current_pos (int min_pos, int max_pos)
{
	int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int ();
	current_pos = TrackerUtils::clamp (position, min_pos, max_pos);
}

Evoral::Parameter
Grid::get_param (size_t mti, size_t cgi) const
{
	IndexBimap::right_const_iterator ac_it = col2auto_cgi[mti].right.find (cgi);
	if (ac_it == col2auto_cgi[mti].right.end ()) {
		return Evoral::Parameter ();
	}
	size_t edited_colnum = ac_it->second;
	IndexParamBimap::left_const_iterator it = col2params[mti].left.find (edited_colnum);
	if (it == col2params[mti].left.end ()) {
		return Evoral::Parameter ();
	}
	const Evoral::Parameter& param = it->second;
	return param;
}

int
Grid::get_cgi (size_t mti, const Evoral::Parameter& param) const
{
	IndexParamBimap::right_const_iterator cp_it = col2params[mti].right.find (param);
	if (cp_it == col2params[mti].right.end ()) {
		return -1;
	}
	size_t coli = cp_it->second;
	IndexBimap::left_const_iterator cac_it = col2auto_cgi[mti].left.find (coli);
	if (cac_it == col2auto_cgi[mti].left.end ()) {
		return -1;
	}
	return cac_it->second;
}

AutomationListPtr
Grid::get_alist (int mti, int mri, const Evoral::Parameter& param)
{
	return pattern.get_alist (mti, mri, param);
}

const AutomationListPtr
Grid::get_alist (int mti, int mri, const Evoral::Parameter& param) const
{
	return pattern.get_alist (mti, mri, param);
}

void
Grid::automation_edited (const string& path, const string& text)
{
	bool is_del = text.empty ();
	double nval;
	if (!is_del && sscanf (text.c_str (), "%lg", &nval) != 1) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	Evoral::Parameter param = get_param (edit_mti, edit_cgi);
	if (!pattern.is_auto_displayable (edit_rowi, edit_mti, edit_mri, param)) {
		clear_editables ();
		return;
	}

	// Can edit
	if (is_del) {
		delete_automation_value (edit_rowi, edit_mti, edit_mri, edit_cgi);
	} else {
		set_automation_value (nval, edit_rowi, edit_mti, edit_mri, edit_cgi);
	}

	clear_editables ();
}

pair<double, bool>
Grid::get_automation_value (int rowi, int mti, int mri, int cgi) const
{
	Evoral::Parameter param = get_param (mti, cgi);
	return pattern.get_automation_value (rowi, mti, mri, param);
}

bool
Grid::has_automation_value (int rowi, int mti, int mri, int cgi) const
{
	return get_automation_value (rowi, mti, mri, cgi).second;
}

double
Grid::get_automation_interpolation_value (int rowi, int mti, int mri, int cgi) const
{
	return get_automation_interpolation_value (rowi, mti, mri, get_param (mti, cgi));
}

double
Grid::get_automation_interpolation_value (int rowi, int mti, int mri, const Evoral::Parameter& param) const
{
	double inter_auto_val = 0;
	if (const AutomationListPtr alist = get_alist (mti, mri, param)) {
		// We need to use ControlList::rt_safe_eval instead of ControlList::eval, otherwise the lock inside eval
		// interferes with the lock inside ControlList::erase. Though if mark_dirty is called outside of the scope
		// of the WriteLock in ControlList::erase and such, then eval can be used.
		// Get corresponding beats and samples
		Temporal::Beats row_relative_beats = pattern.region_relative_beats (rowi, mti, mri);
		uint32_t row_sample = pattern.sample_at_row_at_mti (rowi, mti);
		double awhen = TrackerUtils::is_region_automation (param) ? row_relative_beats.to_double () : row_sample;
		// Get interpolation
		bool ok;
		inter_auto_val = alist->rt_safe_eval (awhen, ok);
	}
	return inter_auto_val;
}

void
Grid::set_automation_value (double val, int rowi, int mti, int mri, int cgi)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_param (mti, cgi);

	// Find delay in case the value has to be created
	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int ();

	return pattern.set_automation_value (val, rowi, mti, mri, param, delay);
}

void
Grid::delete_automation_value (int rowi, int mti, int mri, int cgi)
{
	if (has_automation_value (rowi, mti, mri, cgi)) {
		Evoral::Parameter param = get_param (mti, cgi);
		pattern.delete_automation_value (rowi, mti, mri, param);
	}
}

void
Grid::automation_delay_edited (const string& path, const string& text)
{
	int delay = 0;
	// Parse the edited delay
	if (!text.empty () && sscanf (text.c_str (), "%d", &delay) != 1) {
		clear_editables ();
		return;
	}

	// Check if within acceptable boundaries
	if (delay < edit_mtp->delay_ticks_min () || edit_mtp->delay_ticks_max () < delay) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	Evoral::Parameter param = get_param (edit_mti, edit_cgi);
	if (!pattern.is_auto_displayable (edit_rowi, edit_mti, edit_mri, param)) {
		clear_editables ();
		return;
	}

	// Can edit
	set_automation_delay (delay, edit_rowi, edit_mti, edit_mri, edit_cgi);

	clear_editables ();
}

pair<int, bool>
Grid::get_automation_delay (int rowi, int mti, int mri, int cgi) const
{
	// Find the parameter to automate
	Evoral::Parameter param = get_param (mti, cgi);
	return pattern.get_automation_delay (rowi, mti, mri, param);
}

bool
Grid::has_automation_delay (int rowi, int mti, int mri, int cgi) const
{
	return get_automation_delay (rowi, mti, mri, cgi).second;
}

void
Grid::set_automation_delay (int delay, int rowi, int mti, int mri, int cgi)
{
	Evoral::Parameter param = get_param (mti, cgi);
	return pattern.set_automation_delay (delay, rowi, mti, mri, param);
}

void
Grid::delete_automation_delay (int rowi, int mti, int mri, int cgi)
{
	if (has_automation_delay (rowi, mti, mri, cgi)) {
		set_automation_delay (0, rowi, mti, mri, cgi);
	}
}

double
Grid::lower (int rowi, int mti, const Evoral::Parameter& param) const
{
	return pattern.tps[mti]->lower (rowi, param);
}

double
Grid::upper (int rowi, int mti, const Evoral::Parameter& param) const
{
	return pattern.tps[mti]->upper (rowi, param);
}

void
Grid::register_automation_undo (AutomationListPtr alist, const string& opname, XMLNode& before, XMLNode& after)
{
	tracker_editor.public_editor.begin_reversible_command (opname);
	tracker_editor.session->add_command (new MementoCommand<AutomationList> (*alist.get (), &before, &after));
	tracker_editor.public_editor.commit_reversible_command ();
	tracker_editor.session->set_dirty ();
}

void
Grid::apply_command (size_t mti, size_t mri, MidiModel::NoteDiffCommand* cmd)
{
	// Apply change command
	pattern.apply_command (mti, mri, cmd);
}

void
Grid::follow_playhead (samplepos_t pos)
{
	if (skip_follow_playhead.empty ()) { // Do not skip
		// TODO: relax that interval so that follow goes to the extremes
		if (pattern.first_sample <= pos && pos <= pattern.last_sample && pos != clock_pos) {
			int rowi = pattern.row_at_sample (pos);
			if (rowi != current_rowi) {
				vertical_move_current_cursor (rowi - current_rowi, false, false, false);
			}
		}
		clock_pos = pos;
	} else { // Skip
		skip_follow_playhead.pop ();
	}
}

std::string
Grid::mk_blank (size_t n)
{
	return std::string (n, blank_char);
}

bool
Grid::is_blank (const std::string& str)
{
	return str.back () == blank_char;
}

void
Grid::connect_events ()
{
	if (!tracker_editor._first) {
		return;
	}

	// Connect to key press events
	signal_key_press_event ().connect (sigc::mem_fun (*this, &Grid::key_press), false);
	signal_key_release_event ().connect (sigc::mem_fun (*this, &Grid::key_release), false);

	// Connect to mouse button events
	//
	// Disabled for now because it doesn't work as expected
	//
	signal_button_press_event ().connect (sigc::mem_fun (*this, &Grid::mouse_button_event), false);
	// signal_scroll_event ().connect (sigc::mem_fun (*this, &Grid::scroll_event), false);

	// Connect to clock to follow current row
	ARDOUR_UI::Clock.connect (sigc::mem_fun (*this, &Grid::follow_playhead));
}

void
Grid::setup_tree_view ()
{
	if (!tracker_editor._first) {
		return;
	}

	set_headers_visible (true);
	set_rules_hint (true);
	set_grid_lines (TREE_VIEW_GRID_LINES_BOTH);
	get_selection ()->set_mode (SELECTION_NONE);
	set_enable_search (false);
}

void
Grid::setup_time_column ()
{
	if (time_column) {
		return;
	}

	time_column = new TreeViewColumn (_("Time"), columns.time);
	CellRenderer* time_cellrenderer = time_column->get_first_cell_renderer ();

	// Link to color attributes
	time_column->add_attribute (time_cellrenderer->property_cell_background (), columns._time_background_color);

	append_column (*time_column);
}

void
Grid::setup_data_columns ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		if (mti < gain_columns.size ()) {
			redisplay_visible_left_separator (mti);
			redisplay_visible_right_separator (mti);
			redisplay_track_separator (mti);
			continue;
		}

		setup_left_separator_column (mti);
		setup_region_name_column (mti);

		// Setup column info
		gain_columns.push_back (0);
		trim_columns.push_back (0);
		mute_columns.push_back (0);
		col2params.push_back (IndexParamBimap ());
		col2auto_cgi.push_back (IndexBimap ());
		pan_columns.push_back (vector<size_t> ());
		available_automation_columns.push_back (set<size_t> ());

		// Instantiate note tracks
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK; cgi++) {
			setup_note_column (mti, cgi);
			setup_note_channel_column (mti, cgi);
			setup_note_velocity_column (mti, cgi);
			setup_note_delay_column (mti, cgi);
			setup_note_separator_column (mti, cgi);
		}

		// Instantiate automation tracks
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
			setup_automation_column (mti, cgi);
			setup_automation_delay_column (mti, cgi);
			setup_automation_separator_column (mti, cgi);
		}

		setup_right_separator_column (mti);
		setup_track_separator_column (mti);
	}
}

void
Grid::setup_left_separator_column (size_t mti)
{
	left_separator_columns[mti] = new TreeViewColumn ("", columns.left_separator[mti]);
	CellRenderer* left_separator_cellrenderer = left_separator_columns[mti]->get_first_cell_renderer ();

	// Link to color attributes
	left_separator_columns[mti]->add_attribute (left_separator_cellrenderer->property_cell_background (), columns._left_right_separator_background_color[mti]);

	// Set width
	left_separator_columns[mti]->set_min_width (LEFT_RIGHT_SEPARATOR_WIDTH);
	left_separator_columns[mti]->set_max_width (LEFT_RIGHT_SEPARATOR_WIDTH);

	append_column (*left_separator_columns[mti]);
}

void
Grid::setup_region_name_column (size_t mti)
{
	string label ("");
	region_name_columns[mti] = new TreeViewColumn (label, columns.region_name[mti]);
	CellRendererText* cellrenderer_region_name = dynamic_cast<CellRendererText*> (region_name_columns[mti]->get_first_cell_renderer ());

	// Link to font attributes
	region_name_columns[mti]->add_attribute (cellrenderer_region_name->property_family (), columns._family);

	append_column (*region_name_columns[mti]);

	region_name_columns[mti]->set_visible (false);
}

void
Grid::setup_note_column (size_t mti, size_t cgi)
{
	// TODO: maybe put the information of the mti, cgi, midi_note_type or automation_type in the TreeViewColumn
	note_columns[mti][cgi] = new NoteColumn (columns.note_name[mti][cgi], mti, cgi);
	CellRendererText* note_cellrenderer = dynamic_cast<CellRendererText*> (note_columns[mti][cgi]->get_first_cell_renderer ());

	// // Link to font attributes
	// viewcolumn_note->add_attribute (cellrenderer_note->property_family (), columns._family);

	// Link to color attributes
	note_columns[mti][cgi]->add_attribute (note_cellrenderer->property_cell_background (), columns._note_background_color[mti][cgi]);
	note_columns[mti][cgi]->add_attribute (note_cellrenderer->property_foreground (), columns._note_foreground_color[mti][cgi]);

	// Link to editing methods
	note_cellrenderer->signal_editing_started ().connect (sigc::bind (sigc::mem_fun (*this, &Grid::editing_note_started), mti, cgi));
	note_cellrenderer->signal_editing_canceled ().connect (sigc::mem_fun (*this, &Grid::editing_canceled));
	note_cellrenderer->signal_edited ().connect (sigc::mem_fun (*this, &Grid::note_edited));
	note_cellrenderer->property_editable () = true;

	append_column (*note_columns[mti][cgi]);
}

void
Grid::setup_note_channel_column (size_t mti, size_t cgi)
{
	channel_columns[mti][cgi] = new ChannelColumn (columns.channel[mti][cgi], mti, cgi);
	CellRendererText* channel_cellrenderer = dynamic_cast<CellRendererText*> (channel_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to color attribute
	channel_columns[mti][cgi]->add_attribute (channel_cellrenderer->property_cell_background (), columns._channel_background_color[mti][cgi]);
	channel_columns[mti][cgi]->add_attribute (channel_cellrenderer->property_foreground (), columns._channel_foreground_color[mti][cgi]);

	// Link attributes
	channel_columns[mti][cgi]->add_attribute (channel_cellrenderer->property_attributes (), columns._channel_attributes[mti][cgi]);

	// Link to editing methods
	channel_cellrenderer->signal_editing_started ().connect (sigc::bind (sigc::mem_fun (*this, &Grid::editing_note_channel_started), mti, cgi));
	channel_cellrenderer->signal_editing_canceled ().connect (sigc::mem_fun (*this, &Grid::editing_canceled));
	channel_cellrenderer->signal_edited ().connect (sigc::mem_fun (*this, &Grid::note_channel_edited));
	channel_cellrenderer->property_editable () = true;

	append_column (*channel_columns[mti][cgi]);
}

void
Grid::setup_note_velocity_column (size_t mti, size_t cgi)
{
	velocity_columns[mti][cgi] = new VelocityColumn (columns.velocity[mti][cgi], mti, cgi);
	CellRendererText* velocity_cellrenderer = dynamic_cast<CellRendererText*> (velocity_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to color attribute
	velocity_columns[mti][cgi]->add_attribute (velocity_cellrenderer->property_cell_background (), columns._velocity_background_color[mti][cgi]);
	velocity_columns[mti][cgi]->add_attribute (velocity_cellrenderer->property_foreground (), columns._velocity_foreground_color[mti][cgi]);

	// Link attributes
	velocity_columns[mti][cgi]->add_attribute (velocity_cellrenderer->property_attributes (), columns._velocity_attributes[mti][cgi]);

	// Link to editing methods
	velocity_cellrenderer->signal_editing_started ().connect (sigc::bind (sigc::mem_fun (*this, &Grid::editing_note_velocity_started), mti, cgi));
	velocity_cellrenderer->signal_editing_canceled ().connect (sigc::mem_fun (*this, &Grid::editing_canceled));
	velocity_cellrenderer->signal_edited ().connect (sigc::mem_fun (*this, &Grid::note_velocity_edited));
	velocity_cellrenderer->property_editable () = true;

	append_column (*velocity_columns[mti][cgi]);
}

void
Grid::setup_note_delay_column (size_t mti, size_t cgi)
{
	delay_columns[mti][cgi] = new DelayColumn (columns.delay[mti][cgi], mti, cgi);
	CellRendererText* delay_cellrenderer = dynamic_cast<CellRendererText*> (delay_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to color attribute
	delay_columns[mti][cgi]->add_attribute (delay_cellrenderer->property_cell_background (), columns._delay_background_color[mti][cgi]);
	delay_columns[mti][cgi]->add_attribute (delay_cellrenderer->property_foreground (), columns._delay_foreground_color[mti][cgi]);

	// Link attributes
	delay_columns[mti][cgi]->add_attribute (delay_cellrenderer->property_attributes (), columns._delay_attributes[mti][cgi]);

	// Link to editing methods
	delay_cellrenderer->signal_editing_started ().connect (sigc::bind (sigc::mem_fun (*this, &Grid::editing_note_delay_started), mti, cgi));
	delay_cellrenderer->signal_editing_canceled ().connect (sigc::mem_fun (*this, &Grid::editing_canceled));
	delay_cellrenderer->signal_edited ().connect (sigc::mem_fun (*this, &Grid::note_delay_edited));
	delay_cellrenderer->property_editable () = true;

	append_column (*delay_columns[mti][cgi]);
}

void
Grid::setup_note_separator_column (size_t mti, size_t cgi)
{
	note_separator_columns[mti][cgi] = new TreeViewColumn ("", columns._empty);

	// Set width
	note_separator_columns[mti][cgi]->set_min_width (GROUP_SEPARATOR_WIDTH);
	note_separator_columns[mti][cgi]->set_max_width (GROUP_SEPARATOR_WIDTH);

	append_column (*note_separator_columns[mti][cgi]);
}

void
Grid::setup_automation_column (size_t mti, size_t cgi)
{
	automation_columns[mti][cgi] = new AutomationColumn (columns.automation[mti][cgi], mti, cgi);
	CellRendererText* automation_cellrenderer = dynamic_cast<CellRendererText*> (automation_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to color attributes
	automation_columns[mti][cgi]->add_attribute (automation_cellrenderer->property_cell_background (), columns._automation_background_color[mti][cgi]);
	automation_columns[mti][cgi]->add_attribute (automation_cellrenderer->property_foreground (), columns._automation_foreground_color[mti][cgi]);

	// Link attributes
	automation_columns[mti][cgi]->add_attribute (automation_cellrenderer->property_attributes (), columns._automation_attributes[mti][cgi]);

	// Link to editing methods
	automation_cellrenderer->signal_editing_started ().connect (sigc::bind (sigc::mem_fun (*this, &Grid::editing_automation_started), mti, cgi));
	automation_cellrenderer->signal_editing_canceled ().connect (sigc::mem_fun (*this, &Grid::editing_canceled));
	automation_cellrenderer->signal_edited ().connect (sigc::mem_fun (*this, &Grid::automation_edited));
	automation_cellrenderer->property_editable () = true;

	size_t column = get_columns ().size ();
	append_column (*automation_columns[mti][cgi]);
	col2auto_cgi[mti].insert (IndexBimap::value_type (column, cgi));
	available_automation_columns[mti].insert (column);
	get_column (column)->set_visible (false);
}

void
Grid::setup_automation_delay_column (size_t mti, size_t cgi)
{
	automation_delay_columns[mti][cgi] = new AutomationDelayColumn (columns.automation_delay[mti][cgi], mti, cgi);
	CellRendererText* automation_delay_cellrenderer = dynamic_cast<CellRendererText*> (automation_delay_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to color attributes
	automation_delay_columns[mti][cgi]->add_attribute (automation_delay_cellrenderer->property_cell_background (), columns._automation_delay_background_color[mti][cgi]);
	automation_delay_columns[mti][cgi]->add_attribute (automation_delay_cellrenderer->property_foreground (), columns._automation_delay_foreground_color[mti][cgi]);

	// Link attributes
	automation_delay_columns[mti][cgi]->add_attribute (automation_delay_cellrenderer->property_attributes (), columns._automation_delay_attributes[mti][cgi]);

	// Link to editing methods
	automation_delay_cellrenderer->signal_editing_started ().connect (sigc::bind (sigc::mem_fun (*this, &Grid::editing_automation_delay_started), mti, cgi));
	automation_delay_cellrenderer->signal_editing_canceled ().connect (sigc::mem_fun (*this, &Grid::editing_canceled));
	automation_delay_cellrenderer->signal_edited ().connect (sigc::mem_fun (*this, &Grid::automation_delay_edited));
	automation_delay_cellrenderer->property_editable () = true;

	size_t column = get_columns ().size ();
	append_column (*automation_delay_columns[mti][cgi]);
	get_column (column)->set_visible (false);
}

void
Grid::setup_automation_separator_column (size_t mti, size_t cgi)
{
	automation_separator_columns[mti][cgi] = new TreeViewColumn ("", columns._empty);

	// Set width
	automation_separator_columns[mti][cgi]->set_min_width (GROUP_SEPARATOR_WIDTH);
	automation_separator_columns[mti][cgi]->set_max_width (GROUP_SEPARATOR_WIDTH);

	append_column (*automation_separator_columns[mti][cgi]);
}

void
Grid::setup_right_separator_column (size_t mti)
{
	right_separator_columns[mti] = new TreeViewColumn ("", columns.right_separator[mti]);
	CellRenderer* right_separator_cellrenderer = right_separator_columns[mti]->get_first_cell_renderer ();

	// Link to color attributes
	right_separator_columns[mti]->add_attribute (right_separator_cellrenderer->property_cell_background (), columns._left_right_separator_background_color[mti]);

	// Set width
	right_separator_columns[mti]->set_min_width (LEFT_RIGHT_SEPARATOR_WIDTH);
	right_separator_columns[mti]->set_max_width (LEFT_RIGHT_SEPARATOR_WIDTH);

	append_column (*right_separator_columns[mti]);
}

void
Grid::setup_track_separator_column (size_t mti)
{
	track_separator_columns[mti] = new TreeViewColumn ("", columns.track_separator[mti]);

	// Set width
	track_separator_columns[mti]->set_min_width (TRACK_SEPARATOR_WIDTH);
	track_separator_columns[mti]->set_max_width (TRACK_SEPARATOR_WIDTH);

	append_column (*track_separator_columns[mti]);
}

void
Grid::vertical_move_edit_cursor (int steps)
{
	TreeModel::Path path = edit_path;
	TreeViewColumn* col = get_column (edit_col);
	wrap_around_vertical_move (path, col, steps);
	set_cursor (path, *col, true);
}

void
Grid::wrap_around_vertical_move (TreeModel::Path& path, const TreeViewColumn* col, int steps, bool wrap, bool jump)
{
	TreeModel::Path init_path = path;
	TreeModel::Path last_def_path = path;

	// Move up
	while (steps < 0) {
		--path[0];
		// Wrap
		if (path[0] < 0) {
			if (wrap) {
				path[0] += pattern.global_nrows;
			} else {
				break;
			}
		}
		// Move
		if (is_defined (path, col)) {
			last_def_path = path;
			++steps;
		} else if (!jump) {
			++steps;
		}
		// Avoid infinit loops
		if (path == init_path) {
			break;
		}
	}

	// Move down
	while (0 < steps) {
		++path[0];
		// Wrap
		if ((int)pattern.global_nrows <= path[0]) {
			if (wrap) {
				path[0] -= pattern.global_nrows;
			} else {
				break;
			}
		}
		// Move
		if (is_defined (path, col)) {
			last_def_path = path;
			--steps;
		} else if (!jump) {
			--steps;
		}
		// Avoid infinit loops
		if (path == init_path) {
			break;
		}
	}

	path = last_def_path;
}

void
Grid::wrap_around_horizontal_move (int& colnum, const Gtk::TreeModel::Path& path, int steps, bool tab)
{
	// Keep track of the init column type to support tab and detect infinit loops
	TreeViewColumn* init_col = get_column (colnum);
	size_t pre_mti = get_mti (init_col);

	const int n_col = get_columns ().size ();
	TreeViewColumn* col;

	// Move to left
	while (steps < 0) {
		--colnum;
		if (colnum < 1) {
			colnum = n_col - 1;
		}
		col = get_column (colnum);
		if (col->get_visible () && is_editable (col) && is_defined (path, col)) {
			if (tab) {
				size_t col_mti = get_mti (col);
				if (pre_mti != col_mti) {
					pre_mti = col_mti;
					++steps;
				}
			} else {
				++steps;
			}
		}
		// Exit the infinite loop
		if (init_col == col) {
			break;
		}
	}

	// Move to right
	while (0 < steps) {
		++colnum;
		if (n_col <= colnum) {
			colnum = 1;         // colnum 0 is time
		}
		col = get_column (colnum);
		if (col->get_visible () && is_editable (col) && is_defined (path, col)) {
			if (tab) {
				size_t col_mti = get_mti (col);
				if (pre_mti != col_mti) {
					pre_mti = col_mti;
					--steps;
				}
			} else {
				--steps;
			}
		}
		// Exit the infinite loop
		if (init_col == col) {
			break;
		}
	}
}

bool
Grid::is_editable (TreeViewColumn* col) const
{
	CellRendererText* cellrenderer = dynamic_cast<CellRendererText*> (col->get_first_cell_renderer ());
	return cellrenderer->property_editable ();
}

bool
Grid::is_defined (const Gtk::TreeModel::Path& path, const TreeViewColumn* col)
{
	int rowi = get_row_index (path);
	int mti = get_mti (col);
	if (mti < 0) {
		return false;
	}

	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	if (is_note_type (col)) {
		return tc && is_region_defined (rowi, mti);
	} else {
		size_t coli = get_col_index (col);
		if (tc && tc->auto_type == TrackerColumn::AUTOMATION_DELAY) {
			coli--;
		}
		IndexParamBimap::left_const_iterator it = col2params[mti].left.find (coli);
		Evoral::Parameter param = it->second;
		return pattern.is_automation_defined (rowi, mti, param);
	}
}

bool
Grid::is_region_defined (const Gtk::TreeModel::Path& path, int mti) const
{
	return is_region_defined (get_row_index (path), mti);
}

bool
Grid::is_region_defined (uint32_t rowi, int mti) const
{
	return pattern.is_region_defined (rowi, mti);
}

bool
Grid::is_automation_defined (uint32_t rowi, int mti, int cgi) const
{
	Evoral::Parameter param = get_param (mti, cgi);
	if (param == Evoral::Parameter ()) {
		return false;
	}

	return pattern.is_automation_defined (rowi, mti, param);
}

int
Grid::get_mti (const TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc ? tc->midi_track_idx : -1;
}

size_t
Grid::get_cgi (const TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc->col_group_idx;
}

TrackerColumn::midi_note_type
Grid::get_note_type (const Gtk::TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc->note_type;
}

TrackerColumn::automation_type
Grid::get_auto_type (const Gtk::TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc->auto_type;
}

bool
Grid::is_note_type (const Gtk::TreeViewColumn* col) const
{
	return get_note_type (col) != TrackerColumn::SEPARATOR
		&& get_auto_type (col) == TrackerColumn::AUTOMATION_SEPARATOR;
}

void
Grid::select_current_track ()
{
	TimeAxisView* tav = tracker_editor.public_editor.time_axis_view_from_stripable (current_mtp->track);
	tracker_editor.public_editor.get_selection ().set (tav);    
}

void
Grid::set_step_editing_current_track ()
{
	if (tracker_editor.main_toolbar.step_edit && current_mtp->is_midi_track_pattern ()) {
		current_mtp->midi_track ()->set_step_editing (true, false);
	}
}

void
Grid::unset_step_editing_current_track ()
{
	if (current_mtp->is_midi_track_pattern ()) {
		current_mtp->midi_track ()->set_step_editing (false, false);
	}
}

void
Grid::unset_step_editing_previous_track ()
{
	if (previous_mtp && previous_mtp->is_midi_track_pattern ()) {
		previous_mtp->midi_track ()->set_step_editing (false, false);
	}
}

int
Grid::digit_key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
	// Num keys
	case GDK_0:
	case GDK_parenright:
		return 0;
	case GDK_1:
	case GDK_exclam:
		return 1;
	case GDK_2:
	case GDK_at:
		return 2;
	case GDK_3:
	case GDK_numbersign:
		return 3;
	case GDK_4:
	case GDK_dollar:
		return 4;
	case GDK_5:
	case GDK_percent:
		return 5;
	case GDK_6:
	case GDK_caret:
		return 6;
	case GDK_7:
	case GDK_ampersand:
		return 7;
	case GDK_8:
	case GDK_asterisk:
		return 8;
	case GDK_9:
	case GDK_parenleft:
		return 9;
	default:
		return -1;
	}
}

uint8_t
Grid::pitch_key (GdkEventKey* ev)
{
	int octave = tracker_editor.main_toolbar.octave_spinner.get_value_as_int ();

	switch (ev->keyval) {
	case GDK_z:                 // C
	case GDK_Z:
		return TrackerUtils::pitch (0, octave);
	case GDK_s:                 // C#
	case GDK_S:
		return TrackerUtils::pitch (1, octave);
	case GDK_x:                 // D
	case GDK_X:
		return TrackerUtils::pitch (2, octave);
	case GDK_d:                 // D#
	case GDK_D:
		return TrackerUtils::pitch (3, octave);
	case GDK_c:                 // E
	case GDK_C:
		return TrackerUtils::pitch (4, octave);
	case GDK_v:                 // F
	case GDK_V:
		return TrackerUtils::pitch (5, octave);
	case GDK_g:                 // F#
	case GDK_G:
		return TrackerUtils::pitch (6, octave);
	case GDK_b:                 // G
	case GDK_B:
		return TrackerUtils::pitch (7, octave);
	case GDK_h:                 // G#
	case GDK_H:
		return TrackerUtils::pitch (8, octave);
	case GDK_n:                 // A
	case GDK_N:
		return TrackerUtils::pitch (9, octave);
	case GDK_j:                 // A#
	case GDK_J:
		return TrackerUtils::pitch (10, octave);
	case GDK_m:                 // B
	case GDK_M:
		return TrackerUtils::pitch (11, octave);
	case GDK_q:                 // C+1
	case GDK_Q:
	case GDK_comma:
	case GDK_less:
		return TrackerUtils::pitch (0, octave + 1);
	case GDK_2:                 // C#+1
	case GDK_at:
	case GDK_l:
	case GDK_L:
		return TrackerUtils::pitch (1, octave + 1);
	case GDK_w:                 // D+1
	case GDK_W:
	case GDK_period:
	case GDK_greater:
		return TrackerUtils::pitch (2, octave + 1);
	case GDK_3:                 // D#+1
	case GDK_numbersign:
	case GDK_semicolon:
	case GDK_colon:
		return TrackerUtils::pitch (3, octave + 1);
		break;
	case GDK_e:                 // E+1
	case GDK_E:
	case GDK_slash:
	case GDK_question:
		return TrackerUtils::pitch (4, octave + 1);
	case GDK_r:                 // F+1
	case GDK_R:
		return TrackerUtils::pitch (5, octave + 1);
	case GDK_5:                 // F#+1
	case GDK_percent:
		return TrackerUtils::pitch (6, octave + 1);
	case GDK_t:                 // G+1
	case GDK_T:
		return TrackerUtils::pitch (7, octave + 1);
	case GDK_6:                 // G#+1
	case GDK_caret:
		return TrackerUtils::pitch (8, octave + 1);
	case GDK_y:                 // A+1
	case GDK_Y:
		return TrackerUtils::pitch (9, octave + 1);
	case GDK_7:                 // A#+1
	case GDK_ampersand:
		return TrackerUtils::pitch (10, octave + 1);
	case GDK_u:                 // B+1
	case GDK_U:
		return TrackerUtils::pitch (11, octave + 1);
	case GDK_i:                 // C+2
	case GDK_I:
		return TrackerUtils::pitch (0, octave + 2);
	case GDK_9:                 // C#+2
	case GDK_parenleft:
		return TrackerUtils::pitch (1, octave + 2);
	case GDK_o:                 // D+2
	case GDK_O:
		return TrackerUtils::pitch (2, octave + 2);
	case GDK_0:                 // D#+2
	case GDK_parenright:
		return TrackerUtils::pitch (3, octave + 2);
	case GDK_p:                 // E+2
	case GDK_P:
		return TrackerUtils::pitch (4, octave + 2);
	case GDK_bracketleft:       // F+2
	case GDK_braceleft:
		return TrackerUtils::pitch (5, octave + 2);
	default:
		return -1;
	}
}

bool
Grid::step_editing_check_midi_event ()
{
	ARDOUR::MidiRingBuffer<samplepos_t>& incoming (current_mtp->midi_track ()->step_edit_ring_buffer());
	uint8_t* buf;
	uint32_t bufsize = 32;       // Only 32???

	buf = new uint8_t[bufsize];

	while (incoming.read_space()) {
		samplepos_t time;
		Evoral::EventType type;
		uint32_t size;

		if (!incoming.read_prefix (&time, &type, &size)) {
			break;
		}

		if (size > bufsize) {
			delete [] buf;
			bufsize = size;
			buf = new uint8_t[bufsize];
		}

		if (!incoming.read_contents (size, buf)) {
			break;
		}

		if ((buf[0] & 0xf0) == MIDI_CMD_NOTE_ON && size == 3) {
			uint8_t pitch = buf[1];
			uint8_t ch = buf[0] & 0xf;
			uint8_t vel = buf[2];
			switch (current_note_type) {
			case TrackerColumn::NOTE:
				if (tracker_editor.main_toolbar.overwrite_default) {
					step_editing_set_on_note (pitch, ch, vel, false);
				} else {
					step_editing_set_on_note (pitch, false);
				}
				break;
			case TrackerColumn::CHANNEL:
				if (tracker_editor.main_toolbar.overwrite_default
				    && tracker_editor.main_toolbar.overwrite_existing) {
					step_editing_set_note_channel (ch);
				}
				break;
			case TrackerColumn::VELOCITY:
				if (tracker_editor.main_toolbar.overwrite_default
				    && tracker_editor.main_toolbar.overwrite_existing) {
					step_editing_set_note_velocity (vel);
				}
				break;
			case TrackerColumn::DELAY:
				break;
			default:
				cerr << "Grid::step_editing_check_midi_event: Implementation Error!" << endl;
			}
		}
	}
	delete [] buf;

	return true; // do it again, till we stop
}

bool
Grid::step_editing_note_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// On notes
	case GDK_z:                 // C
	case GDK_Z:
	case GDK_s:                 // C#
	case GDK_S:
	case GDK_x:                 // D
	case GDK_X:
	case GDK_d:                 // D#
	case GDK_D:
	case GDK_c:                 // E
	case GDK_C:
	case GDK_v:                 // F
	case GDK_V:
	case GDK_g:                 // F#
	case GDK_G:
	case GDK_b:                 // G
	case GDK_B:
	case GDK_h:                 // G#
	case GDK_H:
	case GDK_n:                 // A
	case GDK_N:
	case GDK_j:                 // A#
	case GDK_J:
	case GDK_m:                 // B
	case GDK_M:
	case GDK_q:                 // C+1
	case GDK_Q:
	case GDK_comma:
	case GDK_less:
	case GDK_2:                 // C#+1
	case GDK_at:
	case GDK_l:
	case GDK_L:
	case GDK_w:                 // D+1
	case GDK_W:
	case GDK_period:
	case GDK_greater:
	case GDK_3:                 // D#+1
	case GDK_numbersign:
	case GDK_semicolon:
	case GDK_colon:
	case GDK_e:                 // E+1
	case GDK_E:
	case GDK_slash:
	case GDK_question:
	case GDK_r:                 // F+1
	case GDK_R:
	case GDK_5:                 // F#+1
	case GDK_percent:
	case GDK_t:                 // G+1
	case GDK_T:
	case GDK_6:                 // G#+1
	case GDK_caret:
	case GDK_y:                 // A+1
	case GDK_Y:
	case GDK_7:                 // A#+1
	case GDK_ampersand:
	case GDK_u:                 // B+1
	case GDK_U:
	case GDK_i:                 // C+2
	case GDK_I:
	case GDK_9:                 // C#+2
	case GDK_parenleft:
	case GDK_o:                 // D+2
	case GDK_O:
	case GDK_0:                 // D#+2
	case GDK_parenright:
	case GDK_p:                 // E+2
	case GDK_P:
	case GDK_bracketleft:       // F+2
	case GDK_braceleft:
		ret = step_editing_set_on_note (pitch_key (ev));
		break;

	// Off note
	case GDK_equal:
	case GDK_plus:
	case GDK_Caps_Lock:			  // VVT: is it really a good idea?
		ret = step_editing_set_off_note ();
		break;

	// Delete note
	case GDK_BackSpace:
	case GDK_Delete:
		ret = step_editing_delete_note ();
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_Home:
	case GDK_End:
		ret = move_current_cursor_key_press (ev);
		break;

	// Other key not passed to the default entry handler
	case GDK_a:
	case GDK_A:
	case GDK_f:
	case GDK_F:
	case GDK_k:
	case GDK_K:
	case GDK_apostrophe:
	case GDK_quotedbl:
	case GDK_1:
	case GDK_exclam:
	case GDK_4:
	case GDK_dollar:
	case GDK_8:
	case GDK_asterisk:
	case GDK_minus:
	case GDK_underscore:
	case GDK_bracketright:
	case GDK_braceright:
		ret = true;
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *get_column (current_col), true);
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

bool
Grid::step_editing_set_on_note (uint8_t pitch, bool play)
{
	std::pair<uint8_t, uint8_t> ch_vel = set_on_note (pitch, current_rowi, current_mti, current_mri, current_cgi);
	vertical_move_current_cursor_default_steps ();
	if (play)
		play_note (current_mti, pitch, ch_vel.first, ch_vel.second);
	return true;
}

bool
Grid::step_editing_set_on_note (uint8_t pitch, uint8_t ch, uint8_t vel, bool play)
{
	std::pair<uint8_t, uint8_t> ch_vel = set_on_note (pitch, ch, vel, current_rowi, current_mti, current_mri, current_cgi);
	vertical_move_current_cursor_default_steps ();
	if (play)
		play_note (current_mti, pitch, ch_vel.first, ch_vel.second);
	return true;
}

bool
Grid::step_editing_set_off_note ()
{
	set_off_note (current_rowi, current_mti, current_mri, current_cgi);
	vertical_move_current_cursor_default_steps ();
	return true;
}

bool
Grid::step_editing_delete_note ()
{
	delete_note (current_rowi, current_mti, current_mri, current_cgi);
	vertical_move_current_cursor_default_steps ();
	return true;
}

bool
Grid::step_editing_note_channel_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_note_channel_digit (digit_key_press (ev));
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_Home:
	case GDK_End:
		ret = move_current_cursor_key_press (ev);
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *get_column (current_col), true);
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

bool
Grid::step_editing_set_note_channel_digit (int digit)
{
	NotePtr note = get_on_note (current_rowi, current_mti, current_cgi);
	if (note) {
		int ch = note->channel ();
		int new_ch = TrackerUtils::change_digit (ch + 1, digit, current_pos);
		set_note_channel (current_mti, current_mri, note, new_ch - 1);
	}
	vertical_move_current_cursor_default_steps ();
	return true;
}

bool
Grid::step_editing_set_note_channel (uint8_t ch)
{
	NotePtr note = get_on_note (current_rowi, current_mti, current_cgi);
	if (note) {
		set_note_channel (current_mti, current_mri, note, ch);
	}
	vertical_move_current_cursor_default_steps ();
	return true;
}

bool
Grid::step_editing_note_velocity_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_note_velocity_digit (digit_key_press (ev));
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_Home:
	case GDK_End:
		ret = move_current_cursor_key_press (ev);
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *get_column (current_col), true);
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

bool
Grid::step_editing_set_note_velocity_digit (int digit)
{
	NotePtr note = get_on_note (current_rowi, current_mti, current_cgi);
	if (note) {
		int vel = note->velocity ();
		int new_vel = TrackerUtils::change_digit (vel, digit, current_pos);
		set_note_velocity (current_mti, current_mri, note, new_vel);
	}
	vertical_move_current_cursor_default_steps ();
	return true;
}

bool
Grid::step_editing_set_note_velocity (uint8_t vel)
{
	NotePtr note = get_on_note (current_rowi, current_mti, current_cgi);
	if (note) {
		set_note_velocity (current_mti, current_mri, note, vel);
	}
	vertical_move_current_cursor_default_steps ();
	return true;
}

bool
Grid::step_editing_note_delay_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_note_delay (digit_key_press (ev));
		break;

	// Minus
	case GDK_minus:
	case GDK_underscore:
		ret = step_editing_set_note_delay (-1);
		break;

	// Plus
	case GDK_plus:
	case GDK_equal:
		ret = step_editing_set_note_delay (100);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_Home:
	case GDK_End:
		ret = move_current_cursor_key_press (ev);
		break;

	// Delete note delay
	case GDK_BackSpace:
	case GDK_Delete:
		ret = step_editing_delete_note_delay ();
		break;

	// Cell edit
	case GDK_Return:
		// TODO: when leaving text editing underline should be reset
		set_cursor (current_path, *get_column (current_col), true);
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

bool
Grid::step_editing_set_note_delay (int digit)
{
	NotePtr on_note = get_on_note (current_rowi, current_mti, current_cgi);
	NotePtr off_note = get_off_note (current_rowi, current_mti, current_cgi);
	if (on_note || off_note) {
		// Fetch delay
		int old_delay = on_note ? pattern.region_relative_delay_ticks (on_note->time (), current_rowi, current_mti, current_mri)
			: pattern.region_relative_delay_ticks (off_note->end_time (), current_rowi, current_mti, current_mri);

		// Update delay
		int new_delay = TrackerUtils::change_digit_or_sign (old_delay, digit, current_pos);
		set_note_delay (new_delay, current_rowi, current_mti, current_mri, current_cgi);
	}

	// Move the cursor
	vertical_move_current_cursor_default_steps ();
	return true;
}

bool
Grid::step_editing_delete_note_delay ()
{
	NotePtr note = get_note (current_rowi, current_mti, current_cgi);
	if (note) {
		set_note_delay (0, current_rowi, current_mti, current_mri, current_cgi);
	}
	vertical_move_current_cursor_default_steps ();
	return true;
}

bool
Grid::step_editing_automation_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_automation_value (digit_key_press (ev));
		break;

	// Minus
	case GDK_minus:
	case GDK_underscore:
		// Change sign
		ret = step_editing_set_automation_value (-1);
		break;

	// Plus
	case GDK_plus:
	case GDK_equal:
		// Delete automation
		ret = step_editing_set_automation_value (100);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_Home:
	case GDK_End:
		ret = move_current_cursor_key_press (ev);
		break;

	// Delete automation
	case GDK_BackSpace:
	case GDK_Delete:
		ret = step_editing_delete_automation ();
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *get_column (current_col), true);
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

bool
Grid::step_editing_set_automation_value (int digit)
{
	pair<double, bool> val_def = get_automation_value (current_rowi, current_mti, current_mri, current_cgi);
	double oval = val_def.second ? val_def.first : get_automation_interpolation_value (current_rowi, current_mti, current_mri, current_cgi);

	// Set new value
	// Round in case it is int automation
	if (is_int_param (get_param (current_mti, current_cgi))) {
		oval = std::round (oval);
	}

	double nval = TrackerUtils::change_digit_or_sign (oval, digit, current_pos);

	// TODO: replace by lock, and have redisplay_grid_connect_call immediately
	// return when such lock is taken.
	redisplay_grid_connect_call_enabled = false;
	set_automation_value (nval, current_rowi, current_mti, current_mri, current_cgi);

	// Move cursor
	vertical_move_current_cursor_default_steps ();

	// Redisplay model with the new value
	// TODO: optimize
	redisplay_grid_direct_call ();
	// TODO: Need to rerun because apparently redisplay_grid overwrite the
	// underlined cell, once optimized avoid such redundancy as well.
	set_underline_current_step_edit_cell ();
	redisplay_grid_connect_call_enabled = true;

	return true;
}

bool
Grid::step_editing_delete_automation ()
{
	// TODO: replace by lock, and have redisplay_grid_connect_call immediately
	// return when such lock is taken.
	redisplay_grid_connect_call_enabled = false;
	delete_automation_value (current_rowi, current_mti, current_mri, current_cgi);

	// Move cursor
	vertical_move_current_cursor_default_steps ();

	// Redisplay model with the new value
	// TODO: optimize
	redisplay_grid_direct_call ();
	// TODO: Need to rerun because apparently redisplay_grid overwrite the
	// underlined cell, once optimized avoid such redundancy as well.
	set_underline_current_step_edit_cell ();
	redisplay_grid_connect_call_enabled = true;

	return true;
}

bool
Grid::step_editing_automation_delay_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_automation_delay (digit_key_press (ev));
		break;

	// Minus
	case GDK_minus:
	case GDK_underscore:
		ret = step_editing_set_automation_delay (-1);
		break;

	// Plus
	case GDK_plus:
	case GDK_equal:
		ret = step_editing_set_automation_delay (100);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_Home:
	case GDK_End:
		ret = move_current_cursor_key_press (ev);
		break;

	// Delete automation delay
	case GDK_BackSpace:
	case GDK_Delete:
		ret = step_editing_delete_automation_delay ();
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *get_column (current_col), true);
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

bool
Grid::step_editing_set_automation_delay (int digit)
{
	pair<int, bool> val_def = get_automation_delay (current_rowi, current_mti, current_mri, current_cgi);
	int old_delay = val_def.first;

	// For some unknown reason, changing the delay does not trigger a
	// redisplay_grid_connect_call. For that reason we directly call it. But
	// before that we have to disable connect calls.
	redisplay_grid_connect_call_enabled = false;

	// Set new value
	if (val_def.second) {
		int new_delay = TrackerUtils::change_digit_or_sign (old_delay, digit, current_pos);
		set_automation_delay (new_delay, current_rowi, current_mti, current_mri, current_cgi);
	}

	// Move cursor
	vertical_move_current_cursor_default_steps ();

	// TODO: this highly inefficient, optimize
	redisplay_grid_direct_call ();
	set_underline_current_step_edit_cell ();
	redisplay_grid_connect_call_enabled = true;

	return true;
}

bool
Grid::step_editing_delete_automation_delay ()
{
	// For some unknown reason, changing the delay does not trigger a
	// redisplay_grid_connect_call. For that reason we directly call it. But
	// before that we have to disable connect calls.
	redisplay_grid_connect_call_enabled = false;
	delete_automation_delay (current_rowi, current_mti, current_mri, current_cgi);

	// Move cursor
	vertical_move_current_cursor_default_steps ();

	// TODO: this highly inefficient, optimize
	redisplay_grid_direct_call ();
	set_underline_current_step_edit_cell ();
	redisplay_grid_connect_call_enabled = true;

	return true;
}

void
Grid::vertical_move_current_cursor (int steps, bool wrap, bool jump, bool set_playhead)
{
	TreeModel::Path path = current_path;
	TreeViewColumn* col = get_column (current_col);
	wrap_around_vertical_move (path, col, steps, wrap, jump);
	set_current_cursor (path, col, set_playhead);
}

void
Grid::vertical_move_current_cursor_default_steps (bool wrap, bool jump, bool set_playhead)
{
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int ();
	vertical_move_current_cursor (steps, wrap, jump, set_playhead);
}

void
Grid::horizontal_move_current_cursor (int steps, bool tab)
{
	int colnum = current_col;
	TreeModel::Path path = current_path;
	wrap_around_horizontal_move (colnum, current_path, steps, tab);
	TreeViewColumn* col = get_column (colnum);
	set_current_cursor (path, col);
}

bool
Grid::move_current_cursor_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	case GDK_Up:
	case GDK_uparrow:
		vertical_move_current_cursor (-1);
		ret = true;
		break;
	case GDK_Down:
	case GDK_downarrow:
		vertical_move_current_cursor (1);
		ret = true;
		break;
	case GDK_Left:
	case GDK_leftarrow:
		horizontal_move_current_cursor (-1);
		ret = true;
		break;
	case GDK_Right:
	case GDK_rightarrow:
		horizontal_move_current_cursor (1);
		ret = true;
		break;
	case GDK_Tab:
	{
		// TODO: unfortunately Shift-Tab makes the focus go away
		bool is_shift = ev->state & GDK_SHIFT_MASK;
		horizontal_move_current_cursor (is_shift ? -1 : 1, true);
		ret = true;
		break;
	}
	case GDK_Page_Up:
		vertical_move_current_cursor (-16, false);
		ret = true;
		break;
	case GDK_Page_Down:
		vertical_move_current_cursor (16, false);
		ret = true;
		break;
	case GDK_Home:
		vertical_move_current_cursor (-(int)pattern.global_nrows, false);
		ret = true;
		break;
	case GDK_End:
		vertical_move_current_cursor (pattern.global_nrows, false);
		ret = true;
		break;
	}

	return ret;
}

bool
Grid::non_editing_key_press (GdkEventKey* ev)
{
	bool ret = false;
	
	switch (ev->keyval) {
	// On notes
	case GDK_z:                 // C
	case GDK_Z:
	case GDK_s:                 // C#
	case GDK_S:
	case GDK_x:                 // D
	case GDK_X:
	case GDK_d:                 // D#
	case GDK_D:
	case GDK_c:                 // E
	case GDK_C:
	case GDK_v:                 // F
	case GDK_V:
	case GDK_g:                 // F#
	case GDK_G:
	case GDK_b:                 // G
	case GDK_B:
	case GDK_h:                 // G#
	case GDK_H:
	case GDK_n:                 // A
	case GDK_N:
	case GDK_j:                 // A#
	case GDK_J:
	case GDK_m:                 // B
	case GDK_M:
	case GDK_q:                 // C+1
	case GDK_Q:
	case GDK_comma:
	case GDK_less:
	case GDK_2:                 // C#+1
	case GDK_at:
	case GDK_l:
	case GDK_L:
	case GDK_w:                 // D+1
	case GDK_W:
	case GDK_period:
	case GDK_greater:
	case GDK_3:                 // D#+1
	case GDK_numbersign:
	case GDK_semicolon:
	case GDK_colon:
	case GDK_e:                 // E+1
	case GDK_E:
	case GDK_slash:
	case GDK_question:
	case GDK_r:                 // F+1
	case GDK_R:
	case GDK_5:                 // F#+1
	case GDK_percent:
	case GDK_t:                 // G+1
	case GDK_T:
	case GDK_6:                 // G#+1
	case GDK_caret:
	case GDK_y:                 // A+1
	case GDK_Y:
	case GDK_7:                 // A#+1
	case GDK_ampersand:
	case GDK_u:                 // B+1
	case GDK_U:
	case GDK_i:                 // C+2
	case GDK_I:
	case GDK_9:                 // C#+2
	case GDK_parenleft:
	case GDK_o:                 // D+2
	case GDK_O:
	case GDK_0:                 // D#+2
	case GDK_parenright:
	case GDK_p:                 // E+2
	case GDK_P:
	case GDK_bracketleft:       // F+2
	case GDK_braceleft:
		if (last_keyval != ev->keyval) {
			play_note (current_mti, pitch_key (ev));
		}
		ret = true;
		break;

	// Current cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_Home:
	case GDK_End:
		ret = move_current_cursor_key_press (ev);
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *get_column (current_col), true);
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

bool
Grid::non_editing_key_release (GdkEventKey* ev)
{
	// Make sure the current mti is a midi track
	if (!pattern.tps[current_mti]->is_midi_track_pattern ()) {
		return false;
	}

	bool ret = false;

	switch (ev->keyval) {
	// On notes
	case GDK_z:                 // C
	case GDK_Z:
	case GDK_s:                 // C#
	case GDK_S:
	case GDK_x:                 // D
	case GDK_X:
	case GDK_d:                 // D#
	case GDK_D:
	case GDK_c:                 // E
	case GDK_C:
	case GDK_v:                 // F
	case GDK_V:
	case GDK_g:                 // F#
	case GDK_G:
	case GDK_b:                 // G
	case GDK_B:
	case GDK_h:                 // G#
	case GDK_H:
	case GDK_n:                 // A
	case GDK_N:
	case GDK_j:                 // A#
	case GDK_J:
	case GDK_m:                 // B
	case GDK_M:
	case GDK_q:                 // C+1
	case GDK_Q:
	case GDK_comma:
	case GDK_less:
	case GDK_2:                 // C#+1
	case GDK_at:
	case GDK_l:
	case GDK_L:
	case GDK_w:                 // D+1
	case GDK_W:
	case GDK_period:
	case GDK_greater:
	case GDK_3:                 // D#+1
	case GDK_numbersign:
	case GDK_semicolon:
	case GDK_colon:
	case GDK_e:                 // E+1
	case GDK_E:
	case GDK_slash:
	case GDK_question:
	case GDK_r:                 // F+1
	case GDK_R:
	case GDK_5:                 // F#+1
	case GDK_percent:
	case GDK_t:                 // G+1
	case GDK_T:
	case GDK_6:                 // G#+1
	case GDK_caret:
	case GDK_y:                 // A+1
	case GDK_Y:
	case GDK_7:                 // A#+1
	case GDK_ampersand:
	case GDK_u:                 // B+1
	case GDK_U:
	case GDK_i:                 // C+2
	case GDK_I:
	case GDK_9:                 // C#+2
	case GDK_parenleft:
	case GDK_o:                 // D+2
	case GDK_O:
	case GDK_0:                 // D#+2
	case GDK_parenright:
	case GDK_p:                 // E+2
	case GDK_P:
	case GDK_bracketleft:       // F+2
	case GDK_braceleft:
		release_note (current_mti, pitch_key (ev));
		ret = true;
		break;
	}

	return ret;
}

bool
Grid::key_press (GdkEventKey* ev)
{
	// TODO: disable repeating for anything but cursor move, or make editing, really, really, really snappy

	bool ret = false;
	if (tracker_editor.main_toolbar.step_edit) {
		switch (current_auto_type) {
		case TrackerColumn::AUTOMATION_SEPARATOR:
			switch (current_note_type) {
			case TrackerColumn::NOTE:
				ret = step_editing_note_key_press (ev);
				break;
			case TrackerColumn::CHANNEL:
				ret = step_editing_note_channel_key_press (ev);
				break;
			case TrackerColumn::VELOCITY:
				ret = step_editing_note_velocity_key_press (ev);
				break;
			case TrackerColumn::DELAY:
				ret = step_editing_note_delay_key_press (ev);
				break;
			default:
				cerr << "Grid::key_press: Implementation Error!" << endl;
			}
			break;
		case TrackerColumn::AUTOMATION:
			ret = step_editing_automation_key_press (ev);
			break;
		case TrackerColumn::AUTOMATION_DELAY:
			ret = step_editing_automation_delay_key_press (ev);
			break;
		default:
			cerr << "Grid::key_press: Implementation Error!" << endl;
		}
	}
	else
		ret = non_editing_key_press (ev);

	// Store last keyval to avoid repeating note
	last_keyval = ev->keyval;

	return ret;
}

bool
Grid::key_release (GdkEventKey* ev)
{
	bool ret = non_editing_key_release (ev);

	// Reset last keyval
	if (last_keyval == ev->keyval) {
		last_keyval = GDK_VoidSymbol;
	}

	return ret;
}

bool
Grid::mouse_button_event (GdkEventButton* ev)
{
	if (ev->button == 1) {
		TreeModel::Path path;
		TreeViewColumn* col;
		int cell_x, cell_y;
		get_path_at_pos (ev->x, ev->y, path, col, cell_x, cell_y);

		if (ev->type == GDK_2BUTTON_PRESS) {
			// Enter editing cell
			set_cursor (path, *col, true);
		} else if (ev->type == GDK_BUTTON_PRESS) {
			set_current_cursor (path, col, true);
		}

		grab_focus ();
	}

	return true;
}

bool
Grid::scroll_event (GdkEventScroll* ev)
{
	// TODO change values if editing is active, otherwise scroll.
	return false;               // Silence compiler
}
