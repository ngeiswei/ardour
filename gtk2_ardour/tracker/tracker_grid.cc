/*
    Copyright (C) 2015-2018 Nil Geisweiller

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

#include <cctype>
#include <cmath>
#include <map>

#include <boost/algorithm/string.hpp>

#include <gtkmm/cellrenderercombo.h>

#include "pbd/file_utils.h"
#include "pbd/memento_command.h"

#include "evoral/midi_util.h"
#include "evoral/Note.hpp"

#include "ardour/amp.h"
#include "ardour/beats_samples_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/midi_track.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_playlist.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ui_config.h"
#include "note_player.h"
#include "widgets/tooltips.h"
#include "axis_view.h"
#include "editor.h"
#include "midi_region_view.h"

#include "pbd/i18n.h"

#include "tracker_editor.h"
#include "midi_track_toolbar.h"
#include "tracker_utils.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;
using Timecode::BBT_Time;

const std::string TrackerGrid::note_off_str = "===";
const std::string TrackerGrid::undefined_str = "***";

TrackerGrid::TrackerGrid (TrackerEditor& te)
	: tracker_editor (te)
	, pattern (te)
	, current_path (1)
	, current_rowi (0)
	, current_row (NULL)
	, current_col (2)
	, current_mti (0)
	, current_mtp (NULL)
	, current_mri (0)
	, current_cgi (0)
	, current_note_type (TrackerColumn::NOTE)
	, current_auto_type (TrackerColumn::AUTOMATION_SEPARATOR)
	, edit_rowi (-1)
	// , edit_row (NULL)
	, edit_col (-1)
	, edit_mti (-1)
	, edit_mtp (NULL)
	, edit_mri (-1)
	, edit_cgi (-1)
	, editing_editable (NULL)
{
	setup ();
}

TrackerGrid::~TrackerGrid () {}

TrackerGrid::TrackerGridModelColumns::TrackerGridModelColumns()
{
	// The background color differs when the row is on beats and
	// bars. This is to keep track of it.
	add (_background_color);
	add (_family);
	add (time);
	for (size_t mti /* midi track index */ = 0; mti < MAX_NUMBER_OF_MIDI_TRACKS; mti++) {
		add (midi_region_name[mti]);
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			add (note_name[mti][i]);
			add (_note_background_color[mti][i]);
			add (_note_foreground_color[mti][i]);
			add (channel[mti][i]);
			add (_channel_background_color[mti][i]);
			add (_channel_foreground_color[mti][i]);
			add (velocity[mti][i]);
			add (_velocity_background_color[mti][i]);
			add (_velocity_foreground_color[mti][i]);
			add (delay[mti][i]);
			add (_delay_background_color[mti][i]);
			add (_delay_foreground_color[mti][i]);
			add (_on_note[mti][i]);		// We keep that around to play and edit
			add (_off_note[mti][i]);		// We keep that around to play and edit
			add (_empty);
		}
		for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
			add (automation[mti][i]);
			add (_automation_background_color[mti][i]);
			add (_automation_foreground_color[mti][i]);
			add (automation_delay[mti][i]);
			add (_automation_delay_background_color[mti][i]);
			add (_automation_delay_foreground_color[mti][i]);
			add (_empty);
		}
	}
}

size_t
TrackerGrid::select_available_automation_column (size_t mti)
{
	// Find the next available column
	if (available_automation_columns[mti].empty()) {
		std::cout << "Warning: no more available automation column" << std::endl;
		return 0;
	}
	std::set<size_t>::iterator it = available_automation_columns[mti].begin();
	size_t column = *it;
	available_automation_columns[mti].erase(it);

	return column;
}

size_t
TrackerGrid::add_main_automation_column (size_t mti, const Evoral::Parameter& param)
{
	// Select the next available column
	size_t column = select_available_automation_column (mti);
	if (column == 0)
		return column;

	// Associate that column to the parameter
	col2params[mti].insert(ColParamBimap::value_type(column, param)); // TODO: better put this knowledge in an inherited column

	// Set the column title
	string name = TrackerUtils::is_pan_type(param) ?
		pattern.mtps[mti]->midi_track->panner()->describe_parameter (param)
		: pattern.mtps[mti]->midi_track->describe_parameter (param);
	get_column(column)->set_title (name);

	return column;
}

size_t
TrackerGrid::add_midi_automation_column (size_t mti, const Evoral::Parameter& param)
{
	// Insert the corresponding automation control (and connect to the grid if
	// not already there)
	pattern.insert(mti, param);

	// Select the next available column
	size_t column = select_available_automation_column (mti);
	if (column == 0)
		return column;

	// Associate that column to the parameter
	col2params[mti].insert(ColParamBimap::value_type(column, param));

	// Set the column title
	get_column(column)->set_title (pattern.mtps[mti]->midi_track->describe_parameter (param));

	return column;
}

/**
 * Add an AutomationTimeAxisView to display automation for a processor's
 * parameter.
 */
void
TrackerGrid::add_processor_automation_column (size_t mti, boost::shared_ptr<Processor> processor, const Evoral::Parameter& what)
{
	ProcessorAutomationNode* pan;

	if ((pan = tracker_editor.midi_track_toolbars[mti]->find_processor_automation_node (processor, what)) == 0) {
		/* session state may never have been saved with new plugin */
		error << _("programming error: ")
		      << string_compose (X_("processor automation column for %1:%2/%3/%4 not registered with track!"),
		                         processor->name(), what.type(), (int) what.channel(), what.id() )
		      << endmsg;
		abort(); /*NOTREACHED*/
		return;
	}

	if (pan->column) {
		return;
	}

	// Find the next available column
	pan->column = select_available_automation_column (mti);
	if (!pan->column) {
		return;
	}

	// Associate that column to the parameter
	col2params[mti].insert(ColParamBimap::value_type(pan->column, what));

	// Set the column title
	string name = processor->describe_parameter (what);
	get_column(pan->column)->set_title (name);
}

void
TrackerGrid::change_all_channel_tracks_visibility (size_t mti, bool yn, const Evoral::Parameter& param)
{
	const uint16_t selected_channels = pattern.mtps[mti]->midi_track->get_playback_channel_mask();

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (param.type(), chn, param.id());
			CheckMenuItem* menu = tracker_editor.midi_track_toolbars[mti]->automation_child_menu_item (fully_qualified_param);

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
TrackerGrid::update_automation_column_visibility (size_t mti, const Evoral::Parameter& param)
{
	// Find menu item associated to this parameter
	CheckMenuItem* mitem = tracker_editor.midi_track_toolbars[mti]->automation_child_menu_item(param);
	assert (mitem);
	const bool showit = mitem->get_active();

	// Find the column associated to this parameter, assign one if necessary
	ColParamBimap::right_const_iterator it = col2params[mti].right.find(param);
	size_t column = (it == col2params[mti].right.end()) || (it->second == 0) ?
		add_midi_automation_column (mti, param) : it->second;

	// Still no column available, skip
	if (column == 0)
		return;

	if (showit)
		visible_automation_columns.insert (column);
	else
		visible_automation_columns.erase (column);

	/* now trigger a redisplay */
	redisplay_model ();
}

bool
TrackerGrid::is_automation_visible(size_t mti, const Evoral::Parameter& param) const
{
	ColParamBimap::right_const_iterator it = col2params[mti].right.find(param);
	return it != col2params[mti].right.end() &&
		visible_automation_columns.find(it->second) != visible_automation_columns.end();
}

bool
TrackerGrid::has_visible_automation(size_t mti) const
{
	for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
		size_t col = automation_colnum(mti, i);
		bool visible = TrackerUtils::is_in(col, visible_automation_columns);
		if (visible)
			return true;
	}
	return false;
}

bool
TrackerGrid::is_gain_visible(size_t mti) const
{
	return visible_automation_columns.find(gain_columns[mti])
		!= visible_automation_columns.end();
};

bool
TrackerGrid::is_mute_visible(size_t mti) const
{
	return visible_automation_columns.find(mute_columns[mti])
		!= visible_automation_columns.end();
};

bool
TrackerGrid::is_pan_visible(size_t mti) const
{
	bool visible = not pan_columns[mti].empty();
	for (std::vector<size_t>::const_iterator it = pan_columns[mti].begin(); it != pan_columns[mti].end(); ++it) {
		visible = visible_automation_columns.find(*it) != visible_automation_columns.end();
		if (not visible)
			break;
	}
	return visible;
};

void
TrackerGrid::update_gain_column_visibility (size_t mti)
{
	const bool showit = tracker_editor.midi_track_toolbars[mti]->gain_automation_item->get_active();

	if (gain_columns[mti] == 0)
		gain_columns[mti] = add_main_automation_column(mti, Evoral::Parameter(GainAutomation));

	// Still no column available, abort
	if (gain_columns[mti] == 0)
		return;

	if (showit)
		visible_automation_columns.insert (gain_columns[mti]);
	else
		visible_automation_columns.erase (gain_columns[mti]);

	/* now trigger a redisplay */
	redisplay_model ();
}

