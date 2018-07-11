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

#ifndef __ardour_tracker_tracker_grid_h_
#define __ardour_tracker_tracker_grid_h_

#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>

#include "tracker_column.h"
#include "multi_track_pattern.h"
#include "tracker_utils.h"

// Maximum number of note and automation tracks. Temporary limit before a
// dedicated widget is created to replace Gtk::TreeModel::ColumnRecord

// Maximum number of midi tracks in case of multi-track support
#define MAX_NUMBER_OF_MIDI_TRACKS 32

// Maximum number of note tracks (note, channel, vel, del) per midi track
#define MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK 8

// Total maximum number of note tracks
#define MAX_NUMBER_OF_NOTE_TRACKS MAX_NUMBER_OF_MIDI_TRACKS*MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK

// Maximum number of automation columns per midi track
#define MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK 8

// Total maximum number of automation columns
#define MAX_NUMBER_OF_AUTOMATION_TRACKS MAX_NUMBER_OF_MIDI_TRACKS*MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK

#define NUMBER_OF_COL_PER_NOTE_TRACK 5 /*note+channel+velocity+delay+separator*/
#define NUMBER_OF_COL_PER_AUTOMATION_TRACK 3 /*automation+delay+separator*/

class TrackerEditor;

class TrackerGrid : public Gtk::TreeView
{
public:
	TrackerGrid(TrackerEditor& te);
	~TrackerGrid();

