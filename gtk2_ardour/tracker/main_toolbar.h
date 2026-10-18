/*
 * Copyright (C) 2017-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_main_toolbar_h_
#define __ardour_tracker_main_toolbar_h_

#include <ytkmm/accelmap.h>
#include <ytkmm/adjustment.h>
#include <ytkmm/box.h>
#include <ytkmm/menu.h>
#include <ytkmm/menu_elems.h>
#include <ytkmm/separator.h>
#include <ytkmm/spinbutton.h>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "editing.h"

namespace Tracker {

class TrackerEditor;

class MainToolbar : public Gtk::VBox
{
public:
	explicit MainToolbar (TrackerEditor& te);
	~MainToolbar ();
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
	void beats_per_row_selection_done (Editing::GridType st);
	Glib::RefPtr<Gtk::RadioAction> beats_per_row_action (Editing::GridType st);
	void beats_per_row_chosen (Editing::GridType st);
	void set_beats_per_row_to (Editing::GridType st);
	void set_rows_per_beat_to (Editing::GridType st);

	/** Toggle the mode buttons.  Used by both the buttons and the keyboard shortcuts. */
	void toggle_step_edit ();
	void toggle_chord_mode ();
	void toggle_overwrite_default ();
	void toggle_overwrite_existing ();
	void toggle_overwrite_star ();
	void toggle_sync_playhead ();
	void toggle_jump ();
	void toggle_wrap ();
	void toggle_hex ();

	/** Adjust the spinner values and the rows-per-beat setting. */
	void step_size_up ();
	void step_size_down ();
	void octave_up ();
	void octave_down ();
	void channel_up ();
	void channel_down ();
	void velocity_up ();
	void velocity_down ();
	void delay_up ();
	void delay_down ();
	void precision_up ();
	void precision_down ();
	void position_up ();
	void position_down ();
	void rows_per_beat_up ();
	void rows_per_beat_down ();

	/** Trigger the cut/copy/paste operations. */
	void cut ();
	void copy ();
	void paste ();
	void paste_overlay ();

	/** Show the cut/copy/paste popup menu at the given mouse event position. */
	void popup_clipboard_menu (GdkEventButton* ev);

	/**
	 * Register all tracker actions in the "Tracker" binding set.  This is a
	 * static method so that it can be called once at startup, before any
	 * TrackerEditor exists, which ensures that the actions are always present
	 * in the Keyboard Shortcuts window's "Tracker" tab.  The callbacks
	 * forward to the current instance (there is at most one tracker editor).
	 */
	static void register_actions ();

	/**
	 * Triggered upon step edit button press event.
	 */
	bool step_edit_press (GdkEventButton* ev);

	/**
	 * Triggered upon chord mode button press event.
	 */
	bool chord_mode_press (GdkEventButton* ev);

	/**
	 * Triggered upon overwrite default button press event.
	 */
	bool overwrite_default_press (GdkEventButton* ev);

	/**
	 * Triggered upon overwrite existing button press event.
	 */
	bool overwrite_existing_press (GdkEventButton* ev);

	/**
	 * Triggered upon overwrite star button press event.
	 */
	bool overwrite_star_press (GdkEventButton* ev);

	/**
	 * Triggered upon sync playhead button press event.
	 */
	bool sync_playhead_press (GdkEventButton* ev);

	/**
	 * Triggered upon jump to next button press event.
	 */
	bool jump_press (GdkEventButton* ev);

	/**
	 * Triggered upon wrap button press event.
	 */
	bool wrap_press (GdkEventButton* ev);

	/**
	 * Triggered upon hex button press event.
	 */
	bool hex_press (GdkEventButton* ev);

	/**
	 * Trigger when precision_spinner value is changed.
	 */
	void change_precision ();

	/**
	 * Trigger when position_spinner value is changed.
	 */
	void change_position ();

	TrackerEditor& tracker_editor;

	Gtkmm2ext::Bindings* bindings;

	static const int dflt_precision = 2;
	static const int min_precision = 1;
	static const int max_precision = 6;

	static const int dflt_position = 0;
	static const int min_position = -max_precision;
	static const int max_position = max_precision;

	// Horizontal and vertical spacing
	int spacing;

	ArdourWidgets::ArdourDropdown beats_per_row_selector;
	std::vector<std::string>     beats_per_row_strings;
	uint8_t                      rows_per_beat;

	// Buttons
	ArdourWidgets::ArdourButton  step_edit_button;
	bool                         step_edit;

	Gtk::VSeparator              chord_mode_separator;
	ArdourWidgets::ArdourButton  chord_mode_button;
	bool                         chord_mode;

	Gtk::VSeparator              overwrite_default_separator;
	ArdourWidgets::ArdourButton  overwrite_default_button;
	bool                         overwrite_default;

	Gtk::VSeparator              overwrite_existing_separator;
	ArdourWidgets::ArdourButton  overwrite_existing_button;
	bool                         overwrite_existing;

	Gtk::VSeparator              overwrite_star_separator;
	ArdourWidgets::ArdourButton  overwrite_star_button;
	bool                         overwrite_star;

	Gtk::VSeparator              sync_playhead_separator;
	ArdourWidgets::ArdourButton  sync_playhead_button;
	bool                         sync_playhead;

	Gtk::VSeparator              jump_separator;
	ArdourWidgets::ArdourButton  jump_button;
	bool                         jump;

	Gtk::VSeparator              wrap_separator;
	ArdourWidgets::ArdourButton  wrap_button;
	bool                         wrap;

	Gtk::VSeparator              hex_separator;
	ArdourWidgets::ArdourButton  hex_button;
	bool                         hex;
	int                          base; // 16 if hex else 10

	// Spinners
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

	Gtk::VSeparator              precision_separator;
	Gtk::Label                   precision_label;
	Gtk::Adjustment              precision_adjustment;
	Gtk::SpinButton              precision_spinner;
	int                          precision;

	Gtk::VSeparator              position_separator;
	Gtk::Label                   position_label;
	Gtk::Adjustment              position_adjustment;
	Gtk::SpinButton              position_spinner;
	int                          position;

	Gtk::VSeparator              steps_separator;
	Gtk::Label                   steps_label;
	Gtk::Adjustment              steps_adjustment;
	Gtk::SpinButton              steps_spinner;