void
TrackerGrid::update_trim_column_visibility (size_t mti)
{
	// bool const showit = trim_automation_item->get_active();

	// if (showit != string_is_affirmative (trim_track->gui_property ("visible"))) {
	// 	trim_track->set_marked_for_display (showit);

	// 	/* now trigger a redisplay */

	// 	if (!no_redraw) {
	// 		 track_editor.midi_tracks.front()->gui_changed (X_("visible_tracks"), (void *) 0); /* EMIT_SIGNAL */
	// 	}
	// }
}

void
TrackerGrid::update_mute_column_visibility (size_t mti)
{
	const bool showit = tracker_editor.midi_track_toolbars[mti]->mute_automation_item->get_active();

	if (mute_columns[mti] == 0)
		mute_columns[mti] = add_main_automation_column(mti, Evoral::Parameter(MuteAutomation));

	// Still no column available, abort
	if (mute_columns[mti] == 0)
		return;

	if (showit)
		visible_automation_columns.insert (mute_columns[mti]);
	else
		visible_automation_columns.erase (mute_columns[mti]);

	/* now trigger a redisplay */
	redisplay_model ();
}

void
TrackerGrid::update_pan_columns_visibility (size_t mti)
{
	const bool showit = tracker_editor.midi_track_toolbars[mti]->pan_automation_item->get_active();

	if (pan_columns[mti].empty()) {
		set<Evoral::Parameter> const & params = pattern.mtps[mti]->midi_track->panner()->what_can_be_automated ();
		for (set<Evoral::Parameter>::const_iterator p = params.begin(); p != params.end(); ++p) {
			pan_columns[mti].push_back(add_main_automation_column(mti, *p));
		}
	}

	// Still no column available, abort
	if (pan_columns[mti].empty())
		return;

	for (std::vector<size_t>::const_iterator it = pan_columns[mti].begin(); it != pan_columns[mti].end(); ++it) {
		if (showit)
			visible_automation_columns.insert (*it);
		else
			visible_automation_columns.erase (*it);
	}

	/* now trigger a redisplay */
	redisplay_model ();
}

void
TrackerGrid::redisplay_visible_note()
{
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < pattern.mtps[mti]->get_ntracks() and tracker_editor.midi_track_toolbars[mti]->visible_note;
			get_column(note_colnum(mti, i))->set_visible (visible);
		}
		tracker_editor.midi_track_toolbars[mti]->update_visible_note_button ();
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::mti_col_offset(size_t mti) const
{
	return 1 /* time */ + 1 /* first track name */
		+ mti * (MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK * NUMBER_OF_COL_PER_NOTE_TRACK
		         + MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK * NUMBER_OF_COL_PER_AUTOMATION_TRACK
		         + 1 /* track name */);
}

int
TrackerGrid::note_colnum(size_t mti, size_t cgi) const
{
	return mti_col_offset(mti) + cgi * NUMBER_OF_COL_PER_NOTE_TRACK + TrackerColumn::NOTE;
}

void
TrackerGrid::redisplay_visible_channel()
{
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < pattern.mtps[mti]->get_ntracks()
				and tracker_editor.midi_track_toolbars[mti]->visible_note
				and tracker_editor.midi_track_toolbars[mti]->visible_channel;
			get_column(note_channel_colnum(mti, i))->set_visible (visible);
		}
		tracker_editor.midi_track_toolbars[mti]->update_visible_channel_button ();
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::note_channel_colnum(size_t mti, size_t cgi) const
{
	return mti_col_offset(mti) + cgi * NUMBER_OF_COL_PER_NOTE_TRACK + TrackerColumn::CHANNEL;
}

void
TrackerGrid::redisplay_visible_velocity()
{
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < pattern.mtps[mti]->get_ntracks()
				and tracker_editor.midi_track_toolbars[mti]->visible_note
				and tracker_editor.midi_track_toolbars[mti]->visible_velocity;
			get_column(note_velocity_colnum(mti, i))->set_visible (visible);
		}
		tracker_editor.midi_track_toolbars[mti]->update_visible_velocity_button ();
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::note_velocity_colnum(size_t mti, size_t cgi) const
{
	return mti_col_offset(mti) + cgi * NUMBER_OF_COL_PER_NOTE_TRACK + TrackerColumn::VELOCITY;
}

void
TrackerGrid::redisplay_visible_delay()
{
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < pattern.mtps[mti]->get_ntracks()
				and tracker_editor.midi_track_toolbars[mti]->visible_note
				and tracker_editor.midi_track_toolbars[mti]->visible_delay;
			get_column(note_delay_colnum(mti, i))->set_visible (visible);
		}
		tracker_editor.midi_track_toolbars[mti]->update_visible_delay_button ();
	}
	// Do the same for automations
	redisplay_visible_automation_delay ();

	// Keep the window width to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::note_delay_colnum(size_t mti, size_t cgi) const
{
	return mti_col_offset(mti) + cgi * NUMBER_OF_COL_PER_NOTE_TRACK + TrackerColumn::DELAY;
}

void
TrackerGrid::redisplay_visible_note_separator()
{
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		bool is_last_mti = mti == (pattern.mtps.size() - 1);
		bool hva = has_visible_automation(mti);
		size_t ntracks = pattern.mtps[mti]->get_ntracks();
		bool visible_note = tracker_editor.midi_track_toolbars[mti]->visible_note and 0 < ntracks;
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool is_first_gci = (i == 0);
			bool visible;
			if (is_last_mti and !hva and !visible_note) {
				visible = is_first_gci;
			} else {
				visible = visible_note and (i < ntracks - ((hva or is_last_mti) ? 0 : 1));
			}
			get_column(note_separator_colnum(mti, i))->set_visible (visible);
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::note_separator_colnum (size_t mti, size_t cgi) const
{
	return mti_col_offset(mti) + cgi * NUMBER_OF_COL_PER_NOTE_TRACK + TrackerColumn::SEPARATOR;
}

void
TrackerGrid::redisplay_visible_automation()
{
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
			size_t col = automation_colnum(mti, i);
			bool visible = TrackerUtils::is_in(col, visible_automation_columns);
			get_column(col)->set_visible(visible);
		}
	}
	redisplay_visible_automation_delay();

	// Keep the window width to its minimum
	tracker_editor.resize_width();
}

size_t TrackerGrid::automation_col_offset(size_t mti) const
{
	return mti_col_offset(mti)
		+ MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK * NUMBER_OF_COL_PER_NOTE_TRACK;
}

int
TrackerGrid::automation_colnum(size_t mti, size_t cgi) const
{
	return automation_col_offset(mti) + NUMBER_OF_COL_PER_AUTOMATION_TRACK * cgi + TrackerColumn::AUTOMATION;
}

void
TrackerGrid::redisplay_visible_automation_delay()
{
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
			size_t col = automation_delay_colnum(mti, i);
			bool visible = tracker_editor.midi_track_toolbars[mti]->visible_delay and TrackerUtils::is_in(col - 1, visible_automation_columns);
			get_column(col)->set_visible (visible);
		}
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::automation_delay_colnum(size_t mti, size_t cgi) const
{
	return automation_col_offset(mti) + NUMBER_OF_COL_PER_AUTOMATION_TRACK * cgi + TrackerColumn::AUTOMATION_DELAY;
}

void
TrackerGrid::redisplay_visible_automation_separator()
{
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		size_t greatest_visible_col = 0;
		for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
			size_t col = automation_separator_colnum(mti, i);
			bool visible = TrackerUtils::is_in(col - 2, visible_automation_columns);
			if (visible)
				greatest_visible_col = std::max(greatest_visible_col, col);
			get_column(col)->set_visible (visible);
		}
		if (0 < greatest_visible_col and mti != (pattern.mtps.size() - 1))
			get_column(greatest_visible_col)->set_visible (false);
	}

	// Keep the window width to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::automation_separator_colnum (size_t mti, size_t cgi) const
{
	return automation_col_offset(mti) + NUMBER_OF_COL_PER_AUTOMATION_TRACK * cgi + TrackerColumn::AUTOMATION_SEPARATOR;
}

void
TrackerGrid::setup ()
{
	pattern.setup ();

	model = ListStore::create (columns);
	set_model (model);

	setup_time_column();

	// Instantiate midi tracks
	for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
		setup_midi_track_column(mti);

		// Setup column info
		gain_columns.push_back(0);
		trim_columns.push_back(0);
		mute_columns.push_back(0);
		col2params.push_back(ColParamBimap());
		col2autotracks.push_back(ColAutoTrackBimap());
		pan_columns.push_back(std::vector<size_t>());
		available_automation_columns.push_back(std::set<size_t>());

		// Instantiate note tracks
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; cgi++) {
			setup_note_column(mti, cgi);
			setup_note_channel_column(mti, cgi);
			setup_note_velocity_column(mti, cgi);
			setup_note_delay_column(mti, cgi);
			setup_note_separator_column();
		}

		// Instantiate automation tracks
		for (size_t cgi = 0; cgi < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; cgi++) {
			setup_automation_column(mti, cgi);
			setup_automation_delay_column(mti, cgi);
			setup_automation_separator_column();
		}
	}

	// Connect to key press events
	signal_key_press_event().connect (sigc::mem_fun (*this, &TrackerGrid::key_press), false);
	signal_key_release_event().connect (sigc::mem_fun (*this, &TrackerGrid::key_release), false);

	// Connect to mouse button events
	//
	// Disabled for now because it doesn't work as expected
	//
	signal_button_press_event().connect (sigc::mem_fun (*this, &TrackerGrid::button_event), false);
	// signal_scroll_event().connect (sigc::mem_fun (*this, &TrackerGrid::scroll_event), false);

	set_headers_visible (true);
	set_rules_hint (true);
	set_grid_lines (TREE_VIEW_GRID_LINES_BOTH);
	get_selection()->set_mode (SELECTION_NONE);
	set_enable_search(false);

	show ();
}

