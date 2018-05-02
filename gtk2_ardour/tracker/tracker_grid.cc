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
	, nrows (0)
	, current_row (0)
	, current_col (1)
	, current_mti (0)
	, edit_row (-1)
	, edit_col (-1)
	, edit_mti (-1)
	, edit_mtp (NULL)
	, edit_tracknum (-1)
	, editing_editable (NULL)
{}

TrackerGrid::~TrackerGrid () {}

TrackerGrid::TrackerGridModelColumns::TrackerGridModelColumns()
{
	// The background color differs when the row is on beats and
	// bars. This is to keep track of it.
	add (_background_color);
	add (_family);
	add (time);
	for (size_t mti /* midi track index */ = 0; mti < MAX_NUMBER_OF_MIDI_TRACKS; mti++) {
		add (midi_track_name[mti]);
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
			add (_automation[mti][i]);
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
	col2params[mti].insert(ColParamBimap::value_type(column, param));

	// Set the column title
	string name = TrackerUtils::is_pan_type(param) ?
		tracker_editor.midi_tracks[mti]->panner()->describe_parameter (param)
		: tracker_editor.midi_tracks[mti]->describe_parameter (param);
	get_column(column)->set_title (name);

	return column;
}

size_t
TrackerGrid::add_midi_automation_column (size_t mti, const Evoral::Parameter& param)
{
	// If not in param2actrl, add it.
	if (!tracker_editor.param2actrls[mti][param]) {
		tracker_editor.param2actrls[mti][param] = TrackerUtils::is_region_automation (param) ? tracker_editor.midi_models[mti]->automation_control(param, true) : tracker_editor.midi_tracks[mti]->automation_control(param, true); 
		AutomationPattern* ap = get_automation_pattern (mti, param);
		ap->insert(tracker_editor.param2actrls[mti][param]);
	}

	// Select the next available column
	size_t column = select_available_automation_column (mti);
	if (column == 0)
		return column;

	// Associate that column to the parameter
	col2params[mti].insert(ColParamBimap::value_type(column, param));

	// Set the column title
	get_column(column)->set_title (tracker_editor.midi_tracks[mti]->describe_parameter (param));

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
	const uint16_t selected_channels = tracker_editor.midi_tracks[mti]->get_playback_channel_mask();

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
		set<Evoral::Parameter> const & params = tracker_editor.midi_tracks[mti]->panner()->what_can_be_automated ();
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
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < (*mtps)[mti]->np.ntracks and tracker_editor.midi_track_toolbars[mti]->visible_note;
			get_column(note_colnum(mti, i))->set_visible (visible);
		}
		tracker_editor.midi_track_toolbars[mti]->update_visible_note_button ();
	}

	// Keep the window width is kept to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::mti_col_offset(size_t mti)
{
	return mti * (MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK * NUMBER_OF_COL_PER_NOTE_TRACK
	              + MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK * NUMBER_OF_COL_PER_AUTOMATION_TRACK
	              + 1 /* track name */);
}

int
TrackerGrid::note_colnum(size_t mti, size_t tracknum)
{
	return NOTE_COLNUM + mti_col_offset(mti) + tracknum * NUMBER_OF_COL_PER_NOTE_TRACK;
}

void
TrackerGrid::redisplay_visible_channel()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < (*mtps)[mti]->np.ntracks and tracker_editor.midi_track_toolbars[mti]->visible_channel;
			get_column(note_channel_colnum(mti, i))->set_visible (visible);
		}
		tracker_editor.midi_track_toolbars[mti]->update_visible_channel_button ();
	}

	// Keep the window width is kept to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::note_channel_colnum(size_t mti, size_t tracknum)
{
	return CHANNEL_COLNUM + mti_col_offset(mti) + tracknum * NUMBER_OF_COL_PER_NOTE_TRACK;
}

void
TrackerGrid::redisplay_visible_velocity()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < (*mtps)[mti]->np.ntracks and tracker_editor.midi_track_toolbars[mti]->visible_velocity;
			get_column(note_velocity_colnum(mti, i))->set_visible (visible);
		}
		tracker_editor.midi_track_toolbars[mti]->update_visible_velocity_button ();
	}

	// Keep the window width is kept to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::note_velocity_colnum(size_t mti, size_t tracknum)
{
	return VELOCITY_COLNUM + mti_col_offset(mti) + tracknum * NUMBER_OF_COL_PER_NOTE_TRACK;
}

void
TrackerGrid::redisplay_visible_delay()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < (*mtps)[mti]->np.ntracks and tracker_editor.midi_track_toolbars[mti]->visible_delay;
			get_column(note_delay_colnum(mti, i))->set_visible (visible);
		}
		tracker_editor.midi_track_toolbars[mti]->update_visible_delay_button ();
	}
	// Do the same for automations
	redisplay_visible_automation_delay ();

	// Keep the window width is kept to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::note_delay_colnum(size_t mti, size_t tracknum)
{
	return DELAY_COLNUM + mti_col_offset(mti) + tracknum * NUMBER_OF_COL_PER_NOTE_TRACK;
}

void
TrackerGrid::redisplay_visible_note_separator()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			bool visible = i < (*mtps)[mti]->np.ntracks
				and (tracker_editor.midi_track_toolbars[mti]->visible_note or
				     tracker_editor.midi_track_toolbars[mti]->visible_channel or
				     tracker_editor.midi_track_toolbars[mti]->visible_velocity or
				     tracker_editor.midi_track_toolbars[mti]->visible_delay);
			get_column(note_separator_colnum(mti, i))->set_visible (visible);
		}
	}

	// Keep the window width is kept to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::note_separator_colnum (size_t mti, size_t tracknum)
{
	return SEPARATOR_COLNUM + mti_col_offset(mti) + tracknum * NUMBER_OF_COL_PER_NOTE_TRACK;
}

void
TrackerGrid::redisplay_visible_automation()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
			size_t col = automation_colnum(mti, i);
			bool visible = TrackerUtils::is_in(col, visible_automation_columns);
			get_column(col)->set_visible(visible);
		}
	}
	redisplay_visible_automation_delay();

	// Keep the window width is kept to its minimum
	tracker_editor.resize_width();
}

size_t TrackerGrid::automation_col_offset(size_t mti)
{
	return 1 /* time */ + 1 /* first track name */
		+ mti_col_offset(mti)
		+ MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK * NUMBER_OF_COL_PER_NOTE_TRACK;
}

int
TrackerGrid::automation_colnum(size_t mti, size_t tracknum)
{
	return automation_col_offset(mti) + NUMBER_OF_COL_PER_AUTOMATION_TRACK * tracknum;
}

