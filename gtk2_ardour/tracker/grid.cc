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
#include "pbd/convert.h"

#include <pangomm/attributes.h>

#include <ytkmm/cellrenderercombo.h>
#include <ytkmm/tooltip.h>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "evoral/Note.h"
#include "evoral/midi_util.h"

#include "ardour/amp.h"
#include "ardour/midi_model.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
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
#include "track_all_automations_pattern_phenomenal_diff.h"
#include "track_toolbar.h"
#include "tracker_editor.h"
#include "tracker_utils.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;
using namespace Tracker;

const std::string Grid::note_off_str = "===";
const std::string Grid::undisplayable_str = "***";

Grid::Grid (TrackerEditor& te)
	: tracker_editor (te)
	, pattern (te, true /* connect */)
	, prev_pattern (te, false /* not connect */)
	, current_path (1)			  // NEXT: why 1?
	, current_row_idx (-1)
	, current_col_idx (0)
	, current_col (0)
	, current_mti (-1)
	, previous_tp (0)
	, current_tp (0)
	, current_mri (-1)
	, current_cgi (-1)
	, current_pos (0)
	, current_note_type (TrackerColumn::NoteType::NOTE)
	, current_automation_type (TrackerColumn::AutomationType::AUTOMATION_SEPARATOR)
	, current_is_note_type (true)
	, clock_pos (0)
	, edit_row_idx (-1)
	, edit_col (-1)
	, edit_mti (-1)
	, edit_mtp (0)
	, edit_mri (-1)
	, edit_cgi (-1)
	, editing_editable (0)
	, last_keyval (GDK_VoidSymbol)
	, redisplay_grid_connect_call_enabled (true)
	, shift_pressed (false)
	, cellfont ("Monospace")
	, time_column (0)
	, _subgrid_selector (te)
{
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &Grid::parameter_changed));
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &Grid::color_changed));
	read_keyboard_layout ();
	read_colors ();
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
			add (_velocity_alignment[mti][cgi]);
			add (delay[mti][cgi]);
			add (_delay_background_color[mti][cgi]);
			add (_delay_foreground_color[mti][cgi]);
			add (_delay_attributes[mti][cgi]);
			add (_note_empty[mti][cgi]);
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
			add (_automation_empty[mti][cgi]);
		}
		add (right_separator[mti]);
		add (track_separator[mti]);
	}
}

void
Grid::connect_clock ()
{
	// Connect to clock to follow current row
	clock_connection = ARDOUR_UI::Clock.connect (sigc::mem_fun (*this, &Grid::follow_playhead));
}

void
Grid::disconnect_clock ()
{
	// Disconnect from clock to unfollow current row
	clock_connection.disconnect ();
}

void
Grid::set_col_title (Gtk::TreeViewColumn* col, const std::string& title, const std::string& tooltip)
{
	Gtk::Label* l = Gtk::manage (new Label (title));
	ArdourWidgets::set_tooltip (*l, tooltip);
	col->set_widget (*l);
	l->show ();
}

int
Grid::select_available_automation_column (int mti)
{
	// Find the next available column
	if (available_automation_columns[mti].empty()) {
		std::cout << "Warning: no more available automation column" << std::endl;
		return 0;
	}
	std::set<int>::iterator it = available_automation_columns[mti].begin ();
	int column = *it;
	available_automation_columns[mti].erase (it);

	return column;
}

int
Grid::add_main_automation_column (int mti, const Evoral::Parameter& param)
{
	// Select the next available column
	int column = select_available_automation_column (mti);
	if (column == 0) {
		return column;
	}

	// Associate that column to the parameter
	IDParameter id_param (PBD::ID (0) /* id of main is assumed to be 0 */, param);
	col2params[mti].insert (IndexParamBimap::value_type (column, id_param)); // TODO: better put this knowledge in an inherited column

	// Set the column title and tooltip
	std::string long_name = get_name (mti, id_param, false);
	std::string short_name = get_name (mti, id_param, true);
	set_col_title (to_col (column), short_name, long_name);

	return column;
}

int
Grid::add_midi_automation_column (int mti, const Evoral::Parameter& param)
{
	// Insert the corresponding automation control (and connect to the grid if
	// not already there)
	PBD::ID id (0); // id of MIDI is assumed to be 0
	pattern.insert (mti, id, param);

	// Select the next available column
	int column = select_available_automation_column (mti);
	if (column == 0) {
		return column;
	}

	// Associate that column to the parameter
	IDParameter id_param (id, param);
	col2params[mti].insert (IndexParamBimap::value_type (column, id_param));

	// Set the column title and tooltip
	std::string long_name = get_name (mti, id_param, false);
	std::string short_name = get_name (mti, id_param, true);
	set_col_title (to_col (column), short_name, long_name);

	return column;
}

/**
 * Add an AutomationTimeAxisView to display automation for a processor's
 * parameter.
 */
void
Grid::add_processor_automation_column (int mti, ProcessorPtr processor, const Evoral::Parameter& param)
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
	IDParameter id_param (processor->id(), param);
	col2params[mti].insert (IndexParamBimap::value_type (pauno->column, id_param));

	// Set the column title and tooltip
	std::string long_name = get_name (mti, id_param, false);
	std::string short_name = get_name (mti, id_param, true);
	set_col_title (to_col (pauno->column), short_name, long_name);
}

void
Grid::change_all_channel_tracks_visibility (int mti, bool yn, const Evoral::Parameter& param)
{
	if (!pattern.tps[mti]->is_midi_track_pattern ()) {
		return;
	}

	const uint16_t selected_channels = pattern.tps[mti]->midi_track ()->get_playback_channel_mask ();

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (param.type (), chn, param.id ());
			IDParameter id_param(PBD::ID(0), fully_qualified_param);
			CheckMenuItem* menu = tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->automation_child_menu_item (id_param);

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
Grid::update_automation_column_visibility (int mti, const IDParameter& id_param)
{
	if (!pattern.tps[mti]->is_midi_track_pattern ()) { // TODO: Why?  Shouldn't
	                                                   // that apply to audio
	                                                   // track as well?  If it
	                                                   // is specialized for
	                                                   // MIDI, that least it
	                                                   // should be clear in the
	                                                   // method name.
		return;
	}

	// Find menu item associated to this parameter
	CheckMenuItem* mitem = tracker_editor.grid_header->track_headers[mti]->track_toolbar->midi_track_toolbar ()->automation_child_menu_item (id_param);
	assert (mitem);
	const bool showit = mitem->get_active ();

	// Find the column associated to this parameter, assign one if necessary
	IndexParamBimap::right_const_iterator it = col2params[mti].right.find (id_param);
	int column = (it == col2params[mti].right.end ()) || (it->second == 0) ?
		add_midi_automation_column (mti, id_param.second) : it->second;

	// Still no column available, skip
	if (column == 0) {
		return;
	}

	set_automation_column_visible (mti, id_param, column, showit);

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

bool
Grid::is_automation_visible (int mti, const IDParameter& id_param) const
{
	IndexParamBimap::right_const_iterator it = col2params[mti].right.find (id_param);
	return it != col2params[mti].right.end () &&
		visible_automation_columns.find (it->second) != visible_automation_columns.end ();
}

void
Grid::set_automation_column_visible (int mti, const IDParameter& id_param, int column, bool showit)
{
	if (showit) {
		visible_automation_columns.insert (column);
	} else {
		visible_automation_columns.erase (column);
	}
	set_param_enabled (mti, id_param, showit);
}

bool
Grid::has_visible_automation (int mti) const
{
	for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
		int col = automation_colnum (mti, cgi);
		bool visible = TrackerUtils::is_in (col, visible_automation_columns);
		if (visible) {
			return true;
		}
	}
	return false;
}

bool
Grid::is_gain_visible (int mti) const
{
	return visible_automation_columns.find (gain_columns[mti])
		!= visible_automation_columns.end ();
};

bool
Grid::is_trim_visible (int mti) const
{
	return visible_automation_columns.find (trim_columns[mti])
		!= visible_automation_columns.end ();
};

bool
Grid::is_mute_visible (int mti) const
{
	return visible_automation_columns.find (mute_columns[mti])
		!= visible_automation_columns.end ();
};

bool
Grid::is_pan_visible (int mti) const
{
	bool visible = !pan_columns[mti].empty();
	for (std::vector<int>::const_iterator it = pan_columns[mti].begin (); it != pan_columns[mti].end (); ++it) {
		visible = visible_automation_columns.find (*it) != visible_automation_columns.end ();
		if (!visible) {
			break;
		}
	}
	return visible;
};

void
Grid::update_gain_column_visibility (int mti)
{
	const bool showit = tracker_editor.grid_header->track_headers[mti]->track_toolbar->gain_automation_item->get_active ();

	if (gain_columns[mti] == 0) {
		gain_columns[mti] = add_main_automation_column (mti, Evoral::Parameter (GainAutomation));
	}

	// Still no column available, abort
	if (gain_columns[mti] == 0) {
		return;
	}

	IDParameter id_param(PBD::ID(0), Evoral::Parameter (GainAutomation));
	set_automation_column_visible (mti, id_param, gain_columns[mti], showit);

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

void
Grid::update_trim_column_visibility (int mti)
{
	const bool showit = tracker_editor.grid_header->track_headers[mti]->track_toolbar->trim_automation_item->get_active ();

	if (trim_columns[mti] == 0) {
		trim_columns[mti] = add_main_automation_column (mti, Evoral::Parameter (TrimAutomation));
	}

	// Still no column available, abort
	if (trim_columns[mti] == 0) {
		return;
	}

	IDParameter id_param(PBD::ID(0), Evoral::Parameter (TrimAutomation));
	set_automation_column_visible (mti, id_param, trim_columns[mti], showit);

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

void
Grid::update_mute_column_visibility (int mti)
{
	const bool showit = tracker_editor.grid_header->track_headers[mti]->track_toolbar->mute_automation_item->get_active ();

	if (mute_columns[mti] == 0) {
		mute_columns[mti] = add_main_automation_column (mti, Evoral::Parameter (MuteAutomation));
	}

	// Still no column available, abort
	if (mute_columns[mti] == 0) {
		return;
	}

	IDParameter id_param(PBD::ID(0), Evoral::Parameter (MuteAutomation));
	set_automation_column_visible (mti, id_param, mute_columns[mti], showit);

	/* now trigger a redisplay */
	redisplay_grid_direct_call ();
}

void
Grid::update_pan_columns_visibility (int mti)
{
	const bool showit = tracker_editor.grid_header->track_headers[mti]->track_toolbar->pan_automation_item->get_active ();

	if (pan_columns[mti].empty()) {
		std::set<Evoral::Parameter> const & params = pattern.tps[mti]->track->panner ()->what_can_be_automated ();
		for (std::set<Evoral::Parameter>::const_iterator p = params.begin (); p != params.end (); ++p) {
			pan_columns[mti].push_back (add_main_automation_column (mti, *p));
		}
	}

	// Still no column available, abort
	if (pan_columns[mti].empty()) {
		return;
	}

	for (std::vector<int>::const_iterator it = pan_columns[mti].begin (); it != pan_columns[mti].end (); ++it) {
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
			to_col (note_colnum (mti, cgi))->set_visible (visible);
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
Grid::mti_col_offset (int mti) const
{
	return 1 /* time */
		+ mti * (1 /* left separator */ + 1 /* region name */ +
		         MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK * NUMBER_OF_COL_PER_NOTE_TRACK
		         + MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK * NUMBER_OF_COL_PER_AUTOMATION_TRACK
		         + 1 /* right separator */ + 1 /* track separator */);
}

int
Grid::left_separator_colnum (int mti) const
{
	return mti_col_offset (mti) + 1 /* left separator */;
}

void
Grid::redisplay_visible_left_separator (int mti) const
{
	left_separator_columns[mti]->set_visible (pattern.tps[mti]->enabled);
}

int
Grid::region_name_colnum (int mti) const
{
	return left_separator_colnum (mti) + 1 /* region name */;
}

int
Grid::note_colnum (int mti, int cgi) const
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
			to_col (note_channel_colnum (mti, cgi))->set_visible (visible);
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
Grid::note_channel_colnum (int mti, int cgi) const
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
			to_col (note_velocity_colnum (mti, cgi))->set_visible (visible);
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
Grid::note_velocity_colnum (int mti, int cgi) const
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
			to_col (note_delay_colnum (mti, cgi))->set_visible (visible);
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
Grid::note_delay_colnum (int mti, int cgi) const
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
			to_col (note_separator_colnum (mti, cgi))->set_visible (visible);
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();
}

int
Grid::note_separator_colnum (int mti, int cgi) const
{
	return note_delay_colnum (mti, cgi) + 1 /* note group separator */;
}

void
Grid::redisplay_visible_automation ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
			int col = automation_colnum (mti, cgi);
			bool visible = pattern.tps[mti]->enabled && TrackerUtils::is_in (col, visible_automation_columns);
			to_col (col)->set_visible (visible);
		}
	}
	redisplay_visible_automation_delay ();

	// Keep the window width to its minimum
	tracker_editor.resize_width ();
}

int
Grid::automation_col_offset (int mti) const
{
	return mti_col_offset (mti) + 1 /* left separator */ + 1 /* region name */
		+ MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK * NUMBER_OF_COL_PER_NOTE_TRACK;
}

int
Grid::automation_colnum (int mti, int cgi) const
{
	return automation_col_offset (mti) + NUMBER_OF_COL_PER_AUTOMATION_TRACK * cgi;
}

void
Grid::redisplay_visible_automation_delay ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
			int col = automation_delay_colnum (mti, cgi);
			bool visible = pattern.tps[mti]->enabled
				&& tracker_editor.grid_header->track_headers[mti]->track_toolbar->visible_delay
				&& TrackerUtils::is_in (col - 1, visible_automation_columns);
			to_col (col)->set_visible (visible);
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();

	// Align track toolbar
	tracker_editor.grid_header->align ();
}

int
Grid::automation_delay_colnum (int mti, int cgi) const
{
	return automation_colnum (mti, cgi) + 1 /* delay */;
}

void
Grid::redisplay_visible_automation_separator ()
{
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		int greatest_visible_col = 0;
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK; cgi++) {
			int col = automation_separator_colnum (mti, cgi);
			bool visible = pattern.tps[mti]->enabled && TrackerUtils::is_in (col - 2, visible_automation_columns);
			if (visible) {
				greatest_visible_col = std::max (greatest_visible_col, col);
			}
			to_col (col)->set_visible (visible);
		}
		if (0 < greatest_visible_col) {
			to_col (greatest_visible_col)->set_visible (false);
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width ();
}

int
Grid::automation_separator_colnum (int mti, int cgi) const
{
	return automation_delay_colnum (mti, cgi) + 1 /* automation group separator */;
}

int
Grid::right_separator_colnum (int mti) const
{
	return automation_col_offset (mti)
		+ MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK * NUMBER_OF_COL_PER_AUTOMATION_TRACK;
}

void
Grid::redisplay_visible_right_separator (int mti) const
{
	right_separator_columns[mti]->set_visible (pattern.tps[mti]->enabled);
}

bool
Grid::is_hex () const
{
	return base () == 16;
}

int
Grid::base () const
{
	return tracker_editor.main_toolbar.base;
}