void
TrackerGrid::read_colors ()
{
	gtk_bases_color = UIConfiguration::instance().color_str ("gtk_bases");
	beat_background_color = UIConfiguration::instance().color_str ("tracker editor: beat background");
	bar_background_color = UIConfiguration::instance().color_str ("tracker editor: bar background");
	background_color = UIConfiguration::instance().color_str ("tracker editor: background");
	blank_foreground_color = UIConfiguration::instance().color_str ("tracker editor: blank foreground");
	active_foreground_color = UIConfiguration::instance().color_str ("tracker editor: active foreground");
	passive_foreground_color = UIConfiguration::instance().color_str ("tracker editor: passive foreground");
	cursor_color = UIConfiguration::instance().color_str ("tracker editor: cursor");
}

void
TrackerGrid::redisplay_global_columns ()
{
	// Set Time column, row background color, font
	TreeModel::Children::iterator row_it = model->children().begin();
	for (uint32_t rowi = 0; rowi < pattern.global_nrows; rowi++) {
		// Get existing row, or create one if it does exist
		if (row_it == model->children().end())
			row_it = model->append();
		TreeModel::Row row = *row_it++;

		Temporal::Beats row_beats = pattern.earliest_mtp->beats_at_row(rowi);
		uint32_t row_sample = pattern.earliest_mtp->sample_at_row(rowi);

		// Time
		Timecode::BBT_Time row_bbt;
		tracker_editor.session->bbt_time(row_sample, row_bbt);
		stringstream ss;
		print_padded(ss, row_bbt);
		row[columns.time] = ss.str();

		// If the row is on a bar, beat or otherwise, the color differs
		bool is_row_beat = row_beats == row_beats.round_up_to_beat();
		bool is_row_bar = row_bbt.beats == 1;
		std::string row_background_color = (is_row_beat ? (is_row_bar ? bar_background_color : beat_background_color) : background_color);
		row[columns._background_color] = row_background_color;

		// Set font family to Monospace
		row[columns._family] = "Monospace";
	}
}

void
TrackerGrid::reset_off_on_note (TreeModel::Row& row, size_t mti, size_t cgi)
{
	row[columns._off_note[mti][cgi]] = NULL;
	row[columns._on_note[mti][cgi]] = NULL;
}

void
TrackerGrid::redisplay_undefined (TreeModel::Row& row, size_t mti)
{
	// Number of column groups
	size_t ntracks = pattern.mtps[mti]->get_ntracks();
	row[columns.midi_region_name[mti]] = "";
	for (size_t cgi = 0; cgi < ntracks; cgi++) {
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

		// Reset keeping track of the on and off notes
		reset_off_on_note (row, mti, cgi);
	}
}

void
TrackerGrid::redisplay_region_name (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri)
{
	// Render midi region name (for now midi track name). Display names
	// vertically.
	const std::string& name = pattern.midi_region(mti, mri)->name();
	uint32_t name_offset_idx = pattern.to_rrri(rowi, mti, mri) % (name.size() + 1);
	const static std::string name_sep(" ");
	std::string cell_str = " ";
	cell_str += name_offset_idx == name.size() ? name_sep : string{name[name_offset_idx]};
	cell_str += " ";
	row[columns.midi_region_name[mti]] = cell_str;
}

void
TrackerGrid::redisplay_notes (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri)
{
	// Render midi notes pattern
	for (size_t cgi = 0; cgi < pattern.mtps[mti]->get_ntracks(); cgi++) { // cgi stands from column group index

		// Fill background colors
		redisplay_note_background (row, mti, cgi);

		// Fill with blank foreground text and colors
		redisplay_blank_note_foreground (row, mti, cgi);

		// Reset keeping track of the on and off notes
		reset_off_on_note (row, mti, cgi);

		// Display note
		size_t off_notes_count = pattern.off_notes_count (rowi, mti, mri, cgi);
		size_t on_notes_count = pattern.on_notes_count (rowi, mti, mri, cgi);
		if (0 < on_notes_count || 0 < off_notes_count)
			redisplay_note (row, rowi, mti, mri, cgi);
	}
}