void
TrackerGrid::redisplay_visible_automation_delay()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
			size_t col = automation_delay_colnum(mti, i);
			bool visible = tracker_editor.midi_track_toolbars[mti]->visible_delay and TrackerUtils::is_in(col - 1, visible_automation_columns);
			get_column(col)->set_visible (visible);
		}
	}

	// Keep the window width is kept to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::automation_delay_colnum(size_t mti, size_t tracknum)
{
	return automation_col_offset(mti) + NUMBER_OF_COL_PER_AUTOMATION_TRACK * tracknum + 1;
}

void
TrackerGrid::redisplay_visible_automation_separator()
{
	for (size_t mti = 0; mti < mtps->size(); mti++) {
		for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
			size_t col = automation_separator_colnum(mti, i);
			bool visible = TrackerUtils::is_in(col - 2, visible_automation_columns);
			get_column(col)->set_visible (visible);
		}
	}

	// Keep the window width is kept to its minimum
	tracker_editor.resize_width();
}

int
TrackerGrid::automation_separator_colnum (size_t mti, size_t tracknum)
{
	return automation_col_offset(mti) + NUMBER_OF_COL_PER_AUTOMATION_TRACK * tracknum + 2;
}

void
TrackerGrid::setup (std::vector<MidiTrackPattern*>& midi_track_patterns)
{
	mtps = &midi_track_patterns;

	model = ListStore::create (columns);
	set_model (model);

	setup_time_column();

	// Instantiate midi tracks
	for (size_t mti = 0; mti < mtps->size(); mti++) {
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
		for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK; i++) {
			setup_note_column(mti, i);
			setup_note_channel_column(mti, i);
			setup_note_velocity_column(mti, i);
			setup_note_delay_column(mti, i);
			setup_note_separator_column();
		}

		// Instantiate automation tracks
		for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK; i++) {
			setup_automation_column(mti, i);
			setup_automation_delay_column(mti, i);
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
TrackerGrid::redisplay_model ()
{
	if (editing_editable)
		return;

	if (tracker_editor.session) {
		// In case the resolution (lines per beat) has changed
		// TODO: support multi-tracks
		tracker_editor.main_toolbar.delay_spinner.get_adjustment()->set_lower(mtps->front()->delay_ticks_min());
		tracker_editor.main_toolbar.delay_spinner.get_adjustment()->set_upper(mtps->front()->delay_ticks_max());

		// Load colors
		std::string beat_background_color = UIConfiguration::instance().color_str ("tracker editor: beat background");
		std::string bar_background_color = UIConfiguration::instance().color_str ("tracker editor: bar background");
		std::string background_color = UIConfiguration::instance().color_str ("tracker editor: background");
		std::string blank_foreground_color = UIConfiguration::instance().color_str ("tracker editor: blank foreground");
		std::string active_foreground_color = UIConfiguration::instance().color_str ("tracker editor: active foreground");
		std::string passive_foreground_color = UIConfiguration::instance().color_str ("tracker editor: passive foreground");

		// Fill row independent of mtps, and create missing row
		// TODO: need to determine nrow as well as the first row
		
		for (size_t mti = 0; mti < mtps->size(); mti++) {
			MidiTrackPattern* mtp = (*mtps)[mti];

			mtp->set_rows_per_beat(tracker_editor.main_toolbar.rows_per_beat);
			mtp->update();

			TreeModel::Row row;

			nrows = mtp->nrows;

			// Fill rows
			TreeModel::Children::iterator row_it = model->children().begin();
			for (uint32_t irow = 0; irow < nrows; irow++) {
				// Get existing row, or create one if it does exist
				if (row_it == model->children().end())
					row_it = model->append();
				row = *row_it++;

				Temporal::Beats row_beats = mtp->beats_at_row(irow);
				Temporal::Beats row_relative_beats = mtp->region_relative_beats_at_row(irow);
				uint32_t row_sample = mtp->sample_at_row(irow);

				// TODO: process this out of the mti loop
				// Time
				Timecode::BBT_Time row_bbt;
				tracker_editor.session->bbt_time(row_sample, row_bbt);
				stringstream ss;
				print_padded(ss, row_bbt);
				row[columns.time] = ss.str();

				// TODO: process this out of the mti loop
				// If the row is on a bar, beat or otherwise, the color differs
				bool is_row_beat = row_beats == row_beats.round_up_to_beat();
				bool is_row_bar = row_bbt.beats == 1;
				std::string row_background_color = (is_row_beat ? (is_row_bar ? bar_background_color : beat_background_color) : background_color);
				row[columns._background_color] = row_background_color;

				// TODO: process this out of the mti loop
				// Set font family to Monospace
				row[columns._family] = "Monospace";

				// Render midi region name (for now midi track name). Display
				// names vertically.
				const std::string& name = tracker_editor.midi_tracks[mti]->name();
				uint32_t offset_idx = irow % (name.size() + 1);
				const static std::string name_seq(":");
				row[columns.midi_track_name[mti]] = offset_idx == name.size() ? name_seq : string{name[offset_idx]};

				// Render midi notes pattern
				size_t ntracks = mtp->np.ntracks;
				if (ntracks > MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK) {
					// TODO: use Ardour's logger instead of stdout
					std::cout << "Warning: Number of note tracks needed for "
					          << "the tracker interface is too high, "
					          << "some notes might be discarded" << std::endl;
					ntracks = MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK;
				}
				for (size_t i = 0; i < ntracks; i++) {
					// Fill background colors
					row[columns._note_background_color[mti][i]] = row_background_color;
					row[columns._channel_background_color[mti][i]] = row_background_color;
					row[columns._velocity_background_color[mti][i]] = row_background_color;
					row[columns._delay_background_color[mti][i]] = row_background_color;
					
					// Fill with blank
					row[columns.note_name[mti][i]] = "----";
					row[columns.channel[mti][i]] = "--";
					row[columns.velocity[mti][i]] = "---";
					row[columns.delay[mti][i]] = "-----";

					// Grey out infoless cells
					row[columns._note_foreground_color[mti][i]] = blank_foreground_color;
					row[columns._channel_foreground_color[mti][i]] = blank_foreground_color;
					row[columns._velocity_foreground_color[mti][i]] = blank_foreground_color;
					row[columns._delay_foreground_color[mti][i]] = blank_foreground_color;

					// Reset keeping track of the on and off notes
					row[columns._off_note[mti][i]] = NULL;
					row[columns._on_note[mti][i]] = NULL;

					size_t off_notes_count = mtp->np.off_notes[i].count(irow);
					size_t on_notes_count = mtp->np.on_notes[i].count(irow);

					if (on_notes_count > 0 || off_notes_count > 0) {
						if (mtp->np.is_displayable(irow, i)) {
							// Notes off
							NotePattern::RowToNotes::const_iterator i_off = mtp->np.off_notes[i].find(irow);
							if (i_off != mtp->np.off_notes[i].end()) {
								NoteTypePtr note = i_off->second;
								row[columns.note_name[mti][i]] = note_off_str;
								row[columns._note_foreground_color[mti][i]] = active_foreground_color;
								int64_t delay_ticks = mtp->region_relative_delay_ticks(note->end_time(), irow);
								if (delay_ticks != 0) {
									row[columns.delay[mti][i]] = to_string (delay_ticks);
									row[columns._delay_foreground_color[mti][i]] = active_foreground_color;
								}
								// Keep the note off around for playing and editing
								row[columns._off_note[mti][i]] = note;
							}

							// Notes on
							NotePattern::RowToNotes::const_iterator i_on = mtp->np.on_notes[i].find(irow);
							if (i_on != mtp->np.on_notes[i].end()) {
								NoteTypePtr note = i_on->second;
								row[columns.channel[mti][i]] = to_string (note->channel() + 1);
								row[columns.note_name[mti][i]] = ParameterDescriptor::midi_note_name (note->note());
								row[columns.velocity[mti][i]] = to_string ((int)note->velocity());
								row[columns._note_foreground_color[mti][i]] = active_foreground_color;
								row[columns._channel_foreground_color[mti][i]] = active_foreground_color;
								row[columns._velocity_foreground_color[mti][i]] = active_foreground_color;

								int64_t delay_ticks = mtp->region_relative_delay_ticks(note->time(), irow);
								if (delay_ticks != 0) {
									row[columns.delay[mti][i]] = to_string (delay_ticks);
									row[columns._delay_foreground_color[mti][i]] = active_foreground_color;
								}
								// Keep the note around for playing and editing
								row[columns._on_note[mti][i]] = note;
							}
						} else {
							// Too many notes, not displayable
							row[columns.note_name[mti][i]] = undefined_str;
							row[columns._note_foreground_color[mti][i]] = active_foreground_color;
						}
					}
				}

				// Render automation pattern
				for (ColParamBimap::left_const_iterator cp_it = col2params[mti].left.begin(); cp_it != col2params[mti].left.end(); ++cp_it) {
					size_t col_idx = cp_it->first;
					ColAutoTrackBimap::left_const_iterator ca_it = col2autotracks[mti].left.find(col_idx);
					size_t i = ca_it->second;
					const Evoral::Parameter& param = cp_it->second;
					AutomationPattern* ap = get_automation_pattern (mti, param);
					const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
					size_t auto_count = r2at.count(irow);

					if (i >= MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK) {
						// TODO: use Ardour log
						std::cout << "Warning: The automation track number " << i
						          << " exceeds the maximum number of automation tracks "
						          << MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK << std::endl;
						continue;
					}

					// Fill background colors
					row[columns._automation_background_color[mti][i]] = row_background_color;
					row[columns._automation_delay_background_color[mti][i]] = row_background_color;

					// Fill default foreground color
					row[columns._automation_delay_foreground_color[mti][i]] = blank_foreground_color;

					// Fill with blank
					row[columns.automation[mti][i]] = "---";
					row[columns.automation_delay[mti][i]] = "-----";

					if (auto_count > 0) {
						if (ap->is_displayable(irow, param)) {
							AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(irow);
							if (auto_it != r2at.end()) {
								double aval = (*auto_it->second)->value;
								row[columns.automation[mti][i]] = to_string (aval);
								double awhen = (*auto_it->second)->when;
								int64_t delay_ticks = TrackerUtils::is_region_automation (param) ?
									mtp->region_relative_delay_ticks(Temporal::Beats(awhen), irow) : mtp->delay_ticks((samplepos_t)awhen, irow);
								if (delay_ticks != 0) {
									row[columns.automation_delay[mti][i]] = to_string (delay_ticks);
									row[columns._automation_delay_foreground_color[mti][i]] = active_foreground_color;
								}
								// Keep the automation iterator around for editing it
								row[columns._automation[mti][i]] = auto_it->second; // TODO not sure it is useful yet
							}
						} else {
							row[columns.automation[mti][i]] = undefined_str;
						}
						row[columns._automation_foreground_color[mti][i]] = active_foreground_color;
					} else {
						// Interpolation
						double inter_auto_val = 0;
						if (tracker_editor.param2actrls[mti][param]) {
							boost::shared_ptr<AutomationList> alist = tracker_editor.param2actrls[mti][param]->alist();
							// We need to use ControlList::rt_safe_eval instead of ControlList::eval, otherwise the lock inside eval
							// interferes with the lock inside ControlList::erase. Though if mark_dirty is called outside of the scope
							// of the WriteLock in ControlList::erase and such, then eval can be used.
							bool ok;
							double awhen = TrackerUtils::is_region_automation (param) ? row_relative_beats.to_double() : row_sample;
							inter_auto_val = alist->rt_safe_eval(awhen, ok);
						}
						row[columns.automation[mti][i]] = to_string (inter_auto_val);
						row[columns._automation_foreground_color[mti][i]] = passive_foreground_color;
					}
				}
			}

			// Remove unused rows
			for (; row_it != model->children().end();)
				row_it = model->erase(row_it);
		}
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
	std::vector<TreeViewColumn*> cols =
		(std::vector<TreeViewColumn*>)get_columns();
	for (int i = 0; i < (int)cols.size(); i++)
		if (cols[i] == col)
			return i;
	return -1;
}

boost::shared_ptr<TrackerGrid::NoteType>
TrackerGrid::get_on_note(int rowidx)
{
	return get_on_note(TreeModel::Path (1U, rowidx));
}

boost::shared_ptr<TrackerGrid::NoteType>
TrackerGrid::get_on_note(const std::string& path)
{
	return get_on_note(TreeModel::Path (path));
}

boost::shared_ptr<TrackerGrid::NoteType>
TrackerGrid::get_on_note(const TreeModel::Path& path)
{
	TreeModel::iterator iter = model->get_iter (path);
	if (!iter)
		return NoteTypePtr();
	return (*iter)[columns._on_note[edit_mti][edit_tracknum]];
}

boost::shared_ptr<TrackerGrid::NoteType>
TrackerGrid::get_off_note(int rowidx)
{
	return get_off_note(TreeModel::Path ((TreeModel::Path::size_type)1, (TreeModel::Path::value_type)rowidx));
}

boost::shared_ptr<TrackerGrid::NoteType>
TrackerGrid::get_off_note(const std::string& path)
{
	return get_off_note (TreeModel::Path (path));
}

boost::shared_ptr<TrackerGrid::NoteType>
TrackerGrid::get_off_note(const TreeModel::Path& path)
{
	TreeModel::iterator iter = model->get_iter (path);
	if (!iter)
		return NoteTypePtr();
	return (*iter)[columns._off_note[edit_mti][edit_tracknum]];
}

boost::shared_ptr<TrackerGrid::NoteType>
TrackerGrid::get_note(const std::string& path)
{
	return get_note (TreeModel::Path (path));
}

boost::shared_ptr<TrackerGrid::NoteType>
TrackerGrid::get_note(const TreeModel::Path& path)
{
	NoteTypePtr note = get_on_note(path);
	if (!note)
		note = get_off_note(path);
	return note;
}

void
TrackerGrid::editing_note_started (CellEditable* ed, const string& path, int mti, int tracknum)
{
	edit_col = note_colnum (mti, tracknum);
	editing_started (ed, path, mti, tracknum);

	if (ed && tracker_editor.main_toolbar.step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &TrackerGrid::step_editing_note_key_press), false);
		}
	}
}