int
Grid::precision () const
{
	return tracker_editor.main_toolbar.precision;
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

TreeViewColumn*
Grid::first_defined_col ()
{
	std::vector<TreeViewColumn*> cols = (std::vector<TreeViewColumn*>)get_columns ();

	for (int i = 0; i < (int)cols.size (); i++) {
		if (cols[i]->get_visible () && is_editable (cols[i]) && is_cell_defined (current_path, cols[i])) {
			return cols[i];
		}
	}

	return 0;
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
	connect_tooltips ();
	setup_tree_view ();

	show ();
}

void
Grid::read_keyboard_layout ()
{
	PianoKeyBindings::Layout layout = PianoKeyBindings::layout (UIConfiguration::instance().get_vkeybd_layout ());
	_keyboard_layout.set_layout (layout);
}

void
Grid::read_colors ()
{
	gtk_bases_color = UIConfiguration::instance ().color ("gtk_bases");
	beat_background_color = UIConfiguration::instance ().color ("tracker editor: beat background");
	bar_background_color = UIConfiguration::instance ().color ("tracker editor: bar background");
	background_color = UIConfiguration::instance ().color ("tracker editor: background");
	active_foreground_color = UIConfiguration::instance ().color ("tracker editor: active foreground");
	passive_foreground_color = UIConfiguration::instance ().color ("tracker editor: passive foreground");
	cursor_color = UIConfiguration::instance ().color ("tracker editor: cursor");
	current_row_color = UIConfiguration::instance ().color ("tracker editor: current row");
	current_edit_row_color = UIConfiguration::instance ().color ("tracker editor: current edit row");
	selection_color = UIConfiguration::instance ().color ("tracker editor: selection");
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
	for (int row_idx = 0; row_idx < pattern.global_nrows; row_idx++) {
		// Get existing row, or create one if it does exist
		if (row_it == model->children ().end ()) {
			row_it = model->append ();
		}
		TreeModel::Row row = *row_it++;

		// Time
		Temporal::BBT_Time row_bbt = pattern.earliest_tp->bbt_at_row (row_idx);
		row[columns.time] = TrackerUtils::bbt_to_string (row_bbt, base ());

		// If the row is on a bar, beat or otherwise, the color differs
		Temporal::Beats row_beats = pattern.earliest_tp->beats_at_row (row_idx);
		bool is_row_beat = row_beats == row_beats.round_up_to_beat ();
		bool is_row_bar = row_bbt.beats == 1;
		std::string row_background_color = (is_row_beat ? (is_row_bar ?
		                                     TrackerUtils::color_to_string (bar_background_color)
		                                     : TrackerUtils::color_to_string (beat_background_color))
		                                    : TrackerUtils::color_to_string (background_color));
		row[columns._background_color] = row_background_color;

		// Set font family
		row[columns._family] = cellfont;

		row[columns._time_background_color] = row_background_color;
	}
}

void
Grid::redisplay_grid ()
{
	if (editing_editable) {
		return;
	}

	if (!tracker_editor.session) {
		return;
	}

	// In case the resolution (lines per beat) has changed
	tracker_editor.main_toolbar.delay_spinner.get_adjustment ()->set_lower (pattern.tps.front ()->delay_ticks_min ());
	tracker_editor.main_toolbar.delay_spinner.get_adjustment ()->set_upper (pattern.tps.front ()->delay_ticks_max ());

	// Update pattern settings and content
	pattern.update ();

	// After update, compare pattern and prev_pattern to come up with a list of
	// differences to display. For now only worry about redisplaying the
	// changed mti.
	_phenomenal_diff = pattern.phenomenal_diff (prev_pattern);

	// Redisplay the grid
	redisplay_global_columns ();
	redisplay_pattern ();
	redisplay_current_row ();
	remove_unused_rows ();

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

	// Save pattern to prev_pattern for subsequent phenomenal diff calculation
	prev_pattern = pattern;
}

void
Grid::redisplay_grid_direct_call ()
{
	redisplay_grid ();
}

void
Grid::redisplay_grid_connect_call ()
{
	if (redisplay_grid_connect_call_enabled) {
		redisplay_grid ();
	}
}

void
Grid::redisplay_undefined_notes (TreeModel::Row& row, int mti)
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
Grid::redisplay_undefined_note (TreeModel::Row& row, int mti, int cgi)
{
	// cgi stands from column group index
	row[columns.note_name[mti][cgi]] = "";
	row[columns.channel[mti][cgi]] = "";
	row[columns.velocity[mti][cgi]] = "";
	row[columns.delay[mti][cgi]] = "";

	// TODO: replace gtk_bases_color by a custom one
	row[columns._note_background_color[mti][cgi]] = TrackerUtils::color_to_string (gtk_bases_color);
	row[columns._channel_background_color[mti][cgi]] = TrackerUtils::color_to_string (gtk_bases_color);
	row[columns._velocity_background_color[mti][cgi]] = TrackerUtils::color_to_string (gtk_bases_color);
	row[columns._delay_background_color[mti][cgi]] = TrackerUtils::color_to_string (gtk_bases_color);
}

void
Grid::redisplay_undefined_automations (TreeModel::Row& row, int row_idx, int mti)
{
	if (!pattern.tps[mti]->is_midi_track_pattern ()) {
		return;
	}

	int mri = 0;              // consider the first one, all parameters
	                          // should be the same in all other regions

	// Get the AutomationPattern of the first region of the track
	MidiRegionPattern& mrp = pattern.midi_region_pattern (mti, mri);
	AutomationPattern& ap = mrp.mrap;
	for (const Evoral::Parameter& param : ap.get_enabled_parameters ())
	{
		int cgi = to_cgi (mti, PBD::ID (0), param);
		redisplay_undefined_automation (row, mti, cgi);
	}
}

void
Grid::redisplay_track_separator (int mti)
{
	track_separator_columns[mti]->set_visible (pattern.tps[mti]->enabled);
}

void
Grid::redisplay_undefined_region_name (TreeModel::Row& row, int mti)
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
Grid::redisplay_left_right_separator_columns (int mti)
{
	TreeModel::Children::iterator row_it = model->children ().begin ();
	for (int row_idx = 0; row_idx < pattern.global_nrows; row_idx++) {
		TreeModel::Row row = *row_it++;
		redisplay_left_right_separator (row, mti);
	}
}

void
Grid::redisplay_left_right_separator (TreeModel::Row& row, int mti)
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
Grid::redisplay_undefined_automation (Gtk::TreeModel::Row& row, int mti, int cgi)
{
	// Set empty forground
	row[columns.automation[mti][cgi]] = "";
	row[columns.automation_delay[mti][cgi]] = "";

	// Set undefined background color
	row[columns._automation_background_color[mti][cgi]] = TrackerUtils::color_to_string (gtk_bases_color);
	row[columns._automation_delay_background_color[mti][cgi]] = TrackerUtils::color_to_string (gtk_bases_color);
}

void
Grid::redisplay_note_background (TreeModel::Row& row, int mti, int cgi)
{
	std::string row_background_color = row[columns._background_color];
	row[columns._note_background_color[mti][cgi]] = row_background_color;
	row[columns._channel_background_color[mti][cgi]] = row_background_color;
	row[columns._velocity_background_color[mti][cgi]] = row_background_color;
	row[columns._delay_background_color[mti][cgi]] = row_background_color;
}

void
Grid::redisplay_current_row_background ()
{
	if (current_row_idx < 0)
		return;
	std::string color = step_edit() ?
		TrackerUtils::color_to_string (current_edit_row_color)
		: TrackerUtils::color_to_string (current_row_color);
	redisplay_row_background_color (current_row, current_row_idx, color);
}

void
Grid::redisplay_current_note_cursor (TreeModel::Row& row, int mti, int cgi)
{
	std::string color = TrackerUtils::color_to_string (cursor_color);

	switch (current_note_type) {
	case TrackerColumn::NoteType::NOTE:
		row[columns._note_background_color[mti][cgi]] = color;
		break;
	case TrackerColumn::NoteType::CHANNEL:
		row[columns._channel_background_color[mti][cgi]] = color;
		break;
	case TrackerColumn::NoteType::VELOCITY:
		row[columns._velocity_background_color[mti][cgi]] = color;
		break;
	case TrackerColumn::NoteType::DELAY:
		row[columns._delay_background_color[mti][cgi]] = color;
		break;
	default:
		std::cerr << "Grid::redisplay_current_note_cursor: Implementation Error!" << std::endl;
	}
}

void
Grid::redisplay_blank_note_foreground (TreeModel::Row& row, int mti, int cgi)
{
	// Fill with blank
	row[columns.note_name[mti][cgi]] = mk_note_blank ();
	row[columns.channel[mti][cgi]] = mk_ch_blank ();
	row[columns.velocity[mti][cgi]] = mk_vel_blank ();
	row[columns.delay[mti][cgi]] = mk_delay_blank ();

	// Grey out infoless cells
	row[columns._note_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (passive_foreground_color);
	row[columns._channel_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (passive_foreground_color);
	row[columns._velocity_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (passive_foreground_color);
	row[columns._delay_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (passive_foreground_color);
}

void
Grid::redisplay_automation_background (TreeModel::Row& row, int mti, int cgi)
{
	std::string row_background_color = row[columns._background_color];
	row[columns._automation_background_color[mti][cgi]] = row_background_color;
	row[columns._automation_delay_background_color[mti][cgi]] = row_background_color;
}

void
Grid::redisplay_note_foreground (TreeModel::Row& row, int row_idx, int mti, int mri, int cgi)
{
	if (is_note_displayable (row_idx, mti, mri, cgi)) {
		// Notes off
		NotePtr note = pattern.off_note (row_idx, mti, mri, cgi);
		if (note) {
			row[columns.note_name[mti][cgi]] = note_off_str;
			row[columns._note_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
			int delay = get_off_note_delay (note, row_idx, mti, mri);
			if (delay != 0) {
				row[columns.delay[mti][cgi]] = TrackerUtils::num_to_string (delay, base (), precision ());
				row[columns._delay_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
			}
		}

		// Notes on
		note = pattern.on_note (row_idx, mti, mri, cgi);
		if (note) {
			row[columns.note_name[mti][cgi]] = ParameterDescriptor::midi_note_name (note->note ());
			row[columns._note_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
			row[columns.channel[mti][cgi]] = TrackerUtils::channel_to_string (note->channel (), base ());
			row[columns._channel_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
			row[columns.velocity[mti][cgi]] = TrackerUtils::num_to_string ((int)note->velocity (), base (), precision ());
			row[columns._velocity_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
			row[columns._velocity_alignment[mti][cgi]] = Pango::Alignment::ALIGN_RIGHT;

			int delay = get_on_note_delay (note, row_idx, mti, mri);
			if (delay != 0) {
				row[columns.delay[mti][cgi]] = TrackerUtils::num_to_string (delay, base (), precision ());
				row[columns._delay_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
			}
		}
	} else {
		// Too many notes, not displayable
		row[columns.note_name[mti][cgi]] = undisplayable_str;
		row[columns._note_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
	}
}

void
Grid::redisplay_current_automation_cursor (TreeModel::Row& row, int mti, int cgi)
{
	std::string color = TrackerUtils::color_to_string (cursor_color);

	switch (current_automation_type) {
	case TrackerColumn::AutomationType::AUTOMATION:
		row[columns._automation_background_color[mti][cgi]] = color;
		break;
	case TrackerColumn::AutomationType::AUTOMATION_DELAY:
		row[columns._automation_delay_background_color[mti][cgi]] = color;
		break;
	default:
		std::cerr << "Grid::redisplay_current_automation_cursor: Implementation Error!" << std::endl;
	}
}

void
Grid::redisplay_current_cursor ()
{
	if (current_is_note_type) {
		redisplay_current_note_cursor (current_row, current_mti, current_cgi);
	} else {
		redisplay_current_automation_cursor (current_row, current_mti, current_cgi);
	}
}

void
Grid::redisplay_blank_automation_foreground (TreeModel::Row& row, int mti, int cgi)
{
	// Fill with blank
	row[columns.automation[mti][cgi]] = mk_automation_blank ();
	row[columns.automation_delay[mti][cgi]] = mk_delay_blank ();

	// Fill default foreground color
	row[columns._automation_delay_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (passive_foreground_color);
}

void
Grid::redisplay_automation (TreeModel::Row& row, int row_idx, int mti, int mri, int cgi, const IDParameter& id_param)
{
	if (is_automation_displayable (row_idx, mti, mri, id_param)) {
		double val = pattern.get_automation_value (row_idx, mti, mri, id_param).first;
		row[columns.automation[mti][cgi]] = TrackerUtils::num_to_string (val, base (), precision ());
		int delay = pattern.get_automation_delay (row_idx, mti, mri, id_param).first;
		if (delay != 0) {
			row[columns.automation_delay[mti][cgi]] = TrackerUtils::num_to_string (delay, base (), precision ());
			row[columns._automation_delay_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
		}
	} else {
		row[columns.automation[mti][cgi]] = undisplayable_str;
	}
	row[columns._automation_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (active_foreground_color);
}

void
Grid::redisplay_automation_interpolation (TreeModel::Row& row, int row_idx, int mti, int mri, int cgi, const IDParameter& id_param)
{
	double inter_val = get_automation_interpolation_value (row_idx, mti, mri, id_param);
	if (is_int_param (id_param)) {
		row[columns.automation[mti][cgi]] = TrackerUtils::num_to_string ((int)std::round (inter_val), base (), precision ());
	} else {
		row[columns.automation[mti][cgi]] = TrackerUtils::num_to_string (inter_val, base (), precision ());
	}
	row[columns._automation_foreground_color[mti][cgi]] = TrackerUtils::color_to_string (passive_foreground_color);
}

void
Grid::redisplay_cell_background (int row_idx, int col_idx)
{
	int mti = to_mti (to_col (col_idx));
	int cgi = to_cgi (to_col (col_idx));
	Gtk::TreeModel::Row row = to_row (row_idx);
	redisplay_cell_background (row, mti, cgi);
}

void
Grid::redisplay_cell_background (TreeModel::Row& row, int mti, int cgi)
{
	if (current_is_note_type) {
		redisplay_note_background (row, mti, cgi);
	} else {
		redisplay_automation_background (row, mti, cgi);
	}
}

void
Grid::redisplay_row_background (Gtk::TreeModel::Row& row, int row_idx)
{
	if (row_idx < 0)
		return;
	redisplay_row_background_color (row, row_idx, row[columns._background_color]);
}

void
Grid::redisplay_row_background_color (Gtk::TreeModel::Row& row, int row_idx, const std::string& color)
{
	row[columns._time_background_color] = color;
	for (size_t mti = 0; mti < pattern.tps.size (); mti++) {
		redisplay_row_mti_background_color (row, row_idx, mti, color);
	}
}

void
Grid::redisplay_row_mti_background_color (Gtk::TreeModel::Row& row, int row_idx, int mti, const std::string& color)
{
	if (is_region_defined (row_idx, mti)) {
		// Set current row background color for notes
		redisplay_row_mti_notes_background_color (row, row_idx, mti, color);
	}

	// Set current row background colors for MIDI and track automations
	int mri = pattern.to_mri (row_idx, mti);
	IDParameterSet id_params = pattern.get_enabled_parameters (mti, mri);
	redisplay_row_mti_automations_background_color (row, row_idx, mti, id_params, color);
}

void
Grid::redisplay_row_mti_notes_background_color (Gtk::TreeModel::Row& row, int row_idx, int mti, const std::string& color)
{
	for (size_t cgi = 0; cgi < pattern.tps[mti]->midi_track_pattern ()->get_ntracks (); cgi++) {
		row[columns._note_background_color[mti][cgi]] = color;
		row[columns._channel_background_color[mti][cgi]] = color;
		row[columns._velocity_background_color[mti][cgi]] = color;
		row[columns._delay_background_color[mti][cgi]] = color;
	}
}

void
Grid::redisplay_row_mti_automations_background_color (Gtk::TreeModel::Row& row, int row_idx, int mti, const IDParameterSet& id_params, const std::string& color)
{
	for (const IDParameter& id_param : id_params) {
		int cgi = to_cgi (mti, id_param);
		row[columns._automation_background_color[mti][cgi]] = color;
		row[columns._automation_delay_background_color[mti][cgi]] = color;
	}
}

void
Grid::redisplay_current_row ()
{
	int current_row_idx_from_beats = pattern.row_at_beats (current_beats);
	if (is_current_cursor_defined ()) {
		set_current_cursor (current_row_idx_from_beats, current_col);
	} else {
		set_current_row (current_row_idx_from_beats);
	}
}

void
Grid::redisplay_pattern ()
{
	if (_phenomenal_diff.full)
	{
		for (size_t mti = 0; mti < pattern.tps.size (); mti++)
		{
			redisplay_track (mti);
		}
	} else {
		for (const PatternPhenomenalDiff::Mti2TrackPatternDiff::value_type& mti_tpd : _phenomenal_diff.mti2tp_diff)
		{
			redisplay_track (mti_tpd.first, mti_tpd.second);
		}
	}
}

void
Grid::redisplay_track (int mti, const TrackPatternPhenomenalDiff* tp_diff)
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
Grid::redisplay_midi_track (int mti, const MidiTrackPattern& mtp, const MidiTrackPatternPhenomenalDiff* mtp_diff)
{
	if (mtp_diff == 0 || mtp_diff->full) {
		redisplay_inter_midi_regions (mti);
		for (size_t mri = 0; mri < mtp.mrps.size (); mri++) {
			redisplay_midi_region (mti, mri, *mtp.mrps[mri]);
		}
		redisplay_track_all_automations (mti, mtp.track_all_automations_pattern);
	} else {
		// TODO: optimize redisplay_inter_midi_regions so that it only redisplay new inter midi regions
		redisplay_inter_midi_regions (mti);
		for (const MidiTrackPatternPhenomenalDiff::Mri2MidiRegionPatternDiff::value_type& mri_rpd : mtp_diff->mri2mrp_diff) {
			size_t mri = mri_rpd.first;
			redisplay_midi_region (mti, mri, *mtp.mrps[mri], &mri_rpd.second);
		}
		redisplay_track_all_automations (mti, mtp.track_all_automations_pattern, &mtp_diff->taap_diff);
	}
}

void
Grid::redisplay_track_all_automations (int mti, const TrackAllAutomationsPattern& taap, const TrackAllAutomationsPatternPhenomenalDiff* taap_diff)
{
	if (taap_diff == 0 || taap_diff->full) {
		redisplay_main_automations (mti, taap.main_automation_pattern);
		for (const auto& id_pap : taap.id_to_processor_automation_pattern) {
			redisplay_processor_automations (mti, *id_pap.second);
		}
	} else {
		redisplay_main_automations (mti, taap.main_automation_pattern, &taap_diff->main_automation_pattern_phenomenal_diff);
		for (const auto& id_pappd : taap_diff->id_to_processor_automation_pattern_phenomenal_diff) {
			const PBD::ID& id = id_pappd.first;
			auto it = taap.id_to_processor_automation_pattern.find (id);
			if (it != taap.id_to_processor_automation_pattern.end ()) {
				redisplay_processor_automations (mti, *it->second, &id_pappd.second);
			}
		}
	}
}

void
Grid::redisplay_main_automations (int mti, const MainAutomationPattern& map, const AutomationPatternPhenomenalDiff* map_diff)
{
	redisplay_track_automations (mti, PBD::ID (0), map, map_diff);
}

void
Grid::redisplay_processor_automations (int mti, const ProcessorAutomationPattern& pap, const AutomationPatternPhenomenalDiff* pap_diff)
{
	redisplay_track_automations (mti, pap.id (), pap, pap_diff);
}

void
Grid::redisplay_track_automations (int mti, const PBD::ID& id, const TrackAutomationPattern& tap, const AutomationPatternPhenomenalDiff* tap_diff)
{
	if (tap_diff == 0 || tap_diff->full) {
		for (const Evoral::Parameter& param : tap.get_enabled_parameters ()) {
			redisplay_track_automation_param (mti, tap, IDParameter (id, param));
		}
	} else {
		for (AutomationPatternPhenomenalDiff::Param2RowsPhenomenalDiff::const_iterator it = tap_diff->param2rows_diff.begin (); it != tap_diff->param2rows_diff.end (); ++it) {
			redisplay_track_automation_param (mti, tap, IDParameter (id, it->first), &it->second);
		}
	}
}

void
Grid::redisplay_track_automation_param (int mti, const TrackAutomationPattern& tap, const IDParameter& id_param, const RowsPhenomenalDiff* rows_diff)
{
	int cgi = to_cgi (mti, id_param);
	if (cgi < 0) {
		return;
	}

	if (rows_diff == 0 || rows_diff->full) {
		for (int row_idx = 0; row_idx < tap.nrows; row_idx++) {
			redisplay_track_automation_param_row (mti, cgi, row_idx, tap, id_param);
		}
	} else {
		for (std::set<int>::const_iterator it = rows_diff->rows.begin (); it != rows_diff->rows.end (); ++it) {
			redisplay_track_automation_param_row (mti, cgi, *it, tap, id_param);
		}
	}
}

void
Grid::redisplay_track_automation_param_row (int mti, int cgi, int row_idx, const TrackAutomationPattern& tap, const IDParameter& id_param)
{
	// TODO: optimize!
	Gtk::TreeModel::Row row = to_row (row_idx);
	int mri = -1;
	int automation_count = pattern.control_events_count (row_idx, mti, mri, id_param);

	// Fill background colors
	redisplay_automation_background (row, mti, cgi);

	// Fill default blank foreground text and color
	redisplay_blank_automation_foreground (row, mti, cgi);

	if (automation_count > 0) {
		redisplay_automation (row, row_idx, mti, mri, cgi, id_param);
	} else {
		redisplay_automation_interpolation (row, row_idx, mti, mri, cgi, id_param);
	}
}

void
Grid::redisplay_inter_midi_regions (int mti)
{
	TreeModel::Children::iterator row_it = model->children ().begin ();
	for (int row_idx = 0; row_idx < pattern.global_nrows; row_idx++) {
		// Get row
		TreeModel::Row row = *row_it++;
		if (!is_region_defined (row_idx, mti)) {
			redisplay_undefined_notes (row, mti);
			redisplay_undefined_automations (row, row_idx, mti);
		}
	}
}

void
Grid::redisplay_midi_region (int mti, int mri, const MidiRegionPattern& mrp, const MidiRegionPatternPhenomenalDiff* mrp_diff)
{
	if (!pattern.midi_region_pattern (mti, mri).enabled)
		return;

	redisplay_region_notes (mti, mri, mrp.mnp, mrp_diff ? &mrp_diff->mnp_diff : 0);
	redisplay_region_automations (mti, mri, mrp.mrap, mrp_diff ? &mrp_diff->mrap_diff : 0);
}

void
Grid::redisplay_region_notes (int mti, int mri, const MidiNotesPattern& mnp, const MidiNotesPatternPhenomenalDiff* mnp_diff)
{
	if (mnp_diff == 0 || mnp_diff->full) {
		for (size_t cgi = 0; cgi < mnp.ntracks; cgi++) {
			redisplay_note_column (mti, mri, cgi, mnp);
		}
	} else {
		const MidiNotesPatternPhenomenalDiff::Cgi2RowsPhenomenalDiff& cgi2rows_diff = mnp_diff->cgi2rows_diff;
		for (MidiNotesPatternPhenomenalDiff::Cgi2RowsPhenomenalDiff::const_iterator it = cgi2rows_diff.begin (); it != cgi2rows_diff.end (); ++it) {
			redisplay_note_column (mti, mri, it->first, mnp, &it->second);
		}
	}
}

void
Grid::redisplay_region_automations (int mti, int mri, const MidiRegionAutomationPattern& mrap, const MidiRegionAutomationPatternPhenomenalDiff* mrap_diff)
{
	if (mrap_diff == 0 || mrap_diff->full || mrap_diff->ap_diff.full) {
		const MidiTrackPattern* mtp = pattern.tps[mti]->midi_track_pattern ();
		for (ParameterSetConstIt it = mtp->enabled_region_params.begin (); it != mtp->enabled_region_params.end (); ++it)
		{
			const Evoral::Parameter& param = *it;
			// Make sure the param of that region, this may not be the case if
			// it was just added to the tracker interface and another region
			// was already displaying that parameter.
			//
			// TODO: An alternate way to deal with that would be to enable such
			// region during TrackerEditor::setup, Grid::setup or
			// Pattern::setup.
			pattern.midi_region_pattern (mti, mri).mrap.set_param_enabled (param, true);
			redisplay_region_automation_param (mti, mri, mrap, param);
		}
	} else {
		const AutomationPatternPhenomenalDiff& ap_diff = mrap_diff->ap_diff;
		for (AutomationPatternPhenomenalDiff::Param2RowsPhenomenalDiff::const_iterator it = ap_diff.param2rows_diff.begin (); it != ap_diff.param2rows_diff.end (); ++it)
		{
			redisplay_region_automation_param (mti, mri, mrap, it->first, &it->second);
		}
	}
}

void
Grid::redisplay_region_automation_param (int mti, int mri, const MidiRegionAutomationPattern& mrap, const Evoral::Parameter& param, const RowsPhenomenalDiff* rows_diff)
{
	int cgi = to_cgi (mti, PBD::ID (0), param);

	if (cgi < 0) {
		return;
	}

	int row_offset = get_row_offset (mti, mri);
	if (rows_diff == 0 || rows_diff->full) {
		for (int row_idx = 0; row_idx < mrap.nrows; row_idx++) {
			redisplay_region_automation_param_row (mti, mri, cgi, row_idx + row_offset, mrap, param);
		}
	} else {
		for (std::set<int>::const_iterator it = rows_diff->rows.begin (); it != rows_diff->rows.end (); ++it) {
			redisplay_region_automation_param_row (mti, mri, cgi, *it + row_offset, mrap, param);
		}
	}
}

void
Grid::redisplay_region_automation_param_row (int mti, int mri, int cgi, int row_idx, const MidiRegionAutomationPattern& mrap, const Evoral::Parameter& param)
{
	IDParameter id_param (PBD::ID (0), param);
	// TODO: optimize!
	Gtk::TreeModel::Row row = to_row (row_idx);
	int automation_count = pattern.control_events_count (row_idx, mti, mri, id_param);

	// Fill background colors
	redisplay_automation_background (row, mti, cgi);

	// Fill default blank foreground text and color
	redisplay_blank_automation_foreground (row, mti, cgi);

	if (automation_count > 0) {
		redisplay_automation (row, row_idx, mti, mri, cgi, id_param);
	} else {
		redisplay_automation_interpolation (row, row_idx, mti, mri, cgi, id_param);
	}
}

void
Grid::redisplay_note_column (int mti, int mri, int cgi, const MidiNotesPattern& mnp, const RowsPhenomenalDiff* rows_diff)
{
	int row_offset = get_row_offset (mti, mri);
	if (rows_diff == 0 || rows_diff->full) {
		for (int rrrow_idx = 0; rrrow_idx < get_row_size (mti, mri); rrrow_idx++) {
			redisplay_note (mti, mri, cgi, row_offset + rrrow_idx, mnp);
		}
	} else {
		for (std::set<int>::const_iterator row_it = rows_diff->rows.begin (); row_it != rows_diff->rows.end (); ++row_it) {
			redisplay_note (mti, mri, cgi, row_offset + *row_it, mnp);
		}
	}
}

void
Grid::redisplay_note (int mti, int mri, int cgi, int row_idx, const MidiNotesPattern& mnp)
{
	// TODO: optimize!
	Gtk::TreeModel::Row row = to_row (row_idx);

	// Fill background colors
	// TODO: optimize, should only need to redisplay note bg once
	redisplay_note_background (row, mti, cgi);

	// Fill with blank foreground text and colors
	redisplay_blank_note_foreground (row, mti, cgi);

	// Display note
	size_t off_notes_count = pattern.off_notes_count (row_idx, mti, mri, cgi);
	size_t on_notes_count = pattern.on_notes_count (row_idx, mti, mri, cgi);
	if (0 < on_notes_count || 0 < off_notes_count) {
		redisplay_note_foreground (row, row_idx, mti, mri, cgi);
	}
}

void
Grid::redisplay_selection ()
{
	if (_subgrid_selector.has_prev_selection ()) {
		int max_row_idx = std::min(_subgrid_selector.prev_bottom_row_idx, pattern.global_nrows);
		int max_col_idx = _subgrid_selector.prev_right_col_idx;
		// Undisplay selection
		for (int row_idx = _subgrid_selector.prev_top_row_idx; row_idx <= max_row_idx; row_idx++) {
			for (int col_idx = _subgrid_selector.prev_left_col_idx; col_idx <= max_col_idx; col_idx++) {
				redisplay_cell_background (row_idx, col_idx);
			}
		}
		// Redisplay current row background and cursor possibly deleted
		// by previous selection (TODO: redundant most of the time)
		redisplay_current_row_background ();
		redisplay_current_cursor ();
	}
	if (_subgrid_selector.has_selection ()) {
		if (_subgrid_selector.has_destination ()) {
			// Display selection
			for (int row_idx = _subgrid_selector.top_row_idx; row_idx <= _subgrid_selector.bottom_row_idx; row_idx++) {
				for (int col_idx = _subgrid_selector.left_col_idx; col_idx <= _subgrid_selector.right_col_idx; col_idx++) {
					redisplay_cell_selection (row_idx, col_idx);
				}
			}
		}
	}
}

void
Grid::redisplay_cell_selection (int row_idx, int col_idx)
{
	Gtk::TreeViewColumn* col = to_col (col_idx);
	if (is_cell_defined (row_idx, col)) {
		if (is_note_type (col)) {
			redisplay_note_cell_selection (row_idx, col);
		} else {
			redisplay_automation_cell_selection (row_idx, col);
		}
	}
}

void
Grid::redisplay_note_cell_selection (int row_idx, const Gtk::TreeViewColumn* col)
{
	// TODO: can be refactored with Grid::redisplay_current_note_cursor
	int mti = to_mti (col);
	int cgi = to_cgi (col);
	Gtk::TreeModel::Row row = to_row (row_idx);
	switch (get_note_type (col)) {
	case TrackerColumn::NoteType::NOTE:
		row[columns._note_background_color[mti][cgi]] = TrackerUtils::color_to_string (selection_color);
		break;
	case TrackerColumn::NoteType::CHANNEL:
		row[columns._channel_background_color[mti][cgi]] = TrackerUtils::color_to_string (selection_color);
		break;
	case TrackerColumn::NoteType::VELOCITY:
		row[columns._velocity_background_color[mti][cgi]] = TrackerUtils::color_to_string (selection_color);
		break;
	case TrackerColumn::NoteType::DELAY:
		row[columns._delay_background_color[mti][cgi]] = TrackerUtils::color_to_string (selection_color);
		break;
	default:
		std::cerr << "Grid::redisplay_note_cell_selection: Implementation Error!" << std::endl;
	}
}

void
Grid::redisplay_automation_cell_selection (int row_idx, const Gtk::TreeViewColumn* col)
{
	// TODO: can be refactored with Grid::redisplay_current_automation_cursor
	int mti = to_mti (col);
	int cgi = to_cgi (col);
	Gtk::TreeModel::Row row = to_row (row_idx);
	switch (get_automation_type (col)) {
	case TrackerColumn::AutomationType::AUTOMATION:
		row[columns._automation_background_color[mti][cgi]] = TrackerUtils::color_to_string (selection_color);
		break;
	case TrackerColumn::AutomationType::AUTOMATION_DELAY:
		row[columns._automation_delay_background_color[mti][cgi]] = TrackerUtils::color_to_string (selection_color);
		break;
	default:
		std::cerr << "Grid::redisplay_automation_cell_selection: Implementation Error!" << std::endl;
	}
}

void
Grid::remove_unused_rows ()
{
	TreeModel::Children::const_iterator row_it = model->children ().begin ();
	std::advance (row_it, pattern.global_nrows);
	for (; row_it != model->children ().end ();) {
		row_it = model->erase (row_it);
	}
}

void
Grid::unset_underline_current_step_edit_cell ()
{
	if (current_is_note_type) {
		unset_underline_current_step_edit_note_cell ();
	} else {
		unset_underline_current_step_edit_automation_cell ();
	}
}

void
Grid::unset_underline_current_step_edit_note_cell ()
{
	Gtk::TreeModel::Row& row = current_row;
	int mti = current_mti;
	int cgi = current_cgi;
	switch (current_note_type) {
	case TrackerColumn::NoteType::NOTE:
		break;
	case TrackerColumn::NoteType::CHANNEL: {
		std::string val_str = row[columns.channel[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		row[columns.channel[mti][cgi]] = TrackerUtils::int_unpad (val_str, base ());
		row[columns._channel_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	case TrackerColumn::NoteType::VELOCITY: {
		std::string val_str = row[columns.velocity[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		row[columns.velocity[mti][cgi]] = TrackerUtils::int_unpad (val_str, base ());
		row[columns._velocity_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	case TrackerColumn::NoteType::DELAY: {
		std::string val_str = row[columns.delay[mti][cgi]];
		if (is_blank (val_str)) {
			// For some unknown reason the attributes must be reset
			row[columns._delay_attributes[mti][cgi]] = Pango::AttrList ();
			break;
		}
		bool is_null = TrackerUtils::string_to_num<int> (val_str, base ()) == 0;
		row[columns.delay[mti][cgi]] = is_null ? mk_delay_blank () : TrackerUtils::int_unpad (val_str, base ());
		row[columns._delay_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	default:
		std::cerr << "Grid::redisplay_current_note_cursor: Implementation Error!" << std::endl;
	}
}

void
Grid::unset_underline_current_step_edit_automation_cell ()
{
	Gtk::TreeModel::Row& row = current_row;
	int mti = current_mti;
	int cgi = current_cgi;
	switch (current_automation_type) {
	case TrackerColumn::AutomationType::AUTOMATION: {
		std::string val_str = row[columns.automation[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		row[columns.automation[mti][cgi]] = TrackerUtils::float_unpad (val_str, base (), precision ());
		row[columns._automation_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	case TrackerColumn::AutomationType::AUTOMATION_DELAY: {
		std::string val_str = row[columns.automation_delay[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		bool is_null = TrackerUtils::string_to_num<int> (val_str, base ()) == 0;
		row[columns.automation_delay[mti][cgi]] = is_null ? mk_delay_blank () : TrackerUtils::int_unpad (val_str, base ());
		row[columns._automation_delay_attributes[mti][cgi]] = Pango::AttrList ();
		break;
	}
	default:
		std::cerr << "Grid::redisplay_current_automation_cursor: Implementation Error!" << std::endl;
	}
}

void
Grid::set_underline_current_step_edit_cell ()
{
	if (step_edit()) {
		if (current_is_note_type) {
			set_underline_current_step_edit_note_cell ();
		} else {
			set_underline_current_step_edit_automation_cell ();
		}
	}
}

void
Grid::set_underline_current_step_edit_note_cell ()
{
	Gtk::TreeModel::Row& row = current_row;
	int row_idx = current_row_idx;
	int mti = current_mti;
	int cgi = current_cgi;
	switch (current_note_type) {
	case TrackerColumn::NoteType::NOTE:
		break;
	case TrackerColumn::NoteType::CHANNEL: {
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
	case TrackerColumn::NoteType::VELOCITY: {
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
	case TrackerColumn::NoteType::DELAY: {
		std::string val_str = row[columns.delay[mti][cgi]];
		NotePtr note = get_note (row_idx, mti, cgi);
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
		std::cerr << "Grid::redisplay_current_note_cursor: Implementation Error!" << std::endl;
	}
}

void
Grid::set_underline_current_step_edit_automation_cell ()
{
	Gtk::TreeModel::Row& row = current_row;
	int row_idx = current_row_idx;
	int mti = current_mti;
	int mri = current_mri;
	int cgi = current_cgi;
	switch (current_automation_type) {
	case TrackerColumn::AutomationType::AUTOMATION: {
		std::string val_str = row[columns.automation[mti][cgi]];
		if (is_blank (val_str)) {
			break;
		}
		IDParameter id_param = get_id_param (mti, cgi);
		double l = lower (current_row, mti, id_param);
		double u = upper (current_row, mti, id_param);
		double mlu = std::max (std::abs (l), std::abs (u));
		int min_pos = is_int_param (id_param) ? 0 : MainToolbar::min_position;
		int max_pos = std::floor (std::log10 (mlu));
		set_current_pos (min_pos, max_pos);
		std::pair<std::string, Pango::AttrList> ul = underlined_value (val_str);
		row[columns.automation[mti][cgi]] = ul.first;
		row[columns._automation_attributes[mti][cgi]] = ul.second;
		break;
	}
	case TrackerColumn::AutomationType::AUTOMATION_DELAY: {
		std::string val_str = row[columns.automation_delay[mti][cgi]];
		if (!has_automation_delay (row_idx, mti, mri, cgi)) {
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
		std::cerr << "Grid::redisplay_current_automation_cursor: Implementation Error!" << std::endl;
	}
}

void
Grid::redisplay_audio_track (int mti, const AudioTrackPattern& atp, const AudioTrackPatternPhenomenalDiff* atp_diff)
{
	// TODO: implement
}

void
Grid::set_cell_content (int row_idx, int col_idx, const std::string& text)
{
	if (!is_cell_defined (row_idx, col_idx)) {
		return;
	}

	const TreeViewColumn* col = to_col (col_idx);
	Gtk::TreeModel::Path path = to_path (row_idx);
	int mti = to_mti (col);
	int cgi = to_cgi (col);
	int mri = to_mri (path, col);

	if (is_note_type (col)) {
		switch (get_note_type (col)) {
		case TrackerColumn::NoteType::NOTE:
			set_note_text(row_idx, mti, mri, cgi, text);
			break;
		case TrackerColumn::NoteType::CHANNEL:
			set_note_channel_text (row_idx, mti, mri, cgi, text);
			break;
		case TrackerColumn::NoteType::VELOCITY:
			set_note_velocity_text (row_idx, mti, mri, cgi, text);
			break;
		case TrackerColumn::NoteType::DELAY:
			set_note_delay_text (row_idx, mti, mri, cgi, text);
			break;
		default:
			std::cerr << "Grid::set_cell_content: Implementation Error!" << std::endl;
		}
	} else { // Auto type
		switch (get_automation_type (col)) {
		case TrackerColumn::AutomationType::AUTOMATION:
			set_automation_value_text (row_idx, mti, mri, cgi, text);
			break;
		case TrackerColumn::AutomationType::AUTOMATION_DELAY:
			set_automation_delay_text (row_idx, mti, mri, cgi, text);
			break;
		default:
			std::cerr << "Grid::set_cell_content: Implementation Error!" << std::endl;
		}
	}
}

// TODO: this somewhat duplicates Grid::redisplay_note_foreground.  Is there a
// way to refactorize that?
std::string
Grid::get_cell_content (int row_idx, int col_idx) const
{
	const TreeViewColumn* col = to_col (col_idx);
	Gtk::TreeModel::Path path = to_path (row_idx);
	int mti = to_mti (col);
	int cgi = to_cgi (col);
	int mri = to_mri (path, col);

	if (is_note_type (col)) {
		if (!has_note (path, mti, cgi)) {
			return "";
		}

		// Ignore ***
		if (!is_note_displayable (row_idx, mti, mri, cgi)) {
			return "";
		}

		NotePtr on_note = get_on_note (row_idx, mti, cgi);
		NotePtr off_note = get_off_note (row_idx, mti, cgi);
		switch (get_note_type (col)) {
		case TrackerColumn::NoteType::NOTE:
			if (on_note) {
				return ParameterDescriptor::midi_note_name (on_note->note ());
			} else {
				return note_off_str;
			}
			break;
		case TrackerColumn::NoteType::CHANNEL:
			if (on_note) {
				return TrackerUtils::channel_to_string (on_note->channel (), base ());
			} else {
				return "";
			}
			break;
		case TrackerColumn::NoteType::VELOCITY:
			if (on_note) {
				return TrackerUtils::num_to_string ((int)on_note->velocity (), base (), precision ());
			} else {
				return "";
			}
			break;
		case TrackerColumn::NoteType::DELAY: {
			int delay = on_note ? get_on_note_delay (on_note, row_idx, mti, mri)
				: get_off_note_delay (off_note, row_idx, mti, mri);
			if (delay != 0) {
				return TrackerUtils::num_to_string (delay, base (), precision ());
			} else {
				return "";
			}
			break;
		}
		default:
			std::cerr << "Grid::get_cell_content: Implementation Error!" << std::endl;
		}
	} else { // Auto type
		if (!has_automation_value (row_idx, mti, mri, cgi)) {
			return "";
		}

		// For now ignore ***
		if (!is_automation_displayable (row_idx, mti, mri, cgi)) {
			return "";
		}

		switch (get_automation_type (col)) {
		case TrackerColumn::AutomationType::AUTOMATION: {
			double value = get_automation_value (row_idx, mti, mri, cgi).first;
			return TrackerUtils::num_to_string (value, base (), precision ());
			break;
		}
		case TrackerColumn::AutomationType::AUTOMATION_DELAY: {
			int delay = get_automation_delay (row_idx, mti, mri, cgi).first;
			if (delay != 0) {
				return TrackerUtils::num_to_string (delay, base (), precision ());
			} else {
				return "";
			}
			break;
		}
		default:
			std::cerr << "Grid::get_cell_content: Implementation Error!" << std::endl;
		}
	}
	return "";
}

int
Grid::get_time_width () const
{
	return time_column->get_width ();
}

int
Grid::get_track_width (int mti) const
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
Grid::get_right_separator_width (int mti) const
{
	return right_separator_columns[mti]->get_width ();
}

int
Grid::get_track_separator_width () const
{
	return TRACK_SEPARATOR_WIDTH;
}

std::string
Grid::get_name (int mti, const IDParameter& id_param, bool shorten) const
{
	std::string long_name = pattern.get_name (mti, id_param);
	size_t tl = 7;               // target length
	return shorten ? PBD::short_version(long_name, tl) : long_name;
}

void
Grid::set_param_enabled (int mti, const IDParameter& id_param, bool enabled)
{
	pattern.set_param_enabled (mti, id_param, enabled);
}

bool
Grid::is_param_enabled (int mti, const IDParameter& id_param) const
{
	return pattern.is_param_enabled (mti, id_param);
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
	return underlined_value (TrackerUtils::num_to_string (val, base (), precision ()));
}

std::pair<std::string, Pango::AttrList>
Grid::underlined_value (float val) const
{
	return underlined_value (TrackerUtils::num_to_string (val, base (), precision ()));
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
Grid::is_int_param (const IDParameter& id_param) const
{
	return TrackerUtils::is_region_automation (id_param.second);
}

void
Grid::parameter_changed (std::string const& p)
{
	if (p == "vkeybd-layout") {
		read_keyboard_layout ();
	}
}

void
Grid::color_changed ()
{
	read_colors ();
}

void
Grid::scroll_to_current_row ()
{
	scroll_to_row (current_path);
}

std::string
Grid::to_string (const std::string& indent) const
{
	std::stringstream ss;

	std::string indent1 = indent + "  ";
	std::string indent2 = indent + "    ";
	// TODO: add more attributes

	// col2params
	ss << indent << "col2params:";
	for (unsigned mti = 0; mti != col2params.size(); ++mti) {
		ss << std::endl << indent1 << "col2params[" << mti << "]:";
		for (const auto& cip : col2params[mti].left) {
			ss << std::endl << indent2 << "id_param[col=" << cip.first << "] = "
			   << "(id=" << cip.second.first.to_s () << ", param=" << cip.second.second << ")";
		}
	}
	return ss.str ();
}

/////////////////////
// Edit Pattern    //
/////////////////////

int
Grid::to_row_index (const std::string& path_str) const
{
	return to_row_index (to_path (path_str));
}

int
Grid::to_row_index (const TreeModel::Path& path) const
{
	if (path.empty())
		return -1;
	return path.front ();
}

Gtk::TreeModel::Path
Grid::to_path (const std::string& path_str) const
{
	return TreeModel::Path (path_str);
}

Gtk::TreeModel::Path
Grid::to_path (int row_idx) const
{
	return TreeModel::Path (1U, row_idx);
}

int
Grid::get_row_offset (int mti, int mri) const
{
	return pattern.row_offset[mti] + pattern.tps[mti]->midi_track_pattern ()->row_offset[mri];
}

int
Grid::get_row_size (int mti, int mri) const
{
	return pattern.midi_region_pattern (mti, mri).nrows;
}

Gtk::TreeModel::Row
Grid::to_row (int row_idx) const
{
	// TODO: can probably be optimized by using

	// const Gtk::TreeIter iter(m_scopeModel->get_iter(path));
	// if(!iter)
	// 	return false;
	// const auto row = *iter;

	TreeModel::Children::const_iterator row_it = model->children ().begin ();
	std::advance (row_it, row_idx);
	return *row_it;
}

int
Grid::to_col_index (const TreeViewColumn* col)
{
	const std::vector<TreeViewColumn*>& cols = (std::vector<TreeViewColumn*>)get_columns ();

	for (int i = 0; i < (int)cols.size (); i++) {
		if (cols[i] == col) {
			return i;
		}
	}

	return -1;
}

Gtk::TreeViewColumn*
Grid::to_col (int col_idx)
{
	return get_column (col_idx);
}

const Gtk::TreeViewColumn*
Grid::to_col (int col_idx) const
{
	return get_column (col_idx);
}

bool
Grid::is_note_displayable (int row_idx, int mti, int mri, int cgi) const
{
	return pattern.is_note_displayable (row_idx, mti, mri, cgi);
}

bool
Grid::is_automation_displayable (int row_idx, int mti, int mri, int cgi) const
{
	IDParameter id_param = get_id_param (mti, cgi);
	return is_automation_displayable (row_idx, mti, mri, id_param);
}

bool
Grid::is_automation_displayable (int row_idx, int mti, int mri, const IDParameter& id_param) const
{
	return pattern.is_automation_displayable (row_idx, mti, mri, id_param);
}

NotePtr
Grid::get_on_note (const std::string& path, int mti, int cgi) const
{
	return get_on_note (to_path (path), mti, cgi);
}

NotePtr
Grid::get_on_note (const TreeModel::Path& path, int mti, int cgi) const
{
	return get_on_note (to_row_index (path), mti, cgi);
}

NotePtr
Grid::get_on_note (int row_idx, int mti, int cgi) const
{
	return pattern.on_note (row_idx, mti, pattern.to_mri (row_idx, mti), cgi);
}

NotePtr
Grid::get_off_note (const std::string& path_str, int mti, int cgi) const
{
	return get_off_note (to_path (path_str), mti, cgi);
}

NotePtr
Grid::get_off_note (const TreeModel::Path& path, int mti, int cgi) const
{
	return get_off_note (to_row_index (path), mti, cgi);
}

NotePtr
Grid::get_off_note (int row_idx, int mti, int cgi) const
{
	return pattern.off_note (row_idx, mti, pattern.to_mri (row_idx, mti), cgi);
}

int
Grid::get_on_note_delay (NotePtr on_note, int row_idx, int mti, int mri) const
{
	return pattern.region_relative_delay_ticks (on_note->time (), row_idx, mti, mri);
}

int
Grid::get_off_note_delay (NotePtr off_note, int row_idx, int mti, int mri) const
{
	return pattern.region_relative_delay_ticks (off_note->end_time (), row_idx, mti, mri);
}

NotePtr
Grid::get_note (int row_idx, int mti, int cgi) const
{
	return get_note (to_path (row_idx), mti, cgi);
}

NotePtr
Grid::get_note (const std::string& path_str, int mti, int cgi) const
{
	return get_note (to_path (path_str), mti, cgi);
}

NotePtr
Grid::get_note (const TreeModel::Path& path, int mti, int cgi) const
{
	NotePtr on_note = get_on_note (path, mti, cgi);
	if (on_note) {
		return on_note;
	}
	return get_off_note (path, mti, cgi);
}

bool
Grid::has_note (const TreeModel::Path& path, int mti, int cgi) const
{
	return get_note (path, mti, cgi) != 0;
}

void
Grid::editing_note_started (CellEditable* ed, const std::string& path, int mti, int cgi)
{
	edit_col = note_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_note_channel_started (CellEditable* ed, const std::string& path, int mti, int cgi)
{
	edit_col = note_channel_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_note_velocity_started (CellEditable* ed, const std::string& path, int mti, int cgi)
{
	edit_col = note_velocity_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_note_delay_started (CellEditable* ed, const std::string& path, int mti, int cgi)
{
	edit_col = note_delay_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_automation_started (CellEditable* ed, const std::string& path, int mti, int cgi)
{
	edit_col = automation_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_automation_delay_started (CellEditable* ed, const std::string& path, int mti, int cgi)
{
	edit_col = automation_delay_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
Grid::editing_started (CellEditable* ed, const std::string& path, int mti, int cgi)
{
	edit_path = TreePath (path);
	edit_row_idx = to_row_index (edit_path);
	edit_mti = mti;
	edit_mtp = pattern.tps[edit_mti];
	edit_mri = pattern.to_mri (edit_row_idx, edit_mti);
	edit_cgi = cgi;
	editing_editable = ed;

	// For now set the current cursor to the edit cursor
	current_mti = mti;
}

void
Grid::clear_editables ()
{
	edit_path.clear ();
	edit_row_idx = -1;
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
Grid::parse_pitch (const std::string& text) const
{
	return TrackerUtils::parse_pitch (text, tracker_editor.main_toolbar.octave_spinner.get_value_as_int ());
}

void
Grid::note_edited (const std::string& path, const std::string& text)
{
	set_note_text (edit_row_idx, edit_mti, edit_mri, edit_cgi, text);
	clear_editables ();
}

void
Grid::set_note_text (int row_idx, int mti, int mri, int cgi, const std::string& text)
{
	std::string norm_text = boost::erase_all_copy (text, " ");
	bool is_del = norm_text.empty();
	bool is_off = !is_del && (norm_text[0] == note_off_str[0]);
	uint8_t pitch = parse_pitch (norm_text);
	bool is_on = pitch <= 127;

	if (is_on) {
		set_on_note (row_idx, mti, mri, cgi, pitch);
	} else if (is_off) {
		set_off_note (row_idx, mti, mri, cgi);
	} else if (is_del) {
		delete_note (row_idx, mti, mri, cgi);
	}
}

std::pair<uint8_t, uint8_t>
Grid::set_on_note (int row_idx, int mti, int mri, int cgi, uint8_t pitch)
{
	uint8_t channel = tracker_editor.main_toolbar.channel_spinner.get_value_as_int () - 1;
	uint8_t velocity = tracker_editor.main_toolbar.velocity_spinner.get_value_as_int ();
	return set_on_note (row_idx, mti, mri, cgi, pitch, channel, velocity);
}

std::pair<uint8_t, uint8_t>
Grid::set_on_note (int row_idx, int mti, int mri, int cgi, uint8_t pitch, uint8_t ch, uint8_t vel)
{
	// Ignore *** if configured as such
	if (skip_note_stars (row_idx, mti, mri, cgi)) {
		return std::make_pair(ch, vel);
	}
	// NEXT.3: support ***

	uint8_t new_ch = ch;
	uint8_t new_vel = vel;

	// Abort if the new pitch is invalid
	if (127 < pitch) {
		return std::make_pair(0, 0);
	}

	NotePtr on_note = get_on_note (row_idx, mti, cgi);
	NotePtr off_note = get_off_note (row_idx, mti, cgi);

	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int ();

	MidiModel::NoteDiffCommand* cmd = nullptr;

	if (on_note) {
		// Change the pitch of the on note
		char const * opname = _("change note");
		// TODO: move the following outside of Grid.  Maybe within Pattern or
		// such.  That way the ardour/midi_model.h include can be removed.
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->change (on_note, MidiModel::NoteDiffCommand::NoteNumber, pitch);
		if (tracker_editor.main_toolbar.overwrite_existing) {
			cmd->change (on_note, MidiModel::NoteDiffCommand::Channel, ch);
			cmd->change (on_note, MidiModel::NoteDiffCommand::Velocity, vel);
			set_note_delay_update_cmd (on_note, off_note, row_idx, mti, mri, delay, cmd);
		} else {
			new_ch = on_note->channel ();
			new_vel = on_note->velocity ();
		}
	} else if (off_note) {
		// Replace off note by another (non-off) note. Calculate the start
		// time and length of the new on note.
		Temporal::Beats start = off_note->end_time ();
		Temporal::Beats end = pattern.next_on_note_beats (row_idx, mti, mri, cgi);
		Temporal::Beats length = end - start;
		// Build note using defaults
		NotePtr new_note (new NoteType (ch, start, length, pitch, vel));
		char const * opname = _("add note");
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->add (new_note);
		if (tracker_editor.main_toolbar.overwrite_existing) {
			set_note_delay_update_cmd (new_note, off_note, row_idx, mti, mri, delay, cmd);
		}
		// Pre-emptively add the note in the pattern so that it knows in which
		// track it is supposed to be.
		pattern.midi_notes_pattern (mti, mri).add (cgi, new_note);
	} else {
		// Create a new on note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = pattern.region_relative_beats (row_idx, mti, mri, delay);
		NotePtr prev_note = pattern.find_prev_on_note (row_idx, mti, mri, cgi);
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
		Temporal::Beats end = pattern.next_on_note_beats (row_idx, mti, mri, cgi);
		// If new note occur between the on note and off note of the previous
		// note, then use the off note of the previous note as off note of the
		// new note.
		if (prev_note && here < prev_end && prev_end < end)
			end = prev_end;
		Temporal::Beats length = end - here;
		NotePtr new_note (new NoteType (ch, here, length, pitch, vel));
		cmd->add (new_note);
		// Pre-emptively add the note in mnp so that it knows in which track it is
		// supposed to be.
		pattern.midi_notes_pattern (mti, mri).add (cgi, new_note);
	}

	// Apply note changes
	apply_command (mti, mri, cmd);

	return std::make_pair(new_ch, new_vel);
}

void
Grid::set_off_note (int row_idx, int mti, int mri, int cgi)
{
	// Ignore *** if configured as such
	if (skip_note_stars (row_idx, mti, mri, cgi)) {
		return;
	}
	// NEXT.3: support ***

	NotePtr on_note = get_on_note (row_idx, mti, cgi);
	NotePtr off_note = get_off_note (row_idx, mti, cgi);

	MidiModel::NoteDiffCommand* cmd = nullptr;

	if (on_note) {
		// Replace the on note by an off note, that is remove the on note
		char const * opname = _("delete note");
		cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is no off note, update the length of the preceding node
		// to match the new off note (smart off note).
		if (!off_note) {
			NotePtr prev_note = pattern.find_prev_on_note (row_idx, mti, mri, cgi);
			if (prev_note) {
				Temporal::Beats length = on_note->time () - prev_note->time ();
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else {
		// Create a new off note in an empty cell or update existing off note
		int delay = (off_note and !tracker_editor.main_toolbar.overwrite_existing) ?
			get_off_note_delay (off_note, row_idx, mti, mri)
			: tracker_editor.main_toolbar.delay_spinner.get_value_as_int ();

		// Fetch useful information for most cases
		Temporal::Beats here = pattern.region_relative_beats (row_idx, mti, mri, delay);
		NotePtr prev_note = pattern.find_prev_on_note (row_idx, mti, mri, cgi);
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
	apply_command (mti, mri, cmd);
}

void
Grid::delete_note (int row_idx, int mti, int mri, int cgi)
{
	// Ignore *** if configured as such
	if (skip_note_stars (row_idx, mti, mri, cgi)) {
		return;
	}

	MidiModel::NoteDiffCommand* cmd = nullptr;
	char const * opname = _("delete note");
	cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);

	// Remove all on notes
	RowToNotesRange on_rng = pattern.on_notes_range (row_idx, mti, mri, cgi);
	for (; on_rng.first != on_rng.second; ++on_rng.first) {
		NotePtr on_note = on_rng.first->second;
		cmd->remove (on_note);
	}

	// Change duration of off note if necessary
	NotePtr off_note = get_off_note (row_idx, mti, cgi);
	if (off_note) {
		NotePtr prev_note = pattern.find_prev_on_note (row_idx, mti, mri, cgi);
		if (prev_note && (prev_note == off_note)) {
			// Calculate the length of the previous note and update it to end at
			// the next note
			Temporal::Beats start = prev_note->time ();
			Temporal::Beats on_end = pattern.next_on_note_beats (row_idx, mti, mri, cgi);
			Temporal::Beats off_end = pattern.next_off_note_beats (row_idx, mti, mri, cgi);
			Temporal::Beats end = on_end < off_end ? on_end : off_end;
			Temporal::Beats length = end - start;
			cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
		}
	}

	// Apply note changes
	apply_command (mti, mri, cmd);
}

void
Grid::note_channel_edited (const std::string& path, const std::string& text)
{
	set_note_channel_text (edit_row_idx, edit_mti, edit_mri, edit_cgi, text);
	clear_editables ();
}

void
Grid::set_note_channel_text (int row_idx, int mti, int mri, int cgi, const std::string& text)
{
	// Ignore ***
	if (!is_note_displayable (row_idx, mti, mri, cgi)) {
		return;
	}

	NotePtr note = get_on_note (row_idx, mti, cgi);
	if (text.empty() || !note) {
		return;
	}

	if (TrackerUtils::is_number<int> (text, base ())) {
		set_note_channel (mti, mri, note, TrackerUtils::string_to_channel (text, base ()));
	}
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
Grid::note_velocity_edited (const std::string& path, const std::string& text)
{
	set_note_velocity_text (edit_row_idx, edit_mti, edit_mri, edit_cgi, text);
	clear_editables ();
}

void
Grid::set_note_velocity_text (int row_idx, int mti, int mri, int cgi, const std::string& text)
{
	NotePtr note = get_on_note (row_idx, mti, cgi);
	if (text.empty() || !note) {
		return;
	}

	// Ignore ***
	if (!is_note_displayable (row_idx, mti, mri, cgi)) {
		return;
	}

	// Parse the edited velocity and set the note velocity
	if (TrackerUtils::is_number<int> (text, base ())) {
		int vel = TrackerUtils::string_to_num<int> (text, base ());
		set_note_velocity (mti, mri, note, vel);
	}
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
Grid::note_delay_edited (const std::string& path, const std::string& text)
{
	set_note_delay_text (edit_row_idx, edit_mti, edit_mri, edit_cgi, text);
	clear_editables ();
}

void
Grid::set_note_delay_text (int row_idx, int mti, int mri, int cgi, const std::string& text)
{
	// Ignore ***
	if (!is_note_displayable (row_idx, mti, mri, cgi)) {
		return;
	}

	// Parse the edited delay and set note delay
	if (TrackerUtils::is_number<int> (text, base ())) {
		int delay = TrackerUtils::string_to_num<int> (text, base ());
		set_note_delay (row_idx, mti, mri, cgi, delay);
	}
}

void
Grid::set_note_delay (int row_idx, int mti, int mri, int cgi, int delay)
{
	NotePtr on_note = get_on_note (row_idx, mti, cgi);
	NotePtr off_note = get_off_note (row_idx, mti, cgi);
	char const* opname = _("change note delay");
	MidiModel::NoteDiffCommand* cmd = pattern.midi_model (mti, mri)->new_note_diff_command (opname);
	set_note_delay_update_cmd (on_note,  off_note, row_idx, mti, mri, delay, cmd);
	apply_command (mti, mri, cmd);
}

void
Grid::set_note_delay_update_cmd (NotePtr on_note,  NotePtr off_note, int row_idx, int mti, int mri, int delay, MidiModel::NoteDiffCommand* cmd)
{
	if (!on_note && !off_note) {
		return;
	}

	// Get corresponding midi region pattern
	const MidiRegionPattern& mrp = pattern.midi_region_pattern (mti, mri);

	// Check if within acceptable boundaries
	if (delay < mrp.delay_ticks_min () || mrp.delay_ticks_max () < delay) {
		return;
	}

	if (on_note) {
		// Modify the start time and length according to the new on note delay

		// Change start time according to new delay
		int delta = delay - get_on_note_delay (on_note, row_idx, mti, mri);
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
		int delta = delay - get_off_note_delay (off_note, row_idx, mti, mri);
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
}

void
Grid::play_note (int mti, uint8_t pitch)
{
	uint8_t ch = tracker_editor.main_toolbar.channel_spinner.get_value_as_int () - 1;
	uint8_t vel = tracker_editor.main_toolbar.velocity_spinner.get_value_as_int ();
	play_note (mti, pitch, ch, vel);
}

void
Grid::play_note (int mti, uint8_t pitch, uint8_t ch, uint8_t vel)
{
	uint8_t event[3];
	event[0] = (MIDI_CMD_NOTE_ON | ch);
	event[1] = pitch;
	event[2] = vel;
	send_live_midi_event (mti, event);

	pressed_keys_pitch_to_channel[pitch] = ch;
}

void
Grid::release_note (int mti, uint8_t pitch)
{
	uint8_t event[3];
	std::map<uint8_t, uint8_t>::iterator it = pressed_keys_pitch_to_channel.find(pitch);
	bool is_present = it != pressed_keys_pitch_to_channel.end();
	uint8_t ch = is_present ? it->second : (tracker_editor.main_toolbar.channel_spinner.get_value_as_int () - 1);
	event[0] = (MIDI_CMD_NOTE_OFF | ch);
	event[1] = pitch;
	event[2] = 0;
	send_live_midi_event (mti, event);

	if (is_present)
		pressed_keys_pitch_to_channel.erase (it);
}

void
Grid::send_live_midi_event (int mti, const uint8_t* buf)
{
	pattern.tps[mti]->midi_track ()->write_immediate_event (Evoral::LIVE_MIDI_EVENT, 3, buf);
}

void
Grid::set_current_cursor (int row_idx, TreeViewColumn* col, bool set_playhead)
{
	set_current_cursor (to_path (row_idx), col, set_playhead);
}

void
Grid::set_current_cursor (const TreeModel::Path& path, TreeViewColumn* col, bool set_playhead)
{
	// NEXT: maybe make sure not to recall if current already set

	// Make sure the cell is defined
	if (!is_cell_defined (path, col)) {
		return;
	}

	// Set current row and col, including selecting track, drawing row and
	// cursor
	set_current_row (path, set_playhead);
	set_current_col (col);

	// Set selector source or destination
	set_selector (path, col);

	// TODO: remove that when no longer necessary
	// Align track toolbar
	tracker_editor.grid_header->align ();
}

void
Grid::set_current_cursor_undefined ()
{
	// Remember the current mtp to update the current track and the
	// step editor
	previous_tp = current_tp;

	current_tp = 0;
	current_col_idx = -1;
	current_col = 0;
	current_mti = -1;
	current_mri = -1;
	current_cgi = -1;
	current_note_type = TrackerColumn::NoteType::NOTE;
	current_automation_type = TrackerColumn::AutomationType::AUTOMATION_SEPARATOR;

	if (!is_current_row_defined ())
		return;

	set_current_row (current_path, true);
}

bool
Grid::is_current_col_defined () const
{
	return current_col != 0;
}

bool
Grid::is_current_cursor_defined ()
{
	return is_current_col_defined () && is_cell_defined (current_path, current_col);
}

void
Grid::set_current_row (int row_idx, bool set_playhead)
{
	set_current_row (to_path (row_idx), set_playhead);
}

void
Grid::set_current_row (const Gtk::TreeModel::Path& path, bool set_playhead)
{
	// NEXT: maybe make sure not to recall if current already set

	// Reset background color over the previous row
	redisplay_row_background (current_row, current_row_idx);

	// Unset underline over previous cursor
	unset_underline_current_step_edit_cell ();

	// Update current row
	current_path = path;
	current_row_idx = to_row_index (path);

	// Abort if current_row_idx is negative.  This may happen if the user clicks
	// on an area with no row.
	if (current_row_idx < 0) {
		return;
	}

	current_beats = pattern.beats_at_row (current_row_idx);
	current_row = to_row (current_row_idx);

	// Display current row background color
	redisplay_current_row_background ();

	// Readjust scroller
	scroll_to_current_row ();

	// Update playhead accordingly
	if (set_playhead && tracker_editor.main_toolbar.sync_playhead) {
		clock_pos = Temporal::timepos_t (pattern.sample_at_row (current_row_idx));
		skip_follow_playhead.push (current_row_idx);
		tracker_editor.session->request_locate (clock_pos.samples ());
	}

	// TODO: remove that when no longer necessary
	// Align track toolbar
	tracker_editor.grid_header->align ();
}

void
Grid::set_current_col (TreeViewColumn* col)
{
	// Remember the current mtp to update the current track and the
	// step editor
	previous_tp = current_tp;

	// Update current col
	current_col = col;
	current_col_idx = to_col_index (col);

	// Update current mti, mtp, cgi and types
	current_mti = to_mti (col);
	current_tp = pattern.tps[current_mti];
	current_mri = pattern.to_mri (current_row_idx, current_mti);
	current_cgi = to_cgi (col);
	current_note_type = get_note_type (col);
	current_automation_type = get_automation_type (col);
	current_is_note_type = is_note_type (col);

	// Update selected track and step editor
	if (previous_tp != current_tp) {
		select_current_track();
		unset_step_editing_previous_track ();
		set_step_editing_current_track ();
	}

	// Now display current row cursor background colors
	redisplay_current_cursor ();

	// Set underline
	set_underline_current_step_edit_cell ();
}

// TODO: this is unused, maybe remove
void
Grid::set_current_row_undefined ()
{
	// Reset background color over the previous row
	redisplay_row_background (current_row, current_row_idx);

	// Unset underline over previous cursor
	unset_underline_current_step_edit_cell ();

	// Set undefined row
	current_row_idx = -1;
}

bool
Grid::is_current_row_defined ()
{
	return 0 <= current_row_idx;
}

void
Grid::set_current_pos (int min_pos, int max_pos)
{
	int position = tracker_editor.main_toolbar.position;
	current_pos = TrackerUtils::clamp (position, min_pos, max_pos);
}

void
Grid::set_selector (const TreeModel::Path& path, const TreeViewColumn* col)
{
	int row_idx = to_row_index (path);
	int col_idx = to_col_index (col);
	if (is_shift_pressed ()) {
		_subgrid_selector.set_destination (row_idx, col_idx);
	} else {
		_subgrid_selector.set_source (row_idx, col_idx);
	}
	redisplay_selection ();
}

IDParameter
Grid::get_id_param (int mti, int cgi) const
{
	IndexBimap::right_const_iterator ac_it = col2auto_cgi[mti].right.find (cgi);
	if (ac_it == col2auto_cgi[mti].right.end ()) {
		return TrackerUtils::defaultIDParameter ();
	}
	int edited_colnum = ac_it->second;
	IndexParamBimap::left_const_iterator it = col2params[mti].left.find (edited_colnum);
	if (it == col2params[mti].left.end ()) {
		return TrackerUtils::defaultIDParameter ();
	}
	return it->second;
}

int
Grid::to_cgi (int mti, const PBD::ID& id, const Evoral::Parameter& param) const
{
	IDParameter id_param (id, param);
	return to_cgi (mti, id_param);
}

int
Grid::to_cgi (int mti, const IDParameter& id_param) const
{
	IndexParamBimap::right_const_iterator cp_it = col2params[mti].right.find (id_param);
	if (cp_it == col2params[mti].right.end ()) {
		return -1;
	}
	int coli = cp_it->second;
	IndexBimap::left_const_iterator cac_it = col2auto_cgi[mti].left.find (coli);
	if (cac_it == col2auto_cgi[mti].left.end ()) {
		return -1;
	}
	return cac_it->second;
}

void
Grid::automation_edited (const std::string& path, const std::string& text)
{
	set_automation_value_text (edit_row_idx, edit_mti, edit_mri, edit_cgi, text);
	clear_editables ();
}

std::vector<Temporal::BBT_Time>
Grid::get_automation_bbt_seq (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return pattern.get_automation_bbt_seq (rowi, mti, mri, id_param);
}

std::pair<double, bool>
Grid::get_automation_value (int row_idx, int mti, int mri, int cgi) const
{
	IDParameter id_param = get_id_param (mti, cgi);
	return pattern.get_automation_value (row_idx, mti, mri, id_param);
}

bool
Grid::has_automation_value (int row_idx, int mti, int mri, int cgi) const
{
	return get_automation_value (row_idx, mti, mri, cgi).second;
}

std::vector<double>
Grid::get_automation_value_seq (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return pattern.get_automation_value_seq (rowi, mti, mri, id_param);
}

double
Grid::get_automation_interpolation_value (int row_idx, int mti, int mri, int cgi) const
{
	IDParameter id_param = get_id_param (mti, cgi);
	return get_automation_interpolation_value (row_idx, mti, mri, id_param);
}

double
Grid::get_automation_interpolation_value (int row_idx, int mti, int mri, const IDParameter& id_param) const
{
	return pattern.get_automation_interpolation_value (row_idx, mti, mri, id_param);
}

void
Grid::set_automation_value_text (int row_idx, int mti, int mri, int cgi, const std::string& text)
{
	bool is_del = text.empty();
	bool is_valid = TrackerUtils::is_number<double> (text, base ());
	if (!is_del && !is_valid) {
		return;
	}

	// Can edit
	if (is_del) {
		delete_automation_value (row_idx, mti, mri, cgi);
	} else {
		double nval = TrackerUtils::string_to_num<double> (text, base ());
		set_automation_value (row_idx, mti, mri, cgi, nval);
	}
}

void
Grid::set_automation_value (int row_idx, int mti, int mri, int cgi, double val)
{
	// Ignore *** if configured as such
	if (skip_automation_stars (row_idx, mti, mri, cgi)) {
		return;
	}
	// NEXT.3: support ***

	// Find the parameter to automate
	IDParameter id_param = get_id_param (mti, cgi);

	// Find delay in case the value has to be created
	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int ();

	return pattern.set_automation_value (val, row_idx, mti, mri, id_param, delay);
}

void
Grid::delete_automation_value (int row_idx, int mti, int mri, int cgi)
{
	// Ignore *** if configured as such
	if (skip_automation_stars (row_idx, mti, mri, cgi)) {
		return;
	}
	// NEXT.3: support ***

	if (has_automation_value (row_idx, mti, mri, cgi)) {
		IDParameter id_param = get_id_param (mti, cgi);
		pattern.delete_automation_value (row_idx, mti, mri, id_param);
	}
}

std::vector<int>
Grid::get_automation_delay_seq (int rowi, int mti, int mri, const IDParameter& id_param) const
{
	return pattern.get_automation_delay_seq (rowi, mti, mri, id_param);
}

void
Grid::automation_delay_edited (const std::string& path, const std::string& text)
{
	set_automation_delay_text (edit_row_idx, edit_mti, edit_mri, edit_cgi, text);
	clear_editables ();
}

std::pair<int, bool>
Grid::get_automation_delay (int row_idx, int mti, int mri, int cgi) const
{
	// Find the parameter to automate
	IDParameter id_param = get_id_param (mti, cgi);
	return pattern.get_automation_delay (row_idx, mti, mri, id_param);
}

bool
Grid::has_automation_delay (int row_idx, int mti, int mri, int cgi) const
{
	return get_automation_delay (row_idx, mti, mri, cgi).second;
}

void
Grid::set_automation_delay (int row_idx, int mti, int mri, int cgi, int delay)
{
	IDParameter id_param = get_id_param (mti, cgi);
	return pattern.set_automation_delay (delay, row_idx, mti, mri, id_param);
}

void
Grid::delete_automation_delay (int row_idx, int mti, int mri, int cgi)
{
	if (has_automation_delay (row_idx, mti, mri, cgi)) {
		set_automation_delay (row_idx, mti, mri, cgi, 0);
	}
}

void
Grid::set_automation_delay_text (int row_idx, int mti, int mri, int cgi, const std::string& text)
{
	bool is_del = text.empty();
	bool is_valid = TrackerUtils::is_number<int> (text, base ());
	if (!is_del && !is_valid) {
		return;
	}

	// Ignore ***
	if (!is_automation_displayable (row_idx, mti, mri, cgi)) {
		return;
	}

	// Check if within acceptable boundaries (NEXT: maybe clamp instead)
	int delay = TrackerUtils::string_to_num<int> (text, base ());
	// TODO: re-enable
	// if (delay < edit_mtp->delay_ticks_min () || edit_mtp->delay_ticks_max () < delay) {
	// 	return;
	// }

	// Can edit
	set_automation_delay (row_idx, mti, mri, cgi, delay);
}

double
Grid::lower (int row_idx, int mti, const IDParameter& id_param) const
{
	return pattern.tps[mti]->lower (row_idx, id_param);
}

double
Grid::upper (int row_idx, int mti, const IDParameter& id_param) const
{
	return pattern.tps[mti]->upper (row_idx, id_param);
}

void
Grid::apply_command (int mti, int mri, MidiModel::NoteDiffCommand* cmd)
{
	// Apply change command
	if (cmd) {
		pattern.apply_command (mti, mri, cmd);
	}
}

void
Grid::follow_playhead (Temporal::timepos_t pos)
{
	if (skip_follow_playhead.empty()) { // Do not skip
		if (pattern.position <= pos && pos < pattern.end && pos != clock_pos) {
			int row_idx = pattern.row_at_time (pos);
			if (row_idx != current_row_idx) {
				if (is_cell_defined (row_idx, current_col)) {
					set_current_cursor (row_idx, current_col, false);
				} else {
					set_current_row (row_idx, false);
				}
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

std::string
Grid::mk_note_blank () const
{
	return mk_blank (4);
}

std::string
Grid::mk_ch_blank () const
{
	return mk_blank (is_hex () ? 1 : 2);
}

std::string
Grid::mk_vel_blank () const
{
	return mk_blank (is_hex () ? 2 : 3);
}

std::string
Grid::mk_delay_blank () const
{
	return mk_blank(is_hex() ? HEX_DELAY_DIGITS : DEC_DELAY_DIGITS);
}

std::string
Grid::mk_automation_blank () const
{
	return mk_blank (is_hex () ? 3 : 4);
}

bool
Grid::skip_note_stars (int row_idx, int mti, int mri, int cgi) const
{
	return !is_note_displayable (row_idx, mti, mri, cgi)
		and !tracker_editor.main_toolbar.overwrite_star;
}

bool
Grid::skip_automation_stars (int row_idx, int mti, int mri, int cgi) const
{
	return !is_automation_displayable (row_idx, mti, mri, cgi)
		and !tracker_editor.main_toolbar.overwrite_star;
}

bool
Grid::is_blank (const std::string& str)
{
	return str.back () == blank_char;
}

void
Grid::connect_tooltips ()
{
	set_has_tooltip ();
	signal_query_tooltip ().connect (sigc::mem_fun(*this, &Grid::set_tooltip), true);
}

bool
Grid::set_tooltip (int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
{
	Gtk::TreeModel::Path path;
	TreeViewColumn* col;
	int cellx, celly;
	int binx, biny;

	convert_widget_to_bin_window_coords(x, y, binx, biny);

	if (!get_path_at_pos(binx, biny, path, col, cellx, celly))
		return false;

	int row_idx = to_row_index (path);
	int mti = to_mti (col);
	int mri = to_mri (path, col);
	int cgi = to_cgi (col);

	std::string tooltip_msg;
	if (time_column == col) {
		tooltip_msg = time_tooltip_msg (row_idx);
	} else if (is_note_type (col)) {
		tooltip_msg = note_tooltip_msg (row_idx, mti, mri, cgi);
	} else if (is_automation_type (col)) {
		tooltip_msg = automation_tooltip_msg (row_idx, mti, mri, cgi);
	}

	if (tooltip_msg.empty())
		return false;

	tooltip->set_markup (_(tooltip_msg.c_str ()));
	set_tooltip_cell(tooltip, &path, col, 0);
	return true;
}

std::string
Grid::time_tooltip_msg (int row_idx) const
{
	Temporal::BBT_Time row_bbt = pattern.earliest_tp->bbt_at_row (row_idx);
	std::string dec_row = TrackerUtils::num_to_string (row_idx, 10);
	std::string hex_row = TrackerUtils::num_to_string (row_idx, 16);
	std::string dec_bbt = TrackerUtils::bbt_to_string (row_bbt, 10);
	std::string hex_bbt = TrackerUtils::bbt_to_string (row_bbt, 16);
	std::stringstream ss;
	ss << TrackerUtils::underline("Row") << " (Dec): "
	   << TrackerUtils::bold(dec_row) << ", "
	   << TrackerUtils::underline("BBT") << " (Dec): "
	   << TrackerUtils::bold(dec_bbt) << std::endl
	   << TrackerUtils::underline("Row") << " (Hex): "
	   << TrackerUtils::bold(hex_row) << ", "
	   << TrackerUtils::underline("BBT") << " (Hex): "
	   << TrackerUtils::bold(hex_bbt);
	return ss.str ();
}

std::string
Grid::on_note_tooltip_msg (NotePtr on_note, const MidiNotesPattern& mnp, int row_idx, int mti, int mri)
{
	std::stringstream ss;
	Temporal::BBT_Time bbt = mnp.on_note_bbt (on_note);
	std::string note_name = ParameterDescriptor::midi_note_name (on_note->note ());
	int ch = on_note->channel ();
	int vel = on_note->velocity ();
	int delay = get_on_note_delay (on_note, row_idx, mti, mri);
	ss << TrackerUtils::underline("BBT") << ": "
		<< TrackerUtils::bold(TrackerUtils::bbt_to_string (bbt, base ())) << ", "
		<< TrackerUtils::underline("Note") << ": "
		<< TrackerUtils::bold(note_name) << ", "
		<< TrackerUtils::underline("Channel") << ": "
		<< TrackerUtils::bold(TrackerUtils::channel_to_string (ch, base ())) << ", "
		<< TrackerUtils::underline("Velocity") << ": "
		<< TrackerUtils::bold(TrackerUtils::num_to_string (vel, base ())) << ", "
		<< TrackerUtils::underline("Delay") << ": "
		<< TrackerUtils::bold(TrackerUtils::num_to_string (delay, base ()));
	return ss.str ();
}

std::string
Grid::off_note_tooltip_msg (NotePtr off_note, const MidiNotesPattern& mnp, int row_idx, int mti, int mri)
{
	std::stringstream ss;
	Temporal::BBT_Time bbt = mnp.off_note_bbt (off_note);
	int ch = off_note->channel ();
	int delay = get_off_note_delay (off_note, row_idx, mti, mri);
	ss << TrackerUtils::underline("BBT") << ": "
		<< TrackerUtils::bold(TrackerUtils::bbt_to_string (bbt, base ())) << ", "
		<< TrackerUtils::underline("Note") << ": "
		<< TrackerUtils::bold("Off") << ", "
		<< TrackerUtils::underline("Channel") << ": "
		<< TrackerUtils::bold(TrackerUtils::channel_to_string (ch, base ())) << ", "
		<< TrackerUtils::underline("Delay") << ": "
		<< TrackerUtils::bold(TrackerUtils::num_to_string (delay, base ()));
	return ss.str ();
}

std::string
Grid::note_tooltip_msg (int row_idx, int mti, int mri, int cgi)
{
	size_t off_count = pattern.off_notes_count (row_idx, mti, mri, cgi);
	size_t on_count = pattern.on_notes_count (row_idx, mti, mri, cgi);
	if (0 < off_count || 0 < on_count) {
		std::stringstream ss;
		ss << TrackerUtils::underline("Track") << ": "
		   << "<b>" << pattern.tps[mti]->track->name () << "</b>" << std::endl;
		ss << TrackerUtils::underline("Region") << ": "
		   << "<b>" << pattern.midi_region (mti, mri)->name () << "</b>" << std::endl;
		ss << TrackerUtils::underline("Notes") << ":";
		const MidiNotesPattern& mnp = pattern.midi_notes_pattern (mti, mri);
		if (0 < off_count) {
			RowToNotesRange off_rng = pattern.off_notes_range (row_idx, mti, mri, cgi);
			for (; off_rng.first != off_rng.second; ++off_rng.first) {
				NotePtr off_note = off_rng.first->second;
				ss << std::endl << "  " << off_note_tooltip_msg (off_note, mnp, row_idx, mti, mri);
			}
		}
		if (0 < on_count) {
			RowToNotesRange on_rng = pattern.on_notes_range (row_idx, mti, mri, cgi);
			for (; on_rng.first != on_rng.second; ++on_rng.first) {
				NotePtr on_note = on_rng.first->second;
				ss << std::endl << "  " << on_note_tooltip_msg (on_note, mnp, row_idx, mti, mri);
			}
		}
		return ss.str ();
	}
	return std::string ();
}

std::string
Grid::automation_tooltip_msg (int row_idx, int mti, int mri, int cgi)
{
	IDParameter id_param = get_id_param (mti, cgi);
	std::stringstream ss;
	size_t count = pattern.control_events_count (row_idx, mti, mri, id_param);
	if (0 < count) {
		ss << TrackerUtils::underline("Track") << ": "
		   << TrackerUtils::bold(pattern.tps[mti]->track->name ()) << std::endl;
		if (0 <= mri) {
			ss << TrackerUtils::underline("Region") << ": "
			   << TrackerUtils::bold(pattern.midi_region (mti, mri)->name ()) << std::endl;
		}
		ss << TrackerUtils::underline("Parameter") << ": "
		   << TrackerUtils::bold(get_name (mti, id_param, false)) << std::endl;
		ss << TrackerUtils::underline("Values") << ":";

		std::vector<Temporal::BBT_Time> bbt_seq = pattern.get_automation_bbt_seq (row_idx, mti, mri, id_param);
		std::vector<double> value_seq = pattern.get_automation_value_seq (row_idx, mti, mri, id_param);
		std::vector<int>  delay_seq = pattern.get_automation_delay_seq (row_idx, mti, mri, id_param);
		size_t seq_size = bbt_seq.size ();

		// Make sure all sequences have the same size
		assert (seq_size == value_seq.size());
		assert (seq_size == delay_seq.size());

		for (size_t i = 0; i < seq_size; i++) {
			Temporal::BBT_Time bbt = bbt_seq[i];
			double value = value_seq[i];
			int delay = delay_seq[i];
			const int precision = 6;
			ss << std::endl << "  " << TrackerUtils::underline("BBT") << ": "
			   << TrackerUtils::bold(TrackerUtils::bbt_to_string (bbt, base ())) << ", "
			   << TrackerUtils::underline("Value") << ": "
			   << TrackerUtils::bold(TrackerUtils::num_to_string (value, base (), precision)) << ", "
			   << TrackerUtils::underline("Delay") << ": "
			   << TrackerUtils::bold(TrackerUtils::num_to_string (delay, base ()));
		}
		return ss.str ();
	} else {
		return "";
	}
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
	signal_button_press_event ().connect (sigc::mem_fun (*this, &Grid::mouse_button_event), false);
	// Scroll event is disabled for now because it doesn't work properly
	// signal_scroll_event ().connect (sigc::mem_fun (*this, &Grid::scroll_event), false);

	if (tracker_editor.main_toolbar.sync_playhead)
		connect_clock ();
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

	time_column = Gtk::manage (new TreeViewColumn (_("Time"), columns.time));
	Gtk::CellRenderer* time_cellrenderer = time_column->get_first_cell_renderer ();

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
		pan_columns.push_back (std::vector<int> ());
		available_automation_columns.push_back (std::set<int> ());

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
Grid::setup_init_cursor ()
{
	setup_init_row ();
	setup_init_col ();
}

void
Grid::setup_init_row ()
{
	// Set initial row as first row as that is what the user likely expects
	set_current_row (0, true);
}

void
Grid::setup_init_col ()
{
	TreeViewColumn* col = first_defined_col ();
	if (col) {
		set_current_col (col);
	}

	// TODO: If no current col is defined and a column is created,
	// automatically jump to it.
}

void
Grid::setup_left_separator_column (int mti)
{
	left_separator_columns[mti] = Gtk::manage (new TreeViewColumn ("", columns.left_separator[mti]));
	CellRenderer* left_separator_cellrenderer = left_separator_columns[mti]->get_first_cell_renderer ();

	// Link to color attributes
	left_separator_columns[mti]->add_attribute (left_separator_cellrenderer->property_cell_background (), columns._left_right_separator_background_color[mti]);

	// Set width
	left_separator_columns[mti]->set_min_width (LEFT_RIGHT_SEPARATOR_WIDTH);
	left_separator_columns[mti]->set_max_width (LEFT_RIGHT_SEPARATOR_WIDTH);

	append_column (*left_separator_columns[mti]);
}

void
Grid::setup_region_name_column (int mti)
{
	std::string label ("");
	region_name_columns[mti] = Gtk::manage (new TreeViewColumn (label, columns.region_name[mti]));
	CellRendererText* cellrenderer_region_name = dynamic_cast<CellRendererText*> (region_name_columns[mti]->get_first_cell_renderer ());

	// Link to font attributes
	if (!cellfont.empty()) {
		region_name_columns[mti]->add_attribute (cellrenderer_region_name->property_family (), columns._family);
	}

	append_column (*region_name_columns[mti]);

	region_name_columns[mti]->set_visible (false);
}

void
Grid::setup_note_column (int mti, int cgi)
{
	// TODO: maybe put the information of the mti, cgi, midi_note_type or automation_type in the TreeViewColumn
	note_columns[mti][cgi] = Gtk::manage (new NoteColumn (columns.note_name[mti][cgi], mti, cgi));
	CellRendererText* note_cellrenderer = dynamic_cast<CellRendererText*> (note_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to font attributes
	if (!cellfont.empty()) {
		note_columns[mti][cgi]->add_attribute (note_cellrenderer->property_family (), columns._family);
	}

	// Link to color attributes
	note_columns[mti][cgi]->add_attribute (note_cellrenderer->property_cell_background (), columns._note_background_color[mti][cgi]);
	note_columns[mti][cgi]->add_attribute (note_cellrenderer->property_foreground (), columns._note_foreground_color[mti][cgi]);

	// Link to editing methods
	// NEXT: refactor
	note_cellrenderer->signal_editing_started ().connect (sigc::bind (sigc::mem_fun (*this, &Grid::editing_note_started), mti, cgi));
	note_cellrenderer->signal_editing_canceled ().connect (sigc::mem_fun (*this, &Grid::editing_canceled));
	note_cellrenderer->signal_edited ().connect (sigc::mem_fun (*this, &Grid::note_edited));
	note_cellrenderer->property_editable () = true;

	append_column (*note_columns[mti][cgi]);
}

void
Grid::setup_note_channel_column (int mti, int cgi)
{
	channel_columns[mti][cgi] = Gtk::manage (new ChannelColumn (columns.channel[mti][cgi], mti, cgi));
	CellRendererText* channel_cellrenderer = dynamic_cast<CellRendererText*> (channel_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to font attributes
	if (!cellfont.empty()) {
		channel_columns[mti][cgi]->add_attribute (channel_cellrenderer->property_family (), columns._family);
	}

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
Grid::setup_note_velocity_column (int mti, int cgi)
{
	velocity_columns[mti][cgi] = Gtk::manage (new VelocityColumn (columns.velocity[mti][cgi], mti, cgi));
	CellRendererText* velocity_cellrenderer = dynamic_cast<CellRendererText*> (velocity_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to font attributes
	if (!cellfont.empty()) {
		velocity_columns[mti][cgi]->add_attribute (velocity_cellrenderer->property_family (), columns._family);
	}

	// Link to color attribute
	velocity_columns[mti][cgi]->add_attribute (velocity_cellrenderer->property_cell_background (), columns._velocity_background_color[mti][cgi]);
	velocity_columns[mti][cgi]->add_attribute (velocity_cellrenderer->property_foreground (), columns._velocity_foreground_color[mti][cgi]);

	// Link to attributes
	velocity_columns[mti][cgi]->add_attribute (velocity_cellrenderer->property_attributes (), columns._velocity_attributes[mti][cgi]);

	// Link to alignment
	// TODO: support aligment
	velocity_columns[mti][cgi]->add_attribute (velocity_cellrenderer->property_alignment (), columns._velocity_alignment[mti][cgi]);
	// velocity_cellrenderer->property_alignment () = Pango::Alignment::ALIGN_RIGHT;

	// Link to editing methods
	velocity_cellrenderer->signal_editing_started ().connect (sigc::bind (sigc::mem_fun (*this, &Grid::editing_note_velocity_started), mti, cgi));
	velocity_cellrenderer->signal_editing_canceled ().connect (sigc::mem_fun (*this, &Grid::editing_canceled));
	velocity_cellrenderer->signal_edited ().connect (sigc::mem_fun (*this, &Grid::note_velocity_edited));
	velocity_cellrenderer->property_editable () = true;

	append_column (*velocity_columns[mti][cgi]);
}

void
Grid::setup_note_delay_column (int mti, int cgi)
{
	delay_columns[mti][cgi] = Gtk::manage (new DelayColumn (columns.delay[mti][cgi], mti, cgi));
	CellRendererText* delay_cellrenderer = dynamic_cast<CellRendererText*> (delay_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to font attributes
	if (!cellfont.empty()) {
		delay_columns[mti][cgi]->add_attribute (delay_cellrenderer->property_family (), columns._family);
	}

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
Grid::setup_note_separator_column (int mti, int cgi)
{
	note_separator_columns[mti][cgi] = Gtk::manage (new TreeViewColumn ("", columns._note_empty[mti][cgi]));

	// Set width
	note_separator_columns[mti][cgi]->set_min_width (GROUP_SEPARATOR_WIDTH);
	note_separator_columns[mti][cgi]->set_max_width (GROUP_SEPARATOR_WIDTH);

	append_column (*note_separator_columns[mti][cgi]);
}

void
Grid::setup_automation_column (int mti, int cgi)
{
	automation_columns[mti][cgi] = Gtk::manage (new AutomationColumn (columns.automation[mti][cgi], mti, cgi));
	CellRendererText* automation_cellrenderer = dynamic_cast<CellRendererText*> (automation_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to font attributes
	if (!cellfont.empty()) {
		automation_columns[mti][cgi]->add_attribute (automation_cellrenderer->property_family (), columns._family);
	}

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

	int column = get_columns ().size ();
	append_column (*automation_columns[mti][cgi]);
	col2auto_cgi[mti].insert (IndexBimap::value_type (column, cgi));
	available_automation_columns[mti].insert (column);
	to_col (column)->set_visible (false);
}

void
Grid::setup_automation_delay_column (int mti, int cgi)
{
	automation_delay_columns[mti][cgi] = Gtk::manage (new AutomationDelayColumn (columns.automation_delay[mti][cgi], mti, cgi));
	CellRendererText* automation_delay_cellrenderer = dynamic_cast<CellRendererText*> (automation_delay_columns[mti][cgi]->get_first_cell_renderer ());

	// Link to font attributes
	if (!cellfont.empty()) {
		automation_delay_columns[mti][cgi]->add_attribute (automation_delay_cellrenderer->property_family (), columns._family);
	}

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

	int column = get_columns ().size ();
	append_column (*automation_delay_columns[mti][cgi]);
	to_col (column)->set_visible (false);
}

void
Grid::setup_automation_separator_column (int mti, int cgi)
{
	automation_separator_columns[mti][cgi] = Gtk::manage (new TreeViewColumn ("", columns._automation_empty[mti][cgi]));

	// Set width
	automation_separator_columns[mti][cgi]->set_min_width (GROUP_SEPARATOR_WIDTH);
	automation_separator_columns[mti][cgi]->set_max_width (GROUP_SEPARATOR_WIDTH);

	append_column (*automation_separator_columns[mti][cgi]);
}

void
Grid::setup_right_separator_column (int mti)
{
	right_separator_columns[mti] = Gtk::manage (new TreeViewColumn ("", columns.right_separator[mti]));
	CellRenderer* right_separator_cellrenderer = right_separator_columns[mti]->get_first_cell_renderer ();

	// Link to color attributes
	right_separator_columns[mti]->add_attribute (right_separator_cellrenderer->property_cell_background (), columns._left_right_separator_background_color[mti]);

	// Set width
	right_separator_columns[mti]->set_min_width (LEFT_RIGHT_SEPARATOR_WIDTH);
	right_separator_columns[mti]->set_max_width (LEFT_RIGHT_SEPARATOR_WIDTH);

	append_column (*right_separator_columns[mti]);
}

void
Grid::setup_track_separator_column (int mti)
{
	track_separator_columns[mti] = Gtk::manage (new TreeViewColumn ("", columns.track_separator[mti]));

	// Set width
	track_separator_columns[mti]->set_min_width (TRACK_SEPARATOR_WIDTH);
	track_separator_columns[mti]->set_max_width (TRACK_SEPARATOR_WIDTH);

	append_column (*track_separator_columns[mti]);
}

void
Grid::vertical_move (TreeModel::Path& path, const TreeViewColumn* col, int steps, bool wrap, bool jump)
{
	TreeModel::Path init_path = path;
	TreeModel::Path last_def_path = path;
	int init_steps = steps;

	// Move up
	while ((jump && init_steps < 0) || steps < 0) {
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
		if (!col || is_cell_defined (path, col)) {
			last_def_path = path;
			if (jump && !is_cell_blank (path, col)) {
				steps = 0;
				break;
			}
			steps++;
		}

		// Avoid infinite loops
		if (path == init_path) {
			break;
		}
	}

	// Move down
	while ((jump && 0 < init_steps) || 0 < steps) {
		++path[0];

		// Wrap
		if (pattern.global_nrows <= path[0]) {
			if (wrap) {
				path[0] -= pattern.global_nrows;
			} else {
				break;
			}
		}

		// Move
		if (!col || is_cell_defined (path, col)) {
			last_def_path = path;
			if (jump && !is_cell_blank (path, col)) {
				steps = 0;
				break;
			}
			steps--;
		}

		// Avoid infinite loops
		if (path == init_path) {
			break;
		}
	}

	// Set final path
	path = last_def_path;

	// If jump has failed rerun once more without jump
	if (jump && (path == init_path || is_cell_blank (path, col))) {
		path = init_path;
		return vertical_move (path, col, init_steps, wrap, false);
	}
}

// TODO: support jump, maybe
void
Grid::horizontal_move (int& colnum, const Gtk::TreeModel::Path& path, int steps, bool group, bool track, bool jump)
{
	// Keep track of the init column type to support tab and detect infinite loops
	TreeViewColumn* init_col = to_col (colnum);
	int pre_mti = to_mti (init_col);
	int pre_cgi = to_cgi (init_col);
	TrackerColumn::NoteType pre_note_type = get_note_type (init_col);
	TrackerColumn::AutomationType pre_automation_type = get_automation_type (init_col);

	const int n_col = get_columns ().size ();
	TreeViewColumn* col;

	// Move to left
	while (steps < 0) {
		--colnum;
		if (colnum < 1) {
			colnum = n_col - 1;
		}
		col = to_col (colnum);
		if (col->get_visible () && is_editable (col) && is_cell_defined (path, col)) {
			if (group) {
				int col_cgi = to_cgi (col);
				int col_mti = to_mti (col);
				TrackerColumn::NoteType col_note_type = get_note_type (col);
				TrackerColumn::AutomationType col_automation_type = get_automation_type (col);
				if (pre_cgi != col_cgi && pre_note_type == col_note_type && pre_automation_type == col_automation_type && pre_mti == col_mti) {
					pre_cgi = col_cgi;
					++steps;
				}
			} else if (track) {
				int col_mti = to_mti (col);
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
		col = to_col (colnum);
		if (col->get_visible () && is_editable (col) && is_cell_defined (path, col)) {
			if (group) {
				int col_cgi = to_cgi (col);
				int col_mti = to_mti (col);
				TrackerColumn::NoteType col_note_type = get_note_type (col);
				TrackerColumn::AutomationType col_automation_type = get_automation_type (col);
				if (pre_cgi != col_cgi && pre_note_type == col_note_type && pre_automation_type == col_automation_type && pre_mti == col_mti) {
					pre_cgi = col_cgi;
					--steps;
				}
			} else if (track) {
				int col_mti = to_mti (col);
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
Grid::step_edit() const
{
	return tracker_editor.main_toolbar.step_edit;
}

bool
Grid::chord_mode() const
{
	return tracker_editor.main_toolbar.chord_mode;
}

bool
Grid::wrap() const
{
	return tracker_editor.main_toolbar.wrap;
}

bool
Grid::jump() const
{
	return tracker_editor.main_toolbar.jump;
}

bool
Grid::is_editable (int col_idx) const
{
	return is_editable (const_cast<TreeViewColumn*>(to_col (col_idx)));
}

bool
Grid::is_editable (TreeViewColumn* col) const
{
	if (col == 0)
		return false;

	CellRendererText* cellrenderer = dynamic_cast<CellRendererText*> (col->get_first_cell_renderer ());
	return cellrenderer->property_editable ();
}

bool
Grid::is_visible (int col_idx) const
{
	return is_visible (const_cast<TreeViewColumn*>(to_col (col_idx)));
}

bool
Grid::is_visible (TreeViewColumn* col) const
{
	if (col == 0)
		return false;
	return col->get_visible ();
}

bool
Grid::is_cell_defined (int row_idx, int col_idx)
{
	return is_cell_defined (to_path (row_idx), to_col (col_idx));
}

bool
Grid::is_cell_defined (int row_idx, const TreeViewColumn* col)
{
	return is_cell_defined (to_path (row_idx), col);
}

bool
Grid::is_cell_defined (const Gtk::TreeModel::Path& path, const TreeViewColumn* col)
{
	int row_idx = to_row_index (path);
	if (row_idx < 0) {
		return false;
	}
	int mti = to_mti (col);
	if (mti < 0) {
		return false;
	}

	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	if (is_note_type (col)) {
		return tc && is_region_defined (row_idx, mti);
	} else {
		int coli = to_col_index (col);
		if (tc && tc->automation_type == TrackerColumn::AutomationType::AUTOMATION_DELAY) {
			coli--;
		}
		IndexParamBimap::left_const_iterator it = col2params[mti].left.find (coli);
		return pattern.is_automation_defined (row_idx, mti, it->second);
	}
}

bool
Grid::is_region_defined (const Gtk::TreeModel::Path& path, int mti) const
{
	return is_region_defined (to_row_index (path), mti);
}

bool
Grid::is_region_defined (int row_idx, int mti) const
{
	return pattern.is_region_defined (row_idx, mti);
}

bool
Grid::is_automation_defined (int row_idx, int mti, int cgi) const
{
	IDParameter id_param = get_id_param (mti, cgi);
	if (id_param == TrackerUtils::defaultIDParameter ()) {
		return false;
	}

	return pattern.is_automation_defined (row_idx, mti, id_param);
}

bool
Grid::is_cell_blank (int row_idx, const Gtk::TreeViewColumn* col)
{
	return is_cell_blank (to_path (row_idx), col);
}

bool
Grid::is_cell_blank (const Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col)
{
	if (!is_cell_defined (path, col))
		return false;

	int mti = to_mti (col);
	int cgi = to_cgi (col);
	if (0 <= mti && 0 <= cgi) {
		if (is_note_type (col)) {
			return !has_note (path, mti, cgi);
		} else {
			int mri = to_mri (path, col);
			return !has_automation_value (to_row_index (path), mti, mri, cgi);
		}
	}
	return false;
}

int
Grid::to_mti (const TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc ? tc->mti : -1;
}

int
Grid::to_cgi (const TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	if (!tc)
		return -1;
	return tc->cgi;
}

int
Grid::to_mri (const Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col) const
{
	int mti = to_mti (col);
	if (0 <= mti)
		return pattern.to_mri (to_row_index (path), mti);
	return -1;
}

TrackerColumn::NoteType
Grid::get_note_type (const Gtk::TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc->note_type;
}

TrackerColumn::AutomationType
Grid::get_automation_type (const Gtk::TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc->automation_type;
}

bool
Grid::is_note_type (const Gtk::TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc && tc->is_note_type ();
}

bool
Grid::is_automation_type (const Gtk::TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*> (col);
	return tc && tc->is_automation_type ();
}

void
Grid::select_current_track ()
{
	TimeAxisView* tav = tracker_editor.public_editor.time_axis_view_from_stripable (current_tp->track);
	tracker_editor.public_editor.get_selection ().set (tav);
}

void
Grid::set_step_editing_current_track ()
{
	if (step_edit() && current_tp && current_tp->is_midi_track_pattern ()) {
		current_tp->midi_track ()->set_step_editing (true, false);
	}
}

void
Grid::unset_step_editing_current_track ()
{
	if (current_tp && current_tp->is_midi_track_pattern ()) {
		current_tp->midi_track ()->set_step_editing (false, false);
	}
}

void
Grid::unset_step_editing_previous_track ()
{
	if (previous_tp && previous_tp->is_midi_track_pattern ()) {
		previous_tp->midi_track ()->set_step_editing (false, false);
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
	case GDK_a:
	case GDK_A:
		return is_hex () ? 10 : -1;
	case GDK_b:
	case GDK_B:
		return is_hex () ? 11 : -1;
	case GDK_c:
	case GDK_C:
		return is_hex () ? 12 : -1;
	case GDK_d:
	case GDK_D:
		return is_hex () ? 13 : -1;
	case GDK_e:
	case GDK_E:
		return is_hex () ? 14 : -1;
	case GDK_f:
	case GDK_F:
		return is_hex () ? 15 : -1;
	default:
		return -1;
	}
}

uint8_t
Grid::pitch_key (GdkEventKey* ev)
{
	char const* key = PianoKeyBindings::get_keycode (ev);
	int relative_pitch = _keyboard_layout.key_binding (key);
	if (relative_pitch < 0 || 127 < relative_pitch)
		return -1;
	int octave = tracker_editor.main_toolbar.octave_spinner.get_value_as_int ();
	uint8_t absolute_pitch = TrackerUtils::pitch ((uint8_t)relative_pitch, octave);
	return 127 < absolute_pitch ? -1 : absolute_pitch;
}

bool
Grid::step_editing_check_midi_event ()
{
	// Make sure some current cursor is defined
	if (!is_current_cursor_defined ())
		return true;

	ARDOUR::MidiRingBuffer<samplepos_t>& incoming (current_tp->midi_track ()->step_edit_ring_buffer());
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

		if ((buf[0] & 0xf0) == MIDI_CMD_NOTE_OFF && size == 3) {
			uint8_t pitch = buf[1];
			current_on_notes.erase(pitch);
			if (chord_mode() && current_on_notes.empty()) {
				vertical_move_current_cursor_default_steps (wrap(), jump());
			}
		}
		else if ((buf[0] & 0xf0) == MIDI_CMD_NOTE_ON && size == 3) {
			uint8_t pitch = buf[1];
			uint8_t ch = buf[0] & 0xf;
			uint8_t vel = buf[2];
			current_on_notes.insert(pitch);
			switch (current_note_type) {
			case TrackerColumn::NoteType::NOTE:
				if (tracker_editor.main_toolbar.overwrite_default) {
					step_editing_set_on_note (pitch, ch, vel, false);
				} else {
					step_editing_set_on_note (pitch, false);
				}
				break;
			case TrackerColumn::NoteType::CHANNEL:
				if (tracker_editor.main_toolbar.overwrite_default
				    && tracker_editor.main_toolbar.overwrite_existing) {
					step_editing_set_note_channel (ch);
				}
				break;
			case TrackerColumn::NoteType::VELOCITY:
				if (tracker_editor.main_toolbar.overwrite_default
				    && tracker_editor.main_toolbar.overwrite_existing) {
					step_editing_set_note_velocity (vel);
				}
				break;
			case TrackerColumn::NoteType::DELAY:
				break;
			default:
				std::cerr << "Grid::step_editing_check_midi_event: Implementation Error!" << std::endl;
			}
		}
	}
	delete [] buf;

	return true; // do it again, till we stop
}

bool
Grid::step_editing_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (current_automation_type) {
	case TrackerColumn::AutomationType::AUTOMATION_SEPARATOR:
		switch (current_note_type) {
		case TrackerColumn::NoteType::NOTE:
			ret = step_editing_note_key_press (ev);
			break;
		case TrackerColumn::NoteType::CHANNEL:
			ret = step_editing_note_channel_key_press (ev);
			break;
		case TrackerColumn::NoteType::VELOCITY:
			ret = step_editing_note_velocity_key_press (ev);
			break;
		case TrackerColumn::NoteType::DELAY:
			ret = step_editing_note_delay_key_press (ev);
			break;
		default:
			std::cerr << "Grid::key_press: Implementation Error!" << std::endl;
		}
		break;
	case TrackerColumn::AutomationType::AUTOMATION:
		ret = step_editing_automation_key_press (ev);
		break;
	case TrackerColumn::AutomationType::AUTOMATION_DELAY:
		ret = step_editing_automation_delay_key_press (ev);
		break;
	default:
		std::cerr << "Grid::key_press: Implementation Error!" << std::endl;
	}

	return ret;
}

bool
Grid::step_editing_note_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Off note
	case GDK_equal:
	case GDK_plus:
	case GDK_Caps_Lock:
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

	// Set shift modifier (TODO: very hacky way)
	case GDK_Shift_L:
	case GDK_Shift_R:
		ret = shift_key_press ();
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *to_col (current_col_idx), true);
		ret = true;
		break;

	default: {
		// On notes
		uint8_t pitch = pitch_key (ev);
		if (is_current_cursor_defined () && pitch < 128) {
			if (!chord_mode() || last_keyval != ev->keyval) { // Avoid key repeat in chord mode
				current_on_notes.insert(pitch);
				ret = step_editing_set_on_note (pitch);
			} else {
				ret = true;
			}
		}
		break;
	}
	}

	return ret;
}

bool
Grid::step_editing_set_on_note (uint8_t pitch, bool play)
{
	std::pair<uint8_t, uint8_t> ch_vel = set_on_note (current_row_idx, current_mti, current_mri, current_cgi, pitch);

	// In chord mode, move the cursor to the right to form a chord and differ
	// vertical move once the notes have been released.
	if (chord_mode()) {
		horizontal_move_current_cursor (1, true);
	} else {
		vertical_move_current_cursor_default_steps (wrap(), jump());
	}

	if (play) {
		play_note (current_mti, pitch, ch_vel.first, ch_vel.second);
	}
	return true;
}

bool
Grid::step_editing_set_on_note (uint8_t pitch, uint8_t ch, uint8_t vel, bool play)
{
	std::pair<uint8_t, uint8_t> ch_vel = set_on_note (current_row_idx, current_mti, current_mri, current_cgi, pitch, ch, vel);

	// In chord mode, move the cursor to the right to form a chord and differ
	// vertical move once the notes have been released.
	if (chord_mode()) {
		horizontal_move_current_cursor (1, true);
	} else {
		vertical_move_current_cursor_default_steps (wrap(), jump());
	}

	if (play)
		play_note (current_mti, pitch, ch_vel.first, ch_vel.second);
	return true;
}

bool
Grid::step_editing_set_off_note ()
{
	set_off_note (current_row_idx, current_mti, current_mri, current_cgi);
	vertical_move_current_cursor_default_steps (wrap(), jump());
	return true;
}

bool
Grid::step_editing_delete_note ()
{
	delete_note (current_row_idx, current_mti, current_mri, current_cgi);
	vertical_move_current_cursor_default_steps (wrap(), jump());
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
	case GDK_a:
	case GDK_A:
	case GDK_b:
	case GDK_B:
	case GDK_c:
	case GDK_C:
	case GDK_d:
	case GDK_D:
	case GDK_e:
	case GDK_E:
	case GDK_f:
	case GDK_F:
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

	// Set shift modifier (TODO: very hacky way)
	case GDK_Shift_L:
	case GDK_Shift_R:
		ret = shift_key_press ();
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *to_col (current_col_idx), true);
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
	NotePtr note = get_on_note (current_row_idx, current_mti, current_cgi);
	if (note) {
		int ch = note->channel ();
		int new_ch = TrackerUtils::change_digit (ch + (is_hex () ? 0 : 1), digit, current_pos, base (), precision ());
		set_note_channel (current_mti, current_mri, note, new_ch - (is_hex () ? 0 : 1));
	}
	vertical_move_current_cursor_default_steps (wrap(), jump());
	return true;
}

bool
Grid::step_editing_set_note_channel (uint8_t ch)
{
	NotePtr note = get_on_note (current_row_idx, current_mti, current_cgi);
	if (note) {
		set_note_channel (current_mti, current_mri, note, ch);
	}
	vertical_move_current_cursor_default_steps (wrap(), jump());
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
	case GDK_a:
	case GDK_A:
	case GDK_b:
	case GDK_B:
	case GDK_c:
	case GDK_C:
	case GDK_d:
	case GDK_D:
	case GDK_e:
	case GDK_E:
	case GDK_f:
	case GDK_F:
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

	// Set shift modifier (TODO: very hacky way)
	case GDK_Shift_L:
	case GDK_Shift_R:
		ret = shift_key_press ();
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *to_col (current_col_idx), true);
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
	NotePtr note = get_on_note (current_row_idx, current_mti, current_cgi);
	if (note) {
		int vel = note->velocity ();
		int new_vel = TrackerUtils::change_digit (vel, digit, current_pos, base (), precision ());
		set_note_velocity (current_mti, current_mri, note, new_vel);
	}
	vertical_move_current_cursor_default_steps (wrap(), jump());
	return true;
}

bool
Grid::step_editing_set_note_velocity (uint8_t vel)
{
	NotePtr note = get_on_note (current_row_idx, current_mti, current_cgi);
	if (note) {
		set_note_velocity (current_mti, current_mri, note, vel);
	}
	vertical_move_current_cursor_default_steps (wrap(), jump());
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
	case GDK_a:
	case GDK_A:
	case GDK_b:
	case GDK_B:
	case GDK_c:
	case GDK_C:
	case GDK_d:
	case GDK_D:
	case GDK_e:
	case GDK_E:
	case GDK_f:
	case GDK_F:
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

	// Set shift modifier (TODO: very hacky way)
	case GDK_Shift_L:
	case GDK_Shift_R:
		ret = shift_key_press ();
		break;

	// Delete note delay
	case GDK_BackSpace:
	case GDK_Delete:
		ret = step_editing_delete_note_delay ();
		break;

	// Cell edit
	case GDK_Return:
		// TODO: when leaving text editing underline should be reset
		set_cursor (current_path, *to_col (current_col_idx), true);
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
	NotePtr on_note = get_on_note (current_row_idx, current_mti, current_cgi);
	NotePtr off_note = get_off_note (current_row_idx, current_mti, current_cgi);
	if (on_note || off_note) {
		// Fetch delay
		int old_delay = on_note ? get_on_note_delay (on_note, current_row_idx, current_mti, current_mri)
			: get_off_note_delay (off_note, current_row_idx, current_mti, current_mri);

		// Update delay
		int new_delay = TrackerUtils::change_digit_or_sign (old_delay, digit, current_pos, base (), precision ());
		set_note_delay (current_row_idx, current_mti, current_mri, current_cgi, new_delay);
	}

	// Move the cursor
	vertical_move_current_cursor_default_steps (wrap(), jump());
	return true;
}

bool
Grid::step_editing_delete_note_delay ()
{
	NotePtr note = get_note (current_row_idx, current_mti, current_cgi);
	if (note) {
		set_note_delay (current_row_idx, current_mti, current_mri, current_cgi, 0);
	}
	vertical_move_current_cursor_default_steps (wrap(), jump());
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
	case GDK_a:
	case GDK_A:
	case GDK_b:
	case GDK_B:
	case GDK_c:
	case GDK_C:
	case GDK_d:
	case GDK_D:
	case GDK_e:
	case GDK_E:
	case GDK_f:
	case GDK_F:
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

	// Set shift modifier (TODO: very hacky way)
	case GDK_Shift_L:
	case GDK_Shift_R:
		ret = shift_key_press ();
		break;

	// Delete automation
	case GDK_BackSpace:
	case GDK_Delete:
		ret = step_editing_delete_automation ();
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *to_col (current_col_idx), true);
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
	std::pair<double, bool> val_def = get_automation_value (current_row_idx, current_mti, current_mri, current_cgi);
	double oval = val_def.second ? val_def.first : get_automation_interpolation_value (current_row_idx, current_mti, current_mri, current_cgi);

	// Set new value
	// Round in case it is int automation
	if (is_int_param (get_id_param (current_mti, current_cgi))) {
		oval = std::round (oval);
	}

	double nval = TrackerUtils::change_digit_or_sign (oval, digit, current_pos, base (), precision ());

	// TODO: replace by lock, and have redisplay_grid_connect_call immediately
	// return when such lock is taken.
	redisplay_grid_connect_call_enabled = false;
	set_automation_value (current_row_idx, current_mti, current_mri, current_cgi, nval);

	// Move cursor
	vertical_move_current_cursor_default_steps (wrap(), jump());

	// Redisplay model with the new value
	// TODO: optimize
	redisplay_grid_direct_call (); // NEXT: this shouldn't be necessary
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
	delete_automation_value (current_row_idx, current_mti, current_mri, current_cgi);

	// Move cursor
	vertical_move_current_cursor_default_steps (wrap(), jump());

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
	case GDK_a:
	case GDK_A:
	case GDK_b:
	case GDK_B:
	case GDK_c:
	case GDK_C:
	case GDK_d:
	case GDK_D:
	case GDK_e:
	case GDK_E:
	case GDK_f:
	case GDK_F:
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

	// Set shift modifier (TODO: very hacky way)
	case GDK_Shift_L:
	case GDK_Shift_R:
		ret = shift_key_press ();
		break;

	// Delete automation delay
	case GDK_BackSpace:
	case GDK_Delete:
		ret = step_editing_delete_automation_delay ();
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *to_col (current_col_idx), true);
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
	std::pair<int, bool> val_def = get_automation_delay (current_row_idx, current_mti, current_mri, current_cgi);
	int old_delay = val_def.first;

	// For some unknown reason, changing the delay does not trigger a
	// redisplay_grid_connect_call. For that reason we directly call it. But
	// before that we have to disable connect calls.
	redisplay_grid_connect_call_enabled = false;

	// Set new value
	if (val_def.second) {
		int new_delay = TrackerUtils::change_digit_or_sign (old_delay, digit, current_pos, base (), precision ());
		set_automation_delay (current_row_idx, current_mti, current_mri, current_cgi, new_delay);
	}

	// Move cursor
	vertical_move_current_cursor_default_steps (wrap(), jump());

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
	delete_automation_delay (current_row_idx, current_mti, current_mri, current_cgi);

	// Move cursor
	vertical_move_current_cursor_default_steps (wrap(), jump());

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
	TreeViewColumn* col = to_col (current_col_idx);
	vertical_move (path, col, steps, wrap, jump);
	set_current_cursor (path, col, set_playhead);
}

void
Grid::vertical_move_current_cursor_default_steps (bool wrap, bool jump, bool set_playhead)
{
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int ();
	vertical_move_current_cursor (steps, wrap, jump, set_playhead);
}

void
Grid::vertical_move_current_row (int steps, bool wrap, bool jump, bool set_playhead)
{
	TreeModel::Path path = current_path;
	vertical_move (path, 0, steps, wrap, jump);
	set_current_row (path, set_playhead);
}

void
Grid::horizontal_move_current_cursor (int steps, bool group, bool track)
{
	int colnum = current_col_idx;
	TreeModel::Path path = current_path;
	horizontal_move (colnum, current_path, steps, group, track);
	TreeViewColumn* col = to_col (colnum);
	set_current_cursor (path, col);
}

bool
Grid::move_current_cursor_key_press (GdkEventKey* ev)
{
	bool ret = false;

	if (is_current_cursor_defined ()) {
		switch (ev->keyval) {
		case GDK_Up:
		case GDK_uparrow:
			vertical_move_current_cursor (-1, wrap(), jump());
			ret = true;
			break;
		case GDK_Down:
		case GDK_downarrow:
			vertical_move_current_cursor (1, wrap(), jump());
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
			bool group = false;
			bool track = true;
			horizontal_move_current_cursor (is_shift ? -1 : 1, group, track);
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
			vertical_move_current_cursor (-pattern.global_nrows, false);
			ret = true;
			break;
		case GDK_End:
			vertical_move_current_cursor (pattern.global_nrows, false);
			ret = true;
			break;
		}
	} else {                     // No current cursor
		switch (ev->keyval) {
		case GDK_Up:
		case GDK_uparrow:
			vertical_move_current_row (-1, wrap());
			ret = true;
			break;
		case GDK_Down:
		case GDK_downarrow:
			vertical_move_current_row (1, wrap());
			ret = true;
			break;
		case GDK_Left:
		case GDK_leftarrow:
		case GDK_Right:
		case GDK_rightarrow:
		case GDK_Tab:
			ret = true;
			break;
		case GDK_Page_Up:
			vertical_move_current_row (-16, false);
			ret = true;
			break;
		case GDK_Page_Down:
			vertical_move_current_row (16, false);
			ret = true;
			break;
		case GDK_Home:
			vertical_move_current_row (-pattern.global_nrows, false);
			ret = true;
			break;
		case GDK_End:
			vertical_move_current_row (pattern.global_nrows, false);
			ret = true;
			break;
		}
		if (is_cell_defined (current_path, current_col))
			set_current_col (current_col);
	}

	return ret;
}

bool
Grid::non_editing_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

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

	// Set shift modifier (TODO: very hacky way)
	case GDK_Shift_L:
	case GDK_Shift_R:
		ret = shift_key_press ();
		break;

	// Cell edit
	case GDK_Return:
		set_cursor (current_path, *to_col (current_col_idx), true);
		ret = true;
		break;

	case GDK_space:
	// NEXT: pass space to Ardour's Editor. Maybe the problem is that
	// signal_row_activated() is triggered or shit. Another option, perhaps, is
	// to overload Widget::on_key_press_event.
	//
	// See las feedback
	//
	// <las> nilg: YourTreeview.signal_key_press_event().connect (sigc::bind
	//       (gsigc::slot (ARDOUR_UI::instance(), &ARDOUR_UI::key_event_handler),
	//       dynamic_cast<Gtk::Window*> (get_top_level()))  [23:05]
	// <las> nilg: something like. it will forward the event through ardour's entire
	//       key handling mechanism, firing up shortcuts etc. as appropriate  [23:06]
	// <nilg> Wow, thanks, I'm gonna need to some time to parse that line ;-)
	// <las> nilg: you should *not* attempt to handle shortcuts etc. directly from
	//       your code. they must be done by creating actions and a binding set
	// <las> "when a key press event happens, call
	//       ARDOUR_UI::instance()->key_event_handler() with the event and the top
	//       level window we're in  [23:07]
	// <las> nilg: the rest happens automagically

	// More comments from las
	// <las> nilg: Gtk::Widget::get_top_level() returns the Gtk::Widget* that is
	//       uppermost in the parent/child heirachy in which the widget has been
	//       placed  [23:16]
	// <las> nilg: it should always be a Gtk::Window, but GTK itself doesn't require
	//       this (you might not have placed it in a window yet)
	// <las> nilg: do NOT copy how plugins handle keypresses, that it totally wrong
	//                                                                         [23:17]
	// <las> nilg: as mentioned, you need to define a set of actions, then a binding
	//       set
	// <las> nilg: binding sets can be associated with a specific widget and will be
	//       "activated" in preference to similar bindings at the global scope
	// <las> nilg: we do this now for MIDI editing, for example
	// <las> nilg: the arrow keys have global shortcuts/bindings, but there is a
	//       binding set that takes effect when editing a MIDI region  [23:18]
	// <las> nilg: we do not want any more ad-hoc key event handling code, we want
	//       less
	// <las> nilg: see Editor::register_midi_actions()  [23:21]
	// <las> nilg: it's slightly complex because it passes a ptr-to-method to
	//       Editor::midi_action() whose job is to call the method for every selected
	//       MIDI region
	// <las> nilg: and then see the bottom of gtk2_ardour/ardour.keys.in  [23:22]
	//
	// <nilg> OK, thanks las. May I ask why plugins handle keypresses wrong?  [23:25]
	// <nilg> Also, does Virtual MIDI Keyboard handle keypresses right?  [23:26]
	// <las> nilg: plugins are not integrated with our key handling system and we
	//       have to go to special lengths to allow them to work
	// <las> nilg: yes, the vmkbd does handle them directly. it's not unacceptable,
	//       but it's undesirable
	// <nilg> I see...  [23:27]
	// <las> nilg: the overriding design ethic needs to be that everything a user can
	//       do can be done via any mechanism, which means defining an action for it,
	//       and thus allowing keyboard bindings, MIDI, OSC, JSON all to drive that
	//       action
	// <las> nilg: in addition, we want a unified mechanism for users to be able to
	//       change any and all keyboard shortcuts/bindings, not something
	//       per-window/dialog/widget
	// <las> nilg: the current codebase is NOT perfect in this respect but we want to
	//       make it better, not worse
	// <las> nilg: note that the step entry dialog *does* use bindings the "right
	//       way"  [23:29]

	// Other hints:
	// 1. Study Virtual MIDI keyboard (acceptable but undesirable)
	// 2. Study plugins (although las says that it's completely wrong)
		std::cout << "TODO: pass space to Ardour's Editor" << std::endl;
		ret = false;
		break;

	default: {
		// On notes
		uint8_t pitch = pitch_key (ev);
		if (is_current_cursor_defined () && pitch < 128) {
			if (last_keyval != ev->keyval) { // Avoid key repeat
				play_note (current_mti, pitch);
			}
			ret = true;
		}
		break;
	}
	}

	return ret;
}

// Experiments on key bindings (and allows to completely by-pass TreeView
// default key-bindings)
bool
Grid::on_key_press_event (GdkEventKey* event)
{
	return false;
}

bool
Grid::step_editing_key_release (GdkEventKey* ev)
{
	// Make sure there is a current track
	if (!current_tp) {
		return false;
	}

	// Make sure it is a midi track
	if (!current_tp->is_midi_track_pattern ()) {
		return false;
	}

	// Release a possible on note and move cursor if in chord mode
	uint8_t pitch = pitch_key (ev);
	if (pitch < 128) {
		release_note (current_mti, pitch);
		current_on_notes.erase(pitch);
		if (current_note_type == TrackerColumn::NoteType::NOTE && chord_mode() && current_on_notes.empty()) {
			vertical_move_current_cursor_default_steps (wrap(), jump());
		}

		return true;
	}

	return false;
}

bool
Grid::non_editing_key_release (GdkEventKey* ev)
{
	// Make sure there is a current track
	if (!current_tp) {
		return false;
	}

	// Make sure it is a midi track
	if (!current_tp->is_midi_track_pattern ()) {
		return false;
	}

	// Release a possible on note
	uint8_t pitch = pitch_key (ev);
	if (pitch < 128) {
		release_note (current_mti, pitch);
		return true;
	}

	return false;
}

bool
Grid::key_press (GdkEventKey* ev)
{
	bool ret = step_edit() ? step_editing_key_press (ev) : non_editing_key_press (ev);

	// Store last keyval to avoid repeating note during key repeat
	last_keyval = ev->keyval;

	return ret;
}

bool
Grid::key_release (GdkEventKey* ev)
{
	// Hack to handle shift modifer
	switch (ev->keyval) {
	case GDK_Shift_L:
	case GDK_Shift_R: return shift_key_release ();
	}

	bool ret = step_edit() ? step_editing_key_release (ev) : non_editing_key_release (ev);

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
			if (is_cell_defined (path, col)) {
				// Enter editing cell
				set_cursor (path, *col, true);
			}
		} else if (ev->type == GDK_BUTTON_PRESS) {
			if (is_cell_defined (path, col)) {
				set_current_cursor (path, col, true);
			} else {
				if (is_cell_defined (path, current_col)) {
					set_current_cursor (path, current_col, true);
				} else {
					set_current_row (path, true);
				}
			}
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

bool
Grid::shift_key_press ()
{
	shift_pressed = true;
	return true;
}

bool
Grid::shift_key_release ()
{
	shift_pressed = false;
	return true;
}

bool
Grid::is_shift_pressed () const
{
	return shift_pressed;
}
