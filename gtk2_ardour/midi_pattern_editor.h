/*
    Copyright (C) 2015-2016 Nil Geisweiller

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

#ifndef __ardour_gtk2_midi_editor_h_
#define __ardour_gtk2_midi_editor_h_

#include <boost/bimap/bimap.hpp>

#include <gtkmm/separator.h>
#include <gtkmm/treeview.h>
#include <gtkmm/table.h>
#include <gtkmm/box.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>

#include "gtkmm2ext/bindings.h"

#include "evoral/types.hpp"

#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "ardour_window.h"
#include "editing.h"
#include "midi_time_axis.h"

#include "note_pattern.h"
#include "track_automation_pattern.h"
#include "region_automation_pattern.h"

namespace Evoral {
	template<typename Time> class Note;
};

namespace MIDI {
namespace Name {
class MasterDeviceNames;
class CustomDeviceMode;
}
}

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
	class Route;
	class Processor;
};

class AutomationTimeAxisView;

// Maximum number of note and automation tracks. Temporary limit before a
// dedicated widget is created to replace Gtk::TreeModel::ColumnRecord

// Maximum number of note columns in the midi pattern editor
#define MAX_NUMBER_OF_NOTE_TRACKS 64

// Maximum number of automation columns in the midi pattern editor
#define MAX_NUMBER_OF_AUTOMATION_TRACKS 64

// Test if element is in container
template<typename C>
bool is_in(const typename C::value_type& element, const C& container)
{
	return container.find(element) != container.end();
}

// Test if key is in map
template<typename M>
bool is_key_in(const typename M::key_type& key, const M& map)
{
	return map.find(key) != map.end();
}

class MidiPatternEditor : public ArdourWindow
{
  public:
	typedef Evoral::Note<Temporal::Beats> NoteType;

	MidiPatternEditor(ARDOUR::Session*, MidiTimeAxisView*, boost::shared_ptr<ARDOUR::Route>, boost::shared_ptr<ARDOUR::MidiRegion>, boost::shared_ptr<ARDOUR::MidiTrack>);
	~MidiPatternEditor();

  private:

	///////////////////
	// Automation	 //
	///////////////////

	struct ProcessorAutomationNode {
		Evoral::Parameter                         what;
		Gtk::CheckMenuItem*                       menu_item;
		// corresponding column index. If set to 0 then undetermined yet
		size_t                                    column;
		MidiPatternEditor&                        parent;

		ProcessorAutomationNode (Evoral::Parameter w, Gtk::CheckMenuItem* mitem, MidiPatternEditor& p)
		    : what (w), menu_item (mitem), column(0), parent (p) {}

	    ~ProcessorAutomationNode ();
	};

	struct ProcessorAutomationInfo {
	    boost::shared_ptr<ARDOUR::Processor>  processor;
	    bool                                  valid;
	    Gtk::Menu*                            menu;
	    std::vector<ProcessorAutomationNode*> columns;

	    ProcessorAutomationInfo (boost::shared_ptr<ARDOUR::Processor> i)
		    : processor (i), valid (true), menu (0) {}

	    ~ProcessorAutomationInfo ();
	};

	/** Information about all automatable processor parameters that apply to
	 *  this route.  The Amp processor is not included in this list.
	 */
	std::list<ProcessorAutomationInfo*> processor_automation;

	// Column index of the first automation
	size_t automation_col_offset;

	// List of column indices currently unassigned to an automation
	std::set<size_t> available_automation_columns;

	// Map column index to automation parameter and vice versa
	typedef boost::bimaps::bimap<size_t, Evoral::Parameter> ColParamBimap;
	ColParamBimap col2param;

	// Keep track of all visible automation columns
	std::set<size_t> visible_automation_columns;

	// Map Parameter to AutomationControl
	typedef std::map<Evoral::Parameter, boost::shared_ptr<ARDOUR::AutomationControl> > Parameter2AutomationControl;
	Parameter2AutomationControl param2actrl;

	// Map column index to automation track index and vice versa
	typedef boost::bimaps::bimap<size_t, size_t> ColAutoTrackBimap;
	ColAutoTrackBimap col2autotrack;

	ArdourWidgets::ArdourButton  automation_button;
	Gtk::Menu                    subplugin_menu;
	Gtk::Menu*                   automation_action_menu;
	Gtk::Menu*                   controller_menu;

	typedef std::map<Evoral::Parameter, Gtk::CheckMenuItem*> ParameterMenuMap;
	/** parameter -> menu item map for the plugin automation menu */
	ParameterMenuMap _subplugin_menu_map;
	/** parameter -> menu item map for the channel command items */
	ParameterMenuMap _channel_command_menu_map;
	/** parameter -> menu item map for the controller menu */
	ParameterMenuMap _controller_menu_map;

	/** Set of pan parameter types */
	std::set<ARDOUR::AutomationType> _pan_param_types;

	size_t gain_column;
	size_t trim_column; // TODO: support audio tracks
	size_t mute_column;
	std::vector<size_t> pan_columns;

	Gtk::CheckMenuItem* gain_automation_item;
	Gtk::CheckMenuItem* trim_automation_item;
	Gtk::CheckMenuItem* mute_automation_item;
	Gtk::CheckMenuItem* pan_automation_item;

	ProcessorAutomationNode* find_processor_automation_node (boost::shared_ptr<ARDOUR::Processor> processor, Evoral::Parameter what);

	Gtk::CheckMenuItem* automation_child_menu_item (const Evoral::Parameter& param);

	bool is_pan_type (const Evoral::Parameter& param) const;
	// Assign an automation parameter to a column and return the corresponding
	// column index
	size_t select_available_automation_column ();
	size_t add_main_automation_column (const Evoral::Parameter& param);
	size_t add_midi_automation_column (const Evoral::Parameter& param);
	void add_processor_automation_column (boost::shared_ptr<ARDOUR::Processor> processor, const Evoral::Parameter& what);

	void build_param2actrl ();
	void build_pattern ();
	void update_automation_patterns ();
	void connect (const Evoral::Parameter&);

	virtual void show_all_automation ();
	bool has_pan_automation() const;
	virtual void show_existing_automation ();
	virtual void hide_all_automation ();

	void setup_processor_menu_and_curves ();
	void add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor>);
	void processor_menu_item_toggled (MidiPatternEditor::ProcessorAutomationInfo*, MidiPatternEditor::ProcessorAutomationNode*);
	void build_automation_action_menu ();
	void add_channel_command_menu_item (Gtk::Menu_Helpers::MenuList& items, const std::string& label, ARDOUR::AutomationType auto_type, uint8_t cmd);
	void change_all_channel_tracks_visibility (bool yn, Evoral::Parameter param);
	void update_automation_column_visibility (const Evoral::Parameter& param);
	void build_controller_menu ();
	boost::shared_ptr<MIDI::Name::MasterDeviceNames> get_device_names();
	void add_single_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name);
	void add_multi_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name);

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

	// Show/hide gain, mute and pan
	void show_all_main_automations ();
	void show_existing_main_automations ();
	void hide_main_automations ();

	// Show midi automations
	void show_existing_midi_automations ();
	void hide_midi_automations ();

	// Show processor automations
	void show_all_processor_automations ();
	void show_existing_processor_automations ();
	void hide_processor_automations ();

	////////////////////////////
	// Other (to sort out)	  //
	////////////////////////////

	typedef boost::shared_ptr<NoteType> NoteTypePtr;

	struct MidiPatternModelColumns : public Gtk::TreeModel::ColumnRecord {
		MidiPatternModelColumns()
		{
			// The background color differs when the row is on beats and
			// bars. This is to keep track of it.
			add (_background_color);
			add (time);
			for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS; i++) {
				add (note_name[i]);
				add (_note_foreground_color[i]);
				add (channel[i]);
				add (_channel_foreground_color[i]);
				add (velocity[i]);
				add (_velocity_foreground_color[i]);
				add (delay[i]);
				add (_delay_foreground_color[i]);
				add (_on_note[i]);		// We keep that around to play and edit
				add (_off_note[i]);		// We keep that around to play and edit
			}
			for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS; i++) {
				add (automation[i]);
				add (_automation[i]);
				add (_automation_foreground_color[i]);
				add (automation_delay[i]);
				add (_automation_delay_foreground_color[i]);
			}
		};
		Gtk::TreeModelColumn<std::string> _background_color;
		Gtk::TreeModelColumn<std::string> time;
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
		NOTE_COLNUM=1,			// from 1 cause the first column is for time
		CHANNEL_COLNUM,
		VELOCITY_COLNUM,
		DELAY_COLNUM
	};

	static const std::string note_off_str;

	// If the resolution isn't fine enough and multiple notes do not fit in the
	// same row, then this string is printed.
	static const std::string undefined_str;

	MidiTimeAxisView* midi_time_axis_view;
	boost::shared_ptr<ARDOUR::Route> route;

	MidiPatternModelColumns      columns;
	Glib::RefPtr<Gtk::ListStore> model;
	uint32_t                     nrows;
	Gtk::TreeView                view;
	Gtk::ScrolledWindow          scroller;
	Gtk::TreeModel::Path         edit_path;
	int                          edit_tracknum;
	int                          edit_colnum;
	Gtk::CellEditable*           editing_editable;
	Gtk::Table                   buttons;
	Gtk::HBox                    toolbar;
	Gtk::VBox                    vbox;
	Gtkmm2ext::ActionMap         myactions;

	// TODO: put this toolbar in its own class
	ArdourWidgets::ArdourDropdown beats_per_row_selector;
	std::vector<std::string>     beats_per_row_strings;
	uint8_t                      rows_per_beat;
	ArdourWidgets::ArdourButton  visible_note_button;
	ArdourWidgets::ArdourButton  visible_channel_button;
	ArdourWidgets::ArdourButton  visible_velocity_button;
	ArdourWidgets::ArdourButton  visible_delay_button;
	bool                         visible_note;
	bool                         visible_channel;
	bool                         visible_velocity;
	bool                         visible_delay;
	Gtk::VSeparator              rm_add_note_column_separator;
	ArdourWidgets::ArdourButton  remove_note_column_button;
	ArdourWidgets::ArdourButton  add_note_column_button;
	Gtk::VSeparator              step_edit_separator;
	ArdourWidgets::ArdourButton  step_edit_button;
	bool                         step_edit;
	Gtk::VSeparator              octave_separator;
	Gtk::Label                   octave_label;
	Gtk::Adjustment              octave_adjustment;
	Gtk::SpinButton              octave_spinner;
	Gtk::VSeparator              channel_separator;
	Gtk::Label                   channel_label;
	Gtk::Adjustment              channel_adjustment;
	Gtk::SpinButton              channel_spinner;
	Gtk::VSeparator              velocity_separator;
	Gtk::Label                   velocity_label;
	Gtk::Adjustment              velocity_adjustment;
	Gtk::SpinButton              velocity_spinner;
	Gtk::VSeparator              delay_separator;
	Gtk::Label                   delay_label;
	Gtk::Adjustment              delay_adjustment;
	Gtk::SpinButton              delay_spinner;	
	Gtk::VSeparator              place_separator;
	Gtk::Label                   place_label;
	Gtk::Adjustment              place_adjustment;
	Gtk::SpinButton              place_spinner;
	Gtk::VSeparator              steps_separator;
	Gtk::Label                   steps_label;
	Gtk::Adjustment              steps_adjustment;
	Gtk::SpinButton              steps_spinner;

	boost::shared_ptr<ARDOUR::MidiRegion> region;
	boost::shared_ptr<ARDOUR::MidiTrack>  track;
	boost::shared_ptr<ARDOUR::MidiModel>  midi_model;

	NotePattern* np;
	TrackAutomationPattern* tap;
	RegionAutomationPattern* rap;

	/** connection used to connect to model's ContentsChanged signal */
	PBD::ScopedConnectionList content_connections;

	void build_beats_per_row_menu ();

	void register_actions ();

	////////////////////////
	// Display Pattern    //
	////////////////////////

	void resize_width();        // Resize to keep the width to the minimum
	bool visible_note_press (GdkEventButton*);
	bool visible_channel_press (GdkEventButton*);
	bool visible_velocity_press (GdkEventButton*);
	bool visible_delay_press (GdkEventButton*);
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
	void automation_click ();
	void update_remove_note_column_button ();
	bool remove_note_column_press (GdkEventButton* ev);
	bool add_note_column_press (GdkEventButton* ev);
	bool step_edit_press (GdkEventButton* ev);

	void setup_tooltips ();
	void setup_toolbar ();
	void setup_time_column ();
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
	void move_cursor (int steps);

	// Move a path by s steps, wrapping around so that is remains [0, nrows).
	void wrap_around_move (Gtk::TreeModel::Path& path, int s) const;

	// Calculate the midi note pitch given the octave and the number of
	// semitones within this octave
	static uint8_t pitch (uint8_t semitones, int octave);

	bool step_editing_note_key_press (GdkEventKey*);
	bool step_editing_note_channel_key_press (GdkEventKey*);
	bool step_editing_note_velocity_key_press (GdkEventKey*);
	bool step_editing_note_delay_key_press (GdkEventKey*);
	bool step_editing_automation_key_press (GdkEventKey*);
	bool step_editing_automation_delay_key_press (GdkEventKey*);

	bool key_press (GdkEventKey*);
	bool key_release (GdkEventKey*);
	bool button_event (GdkEventButton*);
	bool scroll_event (GdkEventScroll*);
	void setup_pattern ();
	void setup_scroller ();
	void redisplay_model ();

	uint32_t get_row_index (const std::string& path);
	uint32_t get_row_index (const Gtk::TreeModel::Path& path);

	// Get note from path and edit_column
	NoteTypePtr get_on_note (int row_idx);
	NoteTypePtr get_on_note (const std::string& path);
	NoteTypePtr get_on_note (const Gtk::TreeModel::Path& path);
	NoteTypePtr get_off_note (int row_idx);
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

	// Get the pitch of a note given its textual description. If the octave
	// number is missing then the default one is used.
	uint8_t parse_pitch (std::string text) const;

	// Midi note callbacks
	void note_edited (const std::string& path, const std::string& text);
	void set_on_note (uint8_t pitch, int irow, int tracknum);
	void set_off_note (int irow, int tracknum);
	void delete_note (int irow, int tracknum);
	void note_channel_edited (const std::string& path, const std::string& text);
	void note_velocity_edited (const std::string& path, const std::string& text);
	void note_delay_edited (const std::string& path, const std::string& text);

	// Automation callbacks
	void automation_edited (const std::string& path, const std::string& text);
	void automation_delay_edited (const std::string& path, const std::string& text);

	void register_automation_undo (boost::shared_ptr<ARDOUR::AutomationList> alist, const std::string& opname, XMLNode& before, XMLNode& after);
	void apply_command (ARDOUR::MidiModel::NoteDiffCommand* cmd);

	/////////////////////////
	// Other (sort out)    //
	/////////////////////////

	bool is_midi_track () const;
	boost::shared_ptr<ARDOUR::MidiTrack> midi_track() const;

	// Beats per row corresponds to a SnapType. I could have user an integer
	// directly but I prefer to use the SnapType to be more consistent with the
	// main editor.
	void set_beats_per_row_to (Editing::SnapType);
	void beats_per_row_selection_done (Editing::SnapType);
	Glib::RefPtr<Gtk::RadioAction> beats_per_row_action (Editing::SnapType);
	void beats_per_row_chosen (Editing::SnapType);

	/**
	 * Clamp x to be within [l, u], that is return max(l, min(u, x))
	 */
	template<typename Num>
	Num clamp(Num x, Num l, Num u)
	{
		return std::max(l, std::min(u, x));
	}

	// Make it up for the lack of C++11 support
	template<typename T> static std::string to_string(const T& v)
	{
		std::stringstream ss;
		ss << v;
		return ss.str();
	}
};

#endif /* __ardour_gtk2_midi_pattern_editor_h_ */
