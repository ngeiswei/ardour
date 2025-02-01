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

#ifndef __ardour_tracker_midi_track_toolbar_h_
#define __ardour_tracker_midi_track_toolbar_h_

#include <ytkmm/box.h>
#include <ytkmm/separator.h>

#include "widgets/ardour_button.h"

#include "ardour/midi_playlist.h"
#include "ardour/pannable.h"
#include "ardour/processor.h"

#include "midi++2/midi++/midnam_patch.h"

#include "midi_track_pattern.h"
#include "track_toolbar.h"

namespace Tracker {

class MidiTrackToolbar : public TrackToolbar
{
public:
	MidiTrackToolbar (TrackerEditor& te,
	                  MidiTrackPattern& mtp,
	                  int mti);
	~MidiTrackToolbar ();

	typedef std::map<Evoral::Parameter, Gtk::CheckMenuItem*> ParameterMenuMap;

	void setup ();

	/**
	 * Build the toolbar layout
	 */
	void setup_note ();
	void setup_channel ();
	void setup_velocity ();
	void setup_rm_add_note_col ();
	void setup_automation ();
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
	void build_automation_menu ();
	void build_show_hide_automations (Gtk::Menu_Helpers::MenuList& items);
	void build_midi_automation_menu ();
	void build_controller_menu ();
	void add_channel_command_menu_item (Gtk::Menu_Helpers::MenuList& items,
	                                    const std::string& label,
	                                    ARDOUR::AutomationType automation_type, uint8_t cmd);
	void add_single_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items,
	                                         int ctl, const std::string& name);
	void add_multi_channel_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items,
	                                        int ctl, const std::string& name);
	Gtk::CheckMenuItem* automation_child_menu_item (const IDParameter& id_param);

	/**
	 * Helpers to update automations
	 */
	void show_all_automation ();
	void show_existing_automation ();
	void hide_all_automation ();

	// Show/hide midi automations
	void show_existing_midi_automations ();
	void hide_midi_automations ();

	/**
	 * Helpers to update buttons status display
	 */
	void update_visible_note_button ();
	void update_visible_channel_button ();
	void update_visible_velocity_button ();
	void update_remove_note_column_button ();
	void update_add_note_column_button ();
	void update_automation_button ();

	void print_widths () const;
	int get_min_width () const;

	std::shared_ptr<MIDI::Name::MasterDeviceNames> get_device_names ();

	MidiTrackPtr midi_track;
	MidiTrackPattern& midi_track_pattern;

	ArdourWidgets::ArdourButton  visible_note_button;
	bool                         visible_note;
	ArdourWidgets::ArdourButton  visible_channel_button;
	bool                         visible_channel;
	ArdourWidgets::ArdourButton  visible_velocity_button;
	bool                         visible_velocity;

	Gtk::VSeparator              rm_add_note_column_separator;
	ArdourWidgets::ArdourButton  remove_note_column_button;
	ArdourWidgets::ArdourButton  add_note_column_button;

	/** parameter -> menu item map for the channel command items */
	ParameterMenuMap _channel_command_menu_map;
};

} // ~namespace tracker

#endif /* __ardour_tracker_midi_track_toolbar_h_ */
