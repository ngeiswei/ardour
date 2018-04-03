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

// Maximum number of note and automation tracks. Temporary limit before a
// dedicated widget is created to replace Gtk::TreeModel::ColumnRecord

// Maximum number of midi tracks in case of multi-track support
#define MAX_NUMBER_OF_MIDI_TRACKS 4

// Maximum number of note tracks (note, channel, vel, del) per midi track
#define MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK 16

// Total maximum number of note tracks
#define MAX_NUMBER_OF_NOTE_TRACKS MAX_NUMBER_OF_MIDI_TRACKS*MAX_NUMBER_OF_NOTE_TRACKS_PER_MIDI_TRACK

// Maximum number of automation columns per midi track
#define MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK 16

// Total maximum number of automation columns
#define MAX_NUMBER_OF_AUTOMATION_TRACKS MAX_NUMBER_OF_MIDI_TRACKS*MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_MIDI_TRACK

class TrackerEditor;

class TrackerGrid : public Gtk::TreeView
{
public:
	TrackerGrid(TrackerEditor& te);
	~TrackerGrid();

	typedef Evoral::Note<Temporal::Beats> NoteType;
	typedef boost::shared_ptr<NoteType> NoteTypePtr;

	struct TrackerGridModelColumns : public Gtk::TreeModel::ColumnRecord {
		TrackerGridModelColumns();
		Gtk::TreeModelColumn<std::string> _background_color;
		Gtk::TreeModelColumn<std::string> time;
		Gtk::TreeModelColumn<std::string> midi_track_name[MAX_NUMBER_OF_MIDI_TRACKS];
		Gtk::TreeModelColumn<std::string> note_name[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _note_foreground_color[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> channel[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _channel_foreground_color[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> velocity[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _velocity_foreground_color[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> delay[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> _delay_foreground_color[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<NoteTypePtr> _on_note[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<NoteTypePtr> _off_note[MAX_NUMBER_OF_NOTE_TRACKS];
		Gtk::TreeModelColumn<std::string> automation[MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<ARDOUR::AutomationList::iterator> _automation[MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> _automation_foreground_color[MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> automation_delay[MAX_NUMBER_OF_AUTOMATION_TRACKS];
		Gtk::TreeModelColumn<std::string> _automation_delay_foreground_color[MAX_NUMBER_OF_AUTOMATION_TRACKS];
	};

	enum midi_note_columns {
		// TODO: support midi-tracks
		NOTE_COLNUM=2,			// from 1 cause the first column is for time + 1 for temporary first midi track
		CHANNEL_COLNUM,
		VELOCITY_COLNUM,
		DELAY_COLNUM
	};

	// Assign an automation parameter to a column and return the corresponding
	// column index
	size_t select_available_automation_column ();
	size_t add_main_automation_column (const Evoral::Parameter& param);
	size_t add_midi_automation_column (const Evoral::Parameter& param);
	void add_processor_automation_column (boost::shared_ptr<ARDOUR::Processor> processor,
	                                      const Evoral::Parameter& what);

	void change_all_channel_tracks_visibility (bool yn, Evoral::Parameter param);
	void update_automation_column_visibility (const Evoral::Parameter& param);

	// Return if the automation column associated to this parameter is currently visible
	bool is_automation_visible(const Evoral::Parameter& param) const;

	// Return true if the gain column is visible
	bool is_gain_visible () const;
	bool is_mute_visible () const;
	bool is_pan_visible () const;
	void update_gain_column_visibility ();
	void update_trim_column_visibility ();
	void update_mute_column_visibility ();
	void update_pan_columns_visibility ();

	////////////////////////
	// Display Pattern    //
	////////////////////////

	void redisplay_visible_note ();
	int note_colnum (int tracknum);
	void redisplay_visible_channel ();
	int note_channel_colnum (int tracknum);
	void redisplay_visible_velocity ();
	int note_velocity_colnum (int tracknum);
	void redisplay_visible_delay ();
	int note_delay_colnum (int tracknum);
	void redisplay_visible_automation ();
	int automation_colnum (int tracknum);
	void redisplay_visible_automation_delay ();
	int automation_delay_colnum (int tracknum);

	void setup ();
	void redisplay_model ();    // TODO rename

	TrackerEditor& tracker_editor;

	// Map column index to automation parameter and vice versa
	typedef boost::bimaps::bimap<size_t, Evoral::Parameter> ColParamBimap;
	ColParamBimap col2param;

	// Keep track of all visible automation columns
	std::set<size_t> visible_automation_columns;

	// TODO have a sequence
	MidiTrackPattern* mtp;
	boost::shared_ptr<ARDOUR::Route> route;

	static const std::string note_off_str;

	// If the resolution isn't fine enough and multiple notes do not fit in the
	// same row, then this string is printed.
	static const std::string undefined_str;

	TrackerGridModelColumns      columns;
	Glib::RefPtr<Gtk::ListStore> model;
	uint32_t                     nrows;
	Gtk::TreeModel::Path         edit_path;
	int                          edit_rowidx;
	int                          edit_tracknum;
	int                          edit_colnum;
	Gtk::CellEditable*           editing_editable;

private:
	void setup_time_column ();
	void setup_midi_track_column (size_t);
	void setup_note_column (size_t);
	void setup_note_channel_column (size_t);
	void setup_note_velocity_column (size_t);
	void setup_note_delay_column (size_t);
	void setup_automation_column (size_t);
	void setup_automation_delay_column (size_t);

	/////////////////////
	// Edit Pattern    //
	/////////////////////

	// Move the cursor steps rows downwards, or upwards if steps is
	// negative. Called while editing.
	void vertical_move_cursor (int steps);

	// Move a path by s steps, wrapping around so that is remains [0, nrows).
	void wrap_around_move (Gtk::TreeModel::Path& path, int s) const;

	// Move the cursor steps columns rightwards, or leftwards if steps is
	// negative.
	void horizontal_move_cursor (int steps, bool tab=false);

	bool move_cursor_key_press (GdkEventKey* ev);
	int digit_key_press (GdkEventKey* ev);
	uint8_t pitch_key (GdkEventKey* ev);

	bool step_editing_note_key_press (GdkEventKey*);
	bool step_editing_set_on_note (uint8_t pitch, int rowidx, int tracknum);
	bool step_editing_set_off_note (int rowidx, int tracknum);
	bool step_editing_delete_note (int rowidx, int tracknum);

	bool step_editing_note_channel_key_press (GdkEventKey*);
	bool step_editing_set_note_channel (int digit, int rowidx, int tracknum);
	bool step_editing_note_velocity_key_press (GdkEventKey*);
	bool step_editing_set_note_velocity (int digit, int rowidx, int tracknum);
	bool step_editing_note_delay_key_press (GdkEventKey*);
	bool step_editing_set_note_delay (int digit, int rowidx, int tracknum);
	bool step_editing_automation_key_press (GdkEventKey*);
	bool step_editing_set_automation (int digit, int rowidx, int tracknum);
	bool step_editing_automation_delay_key_press (GdkEventKey*);
	bool step_editing_set_automation_delay (int digit, int rowidx, int tracknum);

	bool key_press (GdkEventKey*);
	bool key_release (GdkEventKey*);
	bool button_event (GdkEventButton*);
	bool scroll_event (GdkEventScroll*);

	uint32_t get_row_index (const std::string& path);
	uint32_t get_row_index (const Gtk::TreeModel::Path& path);

	// Get note from path and edit_column
	NoteTypePtr get_on_note (int rowidx);
	NoteTypePtr get_on_note (const std::string& path);
	NoteTypePtr get_on_note (const Gtk::TreeModel::Path& path);
	NoteTypePtr get_off_note (int rowidx);
	NoteTypePtr get_off_note (const std::string& path);
	NoteTypePtr get_off_note (const Gtk::TreeModel::Path& path);
	NoteTypePtr get_note (const std::string& path); // on or off
	NoteTypePtr get_note (const Gtk::TreeModel::Path& path); // on or off

	void editing_note_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_note_channel_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_note_velocity_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_note_delay_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_automation_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_automation_delay_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_started (Gtk::CellEditable*, const std::string& path, int);

	void clear_editables ();
	void editing_canceled ();

	// Midi note callbacks
	uint8_t parse_pitch (const std::string& text) const;
	void note_edited (const std::string& path, const std::string& text);
	void set_on_note (uint8_t pitch, int rowidx, int tracknum);
	void set_off_note (int rowidx, int tracknum);
	void delete_note (int rowidx, int tracknum);
	void note_channel_edited (const std::string& path, const std::string& text);
	void set_note_channel (NoteTypePtr note, int ch);
	void note_velocity_edited (const std::string& path, const std::string& text);
	void set_note_velocity (NoteTypePtr note, int vel);
	void note_delay_edited (const std::string& path, const std::string& text);
	void set_note_delay (int delay, int rowidx, int tracknum);

	// Play note
	void play_note(uint8_t pitch);
	void release_note(uint8_t pitch);

	// Automation callbacks
	Evoral::Parameter get_parameter (int automation_tracknum);
	boost::shared_ptr<ARDOUR::AutomationList> get_alist (const Evoral::Parameter& param);
	AutomationPattern* get_automation_pattern (const Evoral::Parameter& param);
	void automation_edited (const std::string& path, const std::string& text);
	std::pair<double, bool> get_automation_value (int rowidx, int tracknum); // return zero if undefined!
	void set_automation (double val, int rowidx, int automation_tracknum);
	void delete_automation (int rowidx, int automation_tracknum);
	void automation_delay_edited (const std::string& path, const std::string& text);
	std::pair<int, bool> get_automation_delay (int rowidx, int tracknum); // return zero if undefined!
	void set_automation_delay (int delay, int rowidx, int automation_tracknum);

	void register_automation_undo (boost::shared_ptr<ARDOUR::AutomationList> alist, const std::string& opname, XMLNode& before, XMLNode& after);
	void apply_command (ARDOUR::MidiModel::NoteDiffCommand* cmd);

	// Map column index to automation track index and vice versa
	typedef boost::bimaps::bimap<size_t, size_t> ColAutoTrackBimap;
	ColAutoTrackBimap col2autotrack;

	size_t gain_column;
	size_t trim_column; // TODO: support audio tracks
	size_t mute_column;
	std::vector<size_t> pan_columns;

	// Column index of the first automation
	size_t automation_col_offset;

	// List of column indices currently unassigned to an automation
	std::set<size_t> available_automation_columns;
};

#endif /* __ardour_tracker_grib_h_ */
