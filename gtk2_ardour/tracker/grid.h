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

#include <boost/bimap/bimap.hpp>

#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <gdkmm/color.h>

#include "tracker_column.h"
#include "multi_track_pattern.h"
#include "midi_region_pattern.h"
#include "tracker_utils.h"

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

class TrackerEditor;

class Grid : public Gtk::TreeView
{
public:
	Grid(TrackerEditor& te);
	~Grid();

	struct GridModelColumns : public Gtk::TreeModel::ColumnRecord {
		GridModelColumns();
		// TODO: add empty columns to separate between each note track and each automations
		Gtk::TreeModelColumn<std::string> _background_color; // TODO: use Gdk::Color, maybe
		Gtk::TreeModelColumn<std::string> _family; // font family
		Gtk::TreeModelColumn<std::string> _empty; // empty column used as separator		
		Gtk::TreeModelColumn<std::string> _time_background_color;
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
		Gtk::TreeModelColumn<std::string> velocity[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _velocity_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _velocity_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> delay[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _delay_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _delay_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<NoteTypePtr> _on_note[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<NoteTypePtr> _off_note[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> automation[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _automation_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _automation_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> automation_delay[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK];
		Gtk::TreeModelColumn<std::string> _automation_delay_background_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> _automation_delay_foreground_color[MAX_NUMBER_OF_TRACKS][MAX_NUMBER_OF_AUTOMATION_TRACKS_PER_TRACK]; // TODO: use Gdk::Color
		Gtk::TreeModelColumn<std::string> right_separator[MAX_NUMBER_OF_TRACKS];
		Gtk::TreeModelColumn<std::string> track_separator[MAX_NUMBER_OF_TRACKS];
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
	bool is_trim_visible (size_t mti) const;
	bool is_mute_visible (size_t mti) const;
	bool is_pan_visible (size_t mti) const;
	void update_gain_column_visibility (size_t mti);
	void update_trim_column_visibility (size_t mti);
	void update_mute_column_visibility (size_t mti);
	void update_pan_columns_visibility (size_t mti);

	////////////////////////
	// Display Pattern    //
	////////////////////////

	std::set<size_t> phenomenal_diff() const;

	void redisplay_visible_note ();
	int mti_col_offset(size_t mti) const;
	int left_separator_colnum (size_t mti) const;
	int region_name_colnum (size_t mti) const;
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
	int right_separator_colnum (size_t mti) const;

	// Return the column of the first defined cell
	int first_defined_col ();

	void setup ();
	void read_colors ();         // Read colors from config
	void redisplay_global_columns (); // time, color, font
	void reset_off_on_note (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);

	void redisplay_left_right_separator_columns ();
	void redisplay_left_right_separator_columns (size_t mti);
	void redisplay_left_right_separator (Gtk::TreeModel::Row& row, size_t mti);
	void redisplay_track_separator (Gtk::TreeModel::Row& row, size_t mti);
	void redisplay_undefined_region_name (Gtk::TreeModel::Row& row, size_t mti);
	void redisplay_region_name (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri);
	void redisplay_undefined_notes (Gtk::TreeModel::Row& row, size_t mti); // Display undefined notes at row and mti
	void redisplay_notes (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri);
	void redisplay_undefined_automation (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);
	void redisplay_automations (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri);
	void redisplay_note_background (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);
	void redisplay_current_note_cursor (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);
	void redisplay_blank_note_foreground (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);
	void redisplay_auto_background (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);
	void redisplay_note_foreground (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi);
	void redisplay_current_auto_cursor (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);
	void redisplay_current_row_background ();
	void redisplay_current_cursor ();
	void redisplay_blank_auto_foreground (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);
	void redisplay_automation (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi, const Evoral::Parameter& param);
	void redisplay_auto_interpolation (Gtk::TreeModel::Row& row, uint32_t rowi, size_t mti, size_t mri, size_t cgi, const Evoral::Parameter& param);
	void redisplay_cell_background (Gtk::TreeModel::Row& row, size_t mti, size_t cgi);
	void redisplay_row_background (Gtk::TreeModel::Row& row, uint32_t rowi);
	void redisplay_row_background_color (Gtk::TreeModel::Row& row, uint32_t rowi, const std::string& color);
	void redisplay_row (Gtk::TreeModel::Row& row, uint32_t rowi);
	void redisplay_model ();
	void redisplay_track (size_t mti, const TrackPatternPhenomenalDiff* tp_diff = 0);
	void redisplay_inter_midi_regions (size_t mti);
	void redisplay_midi_track (size_t mti, const MidiTrackPattern& mtp, const MidiTrackPatternPhenomenalDiff* mtp_diff = 0);
	void redisplay_audio_track (size_t mti, const AudioTrackPattern& atp, const AudioTrackPatternPhenomenalDiff* atp_diff = 0);
	void redisplay_midi_region (size_t mti, size_t mri, const MidiRegionPattern& mrp, const MidiRegionPatternPhenomenalDiff* mrp_diff = 0);
	void redisplay_note_region (size_t mti, size_t mri, const NotePattern& np, const NotePatternPhenomenalDiff* np_diff = 0);
	void redisplay_note_column (size_t mti, size_t mri, size_t cgi, const NotePattern& np, const NoteColPhenomenalDiff* nc_diff = 0);
	void redisplay_note_alternate (size_t mti, size_t mri, size_t cgi, size_t rowi, const NotePattern& np);

	// To align grid header
	int get_time_width() const;
	int get_track_width(size_t mti) const;
	int get_right_separator_width(size_t mti) const;
	int get_track_separator_width() const;

	TrackerEditor& tracker_editor;

	// Map column index to automation parameter and vice versa
	typedef boost::bimaps::bimap<size_t, Evoral::Parameter> IndexParamBimap;
	std::vector<IndexParamBimap> col2params; // For each track

	// Map cgi to automation parameter and vice versa
	std::vector<IndexParamBimap> cgi2params; // For each track

	// Keep track of all visible automation columns across all midi tracks
	std::set<size_t> visible_automation_columns;

	// Represent patterns across all tracks
	MultiTrackPattern pattern;

	// Store previous pattern to optimize redisplay_model by only redisplaying
	// difference
	MultiTrackPattern prev_pattern;

	static const std::string note_off_str;

	// If the resolution isn't fine enough and multiple notes do not fit in the
	// same row, then this string is printed.
	static const std::string undefined_str;

	GridModelColumns             columns;
	Glib::RefPtr<Gtk::ListStore> model;

	// Coordonates associated to current cursor
	Gtk::TreeModel::Path         current_path;
	int                          current_rowi;
	Gtk::TreeModel::Row          current_row;
	int                          current_col;
	int                          current_mti; // multi track index
	TrackPattern*                current_mtp;
	int                          current_mri; // midi region index
	int                          current_cgi; // column group index

	enum TrackerColumn::midi_note_type current_note_type; // NOTE_SEPARATOR means inactive
	enum TrackerColumn::automation_type current_auto_type; // AUTOMATION_SEPARATOR means inactive

	// Clock position
	samplepos_t                  clock_pos;

	// Coordonates associated to edit cursor
	Gtk::TreeModel::Path         edit_path;
	int                          edit_rowi;
	// TreeModel::Row*              edit_row;
	int                          edit_col;
	int                          edit_mti;
	TrackPattern*                edit_mtp;
	int                          edit_mri;
	int                          edit_cgi;
	Gtk::CellEditable*           editing_editable;

	// Keep track of the last keyval to avoid repeating key (except cursor move)
	guint                        last_keyval;

private:
	void init_columns ();
	void setup_time_column ();
	void setup_left_separator_column (size_t mti);
	void setup_region_name_column (size_t mti);
	void setup_note_column (size_t mti, size_t cgi);
	void setup_note_channel_column (size_t mti, size_t cgi);
	void setup_note_velocity_column (size_t mti, size_t cgi);
	void setup_note_delay_column (size_t mti, size_t cgi);
	void setup_note_separator_column (size_t mti, size_t cgi);
	void setup_automation_column (size_t mti, size_t cgi);
	void setup_automation_delay_column (size_t mti, size_t cgi);
	void setup_automation_separator_column (size_t mti, size_t cgi);
	void setup_right_separator_column (size_t mti);
	void setup_track_separator_column (size_t mti);

	/////////////////////
	// Action Utils    //
	/////////////////////

	// Move a path by s steps, if wrap is true it wraps around so that it
	// remains within [0, nrows).
	//
	// If jump is true, then do no count steps over undefined cells. If jump is
	// false, then count the steps, and only move if the final cell is defined.
	void wrap_around_vertical_move (Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col, int s, bool wrap=true, bool jump=true);

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
	//
	// If jump is true, then do no count steps over undefined cells. If jump is
	// false, then count the steps, and only move if the final cell is defined.
	void vertical_move_current_cursor (int steps, bool wrap=true, bool jump=true, bool set_playhead=true);

	// Move the current cursor steps columns rightwards, or leftwards if steps
	// is negative.
	void horizontal_move_current_cursor (int steps, bool tab=false);

	bool move_current_cursor_key_press (GdkEventKey*);

	bool non_editing_key_press (GdkEventKey*);
	bool non_editing_key_release (GdkEventKey*);
	
	bool key_press (GdkEventKey*);
	bool key_release (GdkEventKey*);
	bool mouse_button_event (GdkEventButton*);
	bool scroll_event (GdkEventScroll*);

	// Return the row index of a tree model path
	uint32_t get_row_index (const std::string& path) const;
	uint32_t get_row_index (const Gtk::TreeModel::Path& path) const;

	uint32_t get_row_offset (size_t mti, size_t mri) const;
	uint32_t get_row_size (size_t mti, size_t mri) const;

	// Return the row corresponding to a given row index
	Gtk::TreeModel::Row get_row (uint32_t row_idx);

	// Return the column index of a tree view column, -1 if col doesn't exist.
	// Warning: can't be const because of a get_columns()
	int get_col_index (const Gtk::TreeViewColumn* col);

	// Play note
	void play_note(int mti, uint8_t pitch);
	void release_note(int mti, uint8_t pitch);

	// Set current cursor
	void set_current_cursor (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* col, bool set_playhead=false);

	/////////////////////
	// Editing Actions //
	/////////////////////

	// Move the cursor steps rows downwards, or upwards if steps is
	// negative. Called while editing.
	void vertical_move_edit_cursor (int steps);

	// Check whether a given column is editable
	bool is_editable (Gtk::TreeViewColumn* col) const;

	// Check if the cell is defined at all
	// Warning: can't be const because of get_col_index
	bool is_defined (const Gtk::TreeModel::Path& path, const Gtk::TreeViewColumn* col);
	bool is_region_defined (const Gtk::TreeModel::Path& path, int mti) const;
	bool is_region_defined (uint32_t rowi, int mti) const;
	bool is_automation_defined (uint32_t rowi, int mti, int cgi); // TODO: make this const

	// Return mti corresponding col, or -1 if invalid
	int get_mti(const Gtk::TreeViewColumn* col) const;

	size_t get_cgi(const Gtk::TreeViewColumn* col) const;
	TrackerColumn::midi_note_type get_note_type(const Gtk::TreeViewColumn* col) const;
	TrackerColumn::automation_type get_auto_type(const Gtk::TreeViewColumn* col) const;
	bool is_note_type(const Gtk::TreeViewColumn* col) const;

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
	bool step_editing_set_automation_value (int digit);
	bool step_editing_automation_delay_key_press (GdkEventKey*);
	bool step_editing_set_automation_delay (int digit);

	// Get note from path, mti and cgi
	NoteTypePtr get_on_note (int rowi, int mti, int cgi);
	NoteTypePtr get_on_note (const std::string& path, int mti, int cgi);
	NoteTypePtr get_on_note (const Gtk::TreeModel::Path& path, int mri, int cgi);
	NoteTypePtr get_off_note (int rowi, int mti, int cgi);
	NoteTypePtr get_off_note (const std::string& path, int mti, int cgi);
	NoteTypePtr get_off_note (const Gtk::TreeModel::Path& path, int mti, int cgi);

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
	void set_on_note (uint8_t pitch, int rowi, int mti, int mri, int cgi);
	void set_off_note (int rowi, int mti, int mri, int cgi);
	void delete_note (int rowi, int mti, int mri, int cgi);
	void note_channel_edited (const std::string& path, const std::string& text);
	void set_note_channel (int mti, int mri, NoteTypePtr note, int ch);
	void note_velocity_edited (const std::string& path, const std::string& text);
	void set_note_velocity (int mti, int mri, NoteTypePtr note, int vel);
	void note_delay_edited (const std::string& path, const std::string& text);
	void set_note_delay (int delay, int rowi, int mti, int mri, int cgi);

	// Return parameter at mti and automation cgi. Return the empty parameter if
	// undefined.
	Evoral::Parameter get_param (int mti, int auto_cgi); // TODO: make this const

	boost::shared_ptr<ARDOUR::AutomationList> get_alist (int mti, int mri, const Evoral::Parameter& param);
	void automation_edited (const std::string& path, const std::string& text);
	// TODO: can you make that const?
	std::pair<double, bool> get_automation_value (int rowi, int mti, int mri, int cgi); // return <0.0, false> if undefined
	void set_automation_value (double val, int rowi, int mti, int mri, int automation_cgi);
	void delete_automation_value (int rowi, int mti, int mri, int automation_cgi);
	void automation_delay_edited (const std::string& path, const std::string& text);
	std::pair<int, bool> get_automation_delay (int rowi, int mti, int mri, int cgi); // return (0, false) if undefined
	void set_automation_delay (int delay, int rowi, int mti, int mri, int automation_cgi);

public:
	void register_automation_undo (boost::shared_ptr<ARDOUR::AutomationList> alist, const std::string& opname, XMLNode& before, XMLNode& after);

private:
	void apply_command (size_t mti, size_t mri, ARDOUR::MidiModel::NoteDiffCommand* cmd);
	void follow_current_row (samplepos_t);

	// Map column index to automation cgi and vice versa
	typedef boost::bimaps::bimap<size_t, size_t> IndexBimap;

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
	std::string cursor_step_edit_color;
	std::string current_row_color;
	std::string current_step_edit_row_color;

	// Keep track of phenomenal differences between prev_pattern and pattern so
	// speed up redisplay_model
	MultiTrackPatternPhenomenalDiff _phenomenal_diff;
};

} // ~namespace tracker

#endif /* __ardour_tracker_tracker_grid_h_ */
