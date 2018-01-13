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

#ifndef __ardour_gtk2_tracker_midi_track_toolbar_h_
#define __ardour_gtk2_tracker_midi_track_toolbar_h_

#include <gtkmm/box.h>
#include <gtkmm/separator.h>

#include "widgets/ardour_button.h"

class MidiTrackerEditor;

class MidiTrackToolbar : public Gtk::HBox
{
public:
	MidiTrackToolbar (MidiTrackerEditor& mte);
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
	 * Helpers to update buttons status display
	 */
	void update_visible_note_button();
	void update_visible_channel_button();
	void update_visible_velocity_button();
	void update_visible_delay_button();
	void update_automation_button();
	void update_remove_note_column_button ();

	MidiTrackerEditor& midi_tracker_editor;

	ArdourWidgets::ArdourButton  visible_note_button;
	bool                         visible_note;
	ArdourWidgets::ArdourButton  visible_channel_button;
	bool                         visible_channel;
	ArdourWidgets::ArdourButton  visible_velocity_button;
	bool                         visible_velocity;
	ArdourWidgets::ArdourButton  visible_delay_button;
	bool                         visible_delay;
	ArdourWidgets::ArdourButton  automation_button;
	Gtk::VSeparator              rm_add_note_column_separator;
	ArdourWidgets::ArdourButton  remove_note_column_button;
	ArdourWidgets::ArdourButton  add_note_column_button;
};

#endif /* __ardour_gtk2_tracker_midi_track_toolbar_h_ */