void
TrackerGrid::editing_note_channel_started (CellEditable* ed, const string& path, int mti, int tracknum)
{
	edit_col = note_channel_colnum (mti, tracknum);
	editing_started (ed, path, mti, tracknum);

	if (ed && tracker_editor.main_toolbar.step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &TrackerGrid::step_editing_note_channel_key_press), false);
		}
	}
}

void
TrackerGrid::editing_note_velocity_started (CellEditable* ed, const string& path, int mti, int tracknum)
{
	edit_col = note_velocity_colnum (mti, tracknum);
	editing_started (ed, path, mti, tracknum);

	if (ed && tracker_editor.main_toolbar.step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &TrackerGrid::step_editing_note_velocity_key_press), false);
		}
	}
}

void
TrackerGrid::editing_note_delay_started (CellEditable* ed, const string& path, int mti, int tracknum)
{
	edit_col = note_delay_colnum (mti, tracknum);
	editing_started (ed, path, mti, tracknum);

	if (ed && tracker_editor.main_toolbar.step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &TrackerGrid::step_editing_note_delay_key_press), false);
		}
	}
}

void
TrackerGrid::editing_automation_started (CellEditable* ed, const string& path, int mti, int tracknum)
{
	edit_col = automation_colnum (mti, tracknum);
	editing_started (ed, path, mti, tracknum);

	if (ed && tracker_editor.main_toolbar.step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &TrackerGrid::step_editing_automation_key_press), false);
		}
	}
}

