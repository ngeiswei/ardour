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

#ifndef __ardour_tracker_tracker_grid_h_
#define __ardour_tracker_tracker_grid_h_

#include <boost/bimap/bimap.hpp>

#include <gdkmm/color.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/tooltip.h>

#include "../piano_key_bindings.h"

#include "main_toolbar.h"
#include "midi_region_pattern.h"
#include "multi_track_pattern.h"
#include "track_automation_pattern.h"
#include "tracker_column.h"
#include "tracker_utils.h"
#include "subgrid_selector.h"

namespace Tracker {

// Maximum number of note and automation tracks. Temporary limit before a
// dedicated widget is created to replace Gtk::TreeModel::ColumnRecord

// Maximum number of tracks in case of multi-track support
#define MAX_NUMBER_OF_TRACKS 32

// Maximum number of note tracks (note, channel, vel, del) per midi track
#define MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK 16

// Maximum number of automation columns per midi track
#define MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK 16

#define NUMBER_OF_COL_PER_NOTE_TRACK 5 /*note+channel+velocity+delay+separator*/
#define NUMBER_OF_COL_PER_AUTOMATION_TRACK 3 /*automation+delay+separator*/

#define LEFT_RIGHT_SEPARATOR_WIDTH 16
#define GROUP_SEPARATOR_WIDTH 8
#define TRACK_SEPARATOR_WIDTH 8

// Number digits required to represent delay. 5 char because delay requires 4
// digit + 1 char for - sign.
#define HEX_DELAY_DIGITS 4
#define DEC_DELAY_DIGITS 5

class TrackerEditor;

class Grid : public Gtk::TreeView
{
public:
	explicit Grid (TrackerEditor& te);
	~Grid ();