void
TrackerGrid::redisplay_automations (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri)
{
	// Render automation pattern
	for (ColParamBimap::left_const_iterator cp_it = col2params[mti].left.begin(); cp_it != col2params[mti].left.end(); ++cp_it) {
		size_t col_idx = cp_it->first;
		ColAutoTrackBimap::left_const_iterator ca_it = col2autotracks[mti].left.find(col_idx);
		size_t cgi = ca_it->second;

		const Evoral::Parameter& param = cp_it->second;
		size_t auto_count = pattern.get_automation_list_count(rowi, mti, mri, param);

		if (cgi >= MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK) {
			// TODO: use Ardour log
			std::cout << "Warning: The automation track number " << cgi
			          << " exceeds the maximum number of automation tracks "
			          << MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK << std::endl;
			continue;
		}

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
TrackerGrid::redisplay_note_background (TreeModel::Row& row, size_t mti, size_t cgi)
{
	std::string row_background_color = row[columns._background_color];
	row[columns._note_background_color[mti][cgi]] = row_background_color;
	row[columns._channel_background_color[mti][cgi]] = row_background_color;
	row[columns._velocity_background_color[mti][cgi]] = row_background_color;
	row[columns._delay_background_color[mti][cgi]] = row_background_color;
}

void
TrackerGrid::redisplay_current_note_cursor (TreeModel::Row& row, size_t mti, size_t cgi)
{
	switch (current_note_type) {
	case TrackerColumn::NOTE:
		row[columns._note_background_color[mti][cgi]] = cursor_color;
		break;
	case TrackerColumn::CHANNEL:
		row[columns._channel_background_color[mti][cgi]] = cursor_color;
		break;
	case TrackerColumn::VELOCITY:
		row[columns._velocity_background_color[mti][cgi]] = cursor_color;
		break;
	case TrackerColumn::DELAY:
		row[columns._delay_background_color[mti][cgi]] = cursor_color;
		break;
	default:
		// TODO use Ardour log
		std::cout << "Error";
	}
}

void
TrackerGrid::redisplay_blank_note_foreground (TreeModel::Row& row, size_t mti, size_t cgi)
{
	// Fill with blank
	row[columns.note_name[mti][cgi]] = "----";
	row[columns.channel[mti][cgi]] = "--";
	row[columns.velocity[mti][cgi]] = "---";
	row[columns.delay[mti][cgi]] = "-----";

	// Grey out infoless cells
	row[columns._note_foreground_color[mti][cgi]] = blank_foreground_color;
	row[columns._channel_foreground_color[mti][cgi]] = blank_foreground_color;
	row[columns._velocity_foreground_color[mti][cgi]] = blank_foreground_color;
	row[columns._delay_foreground_color[mti][cgi]] = blank_foreground_color;
}

void
TrackerGrid::redisplay_auto_background (TreeModel::Row& row, size_t mti, size_t cgi)
{
	std::string row_background_color = row[columns._background_color];
	row[columns._automation_background_color[mti][cgi]] = row_background_color;
	row[columns._automation_delay_background_color[mti][cgi]] = row_background_color;
}

void
TrackerGrid::redisplay_note (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi)
{
	if (pattern.is_note_displayable(rowi, mti, mri, cgi)) {
		// Notes off
		NoteTypePtr note = pattern.off_note(rowi, mti, mri, cgi);
		if (note) {
			row[columns.note_name[mti][cgi]] = note_off_str;
			row[columns._note_foreground_color[mti][cgi]] = active_foreground_color;
			int64_t delay = pattern.region_relative_delay_ticks(note->end_time(), rowi, mti, mri);
			if (delay != 0) {
				row[columns.delay[mti][cgi]] = to_string (delay);
				row[columns._delay_foreground_color[mti][cgi]] = active_foreground_color;
			}
			// Keep the note off around for playing and editing
			row[columns._off_note[mti][cgi]] = note;
		}

		// Notes on
		note = pattern.on_note (rowi, mti, mri, cgi);
		if (note) {
			row[columns.channel[mti][cgi]] = to_string (note->channel() + 1);
			row[columns.note_name[mti][cgi]] = ParameterDescriptor::midi_note_name (note->note());
			row[columns.velocity[mti][cgi]] = to_string ((int)note->velocity());
			row[columns._note_foreground_color[mti][cgi]] = active_foreground_color;
			row[columns._channel_foreground_color[mti][cgi]] = active_foreground_color;
			row[columns._velocity_foreground_color[mti][cgi]] = active_foreground_color;

			int64_t delay = pattern.region_relative_delay_ticks(note->time(), rowi, mti, mri);
			if (delay != 0) {
				row[columns.delay[mti][cgi]] = to_string (delay);
				row[columns._delay_foreground_color[mti][cgi]] = active_foreground_color;
			}
			// Keep the note around for playing and editing
			row[columns._on_note[mti][cgi]] = note;
		}
	} else {
		// Too many notes, not displayable
		row[columns.note_name[mti][cgi]] = undefined_str;
		row[columns._note_foreground_color[mti][cgi]] = active_foreground_color;
	}
}

void
TrackerGrid::redisplay_current_auto_cursor (TreeModel::Row& row, size_t mti, size_t cgi)
{
	switch (current_auto_type) {
	case TrackerColumn::AUTOMATION:
		row[columns._automation_background_color[mti][cgi]] = cursor_color;
		break;
	case TrackerColumn::AUTOMATION_DELAY:
		row[columns._automation_delay_background_color[mti][cgi]] = cursor_color;
		break;
	default:
		// TODO use Ardour log
		std::cout << "Error";
	}
}

void
TrackerGrid::redisplay_current_cursor ()
{
	if (current_auto_type == TrackerColumn::AUTOMATION_SEPARATOR)
		redisplay_current_note_cursor (*current_row, current_mti, current_cgi);
	else
		redisplay_current_auto_cursor (*current_row, current_mti, current_cgi);
}

void
TrackerGrid::redisplay_blank_auto_foreground (TreeModel::Row& row, size_t mti, size_t cgi)
{
	// Fill with blank
	row[columns.automation[mti][cgi]] = "---";
	row[columns.automation_delay[mti][cgi]] = "-----";

	// Fill default foreground color
	row[columns._automation_delay_foreground_color[mti][cgi]] = blank_foreground_color;
}

void
TrackerGrid::redisplay_automation (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi, const Evoral::Parameter& param)
{
	if (pattern.is_auto_displayable(rowi, mti, mti, param)) {
		Evoral::ControlEvent* ctl_event = pattern.get_automation_control_event (rowi, mti, mri, param);
		double aval = ctl_event->value;
		row[columns.automation[mti][cgi]] = to_string (aval);
		double awhen = ctl_event->when;
		int64_t delay = TrackerUtils::is_region_automation (param) ?
			pattern.region_relative_delay_ticks(Temporal::Beats(awhen), rowi, mti, mri)
			: pattern.delay_ticks((samplepos_t)awhen, rowi, mti);
		if (delay != 0) {
			row[columns.automation_delay[mti][cgi]] = to_string (delay);
			row[columns._automation_delay_foreground_color[mti][cgi]] = active_foreground_color;
		}
	} else {
		row[columns.automation[mti][cgi]] = undefined_str;
	}
	row[columns._automation_foreground_color[mti][cgi]] = active_foreground_color;
}

void
TrackerGrid::redisplay_auto_interpolation (TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi, const Evoral::Parameter& param)
{
	double inter_auto_val = 0;
	if (boost::shared_ptr<AutomationList> alist = pattern.get_alist (mti, mri, param)) {
		// We need to use ControlList::rt_safe_eval instead of ControlList::eval, otherwise the lock inside eval
		// interferes with the lock inside ControlList::erase. Though if mark_dirty is called outside of the scope
		// of the WriteLock in ControlList::erase and such, then eval can be used.
		// Get corresponding beats and samples
		Temporal::Beats row_relative_beats = pattern.region_relative_beats_at_row(rowi, mti, mri);
		uint32_t row_sample = pattern.sample_at_row(rowi, mti);
		double awhen = TrackerUtils::is_region_automation (param) ? row_relative_beats.to_double() : row_sample;
		// Get interpolation
		bool ok;
		inter_auto_val = alist->rt_safe_eval(awhen, ok);
	}
	row[columns.automation[mti][cgi]] = to_string (inter_auto_val);
	row[columns._automation_foreground_color[mti][cgi]] = passive_foreground_color;
}

void
TrackerGrid::redisplay_model ()
{
	if (editing_editable)
		return;

	if (tracker_editor.session) {

		// In case the resolution (lines per beat) has changed
		// NEXT TODO: support multi track multi region
		tracker_editor.main_toolbar.delay_spinner.get_adjustment()->set_lower(pattern.mtps.front()->delay_ticks_min());
		tracker_editor.main_toolbar.delay_spinner.get_adjustment()->set_upper(pattern.mtps.front()->delay_ticks_max());

		// Load colors from config
		read_colors ();

		// Update pattern settings and content
		pattern.update ();

		// Set time column, row background color and font
		redisplay_global_columns ();

		// Set midi track patterns columns
		TreeModel::Children::iterator row_it;
		// Fill rows
		row_it = model->children().begin();
		for (uint32_t rowi = 0; rowi < pattern.global_nrows; rowi++) {

			// Get row
			TreeModel::Row row = *row_it++;

			// Get corresponding background color
			std::string row_background_color = row[columns._background_color];

			for (size_t mti = 0; mti < pattern.mtps.size(); mti++) {
				// Undefined
				// NEXT TODO: take track automation into account
				if (!pattern.is_defined (rowi, mti)) {
					redisplay_undefined (row, mti);
					continue;
				}

				int mri = pattern.to_mri (rowi, mti);
				assert (-1 < mri);

				redisplay_region_name (row, rowi, mti, mri);
				redisplay_notes (row, rowi, mti, mri);
				redisplay_automations (row, rowi, mti, mri);	
			}

			// Used to draw the background of the cursor
			if ((int)rowi == current_rowi) {
				current_row = &row;
				redisplay_current_cursor ();
			}
		}
		// Remove unused rows
		for (; row_it != model->children().end();)
			row_it = model->erase(row_it);
	}

	set_model (model);

	// In case tracks have been added or removed
	redisplay_visible_note();
	redisplay_visible_channel();
	redisplay_visible_velocity();
	redisplay_visible_delay();
	redisplay_visible_note_separator();
	redisplay_visible_automation();
	redisplay_visible_automation_separator();
}

/////////////////////
// Edit Pattern    //
/////////////////////

uint32_t
TrackerGrid::get_row_index(const std::string& path) const
{
	return get_row_index (TreeModel::Path (path));
}

uint32_t
TrackerGrid::get_row_index(const TreeModel::Path& path) const
{
	return path.front();
}

int
TrackerGrid::get_col_index(TreeViewColumn* col)
{
	std::vector<TreeViewColumn*> cols = (std::vector<TreeViewColumn*>)get_columns();

	for (int i = 0; i < (int)cols.size(); i++)
		if (cols[i] == col)
			return i;

	return -1;
}

NoteTypePtr
TrackerGrid::get_on_note (int rowi, int mti, int cgi)
{
	return get_on_note (TreeModel::Path (1U, rowi), mti, cgi);
}

NoteTypePtr
TrackerGrid::get_on_note (const std::string& path, int mti, int cgi)
{
	return get_on_note (TreeModel::Path (path), mti, cgi);
}

NoteTypePtr
TrackerGrid::get_on_note (const TreeModel::Path& path, int mti, int cgi)
{
	TreeModel::iterator iter = model->get_iter (path);
	if (!iter)
		return NoteTypePtr();
	return (*iter)[columns._on_note[mti][cgi]];
}

NoteTypePtr
TrackerGrid::get_off_note(int rowi, int mti, int cgi)
{
	return get_off_note(TreeModel::Path ((TreeModel::Path::size_type)1, (TreeModel::Path::value_type)rowi), mti, cgi);
}

NoteTypePtr
TrackerGrid::get_off_note(const std::string& path, int mti, int cgi)
{
	return get_off_note (TreeModel::Path (path), mti, cgi);
}

NoteTypePtr
TrackerGrid::get_off_note(const TreeModel::Path& path, int mti, int cgi)
{
	TreeModel::iterator iter = model->get_iter (path);
	if (!iter)
		return NoteTypePtr();
	return (*iter)[columns._off_note[mti][cgi]];
}

void
TrackerGrid::editing_note_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = note_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
TrackerGrid::editing_note_channel_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = note_channel_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
TrackerGrid::editing_note_velocity_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = note_velocity_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
TrackerGrid::editing_note_delay_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = note_delay_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
TrackerGrid::editing_automation_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = automation_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
TrackerGrid::editing_automation_delay_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_col = automation_delay_colnum (mti, cgi);
	editing_started (ed, path, mti, cgi);
}

void
TrackerGrid::editing_started (CellEditable* ed, const string& path, int mti, int cgi)
{
	edit_path = TreePath (path);
	edit_rowi = get_row_index (edit_path);
	edit_mti = mti;
	edit_mtp = pattern.mtps[edit_mti];
	edit_mri = pattern.to_mri(edit_rowi, edit_mti);
	edit_cgi = cgi;
	editing_editable = ed;

	// For now set the current cursor to the edit cursor
	current_mti = mti;
}

void
TrackerGrid::clear_editables ()
{
	edit_path.clear ();
	edit_rowi = -1;
	edit_col = -1;
	edit_mti = -1;
	edit_mtp = NULL;
	edit_mri = -1;
	edit_cgi = -1;
	editing_editable = NULL;

	redisplay_model ();
}

void
TrackerGrid::editing_canceled ()
{
	clear_editables ();
}

uint8_t
TrackerGrid::parse_pitch (const std::string& text) const
{
	return TrackerUtils::parse_pitch(text, tracker_editor.main_toolbar.octave_spinner.get_value_as_int());
}

void
TrackerGrid::note_edited (const std::string& path, const std::string& text)
{
	std::string norm_text = boost::erase_all_copy(text, " ");
	bool is_del = norm_text.empty();
	bool is_off = !is_del and (norm_text[0] == note_off_str[0]);
	uint8_t pitch = parse_pitch (norm_text);
	bool is_on = pitch <= 127;

	// Can't edit ***
	if (!pattern.is_note_displayable(edit_rowi, edit_mti, edit_mri, edit_cgi)) {
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

void
TrackerGrid::set_on_note (uint8_t pitch, int rowi, int mti, int mri, int cgi)
{
	// Abort if the new pitch is invalid
	if (127 < pitch)
		return;

	NoteTypePtr on_note = get_on_note (rowi, mti, cgi);
	NoteTypePtr off_note = get_off_note (rowi, mti, cgi);

	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int();
	uint8_t chan = tracker_editor.main_toolbar.channel_spinner.get_value_as_int() - 1;
	uint8_t vel = tracker_editor.main_toolbar.velocity_spinner.get_value_as_int();

	MidiModel::NoteDiffCommand* cmd = NULL;

	if (on_note) {
		// Change the pitch of the on note
		char const * opname = _("change note");
		cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
		cmd->change (on_note, MidiModel::NoteDiffCommand::NoteNumber, pitch);
	} else if (off_note) {
		// Replace off note by another (non-off) note. Calculate the start
		// time and length of the new on note.
		Temporal::Beats start = off_note->end_time();
		Temporal::Beats end = pattern.next_off(rowi, mti, mri, cgi);
		Temporal::Beats length = end - start;
		// Build note using defaults
		NoteTypePtr new_note(new NoteType(chan, start, length, pitch, vel));
		char const * opname = _("add note");
		cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
		cmd->add (new_note);
		// Pre-emptively add the note in np to so that it knows in
		// which track it is supposed to be.
		pattern.note_pattern(mti, mri).add (cgi, new_note);
	} else {
		// Create a new on note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = pattern.region_relative_beats_at_row(rowi, mti, mri, delay);
		NoteTypePtr prev_note = pattern.find_prev_note(rowi, mti, mri, cgi);
		Temporal::Beats prev_start;
		Temporal::Beats prev_end;
		if (prev_note) {
			prev_start = prev_note->time();
			prev_end = prev_note->end_time();
		}

		char const * opname = _("add note");
		cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
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
		Temporal::Beats end = pattern.next_off(rowi, mti, mri, cgi);
		Temporal::Beats length = end - here;
		NoteTypePtr new_note(new NoteType(chan, here, length, pitch, vel));
		cmd->add (new_note);
		// Pre-emptively add the note in np to so that it knows in
		// which track it is supposed to be.
		pattern.note_pattern(mti, mri).add (cgi, new_note);
	}

	// Apply note changes
	if (cmd)
		apply_command (mti, mri, cmd);
}

void
TrackerGrid::set_off_note (int rowi, int mti, int mri, int cgi)
{
	NoteTypePtr on_note = get_on_note(rowi, mti, cgi);
	NoteTypePtr off_note = get_off_note(rowi, mti, cgi);

	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int();

	MidiModel::NoteDiffCommand* cmd = NULL;

	if (on_note) {
		// Replace the on note by an off note, that is remove the on note
		char const * opname = _("delete note");
		cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is no off note, update the length of the preceding node
		// to match the new off note (smart off note).
		if (!off_note) {
			NoteTypePtr prev_note = pattern.find_prev_note(rowi, mti, mri, cgi);
			if (prev_note) {
				Temporal::Beats length = on_note->time() - prev_note->time();
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else {
		// Create a new off note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = pattern.region_relative_beats_at_row(rowi, mti, mri, delay);
		NoteTypePtr prev_note = pattern.find_prev_note(rowi, mti, mri, cgi);
		Temporal::Beats prev_start;
		Temporal::Beats prev_end;
		if (prev_note) {
			prev_start = prev_note->time();
			prev_end = prev_note->end_time();
		}

		// Update the length the previous note to match the new off
		// note no matter what (smart off note).
		if (prev_note) {
			Temporal::Beats new_length = here - prev_start;
			char const * opname = _("resize note");
			cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
			cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, new_length);
		}
	}

	// Apply note changes
	if (cmd)
		apply_command (mti, mri, cmd);
}

void
TrackerGrid::delete_note (int rowi, int mti, int mri, int cgi)
{
	NoteTypePtr on_note = get_on_note (rowi, mti, cgi);
	NoteTypePtr off_note = get_off_note (rowi, mti, cgi);

	MidiModel::NoteDiffCommand* cmd = NULL;

	if (on_note) {
		// Delete on note and change
		char const * opname = _("delete note");
		cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is an off note, update the length of the preceding note
		// to match the next note or the end of the region.
		if (off_note) {
			NoteTypePtr prev_note = pattern.find_prev_note(rowi, mti, mri, cgi);
			if (prev_note) {
				// Calculate the length of the previous note
				Temporal::Beats start = prev_note->time();
				Temporal::Beats end = pattern.next_off(rowi, mti, mri, cgi);
				Temporal::Beats length = end - start;
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else if (off_note) {
		// Update the length of the corresponding on note so the off note
		// matches the next note or the end of the region.
		Temporal::Beats start = off_note->time();
		Temporal::Beats end = pattern.next_off(rowi, mti, mri, cgi);
		Temporal::Beats length = end - start;
		char const * opname = _("resize note");
		cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
		cmd->change (off_note, MidiModel::NoteDiffCommand::Length, length);
	}

	// Apply note changes
	if (cmd)
		apply_command (mti, mri, cmd);
}

void
TrackerGrid::note_channel_edited (const std::string& path, const std::string& text)
{
	NoteTypePtr note = get_on_note (path, edit_mti, edit_cgi);
	if (text.empty() || !note) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	if (!pattern.is_note_displayable(edit_rowi, edit_mti, edit_mri, edit_cgi)) {
		clear_editables ();
		return;
	}

	int  ch;
	if (sscanf (text.c_str(), "%d", &ch) == 1) {
		ch--;  // Adjust for zero-based counting
		set_note_channel(edit_mti, edit_mri, note, ch);
	}

	clear_editables ();
}

void
TrackerGrid::set_note_channel (int mti, int mri, NoteTypePtr note, int ch)
{
	if (!note)
		return;

	if (0 <= ch && ch < 16 && ch != note->channel()) {
		char const* opname = _("change note channel");

		// Define change command
		MidiModel::NoteDiffCommand* cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
		cmd->change (note, MidiModel::NoteDiffCommand::Channel, ch);

		// Apply change command
		apply_command (mti, mri, cmd);
	}
}

void
TrackerGrid::note_velocity_edited (const std::string& path, const std::string& text)
{
	NoteTypePtr note = get_on_note (path, edit_mti, edit_cgi);
	if (text.empty() || !note) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	if (!pattern.is_note_displayable(edit_rowi, edit_mti, edit_mri, edit_cgi)) {
		clear_editables ();
		return;
	}

	int  vel;
	// Parse the edited velocity and set the note velocity
	if (sscanf (text.c_str(), "%d", &vel) == 1) {
		set_note_velocity (edit_mti, edit_mri, note, vel);
	}

	clear_editables ();
}

void
TrackerGrid::set_note_velocity (int mti, int mri, NoteTypePtr note, int vel)
{
	if (!note)
		return;

	// Change if within acceptable boundaries and different than the previous
	// velocity
	if (0 <= vel && vel <= 127 && vel != note->velocity()) {
		char const* opname = _("change note velocity");

		// Define change command
		MidiModel::NoteDiffCommand* cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);
		cmd->change (note, MidiModel::NoteDiffCommand::Velocity, vel);

		// Apply change command
		apply_command (mti, mri, cmd);
	}
}

void
TrackerGrid::note_delay_edited (const std::string& path, const std::string& text)
{
	// Can't edit ***
	if (!pattern.is_note_displayable(edit_rowi, edit_mti, edit_mri, edit_cgi)) {
		clear_editables ();
		return;
	}

	// Parse the edited delay and set note delay
	int delay;
	if (!text.empty() and sscanf (text.c_str(), "%d", &delay) == 1) {
		set_note_delay (delay, edit_rowi, edit_mti, edit_mri, edit_cgi);
	}

	clear_editables ();
}

void
TrackerGrid::set_note_delay (int delay, int rowi, int mti, int mri, int cgi)
{
	NoteTypePtr on_note = get_on_note (rowi, mti, cgi);
	NoteTypePtr off_note = get_off_note (rowi, mti, cgi);
	if (!on_note && !off_note)
		return;

	char const* opname = _("change note delay");
	MidiModel::NoteDiffCommand* cmd = pattern.midi_model(mti, mri)->new_note_diff_command (opname);

	MidiTrackPattern* mtp = pattern.mtps[mti]; // TODO: move the used methods into pattern

	// Check if within acceptable boundaries
	if (delay < mtp->delay_ticks_min() || mtp->delay_ticks_max() < delay)
		return;

	if (on_note) {
		// Modify the start time and length according to the new on note delay

		// Change start time according to new delay
		int delta = delay - pattern.region_relative_delay_ticks(on_note->time(), rowi, mti, mri);
		Temporal::Beats relative_beats = Temporal::Beats::ticks(delta);
		Temporal::Beats new_start = on_note->time() + relative_beats;
		// Make sure the new_start is still within the visible region
		if (new_start < mtp->start_beats) {
			new_start = mtp->start_beats;
			relative_beats = new_start - on_note->time();
		}
		cmd->change (on_note, MidiModel::NoteDiffCommand::StartTime, new_start);

		// Adjust length so that the end time doesn't change
		Temporal::Beats new_length = on_note->length() - relative_beats;
		cmd->change (on_note, MidiModel::NoteDiffCommand::Length, new_length);

		if (off_note) {
			// There is an off note at the same row. Adjust its length as well.
			Temporal::Beats new_off_length = off_note->length() + relative_beats;
			cmd->change (off_note, MidiModel::NoteDiffCommand::Length, new_off_length);
		}
	}
	else if (off_note) {
		// There is only an off note. Modify its length accoding to the new off
		// note delay.
		int delta = delay - pattern.region_relative_delay_ticks(off_note->end_time(), rowi, mti, mri);
		Temporal::Beats relative_beats = Temporal::Beats::ticks(delta);
		Temporal::Beats new_length = off_note->length() + relative_beats;
		// Make sure the off note is after the on note
		if (new_length < Temporal::Beats::ticks(1))
			new_length = Temporal::Beats::ticks(1);
		// Make sure the off note doesn't go beyong the limit of the region
		if (mtp->end_beats < off_note->time() + new_length)
			new_length = mtp->end_beats - off_note->time();
		cmd->change (off_note, MidiModel::NoteDiffCommand::Length, new_length);
	}

	apply_command (mti, mri, cmd);
}

void TrackerGrid::play_note(int mti, uint8_t pitch)
{
	uint8_t event[3];
	uint8_t chan = tracker_editor.main_toolbar.channel_spinner.get_value_as_int() - 1;
	uint8_t vel = tracker_editor.main_toolbar.velocity_spinner.get_value_as_int();
	event[0] = (MIDI_CMD_NOTE_ON | chan);
	event[1] = pitch;
	event[2] = vel;
	pattern.mtps[mti]->midi_track->write_immediate_event (3, event);
}

void TrackerGrid::release_note(int mti, uint8_t pitch)
{
	uint8_t event[3];
	uint8_t chan = tracker_editor.main_toolbar.channel_spinner.get_value_as_int() - 1;
	event[0] = (MIDI_CMD_NOTE_OFF | chan);
	event[1] = pitch;
	event[2] = 0;
	pattern.mtps[mti]->midi_track->write_immediate_event (3, event);
}

void
TrackerGrid::set_current_cursor (const TreeModel::Path& path, TreeViewColumn* col)
{
	// Set current row
	current_path = path;
	current_rowi = get_row_index (path);

	// Set current col
	current_col = get_col_index (col);

	// Set mti, mtp, cgi and types
	current_mti = get_mti(col);
	current_mtp = pattern.mtps[current_mti];
	current_mri = pattern.to_mri(current_rowi, current_mti);
	current_cgi = get_cgi(col);
	current_note_type = get_note_type(col);
	current_auto_type = get_auto_type(col);

	/* now trigger a redisplay */
	// TODO: optimize that!
	redisplay_model ();

	// Readjust scroller
	scroll_to_row(path);
}

Evoral::Parameter TrackerGrid::get_parameter (int mti, int cgi)
{
	ColAutoTrackBimap::right_const_iterator ac_it = col2autotracks[mti].right.find(cgi);
	if (ac_it == col2autotracks[mti].right.end())
		return Evoral::Parameter();
	size_t edited_colnum = ac_it->second;
	ColParamBimap::left_const_iterator it = col2params[mti].left.find(edited_colnum);
	if (it == col2params[mti].left.end())
		return Evoral::Parameter();
	const Evoral::Parameter& param = it->second;
	return param;
}

boost::shared_ptr<AutomationList>
TrackerGrid::get_alist (int mti, int mri, const Evoral::Parameter& param)
{
	return pattern.get_alist (mti, mri, param);
}

void
TrackerGrid::automation_edited (const std::string& path, const std::string& text)
{
	bool is_del = text.empty();
	double nval;
	if (!is_del and sscanf (text.c_str(), "%lg", &nval) != 1) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	Evoral::Parameter param = get_parameter (edit_mti, edit_cgi);
	if (!pattern.is_auto_displayable (edit_rowi, edit_mti, edit_mri, param)) {
		clear_editables ();
		return;
	}

	// Can edit
	if (is_del)
		delete_automation_value (edit_rowi, edit_mti, edit_mri, edit_cgi);
	else
		set_automation_value (nval, edit_rowi, edit_mti, edit_mri, edit_cgi);

	clear_editables ();
}

std::pair<double, bool>
TrackerGrid::get_automation_value (int rowi, int mti, int mri, int cgi)
{
	Evoral::Parameter param = get_parameter (mti, cgi);
	return pattern.get_automation_value (rowi, mti, mri, param);
}

void
TrackerGrid::set_automation_value (double val, int rowi, int mti, int mri, int cgi)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_parameter (mti, cgi);

	// Find delay in case the value has to be created
	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int ();

	return pattern.set_automation_value (val, rowi, mti, mri, param, delay);
}

void
TrackerGrid::delete_automation_value(int rowi, int mti, int mri, int cgi)
{
	Evoral::Parameter param = get_parameter (mti, cgi);
	return pattern.delete_automation_value (rowi, mti, mri, param);
}

void
TrackerGrid::automation_delay_edited (const std::string& path, const std::string& text)
{
	int delay = 0;
	// Parse the edited delay
	if (!text.empty() and sscanf (text.c_str(), "%d", &delay) != 1) {
		clear_editables ();
		return;
	}

	// Check if within acceptable boundaries
	if (delay < edit_mtp->delay_ticks_min() || edit_mtp->delay_ticks_max() < delay) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	Evoral::Parameter param = get_parameter (edit_mti, edit_cgi);
	if (!pattern.is_auto_displayable(edit_rowi, edit_mti, edit_mri, param)) {
		clear_editables ();
		return;
	}

	// Can edit
	set_automation_delay (delay, edit_rowi, edit_mti, edit_mri, edit_cgi);

	clear_editables ();
}

std::pair<int, bool>
TrackerGrid::get_automation_delay (int rowi, int mti, int mri, int cgi)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_parameter (mti, cgi);
	return pattern.get_automation_delay (rowi, mti, mri, param);
}

void
TrackerGrid::set_automation_delay (int delay, int rowi, int mti, int mri, int cgi)
{
	Evoral::Parameter param = get_parameter (mti, cgi);
	return pattern.set_automation_delay (delay, rowi, mti, mri, param);
}

void
TrackerGrid::register_automation_undo (boost::shared_ptr<AutomationList> alist, const std::string& opname, XMLNode& before, XMLNode& after)
{
	tracker_editor.public_editor.begin_reversible_command (opname);
	tracker_editor.session->add_command (new MementoCommand<ARDOUR::AutomationList> (*alist.get (), &before, &after));
	tracker_editor.public_editor.commit_reversible_command ();
	tracker_editor.session->set_dirty ();
}

void
TrackerGrid::apply_command (size_t mti, size_t mri, MidiModel::NoteDiffCommand* cmd)
{
	// Apply change command
	pattern.apply_command (mti, mri, cmd);

	// reset edit info, since we're done
	// TODO: is this really necessary since clear_editables does that
	edit_cgi = -1;
}

void
TrackerGrid::setup_time_column()
{
	TreeViewColumn* viewcolumn_time  = new TreeViewColumn (_("Time"), columns.time);
	CellRenderer* cellrenderer_time = viewcolumn_time->get_first_cell_renderer ();

	// Link to color attributes
	viewcolumn_time->add_attribute(cellrenderer_time->property_cell_background (), columns._background_color);

	append_column (*viewcolumn_time);
}

void
TrackerGrid::setup_midi_track_column(size_t mti)
{
	std::string label("");
	TreeViewColumn* viewcolumn_midi_track  = new TreeViewColumn (label, columns.midi_region_name[mti]);
	CellRendererText* cellrenderer_midi_track = dynamic_cast<CellRendererText*> (viewcolumn_midi_track->get_first_cell_renderer ());

	// Link to font attributes
	viewcolumn_midi_track->add_attribute(cellrenderer_midi_track->property_family (), columns._family);

	// // Link to color attributes
	// viewcolumn_midi_track->add_attribute(cellrenderer_midi_track->property_cell_background (), columns._background_color);

	append_column (*viewcolumn_midi_track);
}

void
TrackerGrid::setup_note_column (size_t mti, size_t cgi)
{
	// TODO be careful of potential memory leaks
	// TODO: maybe put the information of the mti, cgi, midi_note_type or automation_type in the TreeViewColumn
	NoteColumn* note_column = new NoteColumn (columns.note_name[mti][cgi], mti, cgi);
	CellRendererText* note_cellrenderer = dynamic_cast<CellRendererText*> (note_column->get_first_cell_renderer ());

	// TODO: maybe property_wrap_mode() can be used to have fake multi-line
	// header

	// TODO: play with property_stretch() as well

	// // Link to font attributes
	// viewcolumn_note->add_attribute(cellrenderer_note->property_family (), columns._family);

	// Link to color attributes
	note_column->add_attribute(note_cellrenderer->property_cell_background (), columns._note_background_color[mti][cgi]);
	note_column->add_attribute(note_cellrenderer->property_foreground (), columns._note_foreground_color[mti][cgi]);

	// Link to editing methods
	note_cellrenderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_note_started), mti, cgi));
	note_cellrenderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	note_cellrenderer->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::note_edited));
	note_cellrenderer->property_editable() = true;

	append_column (*note_column);
}