void
TrackerGrid::editing_automation_delay_started (CellEditable* ed, const string& path, int mti, int tracknum)
{
	edit_col = automation_delay_colnum (mti, tracknum);
	editing_started (ed, path, mti, tracknum);

	if (ed && tracker_editor.main_toolbar.step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &TrackerGrid::step_editing_automation_delay_key_press), false);
		}
	}
}

void
TrackerGrid::editing_started (CellEditable* ed, const string& path, int mti, int tracknum)
{
	edit_path = TreePath (path);
	edit_row = get_row_index (edit_path);
	edit_mti = mti;
	edit_mtp = (*mtps)[edit_mti];
	edit_tracknum = tracknum;
	editing_editable = ed;

	// For now set the current cursor to the edit cursor
	current_mti = mti;
}

void
TrackerGrid::clear_editables ()
{
	edit_path.clear ();
	edit_row = -1;
	edit_col = -1;
	edit_mti = -1;
	edit_mtp = NULL;
	edit_tracknum = -1;
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
	if (not edit_mtp->np.is_displayable(edit_row, edit_tracknum)) {
		clear_editables ();
		return;
	}

	if (is_on) {
		set_on_note (pitch, edit_row, edit_mti, edit_tracknum);
	} else if (is_off) {
		set_off_note (edit_row, edit_mti, edit_tracknum);
	} else if (is_del) {
		delete_note (edit_row, edit_mti, edit_tracknum);
	}

	clear_editables ();
}

void
TrackerGrid::set_on_note (uint8_t pitch, int rowidx, int mti, int tracknum)
{
	// Abort if the new pitch is invalid
	if (127 < pitch)
		return;

	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);

	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int();
	uint8_t chan = tracker_editor.main_toolbar.channel_spinner.get_value_as_int() - 1;
	uint8_t vel = tracker_editor.main_toolbar.velocity_spinner.get_value_as_int();

	MidiModel::NoteDiffCommand* cmd = NULL;

	MidiTrackPattern* mtp = (*mtps)[mti];

	if (on_note) {
		// Change the pitch of the on note
		char const * opname = _("change note");
		cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
		cmd->change (on_note, MidiModel::NoteDiffCommand::NoteNumber, pitch);
	} else if (off_note) {
		// Replace off note by another (non-off) note. Calculate the start
		// time and length of the new on note.
		Temporal::Beats start = off_note->end_time();
		Temporal::Beats end = mtp->np.next_off(rowidx, tracknum);
		Temporal::Beats length = end - start;
		// Build note using defaults
		NoteTypePtr new_note(new NoteType(chan, start, length, pitch, vel));
		char const * opname = _("add note");
		cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
		cmd->add (new_note);
		// Pre-emptively add the note in np to so that it knows in
		// which track it is supposed to be.
		mtp->np.add (tracknum, new_note);
	} else {
		// Create a new on note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = mtp->region_relative_beats_at_row(rowidx, delay);
		NoteTypePtr prev_note = mtp->np.find_prev(rowidx, tracknum);
		Temporal::Beats prev_start;
		Temporal::Beats prev_end;
		if (prev_note) {
			prev_start = prev_note->time();
			prev_end = prev_note->end_time();
		}

		char const * opname = _("add note");
		cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
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
		Temporal::Beats end = mtp->np.next_off(rowidx, tracknum);
		Temporal::Beats length = end - here;
		NoteTypePtr new_note(new NoteType(chan, here, length, pitch, vel));
		cmd->add (new_note);
		// Pre-emptively add the note in np to so that it knows in
		// which track it is supposed to be.
		mtp->np.add (tracknum, new_note);
	}

	// Apply note changes
	if (cmd)
		apply_command (mti, cmd);
}

void
TrackerGrid::set_off_note (int rowidx, int mti, int tracknum)
{
	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);

	int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int();

	MidiModel::NoteDiffCommand* cmd = NULL;

	MidiTrackPattern* mtp = (*mtps)[mti];

	if (on_note) {
		// Replace the on note by an off note, that is remove the on note
		char const * opname = _("delete note");
		cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is no off note, update the length of the preceding node
		// to match the new off note (smart off note).
		if (!off_note) {
			NoteTypePtr prev_note = mtp->np.find_prev(rowidx, edit_tracknum);
			if (prev_note) {
				Temporal::Beats length = on_note->time() - prev_note->time();
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else {
		// Create a new off note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = mtp->region_relative_beats_at_row(rowidx, delay);
		NoteTypePtr prev_note = mtp->np.find_prev(rowidx, edit_tracknum);
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
			cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
			cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, new_length);
		}
	}

	// Apply note changes
	if (cmd)
		apply_command (mti, cmd);
}