private:
	/** Adjustable parameter for the increase/decrease shortcuts. */
	enum class AdjustParam {
		STEP_SIZE,
		OCTAVE,
		CHANNEL,
		VELOCITY,
		DELAY,
		PRECISION,
		POSITION,
		ROWS_PER_BEAT
	};

	/** Forward to the current instance (no-op if none exists yet). */
	static void adjust_action (AdjustParam, int direction);
	static void toggle_step_edit_action ();
	static void toggle_chord_mode_action ();
	static void toggle_overwrite_default_action ();
	static void toggle_overwrite_existing_action ();
	static void toggle_overwrite_star_action ();
	static void toggle_sync_playhead_action ();
	static void toggle_jump_action ();
	static void toggle_wrap_action ();
	static void toggle_hex_action ();
	static void cut_action ();
	static void copy_action ();
	static void paste_action ();
	static void paste_overlay_action ();
	static void beats_per_row_chosen_action (Editing::GridType st);

	/** Cycle rows_per_beat among the allowed settings. */
	void rows_per_beat_step (int direction);
	static Editing::GridType grid_type_from_rows_per_beat (uint16_t rpb);

	/** Build the cut/copy/paste popup menu. */
	void build_clipboard_menu ();

	/** The currently alive MainToolbar (there is at most one tracker editor). */
	static MainToolbar* _instance;

	/** Popup menu with the cut/copy/paste operations. */
	Gtk::Menu* clipboard_menu;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_main_toolbar_h_ */