void
TrackerGrid::setup_note_channel_column (size_t mti, size_t cgi)
{
	// TODO be careful of potential memory leaks
	ChannelColumn* channel_column = new ChannelColumn (columns.channel[mti][cgi], mti, cgi);
	CellRendererText* channel_cellrenderer = dynamic_cast<CellRendererText*> (channel_column->get_first_cell_renderer ());

	// Link to color attribute
	channel_column->add_attribute(channel_cellrenderer->property_cell_background (), columns._channel_background_color[mti][cgi]);
	channel_column->add_attribute(channel_cellrenderer->property_foreground (), columns._channel_foreground_color[mti][cgi]);

	// Link to editing methods
	channel_cellrenderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_note_channel_started), mti, cgi));
	channel_cellrenderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	channel_cellrenderer->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::note_channel_edited));
	channel_cellrenderer->property_editable() = true;

	append_column (*channel_column);
}

void
TrackerGrid::setup_note_velocity_column (size_t mti, size_t cgi)
{
	// TODO be careful of potential memory leaks
	VelocityColumn* velocity_column = new VelocityColumn (columns.velocity[mti][cgi], mti, cgi);
	CellRendererText* velocity_cellrenderer = dynamic_cast<CellRendererText*> (velocity_column->get_first_cell_renderer ());

	// Link to color attribute
	velocity_column->add_attribute(velocity_cellrenderer->property_cell_background (), columns._velocity_background_color[mti][cgi]);
	velocity_column->add_attribute(velocity_cellrenderer->property_foreground (), columns._velocity_foreground_color[mti][cgi]);

	// Link to editing methods
	velocity_cellrenderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_note_velocity_started), mti, cgi));
	velocity_cellrenderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	velocity_cellrenderer->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::note_velocity_edited));
	velocity_cellrenderer->property_editable() = true;

	append_column (*velocity_column);
}