void
TrackerGrid::delete_note (int rowidx, int mti, int tracknum)
{
	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);

	MidiModel::NoteDiffCommand* cmd = NULL;

	MidiTrackPattern* mtp = (*mtps)[mti];

	if (on_note) {
		// Delete on note and change
		char const * opname = _("delete note");
		cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is an off note, update the length of the preceding note
		// to match the next note or the end of the region.
		if (off_note) {
			NoteTypePtr prev_note = mtp->np.find_prev(rowidx, edit_tracknum);
			if (prev_note) {
				// Calculate the length of the previous note
				Temporal::Beats start = prev_note->time();
				Temporal::Beats end = mtp->np.next_off(rowidx, edit_tracknum);
				Temporal::Beats length = end - start;
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else if (off_note) {
		// Update the length of the corresponding on note so the off note
		// matches the next note or the end of the region.
		Temporal::Beats start = off_note->time();
		Temporal::Beats end = mtp->np.next_off(rowidx, edit_tracknum);
		Temporal::Beats length = end - start;
		char const * opname = _("resize note");
		cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
		cmd->change (off_note, MidiModel::NoteDiffCommand::Length, length);
	}

	// Apply note changes
	if (cmd)
		apply_command (mti, cmd);
}

void
TrackerGrid::note_channel_edited (const std::string& path, const std::string& text)
{
	NoteTypePtr note = get_on_note(path);
	if (text.empty() || !note) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	if (!edit_mtp->np.is_displayable(edit_row, edit_tracknum)) {
		clear_editables ();
		return;
	}

	int  ch;
	if (sscanf (text.c_str(), "%d", &ch) == 1) {
		ch--;  // Adjust for zero-based counting
		set_note_channel(edit_mti, note, ch);
	}

	clear_editables ();
}

void
TrackerGrid::set_note_channel (int mti, NoteTypePtr note, int ch)
{
	if (!note)
		return;

	if (0 <= ch && ch < 16 && ch != note->channel()) {
		char const* opname = _("change note channel");

		// Define change command
		MidiModel::NoteDiffCommand* cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
		cmd->change (note, MidiModel::NoteDiffCommand::Channel, ch);

		// Apply change command
		apply_command (mti, cmd);
	}
}

void
TrackerGrid::note_velocity_edited (const std::string& path, const std::string& text)
{
	NoteTypePtr note = get_on_note(path);
	if (text.empty() || !note) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	if (!edit_mtp->np.is_displayable(edit_row, edit_tracknum)) {
		clear_editables ();
		return;
	}

	int  vel;
	// Parse the edited velocity and set the note velocity
	if (sscanf (text.c_str(), "%d", &vel) == 1) {
		set_note_velocity (edit_mti, note, vel);
	}

	clear_editables ();
}

void
TrackerGrid::set_note_velocity (int mti, NoteTypePtr note, int vel)
{
	if (!note)
		return;

	// Change if within acceptable boundaries and different than the previous
	// velocity
	if (0 <= vel && vel <= 127 && vel != note->velocity()) {
		char const* opname = _("change note velocity");

		// Define change command
		MidiModel::NoteDiffCommand* cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);
		cmd->change (note, MidiModel::NoteDiffCommand::Velocity, vel);

		// Apply change command
		apply_command (mti, cmd);
	}
}

void
TrackerGrid::note_delay_edited (const std::string& path, const std::string& text)
{
	// Can't edit ***
	if (!edit_mtp->np.is_displayable(edit_row, edit_tracknum)) {
		clear_editables ();
		return;
	}

	// Parse the edited delay and set note delay
	int delay;
	if (!text.empty() and sscanf (text.c_str(), "%d", &delay) == 1) {
		set_note_delay (delay, edit_row, edit_mti, edit_tracknum);
	}

	clear_editables ();
}

void
TrackerGrid::set_note_delay (int delay, int rowidx, int mti, int tracknum)
{
	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);
	if (!on_note && !off_note)
		return;

	char const* opname = _("change note delay");
	MidiModel::NoteDiffCommand* cmd = tracker_editor.midi_models[mti]->new_note_diff_command (opname);

	MidiTrackPattern* mtp = (*mtps)[mti];

	// Check if within acceptable boundaries
	if (delay < mtp->delay_ticks_min() || mtp->delay_ticks_max() < delay)
		return;

	if (on_note) {
		// Modify the start time and length according to the new on note delay

		// Change start time according to new delay
		int delta = delay - mtp->region_relative_delay_ticks(on_note->time(), rowidx);
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
		int delta = delay - mtp->region_relative_delay_ticks(off_note->end_time(), rowidx);
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

	apply_command (mti, cmd);
}

void TrackerGrid::play_note(int mti, uint8_t pitch)
{
	uint8_t event[3];
	uint8_t chan = tracker_editor.main_toolbar.channel_spinner.get_value_as_int() - 1;
	uint8_t vel = tracker_editor.main_toolbar.velocity_spinner.get_value_as_int();
	event[0] = (MIDI_CMD_NOTE_ON | chan);
	event[1] = pitch;
	event[2] = vel;
	tracker_editor.midi_tracks[mti]->write_immediate_event (3, event);
}

void TrackerGrid::release_note(int mti, uint8_t pitch)
{
	uint8_t event[3];
	uint8_t chan = tracker_editor.main_toolbar.channel_spinner.get_value_as_int() - 1;
	event[0] = (MIDI_CMD_NOTE_OFF | chan);
	event[1] = pitch;
	event[2] = 0;
	tracker_editor.midi_tracks[mti]->write_immediate_event (3, event);
}

Evoral::Parameter TrackerGrid::get_parameter (int mti, int tracknum)
{
	ColAutoTrackBimap::right_const_iterator ac_it = col2autotracks[mti].right.find(tracknum);
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
TrackerGrid::get_alist (int mti, const Evoral::Parameter& param)
{
	if (!param)
		return NULL;
	boost::shared_ptr<ARDOUR::AutomationControl> actrl = tracker_editor.param2actrls[mti][param];
	boost::shared_ptr<AutomationList> alist = actrl->alist();
	return alist;
}

AutomationPattern*
TrackerGrid::get_automation_pattern (size_t mti, const Evoral::Parameter& param)
{
	if (!param)
		return NULL;
	return TrackerUtils::is_region_automation (param) ? (AutomationPattern*)&((*mtps)[mti]->rap) : (AutomationPattern*)&((*mtps)[mti]->tap);
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
	Evoral::Parameter param = get_parameter (edit_mti, edit_tracknum);
	AutomationPattern* ap = get_automation_pattern (edit_mti, param);
	if (!ap || not ap->is_displayable(edit_row, param)) {
		clear_editables ();
		return;
	}

	if (is_del)
		delete_automation (edit_row, edit_mti, edit_tracknum);
	else
		set_automation (nval, edit_row, edit_mti, edit_tracknum);

	clear_editables ();
}

std::pair<double, bool>
TrackerGrid::get_automation_value (int rowidx, int mti, int tracknum)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_parameter (mti, tracknum);
	AutomationPattern* ap = get_automation_pattern (mti, param);
	if (!ap)
		return std::make_pair(0.0, false);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);
	if (auto_it != r2at.end()) {
		double aval = (*auto_it->second)->value;
		return std::make_pair(aval, true);
	}
	return std::make_pair(0.0, false);
}

