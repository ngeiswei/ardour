/*
    Copyright (C) 2017 Nil Geisweiller

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

#ifndef __ardour_tracker_main_toolbar_h_
#define __ardour_tracker_main_toolbar_h_

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/separator.h>
#include <gtkmm/spinbutton.h>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "editing.h"

namespace Tracker {

class TrackerEditor;

class MainToolbar : public Gtk::HBox
{
public:
	explicit MainToolbar (TrackerEditor& te);
	void setup ();

	/**
	 * Build the toolbar layout
	 */
	void setup_layout ();
	void setup_tooltips ();

	/**
	 * Build beats_per_row menu
	 */
	void setup_beats_per_row_menu ();
	void load_bindings ();
	void register_actions ();
	void beats_per_row_selection_done (Editing::GridType st);
	Glib::RefPtr<Gtk::RadioAction> beats_per_row_action (Editing::GridType st);
	void beats_per_row_chosen (Editing::GridType st);
	void set_beats_per_row_to (Editing::GridType st);

	/**
	 * Triggered upon step edit button press event.
	 */
	bool step_edit_press (GdkEventButton* ev);

	/**
	 * Trigger when position_spinner value is changed.
	 */
	void change_position ();

	TrackerEditor& tracker_editor;

	Gtkmm2ext::Bindings* bindings;

	static const int min_position = -5;
	static const int max_position = 5;

	ArdourWidgets::ArdourDropdown beats_per_row_selector;
	std::vector<std::string>     beats_per_row_strings;
	uint8_t                      rows_per_beat;
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
	Gtk::VSeparator              position_separator;
	Gtk::Label                   position_label;
	Gtk::Adjustment              position_adjustment;
	Gtk::SpinButton              position_spinner;
	Gtk::VSeparator              steps_separator;
	Gtk::Label                   steps_label;
	Gtk::Adjustment              steps_adjustment;
	Gtk::SpinButton              steps_spinner;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_main_toolbar_h_ */