void
TrackerGrid::setup_note_delay_column (size_t mti, size_t cgi)
{
	// TODO be careful of potential memory leaks
	DelayColumn* delay_column = new DelayColumn (columns.delay[mti][cgi], mti, cgi);
	CellRendererText* delay_cellrenderer = dynamic_cast<CellRendererText*> (delay_column->get_first_cell_renderer ());

	// Link to color attribute
	delay_column->add_attribute(delay_cellrenderer->property_cell_background (), columns._delay_background_color[mti][cgi]);
	delay_column->add_attribute(delay_cellrenderer->property_foreground (), columns._delay_foreground_color[mti][cgi]);

	// Link to editing methods
	delay_cellrenderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_note_delay_started), mti, cgi));
	delay_cellrenderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	delay_cellrenderer->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::note_delay_edited));
	delay_cellrenderer->property_editable() = true;

	append_column (*delay_column);
}

void
TrackerGrid::setup_note_separator_column ()
{
	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_separator = new TreeViewColumn ("", columns._empty);
	append_column (*viewcolumn_separator);
}

void
TrackerGrid::setup_automation_column (size_t mti, size_t cgi)
{
	// TODO be careful of potential memory leaks
	AutomationColumn* automation_column = new AutomationColumn (columns.automation[mti][cgi], mti, cgi);
	CellRendererText* automation_cellrenderer = dynamic_cast<CellRendererText*> (automation_column->get_first_cell_renderer ());

	// Link to color attributes
	automation_column->add_attribute(automation_cellrenderer->property_cell_background (), columns._automation_background_color[mti][cgi]);
	automation_column->add_attribute(automation_cellrenderer->property_foreground (), columns._automation_foreground_color[mti][cgi]);

	// Link to editing methods
	automation_cellrenderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_automation_started), mti, cgi));
	automation_cellrenderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	automation_cellrenderer->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::automation_edited));
	automation_cellrenderer->property_editable() = true;

	size_t column = get_columns().size();
	append_column (*automation_column);
	col2autotracks[mti].insert(ColAutoTrackBimap::value_type(column, cgi));
	available_automation_columns[mti].insert(column);
	get_column(column)->set_visible (false);
}