void
TrackerGrid::set_automation (double val, int rowidx, int mti, int tracknum)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_parameter (mti, tracknum);
	boost::shared_ptr<AutomationList> alist = get_alist (mti, param);
	if (!alist)
		return;

	// Clamp nval to its range
	boost::shared_ptr<ARDOUR::AutomationControl> actrl = tracker_editor.param2actrls[mti][param];
	val = TrackerUtils::clamp (val, actrl->lower (), actrl->upper ());

	// Find the control iterator to change
	AutomationPattern* ap = get_automation_pattern (mti, param);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);

	// Save state for undo
	XMLNode& before = alist->get_state ();

	// If no existing value, insert one
	if (auto_it == r2at.end()) {
		int delay = tracker_editor.main_toolbar.delay_spinner.get_value_as_int ();
		Temporal::Beats row_relative_beats = ap->region_relative_beats_at_row(rowidx, delay);
		uint32_t row_sample = ap->sample_at_row(rowidx, delay);
		double awhen = TrackerUtils::is_region_automation (param) ? row_relative_beats.to_double() : row_sample;
		if (alist->editor_add (awhen, val, false)) {
			XMLNode& after = alist->get_state ();
			register_automation_undo (alist, _("add automation event"), before, after);
		}
		return;
	}

	// Change existing value
	double awhen = (*auto_it->second)->when;
	alist->modify (auto_it->second, awhen, val);
	XMLNode& after = alist->get_state ();
	register_automation_undo (alist, _("change automation event"), before, after);
}

void
TrackerGrid::delete_automation(int rowidx, int mti, int tracknum)
{
	Evoral::Parameter param = get_parameter (mti, tracknum);
	boost::shared_ptr<AutomationList> alist = get_alist (mti, param);
	if (!alist)
		return;

	// Find the control iterator to change
	AutomationPattern* ap = get_automation_pattern (mti, param);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);

	// Save state for undo
	XMLNode& before = alist->get_state ();

	alist->erase (auto_it->second);
	XMLNode& after = alist->get_state ();
	register_automation_undo (alist, _("delete automation event"), before, after);
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
	Evoral::Parameter param = get_parameter (edit_mti, edit_tracknum);
	AutomationPattern* ap = get_automation_pattern (edit_mti, param);
	if (!ap || !ap->is_displayable(edit_row, param)) {
		clear_editables ();
		return;
	}

	set_automation_delay (delay, edit_row, edit_mti, edit_tracknum);

	clear_editables ();
}

std::pair<int, bool>
TrackerGrid::get_automation_delay (int rowidx, int mti, int tracknum)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_parameter (mti, tracknum);
	AutomationPattern* ap = get_automation_pattern (mti, param);
	if (!ap)
		return std::make_pair(0, false);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);
	if (auto_it != r2at.end()) {
		double awhen = (*auto_it->second)->when;
		int delay_ticks = TrackerUtils::is_region_automation (param) ?
			(*mtps)[mti]->region_relative_delay_ticks(Temporal::Beats(awhen), rowidx) : (*mtps)[mti]->delay_ticks((samplepos_t)awhen, rowidx);
		return std::make_pair(delay_ticks, true);
	}
	return std::make_pair(0, false);
}

void
TrackerGrid::set_automation_delay (int delay, int rowidx, int mti, int tracknum)
{
	// Check if within acceptable boundaries
	if (delay < (*mtps)[mti]->delay_ticks_min() || (*mtps)[mti]->delay_ticks_max() < delay)
		return;

	Temporal::Beats row_relative_beats = (*mtps)[mti]->region_relative_beats_at_row(rowidx, delay);
	uint32_t row_sample = (*mtps)[mti]->sample_at_row(rowidx, delay);

	// Find the parameter to change delay
	Evoral::Parameter param = get_parameter (mti, tracknum);
	boost::shared_ptr<AutomationList> alist = get_alist (mti, param);
	if (!alist)
		return;

	// Find the control iterator to change
	AutomationPattern* ap = get_automation_pattern (mti, param);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);

	// If no existing value, abort
	if (auto_it == r2at.end())
		return;

	// Change existing delay
	XMLNode& before = alist->get_state ();
	double awhen = TrackerUtils::is_region_automation (param) ?
		(row_relative_beats < ap->start_beats ? ap->start_beats : row_relative_beats).to_double()
		: row_sample;
	alist->modify (auto_it->second, awhen, (*auto_it->second)->value);
	XMLNode& after = alist->get_state ();
	register_automation_undo (alist, _("change automation event delay"), before, after);
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
TrackerGrid::apply_command (int mti, MidiModel::NoteDiffCommand* cmd)
{
	// Apply change command
	tracker_editor.midi_models[mti]->apply_command (tracker_editor.session, cmd);

	// reset edit info, since we're done
	// TODO: is this really necessary since clear_editables does that
	edit_tracknum = -1;
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
	TreeViewColumn* viewcolumn_midi_track  = new TreeViewColumn (label, columns.midi_track_name[mti]);
	CellRendererText* cellrenderer_midi_track = dynamic_cast<CellRendererText*> (viewcolumn_midi_track->get_first_cell_renderer ());

	// Link to font attributes
	viewcolumn_midi_track->add_attribute(cellrenderer_midi_track->property_family (), columns._family);

	// // Link to color attributes
	// viewcolumn_midi_track->add_attribute(cellrenderer_midi_track->property_cell_background (), columns._background_color);

	append_column (*viewcolumn_midi_track);
}