	struct TrackerGridModelColumns : public Gtk::TreeModel::ColumnRecord {
		TrackerGridModelColumns();
		// TODO: add empty columns to separate between each note track and each automations
		Gtk::TreeModelColumn<std::string> _background_color;
		Gtk::TreeModelColumn<std::string> _family; // font family
		Gtk::TreeModelColumn<std::string> _empty; // empty column used as separator		
		Gtk::TreeModelColumn<std::string> time;
		Gtk::TreeModelColumn<std::string> midi_track_name[MAX_NUMBER_OF_MIDI_TRACKS];
		Gtk::TreeModelColumn<std::string> note_name[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _note_background_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _note_foreground_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> channel[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _channel_background_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _channel_foreground_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> velocity[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _velocity_background_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _velocity_foreground_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> delay[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _delay_background_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _delay_foreground_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<NoteTypePtr> _on_note[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<NoteTypePtr> _off_note[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> automation[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> _automation_background_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> _automation_foreground_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> automation_delay[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> _automation_delay_background_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> _automation_delay_foreground_color[MAX_NUMBER_OF_MIDI_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS];
	};

	// Assign an automation parameter to a column and return the corresponding
	// column index
	size_t select_available_automation_column (size_t mti /* midi track index */);
	size_t add_main_automation_column (size_t mti, const Evoral::Parameter& param);
	size_t add_midi_automation_column (size_t mti, const Evoral::Parameter& param);
	void add_processor_automation_column (size_t mti, boost::shared_ptr<ARDOUR::Processor> processor,
	                                      const Evoral::Parameter& what);

	void change_all_channel_tracks_visibility (size_t mti, bool yn, const Evoral::Parameter& param);
	void update_automation_column_visibility (size_t mti, const Evoral::Parameter& param);

	// Return if the automation column associated to this parameter is currently visible
	bool is_automation_visible(size_t mti, const Evoral::Parameter& param) const;

	// Return true iff there exists some automation in that mti that is visible
	bool has_visible_automation(size_t mti) const;

	// Return true if the gain column is visible
	bool is_gain_visible (size_t mti) const;
	bool is_mute_visible (size_t mti) const;
	bool is_pan_visible (size_t mti) const;
	void update_gain_column_visibility (size_t mti);
	void update_trim_column_visibility (size_t mti);
	void update_mute_column_visibility (size_t mti);
	void update_pan_columns_visibility (size_t mti);

	////////////////////////
	// Display Pattern    //
	////////////////////////

	void redisplay_visible_note ();
	int mti_col_offset(size_t mti) const;
	int note_colnum (size_t mti, size_t cgi /* column group index */) const;
	void redisplay_visible_channel ();
	int note_channel_colnum (size_t mti, size_t cgi) const;
	void redisplay_visible_velocity ();
	int note_velocity_colnum (size_t mti, size_t cgi) const;
	void redisplay_visible_delay ();
	int note_delay_colnum (size_t mti, size_t cgi) const;
	void redisplay_visible_note_separator ();
	int note_separator_colnum (size_t mti, size_t cgi) const;
	void redisplay_visible_automation ();
	size_t automation_col_offset(size_t mti) const;
	int automation_colnum (size_t mti, size_t cgi) const;
	void redisplay_visible_automation_delay ();
	int automation_delay_colnum (size_t mti, size_t cgi) const;
	void redisplay_visible_automation_separator ();
	int automation_separator_colnum (size_t mti, size_t cgi) const;

	void setup (std::vector<MidiTrackPattern*>& midi_track_patterns);
	void read_colors ();         // Read colors from config
	void update_global_columns (); // time, color, font
	void redisplay_model ();    // TODO rename

	TrackerEditor& tracker_editor;

	// Map column index to automation parameter and vice versa
	typedef boost::bimaps::bimap<size_t, Evoral::Parameter> ColParamBimap;
	std::vector<ColParamBimap> col2params; // For each midi track

	// Keep track of all visible automation columns across all midi tracks
	std::set<size_t> visible_automation_columns;

	// Represent patterns across all tracks
	MultiTrackPattern pattern;

	static const std::string note_off_str;

	// If the resolution isn't fine enough and multiple notes do not fit in the
	// same row, then this string is printed.
	static const std::string undefined_str;

	TrackerGridModelColumns      columns;
	Glib::RefPtr<Gtk::ListStore> model;

	// Coordonates associated to current cursor
	Gtk::TreeModel::Path         current_path;
	int                          current_row;
	int                          current_col;
	int                          current_mti;
	MidiTrackPattern*            current_mtp;	
	int                          current_cgi;

	// TODO: probably not necessary
	enum TrackerColumn::midi_note_type current_note_type; // NOTE_SEPARATOR means inactive
	enum TrackerColumn::automation_type current_auto_type; // AUTOMATION_SEPARATOR means inactive

	// Coordonates associated to edit cursor
	Gtk::TreeModel::Path         edit_path;
	int                          edit_row;
	int                          edit_col;
	int                          edit_mti;
	MidiTrackPattern*            edit_mtp;
	int                          edit_cgi;
	Gtk::CellEditable*           editing_editable;

private:
	void setup_time_column ();
	void setup_midi_track_column (size_t mti);
	void setup_note_column (size_t mti, size_t i);
	void setup_note_channel_column (size_t mti, size_t i);
	void setup_note_velocity_column (size_t mti, size_t i);
	void setup_note_delay_column (size_t mti, size_t i);
	void setup_note_separator_column (); // TODO: could be replaced by setup_separator
	void setup_automation_column (size_t mti, size_t i);
	void setup_automation_delay_column (size_t mti, size_t i);
	void setup_automation_separator_column (); // TODO: could be replaced by setup_separator

	/////////////////////
	// Action Utils    //
	/////////////////////

	// Move a path by s steps, wrapping around so that is remains [0, nrows).
	void wrap_around_vertical_move (Gtk::TreeModel::Path& path, int mti, int s);

	// Move a colnum by s steps, wrapping around so that is remains in the
	// visible columns
	void wrap_around_horizontal_move (int& colnum, const Gtk::TreeModel::Path& path, int s, bool tab);

	int digit_key_press (GdkEventKey* ev);
	uint8_t pitch_key (GdkEventKey* ev);

	/////////////////////////
	// Non Editing Actions //
	/////////////////////////

	// Move the cursor steps rows downwards, or upwards if steps is
	// negative.
	void vertical_move_current_cursor (int steps);

	// Move the current cursor steps columns rightwards, or leftwards if steps
	// is negative.
	void horizontal_move_current_cursor (int steps, bool tab=false);

	bool move_current_cursor_key_press (GdkEventKey*);

	bool non_editing_key_press (GdkEventKey*);
	bool non_editing_key_release (GdkEventKey*);
	
	bool key_press (GdkEventKey*);
	bool key_release (GdkEventKey*);
	bool button_event (GdkEventButton*);
	bool scroll_event (GdkEventScroll*);

	// Return the row index of a tree model path
	uint32_t get_row_index (const std::string& path) const;
	uint32_t get_row_index (const Gtk::TreeModel::Path& path) const;

	// Return the column index of a tree view column, -1 if col doesn't exist.
	int get_col_index (Gtk::TreeViewColumn* col);

	// Play note
	void play_note(int mti, uint8_t pitch);
	void release_note(int mti, uint8_t pitch);

	// Set current cursor
	void set_current_cursor (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* col);

	/////////////////////
	// Editing Actions //
	/////////////////////

	// Move the cursor steps rows downwards, or upwards if steps is
	// negative. Called while editing.
	void vertical_move_edit_cursor (int steps);

	// Check whether a given column is editable
	bool is_editable (Gtk::TreeViewColumn* col) const;

	// Check if the cell is defined at all
	bool is_defined (const Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col) const;
	bool is_defined (const Gtk::TreeModel::Path& path, int mti) const;

	// Move the editing cursor steps columns rightwards, or leftwards if steps
	// is negative.
	void horizontal_move_edit_cursor (int steps, bool tab=false);

	bool move_edit_cursor_key_press (GdkEventKey* ev);

	bool step_editing_note_key_press (GdkEventKey*);
	bool step_editing_set_on_note (uint8_t pitch);
	bool step_editing_set_off_note ();
	bool step_editing_delete_note ();

	bool step_editing_note_channel_key_press (GdkEventKey*);
	bool step_editing_set_note_channel (int digit);
	bool step_editing_note_velocity_key_press (GdkEventKey*);
	bool step_editing_set_note_velocity (int digit);
	bool step_editing_note_delay_key_press (GdkEventKey*);
	bool step_editing_set_note_delay (int digit);
	bool step_editing_automation_key_press (GdkEventKey*);
	bool step_editing_set_automation (int digit);
	bool step_editing_automation_delay_key_press (GdkEventKey*);
	bool step_editing_set_automation_delay (int digit);

	// Get note from path, mti and cgi
	NoteTypePtr get_on_note (int rowidx, int mti, int cgi);
	NoteTypePtr get_on_note (const std::string& path, int mti, int cgi);
	NoteTypePtr get_on_note (const Gtk::TreeModel::Path& path, int mti, int cgi);
	NoteTypePtr get_off_note (int rowidx, int mti, int cgi);
	NoteTypePtr get_off_note (const std::string& path, int mti, int cgi);
	NoteTypePtr get_off_note (const Gtk::TreeModel::Path& path, int mti, int cgi);

	void editing_note_started (Gtk::CellEditable*, const std::string& path, int mti, int i);
	void editing_note_channel_started (Gtk::CellEditable*, const std::string& path, int mti, int i);
	void editing_note_velocity_started (Gtk::CellEditable*, const std::string& path, int mti, int i);
	void editing_note_delay_started (Gtk::CellEditable*, const std::string& path, int mti, int i);
	void editing_automation_started (Gtk::CellEditable*, const std::string& path, int mti, int i);
	void editing_automation_delay_started (Gtk::CellEditable*, const std::string& path, int mti, int i);
	void editing_started (Gtk::CellEditable*, const std::string& path, int mti, int i);

	void clear_editables ();
	void editing_canceled ();

	// Midi note callbacks
	uint8_t parse_pitch (const std::string& text) const;
	void note_edited (const std::string& path, const std::string& text);
	void set_on_note (uint8_t pitch, int rowidx, int mti, int cgi);
	void set_off_note (int rowidx, int mti, int cgi);
	void delete_note (int rowidx, int mti, int cgi);
	void note_channel_edited (const std::string& path, const std::string& text);
	void set_note_channel (int mti, NoteTypePtr note, int ch);
	void note_velocity_edited (const std::string& path, const std::string& text);
	void set_note_velocity (int mti, NoteTypePtr note, int vel);
	void note_delay_edited (const std::string& path, const std::string& text);
	void set_note_delay (int delay, int rowidx, int mti, int cgi);

	// Automation callbacks
	Evoral::Parameter get_parameter (int mti, int automation_cgi);
	boost::shared_ptr<ARDOUR::AutomationList> get_alist (int mti, const Evoral::Parameter& param);
	void automation_edited (const std::string& path, const std::string& text);
	std::pair<double, bool> get_automation_value (int rowidx, int mti, int cgi); // return zero if undefined!
	void set_automation (double val, int rowidx, int mti, int automation_cgi);
	void delete_automation (int rowidx, int mti, int automation_cgi);
	void automation_delay_edited (const std::string& path, const std::string& text);
	std::pair<int, bool> get_automation_delay (int rowidx, int mti, int cgi); // return zero if undefined!
	void set_automation_delay (int delay, int rowidx, int mti, int automation_cgi);

	void register_automation_undo (boost::shared_ptr<ARDOUR::AutomationList> alist, const std::string& opname, XMLNode& before, XMLNode& after);
	void apply_command (int mti, ARDOUR::MidiModel::NoteDiffCommand* cmd);

	// Map column index to automation track index and vice versa
	typedef boost::bimaps::bimap<size_t, size_t> ColAutoTrackBimap;

	// ColAutoTrackBimap per midi track
	std::vector<ColAutoTrackBimap> col2autotracks;

	// Gain, trim, mute and pan columns per midi track
	std::vector<size_t> gain_columns;
	std::vector<size_t> trim_columns; // TODO: support audio tracks
	std::vector<size_t> mute_columns;
	std::vector<std::vector<size_t> > pan_columns;

	// List of column indices currently unassigned to an automation per midi track
	std::vector<std::set<size_t>> available_automation_columns;

	// Colors from config
	std::string gtk_bases_color;
	std::string beat_background_color;
	std::string bar_background_color;
	std::string background_color;
	std::string blank_foreground_color;
	std::string active_foreground_color;
	std::string passive_foreground_color;
	std::string cursor_color;
};

#endif /* __ardour_tracker_tracker_grid_h_ */
