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

#ifndef __ardour_tracker_midi_track_toolbar_h_
#define __ardour_tracker_midi_track_toolbar_h_

#include <gtkmm/box.h>
#include <gtkmm/separator.h>

#include "widgets/ardour_button.h"
#include "ardour/processor.h"
#include "ardour/pannable.h"
#include "ardour/midi_playlist.h"

#include "midi_track_pattern.h"
#include "tracker_types.h"

class TrackerEditor;
class TrackerGrid;

struct ProcessorAutomationNode {
	ProcessorAutomationNode (Evoral::Parameter w, Gtk::CheckMenuItem* mitem)
		: what (w), menu_item (mitem), column(0) {}
	// TODO: do you really need this?
	~ProcessorAutomationNode ();

	Evoral::Parameter                         what;
	Gtk::CheckMenuItem*                       menu_item;
	// corresponding column index. If set to 0 then undetermined yet
	size_t                                    column;
};

struct ProcessorAutomationInfo {
	ProcessorAutomationInfo (boost::shared_ptr<ARDOUR::Processor> i)
		: processor (i), menu (0) {}
	// TODO: do you really need this?
	~ProcessorAutomationInfo ();

	boost::shared_ptr<ARDOUR::Processor>  processor;
	Gtk::Menu*                            menu;
	std::vector<ProcessorAutomationNode*> columns; // TODO: why is it called columns?
};

class MidiTrackToolbar : public Gtk::HBox
{
public:
	MidiTrackToolbar (TrackerEditor& mte,
	                  MidiTrackPattern& mtp,
	                  size_t mti);
	~MidiTrackToolbar ();

	typedef std::map<Evoral::Parameter, Gtk::CheckMenuItem*> ParameterMenuMap;

	void setup ();

	/**
	 * Build the toolbar layout
	 */
	void setup_layout ();
	void setup_tooltips ();

	/**
	 * Triggered upon pressing the various buttons.
	 */
	bool visible_note_press (GdkEventButton*);
	bool visible_channel_press (GdkEventButton*);
	bool visible_velocity_press (GdkEventButton*);
	bool visible_delay_press (GdkEventButton*);
	void automation_click ();
	bool remove_note_column_press (GdkEventButton* ev);
	bool add_note_column_press (GdkEventButton* ev);

	/**
	 * Helpers for building automation menu.
	 */
	void build_automation_action_menu ();
	void build_controller_menu ();
	void setup_processor_menu_and_curves ();
	void add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor>);
	void processor_menu_item_toggled (ProcessorAutomationInfo*, ProcessorAutomationNode*);
	void add_channel_command_menu_item (Gtk::Menu_Helpers::MenuList& items, const std::string& label, ARDOUR::AutomationType auto_type, uint8_t cmd);
	void add_single_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name);
	void add_multi_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items, int ctl, const std::string& name);
	ProcessorAutomationNode* find_processor_automation_node (boost::shared_ptr<ARDOUR::Processor> processor, Evoral::Parameter what);
	Gtk::CheckMenuItem* automation_child_menu_item (const Evoral::Parameter& param);

	/**
	 * Helpers to update automations
	 */
	void show_all_automation ();
	void show_existing_automation ();
	void hide_all_automation ();
	// Show/hide gain, mute and pan
	void show_all_main_automations ();
	void show_existing_main_automations ();
	void hide_main_automations ();
	// Show/hide midi automations
	void show_existing_midi_automations ();
	void hide_midi_automations ();
	// Show/hide processor automations
	void show_all_processor_automations ();
	void show_existing_processor_automations ();
	void hide_processor_automations ();

	/**
	 * Helpers to update buttons status display
	 */
	void update_visible_note_button();
	void update_visible_channel_button();
	void update_visible_velocity_button();
	void update_visible_delay_button();
	void update_automation_button();
	void update_remove_note_column_button ();
	void update_add_note_column_button ();

	TrackerEditor& tracker_editor;
	boost::shared_ptr<ARDOUR::MidiTrack> midi_track;
	MidiTrackPattern& midi_track_pattern;
	size_t midi_track_index;    // Corresponding index in multi-track
	TrackerGrid& grid;

	ArdourWidgets::ArdourButton  visible_note_button;
	bool                         visible_note;
	ArdourWidgets::ArdourButton  visible_channel_button;
	bool                         visible_channel;
	ArdourWidgets::ArdourButton  visible_velocity_button;
	bool                         visible_velocity;
	ArdourWidgets::ArdourButton  visible_delay_button;
	bool                         visible_delay;
	ArdourWidgets::ArdourButton  automation_button;
	Gtk::Menu                    subplugin_menu;
	Gtk::Menu*                   automation_action_menu;
	Gtk::Menu*                   controller_menu;
	Gtk::VSeparator              rm_add_note_column_separator;
	ArdourWidgets::ArdourButton  remove_note_column_button;
	ArdourWidgets::ArdourButton  add_note_column_button;

	/**
	 * Information about all automatable processor parameters that apply to
	 * this route (midi track). The Amp processor is not included in this list.
	 */
	std::list<ProcessorAutomationInfo*> processor_automation;

	/** parameter -> menu item map for the plugin automation menu */
	ParameterMenuMap _subplugin_menu_map;
	/** parameter -> menu item map for the channel command items */
	ParameterMenuMap _channel_command_menu_map;
	/** parameter -> menu item map for the controller menu */
	ParameterMenuMap _controller_menu_map;

	Gtk::CheckMenuItem* gain_automation_item;
	Gtk::CheckMenuItem* trim_automation_item;
	Gtk::CheckMenuItem* mute_automation_item;
	Gtk::CheckMenuItem* pan_automation_item;
};

#endif /* __ardour_tracker_midi_track_toolbar_h_ */