void
TrackerGrid::setup_note_column (size_t mti, size_t i)
{
	string note_str(_("Note"));

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_note = new TreeViewColumn (note_str.c_str(), columns.note_name[mti][i]);
	CellRendererText* cellrenderer_note = dynamic_cast<CellRendererText*> (viewcolumn_note->get_first_cell_renderer ());

	// TODO: maybe property_wrap_mode() can be used to have fake multi-line
	// header

	// TODO: play with property_stretch() as well

	// // Link to font attributes
	// viewcolumn_note->add_attribute(cellrenderer_note->property_family (), columns._family);

	// Link to color attributes
	viewcolumn_note->add_attribute(cellrenderer_note->property_cell_background (), columns._note_background_color[mti][i]);
	viewcolumn_note->add_attribute(cellrenderer_note->property_foreground (), columns._note_foreground_color[mti][i]);

	// Link to editing methods
	cellrenderer_note->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_note_started), mti, i));
	cellrenderer_note->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	cellrenderer_note->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::note_edited));
	cellrenderer_note->property_editable() = true;

	append_column (*viewcolumn_note);
}

void
TrackerGrid::setup_note_channel_column (size_t mti, size_t i)
{
	string ch_str(S_("Channel|Ch"));

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_channel = new TreeViewColumn (ch_str.c_str(), columns.channel[mti][i]);
	CellRendererText* cellrenderer_channel = dynamic_cast<CellRendererText*> (viewcolumn_channel->get_first_cell_renderer ());

	// Link to color attribute
	viewcolumn_channel->add_attribute(cellrenderer_channel->property_cell_background (), columns._channel_background_color[mti][i]);
	viewcolumn_channel->add_attribute(cellrenderer_channel->property_foreground (), columns._channel_foreground_color[mti][i]);

	// Link to editing methods
	cellrenderer_channel->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_note_channel_started), mti, i));
	cellrenderer_channel->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	cellrenderer_channel->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::note_channel_edited));
	cellrenderer_channel->property_editable() = true;

	append_column (*viewcolumn_channel);
}

void
TrackerGrid::setup_note_velocity_column (size_t mti, size_t i)
{
	string vel_str(S_("Velocity|Vel"));

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_velocity = new TreeViewColumn (vel_str.c_str(), columns.velocity[mti][i]);
	CellRendererText* cellrenderer_velocity = dynamic_cast<CellRendererText*> (viewcolumn_velocity->get_first_cell_renderer ());

	// Link to color attribute
	viewcolumn_velocity->add_attribute(cellrenderer_velocity->property_cell_background (), columns._velocity_background_color[mti][i]);
	viewcolumn_velocity->add_attribute(cellrenderer_velocity->property_foreground (), columns._velocity_foreground_color[mti][i]);

	// Link to editing methods
	cellrenderer_velocity->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_note_velocity_started), mti, i));
	cellrenderer_velocity->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	cellrenderer_velocity->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::note_velocity_edited));
	cellrenderer_velocity->property_editable() = true;

	append_column (*viewcolumn_velocity);
}

void
TrackerGrid::setup_note_delay_column (size_t mti, size_t i)
{
	string delay_str(_("Delay"));

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_delay = new TreeViewColumn (delay_str.c_str(), columns.delay[mti][i]);
	CellRendererText* cellrenderer_delay = dynamic_cast<CellRendererText*> (viewcolumn_delay->get_first_cell_renderer ());

	// Link to color attribute
	viewcolumn_delay->add_attribute(cellrenderer_delay->property_cell_background (), columns._delay_background_color[mti][i]);
	viewcolumn_delay->add_attribute(cellrenderer_delay->property_foreground (), columns._delay_foreground_color[mti][i]);

	// Link to editing methods
	cellrenderer_delay->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_note_delay_started), mti, i));
	cellrenderer_delay->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	cellrenderer_delay->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::note_delay_edited));
	cellrenderer_delay->property_editable() = true;

	append_column (*viewcolumn_delay);
}

void
TrackerGrid::setup_note_separator_column ()
{
	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_separator = new TreeViewColumn ("", columns._empty);
	append_column (*viewcolumn_separator);
}

void
TrackerGrid::setup_automation_column (size_t mti, size_t i)
{
	stringstream ss_automation;
	ss_automation << "A" << i << ":" << mti;

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_automation = new TreeViewColumn (_(ss_automation.str().c_str()), columns.automation[mti][i]);
	CellRendererText* cellrenderer_automation = dynamic_cast<CellRendererText*> (viewcolumn_automation->get_first_cell_renderer ());

	// Link to color attributes
	viewcolumn_automation->add_attribute(cellrenderer_automation->property_cell_background (), columns._automation_background_color[mti][i]);
	viewcolumn_automation->add_attribute(cellrenderer_automation->property_foreground (), columns._automation_foreground_color[mti][i]);

	// Link to editing methods
	cellrenderer_automation->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_automation_started), mti, i));
	cellrenderer_automation->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	cellrenderer_automation->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::automation_edited));
	cellrenderer_automation->property_editable() = true;

	size_t column = get_columns().size();
	append_column (*viewcolumn_automation);
	col2autotracks[mti].insert(ColAutoTrackBimap::value_type(column, i));
	available_automation_columns[mti].insert(column);
	get_column(column)->set_visible (false);
}

void
TrackerGrid::setup_automation_delay_column (size_t mti, size_t i)
{
	stringstream ss_automation_delay;
	ss_automation_delay << _("Delay");

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_automation_delay = new TreeViewColumn (_(ss_automation_delay.str().c_str()), columns.automation_delay[mti][i]);
	CellRendererText* cellrenderer_automation_delay = dynamic_cast<CellRendererText*> (viewcolumn_automation_delay->get_first_cell_renderer ());

	// Link to color attributes
	viewcolumn_automation_delay->add_attribute(cellrenderer_automation_delay->property_cell_background (), columns._automation_delay_background_color[mti][i]);
	viewcolumn_automation_delay->add_attribute(cellrenderer_automation_delay->property_foreground (), columns._automation_delay_foreground_color[mti][i]);

	// Link to editing methods
	cellrenderer_automation_delay->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &TrackerGrid::editing_automation_delay_started), mti, i));
	cellrenderer_automation_delay->signal_editing_canceled().connect (sigc::mem_fun (*this, &TrackerGrid::editing_canceled));
	cellrenderer_automation_delay->signal_edited().connect (sigc::mem_fun (*this, &TrackerGrid::automation_delay_edited));
	cellrenderer_automation_delay->property_editable() = true;

	size_t column = get_columns().size();
	append_column (*viewcolumn_automation_delay);
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
TrackerGrid::vertical_move_cursor (int steps)
{
	TreeModel::Path path = edit_path;
	wrap_around_move (path, steps);
	TreeViewColumn* col = get_column (edit_col);
	set_cursor (path, *col, true);
}