void
TrackerGrid::setup_automation_delay_column (size_t mti, size_t cgi)
{
	// TODO be careful of potential memory leaks
	AutomationDelayColumn* automation_delay_column = new AutomationDelayColumn (columns.automation_delay[mti][cgi], mti, cgi);
	CellRendererText* automation_delay_cellrenderer = dynamic_cast<CellRendererText*> (automation_delay_column->get_first_cell_renderer ());

	// Link to color attributes
	automation_delay_column->add_attribute(automation_delay_cellrenderer->property_cell_background (), columns._automation_delay_background_color[mti][cgi]);
	automation_delay_column->add_attribute(automation_delay_cellrenderer->property_foreground (), columns._automation_delay_foreground_color[mti][cgi]);

	// Link to editing methods
	automation_delay_cellrenderer->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_automation_delay_started), mti, cgi));
	automation_delay_cellrenderer->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	automation_delay_cellrenderer->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::automation_delay_edited));
	automation_delay_cellrenderer->property_editable() = true;

	size_t column = get_columns().size();
	append_column (*automation_delay_column);
	get_column(column)->set_visible (false);
}

void
TrackerGrid::setup_automation_separator_column ()
{
	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_separator = new TreeViewColumn ("", columns._empty);
	append_column (*viewcolumn_separator);
}

void
TrackerGrid::vertical_move_edit_cursor (int steps)
{
	TreeModel::Path path = edit_path;
	wrap_around_vertical_move (path, edit_mti, steps);
	TreeViewColumn* col = get_column (edit_col);
	set_cursor (path, *col, true);
}

void
TrackerGrid::wrap_around_vertical_move (TreeModel::Path& path, int mti, int steps)
{
	// TODO: make sure that steps is equal to or lower than nrows[mti]
	assert (std::abs(steps) <= pattern.nrows[mti]);

	// Step till it ends up in a defined cell
	do {
		path[0] += steps;
		if (path[0] < 0)
			path[0] += pattern.global_nrows;
		path[0] %= pattern.global_nrows;
	} while (!is_defined (path, mti));
}

void
TrackerGrid::wrap_around_horizontal_move (int& colnum, const Gtk::TreeModel::Path& path, int steps, bool tab)
{
	// TODO: make sure it doesn't loop forever

	// TODO support tab == true, to move from one ardour track to the next
	const int n_col = get_columns().size();
	TreeViewColumn* col;

	while (steps < 0) {
		--colnum;
		if (colnum < 1)
			colnum = n_col - 1;
		col = get_column (colnum);
		if (col->get_visible () and is_editable (col) and is_defined (path, col))
			++steps;
	}
	while (0 < steps) {
		++colnum;
		if (n_col <= colnum)
			colnum = 1;         // colnum 0 is time
		col = get_column (colnum);
		if (col->get_visible () and is_editable (col) and is_defined (path, col))
			--steps;
	}
}

bool
TrackerGrid::is_editable (TreeViewColumn* col) const
{
	CellRendererText* cellrenderer = dynamic_cast<CellRendererText*> (col->get_first_cell_renderer ());
	return cellrenderer->property_editable ();
}

bool
TrackerGrid::is_defined (const Gtk::TreeModel::Path& path, const TreeViewColumn* col) const
{
	return is_defined (path, get_mti(col));
}

bool
TrackerGrid::is_defined (const Gtk::TreeModel::Path& path, int mti) const
{
	return pattern.is_defined (get_row_index (path), mti);
}

size_t
TrackerGrid::get_mti(const TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*>(col);
	return tc->midi_track_idx;
}

size_t
TrackerGrid::get_cgi(const TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*>(col);
	return tc->col_group_idx;
}

TrackerColumn::midi_note_type
TrackerGrid::get_note_type(const Gtk::TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*>(col);
	return tc->note_type;
}