	struct GridModelColumns : public Gtk::TreeModel::ColumnRecord {
		GridModelColumns ();
		// TODO: add empty columns to separate between each note track and each automations
		Gtk::TreeModelColumn<std::string> _background_color; // TODO: use Gdk::Color, maybe
		Gtk::TreeModelColumn<std::string> _family; // font family
		Gtk::TreeModelColumn<std::string> _empty; // empty column used as separator
		Gtk::TreeModelColumn<std::string> _time_background_color;
		// TODO: maybe use a row_idx column to rapidely retrieve the row_idx of a row
		Gtk::TreeModelColumn<std::string> time;
		Gtk::TreeModelColumn<std::string> _left_right_separator_background_color[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> left_separator[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> region_name[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> note_name[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _note_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _note_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> channel[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _channel_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _channel_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<Pango::AttrList> _channel_attributes[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> velocity[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _velocity_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _velocity_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<Pango::AttrList> _velocity_attributes[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<Pango::Alignment> _velocity_alignment[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> delay[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _delay_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _delay_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<Pango::AttrList> _delay_attributes[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> automation[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _automation_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _automation_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<Pango::AttrList> _automation_attributes[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> automation_delay[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _automation_delay_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _automation_delay_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<Pango::AttrList> _automation_delay_attributes[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> right_separator[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> track_separator[MAX_NUMBER_OF_TRACKS];
	};

	// For playhead synchronization
	sigc::connection clock_connection;
	void connect_clock ();
	void disconnect_clock ();

	// Set column header title and tooltip
	void set_col_title (Gtk::TreeViewColumn* col, const std::string& title, const std::string& tooltip);

	// Assign an automation parameter to a column and return the corresponding
	// column index
	int select_available_automation_column (int mti /* midi track index */);
	int add_main_automation_column (int mti, const Evoral::Parameter& param);
	int add_midi_automation_column (int mti, const Evoral::Parameter& param);
	void add_processor_automation_column (int mti, ProcessorPtr processor,
	                                      const Evoral::Parameter& what);

	void change_all_channel_tracks_visibility (int mti, bool yn, const Evoral::Parameter& param);
	void update_automation_column_visibility (int mti, const Evoral::Parameter& param);

	// Return if the automation column associated to this parameter is currently visible
	bool is_automation_visible (int mti, const Evoral::Parameter& param) const;

	void set_automation_column_visible (int mti, const Evoral::Parameter& param, int column, bool showit);

	// Return true iff there exists some automation in that mti that is visible
	bool has_visible_automation (int mti) const;

	// Return true if the gain column is visible
	bool is_gain_visible (int mti) const;
	bool is_trim_visible (int mti) const;
	bool is_mute_visible (int mti) const;
	bool is_pan_visible (int mti) const;
	void update_gain_column_visibility (int mti);
	void update_trim_column_visibility (int mti);
	void update_mute_column_visibility (int mti);
	void update_pan_columns_visibility (int mti);

	////////////////////////
	// Display Pattern    //
	////////////////////////

	void redisplay_visible_note ();
	int mti_col_offset (int mti) const;
	int left_separator_colnum (int mti) const;
	void redisplay_visible_left_separator (int mti) const;
	int region_name_colnum (int mti) const;
	int note_colnum (int mti, int cgi /* column group index */) const;
	void redisplay_visible_channel ();
	int note_channel_colnum (int mti, int cgi) const;
	void redisplay_visible_velocity ();
	int note_velocity_colnum (int mti, int cgi) const;
	void redisplay_visible_delay ();
	int note_delay_colnum (int mti, int cgi) const;
	void redisplay_visible_note_separator ();
	int note_separator_colnum (int mti, int cgi) const;
	void redisplay_visible_automation ();
	int automation_col_offset (int mti) const;
	int automation_colnum (int mti, int cgi) const;
	void redisplay_visible_automation_delay ();
	int automation_delay_colnum (int mti, int cgi) const;
	void redisplay_visible_automation_separator ();
	int automation_separator_colnum (int mti, int cgi) const;
	int right_separator_colnum (int mti) const;
	void redisplay_visible_right_separator (int mti) const;

	int base () const;
	bool is_hex () const;
	int precision () const;

	// Return the column of the first defined and editable cell of the current
	// row
	Gtk::TreeViewColumn* first_defined_col ();

	void setup ();
	void read_keyboard_layout ();     // Read keyboard layout from config
	void read_colors ();              // Read colors from config
	void redisplay_global_columns (); // time, color, font

	// Note: Gtk::TreeModel::Row is TreeRow defined in treeiter.h in gtkmm.
	void reset_off_on_note (Gtk::TreeModel::Row& row, int mti, int cgi);

	void redisplay_grid ();
	void redisplay_grid_direct_call ();
	void redisplay_grid_connect_call ();
	void redisplay_left_right_separator_columns ();
	void redisplay_left_right_separator_columns (int mti);
	void redisplay_left_right_separator (Gtk::TreeModel::Row& row, int mti);
	void redisplay_track_separator (int mti);
	void redisplay_undefined_region_name (Gtk::TreeModel::Row& row, int mti);
	void redisplay_undefined_notes (Gtk::TreeModel::Row& row, int mti); // Display undefined notes at row and mti
	void redisplay_undefined_note (Gtk::TreeModel::Row& row, int mti, int cgi); // Display undefined note at row, mti and cgi
	void redisplay_undefined_automations (Gtk::TreeModel::Row& row, int row_idx, int mti);
	void redisplay_undefined_automation (Gtk::TreeModel::Row& row, int mti, int cgi);
	void redisplay_note_background (Gtk::TreeModel::Row& row, int mti, int cgi);
	void redisplay_current_note_cursor (Gtk::TreeModel::Row& row, int mti, int cgi);
	void redisplay_blank_note_foreground (Gtk::TreeModel::Row& row, int mti, int cgi);
	void redisplay_auto_background (Gtk::TreeModel::Row& row, int mti, int cgi);
	void redisplay_note_foreground (Gtk::TreeModel::Row& row, int row_idx, int mti, int mri, int cgi);
	void redisplay_current_auto_cursor (Gtk::TreeModel::Row& row, int mti, int cgi);
	void redisplay_current_row_background ();
	void redisplay_current_cursor ();
	void redisplay_blank_auto_foreground (Gtk::TreeModel::Row& row, int mti, int cgi);
	void redisplay_automation (Gtk::TreeModel::Row& row, int row_idx, int mti, int mri, int cgi, const Evoral::Parameter& param);
	void redisplay_auto_interpolation (Gtk::TreeModel::Row& row, int row_idx, int mti, int mri, int cgi, const Evoral::Parameter& param);
	void redisplay_cell_background (int row_idx, int col_idx);
	void redisplay_cell_background (Gtk::TreeModel::Row& row, int mti, int cgi);
	void redisplay_row_background (Gtk::TreeModel::Row& row, int row_idx);
	void redisplay_row_background_color (Gtk::TreeModel::Row& row, int row_idx, const std::string& color);
	void redisplay_row_mti_background_color (Gtk::TreeModel::Row& row, int row_idx, int mti, const std::string& color);
	void redisplay_row_mti_notes_background_color (Gtk::TreeModel::Row& row, int row_idx, int mti, const std::string& color);
	void redisplay_row_mti_automations_background_color (Gtk::TreeModel::Row& row, int row_idx, int mti, const AutomationPattern& ap, const std::string& color);
	void redisplay_current_row ();
	void redisplay_pattern ();
	void redisplay_track (int mti, const TrackPatternPhenomenalDiff* tp_diff = 0);
	void redisplay_inter_midi_regions (int mti);
	void redisplay_midi_track (int mti, const MidiTrackPattern& mtp, const MidiTrackPatternPhenomenalDiff* mtp_diff = 0);
	void redisplay_track_automations (int mti, const TrackAutomationPattern& tap, const AutomationPatternPhenomenalDiff* auto_diff = 0);
	void redisplay_track_automation_param (int mti, const TrackAutomationPattern& tap, const Evoral::Parameter& param, const RowsPhenomenalDiff* rows_diff = 0);
	void redisplay_track_automation_param_row (int mti, int cgi, int row_idx, const TrackAutomationPattern& tap, const Evoral::Parameter& param);
	void redisplay_audio_track (int mti, const AudioTrackPattern& atp, const AudioTrackPatternPhenomenalDiff* atp_diff = 0);
	void redisplay_midi_region (int mti, int mri, const MidiRegionPattern& mrp, const MidiRegionPatternPhenomenalDiff* mrp_diff = 0);
	void redisplay_region_notes (int mti, int mri, const NotePattern& np, const NotePatternPhenomenalDiff* np_diff = 0);
	void redisplay_region_automations (int mti, int mri, const RegionAutomationPattern& rap, const RegionAutomationPatternPhenomenalDiff* np_diff = 0);
	void redisplay_region_automation_param (int mti, int mri, const RegionAutomationPattern& rap, const Evoral::Parameter& param, const RowsPhenomenalDiff* rows_diff = 0);
	void redisplay_region_automation_param_row (int mti, int mri, int cgi, int row_idx, const RegionAutomationPattern& rap, const Evoral::Parameter& param);
	void redisplay_note_column (int mti, int mri, int cgi, const NotePattern& np, const RowsPhenomenalDiff* rows_diff = 0);
	void redisplay_note (int mti, int mri, int cgi, int row_idx, const NotePattern& np);
	void redisplay_selection ();
	void redisplay_cell_selection (int row_idx, int col_idx);
	void redisplay_note_cell_selection (int row_idx, const Gtk::TreeViewColumn* col);
	void redisplay_auto_cell_selection (int row_idx, const Gtk::TreeViewColumn* col);
	void remove_unused_rows ();
	void unset_underline_current_step_edit_cell ();
	void unset_underline_current_step_edit_note_cell ();
	void unset_underline_current_step_edit_auto_cell ();
	void set_underline_current_step_edit_cell ();
	void set_underline_current_step_edit_note_cell ();
	void set_underline_current_step_edit_auto_cell ();

	// Modifier/Accessor.
	void set_cell_content (int row_idx, int col_idx, const std::string& text);
	// Return an empty string if the data is blank or interpolated
	std::string get_cell_content (int row_idx, int col_idx) const;

	// To align grid header
	int get_time_width () const;
	int get_track_width (int mti) const;
	int get_right_separator_width (int mti) const;
	int get_track_separator_width () const;

	std::string get_name (int mti, const Evoral::Parameter& param, bool shorten=true) const;

	void set_param_enabled (int mti, const Evoral::Parameter& param, bool enabled);
	bool is_param_enabled (int mti, const Evoral::Parameter& param) const;

	// Render a val with the position to be affected by step editing underlined
	Pango::AttrList char_underline (int ul_idx) const;
	std::pair<std::string, Pango::AttrList> underlined_value (int val) const;
	std::pair<std::string, Pango::AttrList> underlined_value (float val) const;
	std::pair<std::string, Pango::AttrList> underlined_value (const std::string& val_str) const;

	bool is_int_param (const Evoral::Parameter& param) const;

	// Call on user preference change
	void parameter_changed (const std::string& p);
	void color_changed ();

	void scroll_to_current_row ();

	TrackerEditor& tracker_editor;

	// Map column index to automation parameter and vice versa
	typedef boost::bimaps::bimap<int, Evoral::Parameter> IndexParamBimap;
	std::vector<IndexParamBimap> col2params; // For each track

	// Keep track of all visible automation columns across all midi tracks
	std::set<int> visible_automation_columns;

	// Represent patterns across all tracks
	MultiTrackPattern pattern;

	// Store previous pattern to optimize redisplay_grid by only redisplaying
	// difference
	MultiTrackPattern prev_pattern;

	static const std::string note_off_str;

	// If the resolution isn't fine enough and multiple notes do not fit in the
	// same row, then this string is printed.
	static const std::string undisplayable_str;

	GridModelColumns columns;
	Glib::RefPtr<Gtk::ListStore> model;

	// Coordonates associated to current cursor
	Temporal::Beats current_beats;
	Gtk::TreeModel::Path current_path;
	int current_row_idx;
	Gtk::TreeModel::Row current_row;
	int current_col_idx;
	Gtk::TreeViewColumn* current_col;
	int current_mti; // multi track index
	TrackPattern* previous_mtp;
	TrackPattern* current_mtp;
	int current_mri; // midi region index
	int current_cgi; // column group index
	int current_pos; // toolbar position, once adjusted

	enum TrackerColumn::midi_note_type current_note_type; // NOTE_SEPARATOR means inactive
	enum TrackerColumn::automation_type current_auto_type; // AUTOMATION_SEPARATOR means inactive
	bool current_is_note_type;

	// Clock position
	samplepos_t                  clock_pos;

	// Coordonates associated to edited note and value (this is *not* related to
	// step edit).
	Gtk::TreeModel::Path         edit_path;
	int                          edit_row_idx;
	int                          edit_col;
	int                          edit_mti;
	TrackPattern*                edit_mtp;
	int                          edit_mri;
	int                          edit_cgi;
	Gtk::CellEditable*           editing_editable;

	// Keep track of the last keyval to avoid repeating key (except cursor move)
	guint                        last_keyval;

	// Disable redisplay grid by connect call. Used to avoid lock created when
	// editing. TODO: should probably be replaced by a lock, and have
	// redisplay_grid_connect_call immediately return when such lock is taken.
	bool                         redisplay_grid_connect_call_enabled;

private:
	void init_columns ();
	void init_model ();
	void connect_tooltips ();
	bool set_tooltip (int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip);
	std::string time_tooltip_msg (int row_idx) const;
	std::string note_tooltip_msg (int row_idx, int mti, int mri, int cgi);
	std::string auto_tooltip_msg (int row_idx, int mti, int mri, int cgi);
	void connect_events ();
	void setup_tree_view ();
	void setup_time_column ();
	void setup_data_columns ();
public:
	void setup_init_cursor ();
	void setup_init_row ();
	void setup_init_col ();
private:
	void setup_left_separator_column (int mti);
	void setup_region_name_column (int mti);
	void setup_note_column (int mti, int cgi);
	void setup_note_channel_column (int mti, int cgi);
	void setup_note_velocity_column (int mti, int cgi);
	void setup_note_delay_column (int mti, int cgi);
	void setup_note_separator_column (int mti, int cgi);
	void setup_automation_column (int mti, int cgi);
	void setup_automation_delay_column (int mti, int cgi);
	void setup_automation_separator_column (int mti, int cgi);
	void setup_right_separator_column (int mti);
	void setup_track_separator_column (int mti);

	/////////////////////
	// Action Utils    //
	/////////////////////

	// Move a path by s steps, if wrap is true it wraps around so that it
	// remains within [0, nrows).
	//
	// If jump is true, then do no count steps over undefined cells. If jump is
	// false, then count the steps, and only move if the final cell is defined.
	//
	// If col is a null pointer, then it is ignored, meaning all cells are
	// considered defined. This is used when moving a row with an undefined
	// current cursor.
	void vertical_move (Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col, int s, bool wrap=true, bool jump=false);

	// Move a colnum by s steps, wrapping around so that is remains in the
	// visible columns
	void horizontal_move (int& colnum, const Gtk::TreeModel::Path& path, int s, bool tab, bool jump=false);

	int digit_key_press (GdkEventKey* ev);
	uint8_t pitch_key (GdkEventKey* ev);

	/////////////////////////
	// Non Editing Actions //
	/////////////////////////

	// Move the cursor steps rows downwards, or upwards if steps is
	// negative.
	//
	// If jump is true, then do no count steps over undefined cells. If jump is
	// false, then count the steps, and only move if the final cell is defined.
	void vertical_move_current_cursor (int steps, bool wrap=true, bool jump=false, bool set_playhead=true);

	// Like vertical_move_current_cursor using steps set in the main toolbar
	void vertical_move_current_cursor_default_steps (bool wrap=true, bool jump=false, bool set_playhead=true);

	// Move the row downwards or upwards. Used for when the cursor is undefined
	void vertical_move_current_row (int steps, bool wrap=true, bool jump=false, bool set_playhead=true);

	// Move the current cursor steps columns rightwards, or leftwards if steps
	// is negative.
	void horizontal_move_current_cursor (int steps, bool tab=false);

	bool move_current_cursor_key_press (GdkEventKey*);

	bool non_editing_key_press (GdkEventKey*);
	bool on_key_press_event (GdkEventKey*);
	bool non_editing_key_release (GdkEventKey*);

	bool key_press (GdkEventKey*);
	bool key_release (GdkEventKey*);
	bool mouse_button_event (GdkEventButton*);
	bool scroll_event (GdkEventScroll*);

	// Hack until proper dealing with shortcut keys
	bool shift_key_press ();
	bool shift_key_release (); // NEXT
	bool is_shift_pressed () const;
	bool shift_pressed;

	// Return the row index of a tree model path and vice versa
	int to_row_index (const std::string& path_str) const;
	int to_row_index (const Gtk::TreeModel::Path& path) const;
	Gtk::TreeModel::Path to_path (const std::string& path_str) const;
	Gtk::TreeModel::Path to_path (int row_idx) const;

	// TODO: replace all fucking integer types by int, all over the code!
	int get_row_offset (int mti, int mri) const;
	int get_row_size (int mti, int mri) const;

	// Return the row corresponding to a given row index
	Gtk::TreeModel::Row to_row (int row_idx) const;

	// Return the column index of a tree view column, -1 if col doesn't exist.
	int to_col_index (const Gtk::TreeViewColumn* col);

	// Return the TreeViewColumn pointing to the given column index, or
	// NULL if it doesn't exist.
	Gtk::TreeViewColumn* to_col (int col_idx);
	const Gtk::TreeViewColumn* to_col (int col_idx) const;

	// Play note
	void play_note (int mti, uint8_t pitch);
	void play_note (int mti, uint8_t pitch, uint8_t ch, uint8_t vel);
	void release_note (int mti, uint8_t pitch);
	void send_live_midi_event (int mti, const uint8_t* buf);

	// Set current cursor
	void set_current_cursor (int row_idx, Gtk::TreeViewColumn* col, bool set_playhead=false);
	void set_current_cursor (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* col, bool set_playhead=false);

	// Set current cursor undefined, let the current row defined however
	void set_current_cursor_undefined ();

	// Return true iff the current column is defined
	bool is_current_col_defined () const;

	// Return true iff the current cursor is defined.
	// WARNING: cannot be const because of is_cell_defined.
	bool is_current_cursor_defined ();

	// Set current row, including drawing row background
	void set_current_row (int row_idx, bool set_playhead=false);
	void set_current_row (const Gtk::TreeModel::Path& path, bool set_playhead=false);

	// Set current col, including selecting track and drawing cursor background
	// and underlining automation
	void set_current_col (Gtk::TreeViewColumn* col);

	// Set current row undefined
	void set_current_row_undefined ();

	// Return true iff the current row is defined. An undefined row may
	// only happen if there is nothing whatsoever to render.
	bool is_current_row_defined ();

	// Set current position (from main toolbar position spinner)
	void set_current_pos (int min_pos, int max_pos);

	// Set selector source or destination
	void set_selector (const Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col);

	/////////////////////
	// Editing Actions //
	/////////////////////

	////////////////
	// Properties //
	////////////////

	// Check whether a given column is editable. Note, due to technical reasons
	// outside of our control col cannot be const.
public:
	bool is_editable (int col_idx) const; // Accessed by SubgridSelector
	// TODO: see if we can constify col using const_cast in the code
	bool is_editable (Gtk::TreeViewColumn* col) const;
	bool is_visible (int col_idx) const;  // Accessed by SubgridSelector
	// TODO: see if we can constify col using const_cast in the code
	bool is_visible (Gtk::TreeViewColumn* col) const;

	// Check if the cell is defined and editable
	// Warning: can't be const because of to_col_index
public:
	bool is_cell_defined (int row_idx, int col_idx); // Accessed by SubgridSelector
private:
	bool is_cell_defined (int row_idx, const Gtk::TreeViewColumn* col);
	bool is_cell_defined (const Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col);
	bool is_region_defined (const Gtk::TreeModel::Path& path, int mti) const;
	bool is_region_defined (int row_idx, int mti) const;
	bool is_automation_defined (int row_idx, int mti, int cgi) const;

	// Check if the cell is blank, that is it is defined and editable but
	// contains no datum. Warning: cannot be const due to is_cell_defined.
	bool is_cell_blank (int row_idx, const Gtk::TreeViewColumn* col);
	bool is_cell_blank (const Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col);

	// Return mti corresponding col, or -1 if invalid
	int to_mti (const Gtk::TreeViewColumn* col) const;

	// Return cgi (either note or automation) corresponding to col, or -1 if invalid
	int to_cgi (const Gtk::TreeViewColumn* col) const;

	// Return mri corresponding to the cell at the given path and col, or -1 if
	// invalid. Note: method cannot be cosnt due to the use of to_row_index.
	int to_mri (const Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col) const;

	// Return respectively note or auto type of the given col
	TrackerColumn::midi_note_type get_note_type (const Gtk::TreeViewColumn* col) const;
	TrackerColumn::automation_type get_auto_type (const Gtk::TreeViewColumn* col) const;

	// Return true iff the given col corresponds to a note name, channel,
	// velocity or delay.
	bool is_note_type (const Gtk::TreeViewColumn* col) const;

	// Return true iff the given col corresponds to an automation, or automation
	// delay.
	bool is_auto_type (const Gtk::TreeViewColumn* col) const;

	/**
	 * Select the current track on the public editor
	 */
	void select_current_track ();

public:
	/**
	 * Call MidiTrack::set_step_editing on the current (or previous)
	 * track, if it is a midi track.
	 */
	void set_step_editing_current_track ();
	void unset_step_editing_current_track ();
	void unset_step_editing_previous_track ();

	/**
	 * Call many times per second to read the midi ring buffer of the
	 * midi track step editor.
	 */
	bool step_editing_check_midi_event ();

private:
	bool step_editing_note_key_press (GdkEventKey*);
	bool step_editing_set_on_note (uint8_t pitch, bool play=true);
	bool step_editing_set_on_note (uint8_t pitch, uint8_t ch, uint8_t vel, bool play=true);
	bool step_editing_set_off_note ();
	bool step_editing_delete_note ();

	bool step_editing_note_channel_key_press (GdkEventKey*);
	bool step_editing_set_note_channel_digit (int digit);
	bool step_editing_set_note_channel (uint8_t ch);
	bool step_editing_note_velocity_key_press (GdkEventKey*);
	bool step_editing_set_note_velocity_digit (int digit);
	bool step_editing_set_note_velocity (uint8_t vel);
	bool step_editing_note_delay_key_press (GdkEventKey*);
	bool step_editing_set_note_delay (int digit);
	bool step_editing_delete_note_delay ();

	bool step_editing_automation_key_press (GdkEventKey*);
	bool step_editing_set_automation_value (int digit);
	bool step_editing_delete_automation ();
	bool step_editing_automation_delay_key_press (GdkEventKey*);
	bool step_editing_set_automation_delay (int digit);
	bool step_editing_delete_automation_delay ();

	// Whether a note or automation is dispayable, if not then it's
	// content is replaced by ***
	bool is_note_displayable (int row_idx, int mti, int mri, int cgi) const;
	bool is_auto_displayable (int row_idx, int mti, int mri, int cgi) const;
	bool is_auto_displayable (int row_idx, int mti, int mri, const Evoral::Parameter& param) const;

	// Get note from path, mti and cgi
	NotePtr get_on_note (const std::string& path, int mti, int cgi) const;
	NotePtr get_on_note (const Gtk::TreeModel::Path& path, int mri, int cgi) const;
	NotePtr get_on_note (int row_idx, int mti, int cgi) const;
	NotePtr get_off_note (const std::string& path, int mti, int cgi) const;
	NotePtr get_off_note (const Gtk::TreeModel::Path& path, int mti, int cgi) const;
	NotePtr get_off_note (int row_idx, int mti, int cgi) const;

	// Get on/off note delay
	int get_on_note_delay (NotePtr on_note, int row_idx, int mti, int mri) const;
	int get_off_note_delay (NotePtr off_note, int row_idx, int mti, int mri) const;

	// Get on or off note from path, mti and cgi.
	NotePtr get_note (int row_idx, int mti, int cgi) const;
	NotePtr get_note (const std::string& path, int mti, int cgi) const;
	NotePtr get_note (const Gtk::TreeModel::Path& path, int mti, int cgi) const;

	// Return true iff there is a on or off note at this coordonate.
	bool has_note (const Gtk::TreeModel::Path& path, int mti, int cgi) const;

	void editing_note_started (Gtk::CellEditable*, const std::string& path, int mti, int cgi);
	void editing_note_channel_started (Gtk::CellEditable*, const std::string& path, int mti, int cgi);
	void editing_note_velocity_started (Gtk::CellEditable*, const std::string& path, int mti, int cgi);
	void editing_note_delay_started (Gtk::CellEditable*, const std::string& path, int mti, int cgi);
	void editing_automation_started (Gtk::CellEditable*, const std::string& path, int mti, int cgi);
	void editing_automation_delay_started (Gtk::CellEditable*, const std::string& path, int mti, int cgi);
	void editing_started (Gtk::CellEditable*, const std::string& path, int mti, int cgi);

	void clear_editables ();
	void editing_canceled ();

	// Midi note callbacks
	uint8_t parse_pitch (const std::string& text) const;
	void note_edited (const std::string& path, const std::string& text);
	void set_note_text (int row_idx, int mti, int mri, int cgi, const std::string& text);

	// Set a new on note (resp. off note, or none) on the grid. The
	// return pair of set_on_note is the channel and velocity of the
	// new note. It is returned to be passed to play_note.
	std::pair<uint8_t, uint8_t> set_on_note (int row_idx, int mti, int mri, int cgi, uint8_t pitch);
	std::pair<uint8_t, uint8_t> set_on_note (int row_idx, int mti, int mri, int cgi, uint8_t pitch, uint8_t ch, uint8_t vel);
	void set_off_note (int row_idx, int mti, int mri, int cgi);
	void delete_note (int row_idx, int mti, int mri, int cgi);

	void note_channel_edited (const std::string& path, const std::string& text);
	void set_note_channel_text (int row_idx, int mti, int mri, int cgi, const std::string& text);
	void set_note_channel (int mti, int mri, NotePtr note, int ch);

	void note_velocity_edited (const std::string& path, const std::string& text);
	void set_note_velocity_text (int row_idx, int mti, int mri, int cgi, const std::string& text);
	void set_note_velocity (int mti, int mri, NotePtr note, int vel);

	void note_delay_edited (const std::string& path, const std::string& text);
	void set_note_delay_text (int row_idx, int mti, int mri, int cgi, const std::string& text);
	void set_note_delay (int row_idx, int mti, int mri, int cgi, int delay);

	// Return parameter at mti and automation cgi. Return the empty parameter if
	// undefined.
	Evoral::Parameter get_param (int mti, int cgi) const;

	// Return cgi associated to param at mti. If undefined for param return -1.
	int to_cgi (int mti, const Evoral::Parameter& param) const;

	AutomationListPtr get_alist (int mti, int mri, const Evoral::Parameter& param);
	const AutomationListPtr get_alist (int mti, int mri, const Evoral::Parameter& param) const;

	void automation_edited (const std::string& path, const std::string& text);
	// Return automation value at given coordinates. The flag is true iff such
	// value exists.
	std::pair<double, bool> get_automation_value (int row_idx, int mti, int mri, int cgi) const;
	bool has_automation_value (int row_idx, int mti, int mri, int cgi) const;
	// In case no such value exists, then return its interpolation (or default)
	double get_automation_interpolation_value (int row_idx, int mti, int mri, int cgi) const;
	double get_automation_interpolation_value (int row_idx, int mti, int mri, const Evoral::Parameter& param) const;
	void set_automation_value (int row_idx, int mti, int mri, int cgi, double val);
	void delete_automation_value (int row_idx, int mti, int mri, int automation_cgi);
	// Like set_automation_value but provide a text representating the
	// value instead of value itself.  If empty then it deletes the
	// value instead.
	void set_automation_value_text (int row_idx, int mti, int mri, int cgi, const std::string& text);
	void automation_delay_edited (const std::string& path, const std::string& text);
	std::pair<int, bool> get_automation_delay (int row_idx, int mti, int mri, int cgi) const; // return (0, false) if undefined
	bool has_automation_delay (int row_idx, int mti, int mri, int cgi) const; // Whether automation is defined (regardless of whether it is null)
	void set_automation_delay (int row_idx, int mti, int mri, int cgi, int delay);
	void delete_automation_delay (int row_idx, int mti, int mri, int cgi);
	void set_automation_delay_text (int row_idx, int mti, int mri, int cgi, const std::string& text);

	// Return lower and upper bounds of the given parameter
	double lower (int row_idx, int mti, const Evoral::Parameter& param) const;
	double upper (int row_idx, int mti, const Evoral::Parameter& param) const;

public:
	void register_automation_undo (AutomationListPtr alist, const std::string& opname, XMLNode& before, XMLNode& after);

private:
	void apply_command (int mti, int mri, ARDOUR::MidiModel::NoteDiffCommand* cmd);
	void follow_playhead (samplepos_t);

	/**
	 * Create a string of n blank chars.
	 */
	static const char blank_char = '-';
	static std::string mk_blank (size_t n);

	/**
	 * Check whether a string is blank such as "----"
	 */
	static bool is_blank (const std::string& str);

	/**
	 * Create blank strings for note, channel, velocity, delay and automation
	 */
	std::string mk_note_blank () const;
	std::string mk_ch_blank () const;
	std::string mk_vel_blank () const;
	std::string mk_delay_blank () const;
	std::string mk_auto_blank () const;

	const std::string cellfont;

	// Map column index to automation cgi and vice versa
	typedef boost::bimaps::bimap<int, int> IndexBimap;

	// Columns
	Gtk::TreeViewColumn* time_column;
	std::vector<Gtk::TreeViewColumn*> left_separator_columns;
	std::vector<Gtk::TreeViewColumn*> region_name_columns;
	std::vector<std::vector<NoteColumn*> > note_columns;
	std::vector<std::vector<ChannelColumn*> > channel_columns;
	std::vector<std::vector<VelocityColumn*> > velocity_columns;
	std::vector<std::vector<DelayColumn*> > delay_columns;
	std::vector<std::vector<Gtk::TreeViewColumn*> > note_separator_columns;
	std::vector<std::vector<AutomationColumn*> > automation_columns;
	std::vector<std::vector<AutomationDelayColumn*> > automation_delay_columns;
	std::vector<std::vector<Gtk::TreeViewColumn*> > automation_separator_columns;
	std::vector<Gtk::TreeViewColumn*> right_separator_columns;
	std::vector<Gtk::TreeViewColumn*> track_separator_columns;

	// Map column index to automation cgi per track
	std::vector<IndexBimap> col2auto_cgi;

	// Gain, trim, mute and pan columns per midi track
	std::vector<int> gain_columns;
	std::vector<int> trim_columns; // TODO: support audio tracks
	std::vector<int> mute_columns;
	std::vector<std::vector<int> > pan_columns;

	// List of column indices currently unassigned to an automation per midi track
	std::vector<std::set<int>> available_automation_columns;

	// Colors from config
	std::string gtk_bases_color;
	std::string beat_background_color;
	std::string bar_background_color;
	std::string background_color;
	std::string active_foreground_color;
	std::string passive_foreground_color;
	std::string cursor_color;
	std::string current_row_color;
	std::string current_edit_row_color;
	std::string selection_color;

	// Keep track of phenomenal differences between prev_pattern and pattern so
	// speed up redisplay_grid
	MultiTrackPatternPhenomenalDiff _phenomenal_diff;

	// Stack keeping track of when to skip follow_playhead to avoid
	// inconsistencies due to the delays between calling set_current_cursor with
	// set_playhead set to true and Ardour actually setting the playhead.
	//
	// TODO: maybe make sure that it is thread safe.
	std::stack<int> skip_follow_playhead;

	// Mapping from pitch to channel of the current pressed keys, so
	// that release_note knows what is the channel of the note to
	// release.
	std::map<uint8_t, uint8_t> pressed_keys_pitch_to_channel;

	// Mapping PC keyboard key to note pitch
	PianoKeyBindings _keyboard_layout;

	// Subgrid Selector
public:
	SubgridSelector _subgrid_selector; // TODO: used by MainToolBar move to public officially
};

} // ~namespace tracker

#endif /* __ardour_tracker_tracker_grid_h_ */