void TrackerGrid::wrap_around_move (TreeModel::Path& path, int steps) const
{
	path[0] += steps;
	path[0] %= nrows;
	if (path[0] < 0)
		path[0] += nrows;
}

void
TrackerGrid::horizontal_move_cursor (int steps, bool tab)
{
	// TODO support tab == true, to move from one ardour track to the next
	int colnum = edit_col;
	const int n_col = get_columns().size();
	TreeViewColumn* col;

	while (steps < 0) {
		--colnum;
		if (colnum < 1)
			colnum = n_col - 1;
		col = get_column (colnum);
		if (col->get_visible ())
			++steps;
	}
	while (0 < steps) {
		++colnum;
		if (n_col <= colnum)
			colnum = 1;         // colnum 0 is time
		col = get_column (colnum);
		if (col->get_visible ())
			--steps;
	}
	TreeModel::Path path = edit_path;
	set_cursor (path, *col, true);
}

bool
TrackerGrid::move_cursor_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	case GDK_Up:
	case GDK_uparrow:
		vertical_move_cursor(-1);
		ret = true;
		break;
	case GDK_Down:
	case GDK_downarrow:
		vertical_move_cursor(1);
		ret = true;
		break;
	case GDK_Left:
	case GDK_leftarrow:
		horizontal_move_cursor(-1);
		ret = true;
		break;
	case GDK_Right:
	case GDK_rightarrow:
		horizontal_move_cursor(1);
		ret = true;
		break;
	case GDK_Tab:
		horizontal_move_cursor(1, true);
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
		ret = move_cursor_key_press (ev);
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
	play_note (edit_mti, pitch);
	set_on_note (pitch, edit_row, edit_mti, edit_tracknum);
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
	return true;
}

bool
TrackerGrid::step_editing_set_off_note ()
{
	set_off_note (edit_row, edit_mti, edit_tracknum);
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
	return true;
}

bool
TrackerGrid::step_editing_delete_note ()
{
	delete_note (edit_row, edit_mti, edit_tracknum);
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
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
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_note_channel (int digit)
{
	boost::shared_ptr<TrackerGrid::NoteType> note = get_on_note(edit_row);
	if (note) {
		int ch = note->channel();
		int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
		int new_ch = TrackerUtils::change_digit (ch + 1, digit, position);
		set_note_channel (edit_mti, note, new_ch - 1);
	}

	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
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
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_note_velocity (int digit)
{
	boost::shared_ptr<TrackerGrid::NoteType> note = get_on_note(edit_row);
	if (note) {
		int vel = note->velocity();
		int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
		int new_vel = TrackerUtils::change_digit (vel, digit, position);
		set_note_velocity (edit_mti, note, new_vel);
	}
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
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
		ret = move_cursor_key_press (ev);
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
	NoteTypePtr on_note = get_on_note(edit_row);
	NoteTypePtr off_note = get_off_note(edit_row);
	if (!on_note && !off_note) {
		vertical_move_cursor (steps);
		return true;
	}

	// Fetch delay
	int old_delay = on_note ? edit_mtp->region_relative_delay_ticks(on_note->time(), edit_row)
		: edit_mtp->region_relative_delay_ticks(off_note->end_time(), edit_row);

	// Update delay
	int new_delay = TrackerUtils::change_digit_or_sign(old_delay, digit, position);
	set_note_delay (new_delay, edit_row, edit_mti, edit_tracknum);

	// Move the cursor
	vertical_move_cursor (steps);

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
		ret = step_editing_set_automation (digit_key_press (ev));
		break;

	// Minus
	case GDK_minus:
	case GDK_underscore:
		ret = step_editing_set_automation (-1);
		break;

	// Plus
	case GDK_plus:
	case GDK_equal:
		ret = step_editing_set_automation (100);
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
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_automation (int digit)
{
	std::pair<double, bool> val_def = get_automation_value(edit_row, edit_mti, edit_tracknum);
	double oval = val_def.first;

	// Set new value
	int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
	double nval = TrackerUtils::change_digit_or_sign (oval, digit, position);
	set_automation (nval, edit_row, edit_mti, edit_tracknum);

	// Move cursor
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);

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
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
TrackerGrid::step_editing_set_automation_delay (int digit)
{
	std::pair<int, bool> val_def = get_automation_delay (edit_row, edit_mti, edit_tracknum);
	int old_delay = val_def.first;

	// Set new value
	if (val_def.second) {
		int position = tracker_editor.main_toolbar.position_spinner.get_value_as_int();
		int new_delay = TrackerUtils::change_digit_or_sign (old_delay, digit, position);
		set_automation_delay (new_delay, edit_row, edit_mti, edit_tracknum);
	}

	// Move cursor
	int steps = tracker_editor.main_toolbar.steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);

	return true;
}

bool
TrackerGrid::key_press (GdkEventKey* ev)
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
	}

	return ret;
}

bool
TrackerGrid::key_release (GdkEventKey* ev)
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
TrackerGrid::button_event (GdkEventButton* ev)
{
	// TODO: understand why get_path_at_pos does not work
	// if (ev->button == 1) {
		TreeModel::Path path;
		TreeViewColumn* col;
		int cell_x, cell_y;
		get_path_at_pos(ev->x, ev->y, path, col, cell_x, cell_y);
		uint32_t row_idx = get_row_index (path);
		int col_idx = get_col_index (col);
		std::cout << "ev->button = " << ev->button
		          << ", ev->x = " << ev->x
		          << ", ev->y = " << ev->y
		          << ", ev->x_root = " << ev->x_root
		          << ", ev->y_root = " << ev->y_root << std::endl;
		std::cout << "ev->window = " << ev->window
		          << ", get_bin_window()->gobj() = "
		          << get_bin_window()->gobj() << endl;
		int win_x, win_y;
		get_bin_window()->get_position(win_x, win_y);
		std::cout << "win_x = " << win_x << ", win_y = " << win_y << endl;
		std::cout << "path = " << path << ", row_idx = " << row_idx
		          << ", col_idx = " << col_idx
		          << ", cell_x = " << cell_x << ", cell_y = " << cell_y
		          << endl;

		int x, y;
		int bx, by;
		get_pointer (x, y);
		convert_widget_to_bin_window_coords (x, y, bx, by);
		get_path_at_pos (bx, by, path);
		std::cout << "x = " << x << ", y = " << y
		          << ", bx = " << bx << ", by = " << by
		          << ", path-2 = " << path << std::endl;

		std::cout << std::endl;

	// }

		set_cursor (path, *col, true);

		return true;
}

bool
TrackerGrid::scroll_event (GdkEventScroll* ev)
{
	// TODO change values if editing is active, otherwise scroll.
	return false;               // Silence compiler
}