TrackerColumn::automation_type
TrackerGrid::get_auto_type(const Gtk::TreeViewColumn* col) const
{
	const TrackerColumn* tc = dynamic_cast<const TrackerColumn*>(col);
	return tc->auto_type;
}

void
TrackerGrid::horizontal_move_edit_cursor (int steps, bool tab)
{
	int colnum = edit_col;
	TreeModel::Path path = edit_path;
	wrap_around_horizontal_move (colnum, edit_path, steps, tab);
	TreeViewColumn* col = get_column (colnum);
	set_cursor (path, *col, true);
}

bool
TrackerGrid::move_edit_cursor_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	case GDK_Up:
	case GDK_uparrow:
		vertical_move_edit_cursor (-1);
		ret = true;
		break;
	case GDK_Down:
	case GDK_downarrow:
		vertical_move_edit_cursor (1);
		ret = true;
		break;
	case GDK_Left:
	case GDK_leftarrow:
		horizontal_move_edit_cursor(-1);
		ret = true;
		break;
	case GDK_Right:
	case GDK_rightarrow:
		horizontal_move_edit_cursor(1);
		ret = true;
		break;
	case GDK_Tab:
		horizontal_move_edit_cursor(1, true);
		ret = true;
		break;
	}

	return ret;
}

int
TrackerGrid::digit_key_press (GdkEventKey* ev)
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
TrackerGrid::pitch_key (GdkEventKey* ev)
{
	int octave = tracker_editor.main_toolbar.octave_spinner.get_value_as_int();

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
TrackerGrid::step_editing_note_key_press (GdkEventKey* ev)
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

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_on_note (uint8_t pitch)
{
	play_note (current_mti, pitch);
	set_on_note (pitch, current_rowi, current_mti, current_mri, current_cgi);
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_current_cursor (steps);
	return true;
}

bool
TrackerGrid::step_editing_set_off_note ()
{
	set_off_note (current_rowi, current_mti, current_mri, current_cgi);
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_current_cursor (steps);
	return true;
}

bool
TrackerGrid::step_editing_delete_note ()
{
	delete_note (current_rowi, current_mti, current_mri, current_cgi);
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_current_cursor (steps);
	return true;
}

bool
TrackerGrid::step_editing_note_channel_key_press (GdkEventKey* ev)
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
		ret = step_editing_set_note_channel (digit_key_press (ev));
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
		ret = move_current_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_note_channel (int digit)
{
	NoteTypePtr note = get_on_note (current_rowi, current_mti, current_cgi);
	if (note) {
		int ch = note->channel();
		int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
		int new_ch = TrackerUtils::change_digit (ch + 1, digit, position);
		set_note_channel (current_mti, current_mri, note, new_ch - 1);
	}

	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_current_cursor (steps);
	return true;
}

bool
TrackerGrid::step_editing_note_velocity_key_press (GdkEventKey* ev)
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
		ret = step_editing_set_note_velocity (digit_key_press (ev));
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
		ret = move_current_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_note_velocity (int digit)
{
	NoteTypePtr note = get_on_note (current_rowi, current_mti, current_cgi);
	if (note) {
		int vel = note->velocity();
		int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
		int new_vel = TrackerUtils::change_digit (vel, digit, position);
		set_note_velocity (current_mti, current_mri, note, new_vel);
	}
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_current_cursor (steps);
	return true;
}

bool
TrackerGrid::step_editing_note_delay_key_press (GdkEventKey* ev)
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
		ret = move_current_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_note_delay (int digit)
{
	int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	NoteTypePtr on_note = get_on_note (current_rowi, current_mti, current_cgi);
	NoteTypePtr off_note = get_off_note (current_rowi, current_mti, current_cgi);
	if (!on_note && !off_note) {
		vertical_move_edit_cursor (steps);
		return true;
	}

	// Fetch delay
	int old_delay = on_note ? pattern.region_relative_delay_ticks(on_note->time(), current_rowi, current_mti, current_mri)
		: pattern.region_relative_delay_ticks(off_note->end_time(), current_rowi, current_mti, current_mri);

	// Update delay
	int new_delay = TrackerUtils::change_digit_or_sign(old_delay, digit, position);
	set_note_delay (new_delay, current_rowi, current_mti, current_mri, current_cgi);

	// Move the cursor
	vertical_move_current_cursor (steps);

	return true;
}

bool
TrackerGrid::step_editing_automation_key_press (GdkEventKey* ev)
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
		ret = step_editing_set_automation_value (-1);
		break;

	// Plus
	case GDK_plus:
	case GDK_equal:
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
		ret = move_current_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_automation_value (int digit)
{
	std::pair<double, bool> val_def = get_automation_value(current_rowi, current_mti, current_mri, current_cgi);
	double oval = val_def.first;

	// Set new value
	int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
	double nval = TrackerUtils::change_digit_or_sign (oval, digit, position);
	set_automation_value (nval, current_rowi, current_mti, current_mri, current_cgi);

	// Move cursor
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_current_cursor (steps);

	return true;
}

bool
TrackerGrid::step_editing_automation_delay_key_press (GdkEventKey* ev)
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
		ret = move_current_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_automation_delay (int digit)
{
	std::pair<int, bool> val_def = get_automation_delay (current_rowi, current_mti, current_mri, current_cgi);
	int old_delay = val_def.first;

	// Set new value
	if (val_def.second) {
		int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
		int new_delay = TrackerUtils::change_digit_or_sign (old_delay, digit, position);
		set_automation_delay (new_delay, current_rowi, current_mti, current_mri, current_cgi);
	}

	// Move cursor
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_current_cursor (steps);

	return true;
}

void
TrackerGrid::vertical_move_current_cursor (int steps)
{
	TreeModel::Path path = current_path;
	wrap_around_vertical_move (path, current_mti, steps);
	TreeViewColumn* col = get_column (current_col);
	set_current_cursor (path, col);
}

void
TrackerGrid::horizontal_move_current_cursor (int steps, bool tab)
{
	int colnum = current_col;
	TreeModel::Path path = current_path;
	wrap_around_horizontal_move (colnum, current_path, steps, tab);
	TreeViewColumn* col = get_column (colnum);
	set_current_cursor (path, col);
}

bool
TrackerGrid::move_current_cursor_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	case GDK_Up:
	case GDK_uparrow:
		vertical_move_current_cursor(-1);
		ret = true;
		break;
	case GDK_Down:
	case GDK_downarrow:
		vertical_move_current_cursor(1);
		ret = true;
		break;
	case GDK_Left:
	case GDK_leftarrow:
		horizontal_move_current_cursor(-1);
		ret = true;
		break;
	case GDK_Right:
	case GDK_rightarrow:
		horizontal_move_current_cursor(1);
		ret = true;
		break;
	case GDK_Tab:
		horizontal_move_current_cursor(1, true);
		ret = true;
		break;
	}

	return ret;
}

bool
TrackerGrid::non_editing_key_press (GdkEventKey* ev)
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
		play_note (current_mti, pitch_key (ev));
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
		ret = move_current_cursor_key_press (ev);
		break;

	// Set edit cursor
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
TrackerGrid::non_editing_key_release (GdkEventKey* ev)
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
		release_note (current_mti, pitch_key (ev));
		ret = true;
		break;
	}

	return ret;
}

bool
TrackerGrid::key_press (GdkEventKey* ev)
{
	if (tracker_editor.main_toolbar.step_edit) {
		switch (current_auto_type) {
		case TrackerColumn::AUTOMATION_SEPARATOR:
			switch (current_note_type) {
			case TrackerColumn::NOTE:
				return step_editing_note_key_press (ev);
			case TrackerColumn::CHANNEL:
				return step_editing_note_channel_key_press (ev);
			case TrackerColumn::VELOCITY:
				return step_editing_note_velocity_key_press (ev);
			case TrackerColumn::DELAY:
				return step_editing_note_delay_key_press (ev);
			default:
				// TODO use Ardour log
				std::cout << "Error";
			}
		case TrackerColumn::AUTOMATION:
			return step_editing_automation_key_press (ev);
		case TrackerColumn::AUTOMATION_DELAY:
			return step_editing_automation_delay_key_press (ev);
		default:
			// TODO use Ardour log
			std::cout << "Error";
		}
	}
	else return non_editing_key_press (ev);

	return false;
}

bool
TrackerGrid::key_release (GdkEventKey* ev)
{
	return non_editing_key_release (ev);
}

bool
TrackerGrid::button_event (GdkEventButton* ev)
{
	if (ev->button == 1) {
		TreeModel::Path path;
		TreeViewColumn* col;
		int cell_x, cell_y;
		get_path_at_pos(ev->x, ev->y, path, col, cell_x, cell_y);

		if (ev->type == GDK_2BUTTON_PRESS)
			set_cursor (path, *col, true);
		else if (ev->type == GDK_BUTTON_PRESS)
			set_current_cursor (path, col);
	}

	return true;
}

bool
TrackerGrid::scroll_event (GdkEventScroll* ev)
{
	// TODO change values if editing is active, otherwise scroll.
	return false;               // Silence compiler
}
