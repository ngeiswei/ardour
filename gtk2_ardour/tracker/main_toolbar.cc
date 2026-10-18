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

#include <algorithm>

#include "pbd/error.h"
#include "pbd/i18n.h"

#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "actions.h"

#include "main_toolbar.h"
#include "tracker_editor.h"

using namespace Editing;
using namespace PBD;
using namespace Tracker;

#define COMBO_TRIANGLE_WIDTH 25

static const gchar *_beats_per_row_strings[] = {
	N_("1 Row/Bar"),
	N_("1 Row/Beat"),
	N_("2 Rows/Beat"),
	N_("4 Rows/Beat"),
	N_("8 Rows/Beat"),
	N_("16 Rows/Beat"),
	N_("32 Rows/Beat"),
	N_("3 Rows/Beat"),
	N_("6 Rows/Beat"),
	N_("12 Rows/Beat"),
	N_("24 Rows/Beat"),
	N_("5 Rows/Beat"),
	N_("10 Rows/Beat"),
	N_("20 Rows/Beat"),
	N_("7 Rows/Beat"),
	N_("14 Rows/Beat"),
	N_("28 Rows/Beat"),
	0
};

/** Allowed rows_per_beat values, in the order of the beats-per-row menu. */
static const uint16_t _rows_per_beat_choices[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32
};

MainToolbar* MainToolbar::_instance = nullptr;

MainToolbar::MainToolbar (TrackerEditor& te)
	: tracker_editor (te)
	, bindings (0)
	, spacing (4)
	, beats_per_row_strings (I18N (_beats_per_row_strings))
	, rows_per_beat (4)
	, step_edit (false)
	, chord_mode (false)
	, overwrite_default (true)
	, overwrite_existing (false)
	, overwrite_star (false)
	, sync_playhead (false)
	, jump (false)
	, wrap (false)
	, hex (true)
	, base (16)
	, octave_label (_("Octave"))
	, octave_adjustment (4, -1, 9, 1, 2)
	, octave_spinner (octave_adjustment)
	, channel_label (S_("Channel"))
	, channel_adjustment (1, 1, 16, 1, 4)
	, channel_spinner (channel_adjustment)
	, velocity_label (S_("Velocity"))
	, velocity_adjustment (100, 0, 127, 1, 4)
	, velocity_spinner (velocity_adjustment)
	, delay_label (_("Delay"))
	, delay_adjustment (0, 0, 0, 1, 4)
	, delay_spinner (delay_adjustment)
	, precision_label (_("Precision"))
	, precision_adjustment (dflt_precision, min_precision, max_precision, 1, 2)
	, precision_spinner (precision_adjustment)
	, precision (dflt_precision)
	, position_label (_("Position"))
	, position_adjustment (dflt_position, min_position, max_position, 1, 2)
	, position_spinner (position_adjustment)
	, position (dflt_position)
	, steps_label (_("Steps"))
	  // TODO set the boundaries to not be above the number of rows
	, steps_adjustment (1, 0, 255, 1, 4)
	, steps_spinner (steps_adjustment)
	, clipboard_menu (0)
{
	_instance = this;
}

MainToolbar::~MainToolbar ()
{
	delete clipboard_menu;
	if (_instance == this) {
		_instance = nullptr;
	}
}

void
MainToolbar::setup ()
{
	// Setup layout and actions
	setup_layout ();
	setup_tooltips ();
	setup_beats_per_row_menu ();
	load_bindings ();
	set_beats_per_row_to (GridTypeBeatDiv4);

	// Make sure that rows_per_beat is set before anything is redisplayed
	tracker_editor.grid.set_rows_per_beat (rows_per_beat, true);
}

void
MainToolbar::setup_layout ()
{
	set_spacing (spacing);

	// Top row
	Gtk::HBox* tr = Gtk::manage (new Gtk::HBox ());
	tr->set_spacing(spacing);

	// Step edit button
	step_edit_button.set_name ("step edit button");
	step_edit_button.set_text (S_("Step Edit"));
	step_edit_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MainToolbar::step_edit_press), false);
	step_edit_button.set_active_state (step_edit ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	step_edit_button.show ();
	tr->pack_start (step_edit_button, false, false);

	// Steps spinner
	steps_separator.show ();
	tr->pack_start (steps_separator, false, false);
	steps_label.show ();
	tr->pack_start (steps_label, false, false);
	steps_spinner.set_activates_default ();
	steps_spinner.show ();
	tr->pack_start (steps_spinner, false, false);

	// Octave spinner
	octave_separator.show ();
	tr->pack_start (octave_separator, false, false);
	octave_label.show ();
	tr->pack_start (octave_label, false, false);
	octave_spinner.set_activates_default ();
	octave_spinner.show ();
	tr->pack_start (octave_spinner, false, false);

	// Channel spinner
	channel_separator.show ();
	tr->pack_start (channel_separator, false, false);
	channel_label.show ();
	tr->pack_start (channel_label, false, false);
	channel_spinner.set_activates_default ();
	channel_spinner.show ();
	tr->pack_start (channel_spinner, false, false);

	// Velocity spinner
	velocity_separator.show ();
	tr->pack_start (velocity_separator, false, false);
	velocity_label.show ();
	tr->pack_start (velocity_label, false, false);
	velocity_spinner.set_activates_default ();
	velocity_spinner.show ();
	tr->pack_start (velocity_spinner, false, false);

	// Delay spinner
	delay_separator.show ();
	tr->pack_start (delay_separator, false, false);
	delay_label.show ();
	tr->pack_start (delay_label, false, false);
	delay_spinner.set_activates_default ();
	delay_spinner.show ();
	tr->pack_start (delay_spinner, false, false);

	// Precision spinner
	precision_separator.show ();
	tr->pack_start (precision_separator, false, false);
	precision_label.show ();
	tr->pack_start (precision_label, false, false);
	precision_spinner.signal_value_changed ().connect (sigc::mem_fun (*this, &MainToolbar::change_precision), false);
	precision_spinner.set_activates_default ();
	precision_spinner.show ();
	tr->pack_start (precision_spinner, false, false);

	// Position spinner
	position_separator.show ();
	tr->pack_start (position_separator, false, false);
	position_label.show ();
	tr->pack_start (position_label, false, false);
	position_spinner.signal_value_changed ().connect (sigc::mem_fun (*this, &MainToolbar::change_position), false);
	position_spinner.set_activates_default ();
	position_spinner.show ();
	tr->pack_start (position_spinner, false, false);

	tr->show ();
	pack_start (*tr, false, false);

	// Middle row
	Gtk::HBox* mr = Gtk::manage (new Gtk::HBox ());
	mr->set_spacing(spacing);

	// Add beats per row selection
	beats_per_row_selector.show ();
	mr->pack_start (beats_per_row_selector, false, false);

	// Chord mode buttom
	chord_mode_separator.show ();
	mr->pack_start (chord_mode_separator, false, false);
	chord_mode_button.set_name ("chord mode button");
	chord_mode_button.set_text (S_("Chord Mode"));
	chord_mode_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MainToolbar::chord_mode_press), false);
	chord_mode_button.set_active_state (chord_mode ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	chord_mode_button.show ();
	mr->pack_start (chord_mode_button, false, false);

	// Overwrite default button
	overwrite_default_separator.show ();
	mr->pack_start (overwrite_default_separator, false, false);
	overwrite_default_button.set_name ("overwrite default button");
	overwrite_default_button.set_text (S_("Overwrite Default"));
	overwrite_default_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MainToolbar::overwrite_default_press), false);
	overwrite_default_button.set_active_state (overwrite_default ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	overwrite_default_button.show ();
	mr->pack_start (overwrite_default_button, false, false);

	// Overwrite existing button
	overwrite_existing_separator.show ();
	mr->pack_start (overwrite_existing_separator, false, false);
	overwrite_existing_button.set_name ("overwrite existing button");
	overwrite_existing_button.set_text (S_("Overwrite Existing"));
	overwrite_existing_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MainToolbar::overwrite_existing_press), false);
	overwrite_existing_button.set_active_state (overwrite_existing ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	overwrite_existing_button.show ();
	mr->pack_start (overwrite_existing_button, false, false);

	// Overwrite star button
	overwrite_star_separator.show ();
	mr->pack_start (overwrite_star_separator, false, false);
	overwrite_star_button.set_name ("overwrite star button");
	overwrite_star_button.set_text (S_("Overwrite *"));
	overwrite_star_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MainToolbar::overwrite_star_press), false);
	overwrite_star_button.set_active_state (overwrite_star ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	overwrite_star_button.show ();
	mr->pack_start (overwrite_star_button, false, false);

	// Sync playhead
	sync_playhead_separator.show ();
	mr->pack_start (sync_playhead_separator, false, false);
	sync_playhead_button.set_name ("sync playhead button");
	sync_playhead_button.set_text (S_("Sync Playhead"));
	sync_playhead_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MainToolbar::sync_playhead_press), false);
	sync_playhead_button.set_active_state (sync_playhead ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	sync_playhead_button.show ();
	mr->pack_start (sync_playhead_button, false, false);

	// Jump to next
	jump_separator.show ();
	mr->pack_start (jump_separator, false, false);
	jump_button.set_name ("jump button");
	jump_button.set_text (S_("Jump"));
	jump_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MainToolbar::jump_press), false);
	jump_button.set_active_state (jump ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	jump_button.show ();
	mr->pack_start (jump_button, false, false);

	// Wrap vertical move
	wrap_separator.show ();
	mr->pack_start (wrap_separator, false, false);
	wrap_button.set_name ("wrap button");
	wrap_button.set_text (S_("Wrap"));
	wrap_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MainToolbar::wrap_press), false);
	wrap_button.set_active_state (wrap ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	wrap_button.show ();
	mr->pack_start (wrap_button, false, false);

	mr->show ();
	pack_start (*mr, false, false);

	show ();
}

void
MainToolbar::setup_tooltips ()
{
	set_tooltip (beats_per_row_selector, _("Beats per row"));
	step_edit_button.set_tooltip_text (_("Toggle step editing"));
	chord_mode_button.set_tooltip_text (_("Any note entered while a note is still pressed gets entered at the same row"));
	overwrite_default_button.set_tooltip_text (_("MIDI input events overwrite default channel and velocity"));
	overwrite_existing_button.set_tooltip_text (_("Input events overwrite existing channel, velocity and delay"));
	overwrite_star_button.set_tooltip_text (_("Cell containing more than one event, i.e. displayed as ***, can be overwriten. Otherwise, they are read only, and only zooming in, till there is at most one event per cell, make them writable."));
	sync_playhead_button.set_tooltip_text (_("Synchronize current row and playhead"));
	jump_button.set_tooltip_text (_("Jump to the next event on the same row or column while moving the cursor or step editing. If no such event exist, then fallback to regular movement."));
	wrap_button.set_tooltip_text (_("Wrap vertical move"));
	octave_spinner.set_tooltip_text (_("Default octave"));
	channel_spinner.set_tooltip_text (_("Default channel"));
	velocity_spinner.set_tooltip_text (_("Default velocity"));
	delay_spinner.set_tooltip_text (_("Default delay"));
	precision_spinner.set_tooltip_text (_("Maximum number of digits to display after the point of floating point numbers"));
	position_spinner.set_tooltip_text (_("Position from the numerical separator changed when step editing automation. Place value = base^(position-1)."));
	steps_spinner.set_tooltip_text (_("Step size"));
}

void
MainToolbar::setup_beats_per_row_menu ()
{
	using namespace Gtk::Menu_Helpers;

	// TODO GridTypeBar is not yet supported
	// beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBar - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBar)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeat - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeat)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv2 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv2)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv3 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv3)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv4 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv4)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv5 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv5)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv6 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv6)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv7 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv7)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv8 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv8)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv10 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv10)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv12 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv12)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv14 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv14)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv16 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv16)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv20 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv20)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv24 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv24)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv28 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv28)));
	beats_per_row_selector.add_menu_elem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv32 - (int)GridTypeBar], sigc::bind (sigc::mem_fun (*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv32)));

	set_size_request_to_display_given_text (beats_per_row_selector, beats_per_row_strings.back(), COMBO_TRIANGLE_WIDTH, 2);
}

void
MainToolbar::load_bindings ()
{
	bindings = Gtkmm2ext::Bindings::get_bindings (X_("Tracker"));
}

void
MainToolbar::register_actions ()
{
	Gtkmm2ext::Bindings* bindings = Gtkmm2ext::Bindings::get_bindings (X_("Tracker"));
	if (!bindings) {
		error << _("Tracker keybindings are missing") << endmsg;
		return;
	}

	// Beats-per-row radio actions (specific values).  These are what the
	// beats-per-row dropdown menu uses.
	Glib::RefPtr<Gtk::ActionGroup> beats_per_row_actions = ActionManager::create_action_group (bindings, X_("BeatsPerRow"));
	Gtk::RadioAction::Group beats_per_row_choice_group;

	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirtyseconds"), _("Beats Per Row to Thirty Seconds"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv32)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyeighths"), _("Beats Per Row to Twenty Eighths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv28)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyfourths"), _("Beats Per Row to Twenty Fourths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv24)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentieths"), _("Beats Per Row to Twentieths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv20)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-asixteenthbeat"), _("Beats Per Row to Sixteenths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv16)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fourteenths"), _("Beats Per Row to Fourteenths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv14)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twelfths"), _("Beats Per Row to Twelfths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv12)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-tenths"), _("Beats Per Row to Tenths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv10)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-eighths"), _("Beats Per Row to Eighths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv8)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sevenths"), _("Beats Per Row to Sevenths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv7)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixths"), _("Beats Per Row to Sixths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv6)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fifths"), _("Beats Per Row to Fifths"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv5)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-quarters"), _("Beats Per Row to Quarters"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv4)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirds"), _("Beats Per Row to Thirds"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv3)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-halves"), _("Beats Per Row to Halves"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeatDiv2)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-beat"), _("Beats Per Row to Beat"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBeat)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-bar"), _("Beats Per Row to Bar"),
	                                      (sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), Editing::GridTypeBar)));

	// Keyboard shortcuts for the tracker editor.
	Glib::RefPtr<Gtk::ActionGroup> tracker_actions = ActionManager::create_action_group (bindings, X_("Tracker"));

	// Mode toggles
	ActionManager::register_action (tracker_actions, X_("toggle-step-edit"), _("Toggle Step Edit"), sigc::ptr_fun (&MainToolbar::toggle_step_edit_action));
	ActionManager::register_action (tracker_actions, X_("toggle-chord-mode"), _("Toggle Chord Mode"), sigc::ptr_fun (&MainToolbar::toggle_chord_mode_action));
	ActionManager::register_action (tracker_actions, X_("toggle-overwrite-default"), _("Toggle Overwrite Default"), sigc::ptr_fun (&MainToolbar::toggle_overwrite_default_action));
	ActionManager::register_action (tracker_actions, X_("toggle-overwrite-existing"), _("Toggle Overwrite Existing"), sigc::ptr_fun (&MainToolbar::toggle_overwrite_existing_action));
	ActionManager::register_action (tracker_actions, X_("toggle-overwrite-star"), _("Toggle Overwrite *"), sigc::ptr_fun (&MainToolbar::toggle_overwrite_star_action));
	ActionManager::register_action (tracker_actions, X_("toggle-sync-playhead"), _("Toggle Sync Playhead"), sigc::ptr_fun (&MainToolbar::toggle_sync_playhead_action));
	ActionManager::register_action (tracker_actions, X_("toggle-jump"), _("Toggle Jump"), sigc::ptr_fun (&MainToolbar::toggle_jump_action));
	ActionManager::register_action (tracker_actions, X_("toggle-wrap"), _("Toggle Wrap"), sigc::ptr_fun (&MainToolbar::toggle_wrap_action));
	ActionManager::register_action (tracker_actions, X_("toggle-hex"), _("Toggle Hex"), sigc::ptr_fun (&MainToolbar::toggle_hex_action));

	// Value adjustments
	ActionManager::register_action (tracker_actions, X_("increase-step-size"), _("Increase Step Size"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::STEP_SIZE, 1));
	ActionManager::register_action (tracker_actions, X_("decrease-step-size"), _("Decrease Step Size"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::STEP_SIZE, -1));
	ActionManager::register_action (tracker_actions, X_("increase-octave"), _("Increase Octave"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::OCTAVE, 1));
	ActionManager::register_action (tracker_actions, X_("decrease-octave"), _("Decrease Octave"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::OCTAVE, -1));
	ActionManager::register_action (tracker_actions, X_("increase-channel"), _("Increase Channel"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::CHANNEL, 1));
	ActionManager::register_action (tracker_actions, X_("decrease-channel"), _("Decrease Channel"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::CHANNEL, -1));
	ActionManager::register_action (tracker_actions, X_("increase-velocity"), _("Increase Velocity"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::VELOCITY, 1));
	ActionManager::register_action (tracker_actions, X_("decrease-velocity"), _("Decrease Velocity"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::VELOCITY, -1));
	ActionManager::register_action (tracker_actions, X_("increase-delay"), _("Increase Delay"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::DELAY, 1));
	ActionManager::register_action (tracker_actions, X_("decrease-delay"), _("Decrease Delay"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::DELAY, -1));
	ActionManager::register_action (tracker_actions, X_("increase-precision"), _("Increase Precision"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::PRECISION, 1));
	ActionManager::register_action (tracker_actions, X_("decrease-precision"), _("Decrease Precision"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::PRECISION, -1));
	ActionManager::register_action (tracker_actions, X_("increase-position"), _("Increase Position"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::POSITION, 1));
	ActionManager::register_action (tracker_actions, X_("decrease-position"), _("Decrease Position"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::POSITION, -1));
	ActionManager::register_action (tracker_actions, X_("increase-rows-per-beat"), _("Increase Rows per Beat"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::ROWS_PER_BEAT, 1));
	ActionManager::register_action (tracker_actions, X_("decrease-rows-per-beat"), _("Decrease Rows per Beat"), sigc::bind (sigc::ptr_fun (&MainToolbar::adjust_action), AdjustParam::ROWS_PER_BEAT, -1));

	// Cut/copy/paste
	ActionManager::register_action (tracker_actions, X_("cut"), _("Cut"), sigc::ptr_fun (&MainToolbar::cut_action));
	ActionManager::register_action (tracker_actions, X_("copy"), _("Copy"), sigc::ptr_fun (&MainToolbar::copy_action));
	ActionManager::register_action (tracker_actions, X_("paste"), _("Paste"), sigc::ptr_fun (&MainToolbar::paste_action));
	ActionManager::register_action (tracker_actions, X_("paste-overlay"), _("Paste Overlay"), sigc::ptr_fun (&MainToolbar::paste_overlay_action));
}

void
MainToolbar::beats_per_row_selection_done (GridType st)
{
	Glib::RefPtr<Gtk::RadioAction> ract = beats_per_row_action (st);
	if (ract) {
		ract->set_active ();
	}
}

Glib::RefPtr<Gtk::RadioAction>
MainToolbar::beats_per_row_action (GridType st)
{
	const char* action = nullptr;
	Glib::RefPtr<Gtk::Action> act;

	switch (st) {
	case Editing::GridTypeBeatDiv32:
		action = "beats-per-row-thirtyseconds";
		break;
	case Editing::GridTypeBeatDiv28:
		action = "beats-per-row-twentyeighths";
		break;
	case Editing::GridTypeBeatDiv24:
		action = "beats-per-row-twentyfourths";
		break;
	case Editing::GridTypeBeatDiv20:
		action = "beats-per-row-twentieths";
		break;
	case Editing::GridTypeBeatDiv16:
		action = "beats-per-row-asixteenthbeat";
		break;
	case Editing::GridTypeBeatDiv14:
		action = "beats-per-row-fourteenths";
		break;
	case Editing::GridTypeBeatDiv12:
		action = "beats-per-row-twelfths";
		break;
	case Editing::GridTypeBeatDiv10:
		action = "beats-per-row-tenths";
		break;
	case Editing::GridTypeBeatDiv8:
		action = "beats-per-row-eighths";
		break;
	case Editing::GridTypeBeatDiv7:
		action = "beats-per-row-sevenths";
		break;
	case Editing::GridTypeBeatDiv6:
		action = "beats-per-row-sixths";
		break;
	case Editing::GridTypeBeatDiv5:
		action = "beats-per-row-fifths";
		break;
	case Editing::GridTypeBeatDiv4:
		action = "beats-per-row-quarters";
		break;
	case Editing::GridTypeBeatDiv3:
		action = "beats-per-row-thirds";
		break;
	case Editing::GridTypeBeatDiv2:
		action = "beats-per-row-halves";
		break;
	case Editing::GridTypeBeat:
		action = "beats-per-row-beat";
		break;
	case Editing::GridTypeBar:
		action = "beats-per-row-bar";
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible beats-per-row", (int) st) << endmsg;
		abort (); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("BeatsPerRow"), action);

	if (act) {
		Glib::RefPtr<Gtk::RadioAction> ract = Glib::RefPtr<Gtk::RadioAction>::cast_dynamic (act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "Tracker::MainToolbar::beats_per_row_chosen could not find action to match type.") << endmsg;
		return Glib::RefPtr<Gtk::RadioAction> ();
	}
}

void
MainToolbar::beats_per_row_chosen (GridType st)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	Glib::RefPtr<Gtk::RadioAction> ract = beats_per_row_action (st);

	if (ract && ract->get_active ()) {
		set_rows_per_beat_to (st);
	}
}

void
MainToolbar::set_beats_per_row_to (GridType st)
{
	unsigned int snap_ind = (int)st - (int)GridTypeBar;

	std::string str = beats_per_row_strings[snap_ind];

	if (str != beats_per_row_selector.get_text ()) {
		beats_per_row_selector.set_text (str);
	}

	switch (st) {
	case GridTypeBeatDiv32: rows_per_beat = 32; break;
	case GridTypeBeatDiv28: rows_per_beat = 28; break;
	case GridTypeBeatDiv24: rows_per_beat = 24; break;
	case GridTypeBeatDiv20: rows_per_beat = 20; break;
	case GridTypeBeatDiv16: rows_per_beat = 16; break;
	case GridTypeBeatDiv14: rows_per_beat = 14; break;
	case GridTypeBeatDiv12: rows_per_beat = 12; break;
	case GridTypeBeatDiv10: rows_per_beat = 10; break;
	case GridTypeBeatDiv8: rows_per_beat = 8; break;
	case GridTypeBeatDiv7: rows_per_beat = 7; break;
	case GridTypeBeatDiv6: rows_per_beat = 6; break;
	case GridTypeBeatDiv5: rows_per_beat = 5; break;
	case GridTypeBeatDiv4: rows_per_beat = 4; break;
	case GridTypeBeatDiv3: rows_per_beat = 3; break;
	case GridTypeBeatDiv2: rows_per_beat = 2; break;
	case GridTypeBeat: rows_per_beat = 1; break;
	case GridTypeBar: rows_per_beat = 0; break;
	default:
		/* relax */
		break;
	}
}

bool
MainToolbar::step_edit_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	toggle_step_edit ();
	return false;
}

bool
MainToolbar::chord_mode_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	toggle_chord_mode ();
	return false;
}

bool
MainToolbar::overwrite_default_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	toggle_overwrite_default ();
	return false;
}

bool
MainToolbar::overwrite_existing_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	toggle_overwrite_existing ();
	return false;
}

bool
MainToolbar::overwrite_star_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	toggle_overwrite_star ();
	return false;
}

bool
MainToolbar::sync_playhead_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	toggle_sync_playhead ();
	return false;
}

bool
MainToolbar::jump_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	toggle_jump ();
	return false;
}

bool
MainToolbar::wrap_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	toggle_wrap ();
	return false;
}

void
MainToolbar::change_precision ()
{
	precision = precision_spinner.get_value_as_int ();
	// TODO: send signal to TrackerEditor
	tracker_editor.grid.redisplay_grid_direct_call ();
}

void
MainToolbar::change_position ()
{
	position = position_spinner.get_value_as_int ();
	// TODO: possibly replace by signal
	if (step_edit) {
		tracker_editor.grid.unset_underline_current_step_edit_cell ();
		tracker_editor.grid.set_underline_current_step_edit_cell ();
	}
}

void
MainToolbar::toggle_step_edit ()
{
	step_edit = !step_edit;
	step_edit_button.set_active_state (step_edit ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	tracker_editor.grid.redisplay_grid_direct_call ();
	if (step_edit) {
		tracker_editor.connect_midi_event ();
		tracker_editor.grid.set_underline_current_step_edit_cell ();
	} else {
		tracker_editor.disconnect_midi_event ();
		tracker_editor.grid.unset_underline_current_step_edit_cell ();
	}
}

void
MainToolbar::toggle_chord_mode ()
{
	chord_mode = !chord_mode;
	chord_mode_button.set_active_state (chord_mode ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MainToolbar::toggle_overwrite_default ()
{
	overwrite_default = !overwrite_default;
	overwrite_default_button.set_active_state (overwrite_default ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MainToolbar::toggle_overwrite_existing ()
{
	overwrite_existing = !overwrite_existing;
	overwrite_existing_button.set_active_state (overwrite_existing ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MainToolbar::toggle_overwrite_star ()
{
	overwrite_star = !overwrite_star;
	overwrite_star_button.set_active_state (overwrite_star ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MainToolbar::toggle_sync_playhead ()
{
	sync_playhead = !sync_playhead;
	sync_playhead_button.set_active_state (sync_playhead ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	if (sync_playhead) {
		tracker_editor.grid.connect_clock ();
	} else {
		tracker_editor.grid.disconnect_clock ();
	}
}

void
MainToolbar::toggle_jump ()
{
	jump = !jump;
	jump_button.set_active_state (jump ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MainToolbar::toggle_wrap ()
{
	wrap = !wrap;
	wrap_button.set_active_state (wrap ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MainToolbar::toggle_hex ()
{
	hex = !hex;
	base = hex ? 16 : 10;

	tracker_editor.grid.redisplay_grid_direct_call ();
}

void
MainToolbar::cut ()
{
	tracker_editor.grid._subgrid_selector.cut ();
}

void
MainToolbar::copy ()
{
	tracker_editor.grid._subgrid_selector.copy ();
}

void
MainToolbar::paste ()
{
	tracker_editor.grid._subgrid_selector.paste ();
}

void
MainToolbar::paste_overlay ()
{
	tracker_editor.grid._subgrid_selector.paste_overlay ();
}

void
MainToolbar::build_clipboard_menu ()
{
	clipboard_menu = new Gtk::Menu;
	clipboard_menu->set_name ("ArdourContextMenu");

	// The keyboard shortcuts are shown right-aligned in the standard GTK style,
	// from the GtkAccelMap entries filled by the binding system
	// (Bindings::push_to_gtk).  The UIManager normally installs its accel group
	// on actions when it builds menu items from XML; since this menu is built
	// manually, install it here so that create_menu_item() can display the
	// accelerators.
	clipboard_menu->set_accel_group (ActionManager::ui_manager->get_accel_group ());

	static const char* const action_names[] = { "cut", "copy", "paste", "paste-overlay" };
	for (size_t i = 0; i < sizeof (action_names) / sizeof (action_names[0]); ++i) {
		Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Tracker"), action_names[i], false);
		if (act) {
			// Refresh the AccelMap entry from the current binding so that the
			// accelerator hint reflects any change made in the Keyboard Shortcuts
			// window.
			if (bindings) {
				Gtkmm2ext::Bindings::Operation op;
				Gtkmm2ext::KeyboardKey key = bindings->get_binding_for_action (act, op);
				if (!(key == Gtkmm2ext::KeyboardKey::null_key ())) {
					Gtk::AccelMap::change_entry (act->get_accel_path (), key.key (), (Gdk::ModifierType) key.state (), true);
				}
			}

			act->set_accel_group (ActionManager::ui_manager->get_accel_group ());
			clipboard_menu->append (*Gtk::manage (act->create_menu_item ()));
		}
	}

	// Separator, then the Hex mode toggle.
	Gtk::SeparatorMenuItem* separator = Gtk::manage (new Gtk::SeparatorMenuItem ());
	separator->show ();
	clipboard_menu->append (*separator);

	Gtk::CheckMenuItem* hex_item = Gtk::manage (new Gtk::CheckMenuItem (_("Hex")));
	hex_item->set_active (hex);
	hex_item->signal_toggled ().connect (sigc::mem_fun (*this, &MainToolbar::toggle_hex));
	clipboard_menu->append (*hex_item);

	// Give the item the same accel path as the toggle-hex action so that its
	// shortcut (Alt+H) is shown, after refreshing the AccelMap entry from the
	// current binding.
	Glib::RefPtr<Gtk::Action> hex_act = ActionManager::get_action (X_("Tracker"), X_("toggle-hex"), false);
	if (hex_act) {
		if (bindings) {
			Gtkmm2ext::Bindings::Operation op;
			Gtkmm2ext::KeyboardKey key = bindings->get_binding_for_action (hex_act, op);
			if (!(key == Gtkmm2ext::KeyboardKey::null_key ())) {
				Gtk::AccelMap::change_entry (hex_act->get_accel_path (), key.key (), (Gdk::ModifierType) key.state (), true);
			}
		}
		hex_item->set_accel_path (hex_act->get_accel_path ());
	}
	hex_item->show ();
}

void
MainToolbar::popup_clipboard_menu (GdkEventButton* ev)
{
	// Rebuild the menu each time so that the accelerator hints reflect any
	// shortcut change made in the Keyboard Shortcuts window.
	delete clipboard_menu;
	clipboard_menu = 0;
	build_clipboard_menu ();
	clipboard_menu->popup (ev->button, ev->time);
}

void
MainToolbar::set_rows_per_beat_to (GridType st)
{
	set_beats_per_row_to (st);
	tracker_editor.grid.set_rows_per_beat (rows_per_beat, true);
	tracker_editor.grid.redisplay_grid_direct_call ();
}

void MainToolbar::step_size_up ()   { steps_spinner.spin (Gtk::SPIN_STEP_FORWARD, 1.0); }
void MainToolbar::step_size_down () { steps_spinner.spin (Gtk::SPIN_STEP_BACKWARD, 1.0); }
void MainToolbar::octave_up ()      { octave_spinner.spin (Gtk::SPIN_STEP_FORWARD, 1.0); }
void MainToolbar::octave_down ()    { octave_spinner.spin (Gtk::SPIN_STEP_BACKWARD, 1.0); }
void MainToolbar::channel_up ()     { channel_spinner.spin (Gtk::SPIN_STEP_FORWARD, 1.0); }
void MainToolbar::channel_down ()   { channel_spinner.spin (Gtk::SPIN_STEP_BACKWARD, 1.0); }
void MainToolbar::velocity_up ()    { velocity_spinner.spin (Gtk::SPIN_STEP_FORWARD, 1.0); }
void MainToolbar::velocity_down ()  { velocity_spinner.spin (Gtk::SPIN_STEP_BACKWARD, 1.0); }
void MainToolbar::delay_up ()       { delay_spinner.spin (Gtk::SPIN_STEP_FORWARD, 1.0); }
void MainToolbar::delay_down ()     { delay_spinner.spin (Gtk::SPIN_STEP_BACKWARD, 1.0); }
void MainToolbar::precision_up ()   { precision_spinner.spin (Gtk::SPIN_STEP_FORWARD, 1.0); }
void MainToolbar::precision_down () { precision_spinner.spin (Gtk::SPIN_STEP_BACKWARD, 1.0); }
void MainToolbar::position_up ()    { position_spinner.spin (Gtk::SPIN_STEP_FORWARD, 1.0); }
void MainToolbar::position_down ()  { position_spinner.spin (Gtk::SPIN_STEP_BACKWARD, 1.0); }

void
MainToolbar::rows_per_beat_up ()
{
	rows_per_beat_step (1);
}

void
MainToolbar::rows_per_beat_down ()
{
	rows_per_beat_step (-1);
}

void
MainToolbar::rows_per_beat_step (int direction)
{
	const size_t nchoices = sizeof (_rows_per_beat_choices) / sizeof (_rows_per_beat_choices[0]);
	size_t idx = 0;
	for (size_t i = 0; i < nchoices; i++) {
		if (_rows_per_beat_choices[i] == rows_per_beat) {
			idx = i;
			break;
		}
	}
	const int new_idx = std::min ((int) nchoices - 1, std::max (0, (int) idx + direction));
	if ((size_t) new_idx != idx) {
		set_rows_per_beat_to (grid_type_from_rows_per_beat (_rows_per_beat_choices[new_idx]));
	}
}

Editing::GridType
MainToolbar::grid_type_from_rows_per_beat (uint16_t rpb)
{
	switch (rpb) {
	case 1:  return GridTypeBeat;
	case 2:  return GridTypeBeatDiv2;
	case 3:  return GridTypeBeatDiv3;
	case 4:  return GridTypeBeatDiv4;
	case 5:  return GridTypeBeatDiv5;
	case 6:  return GridTypeBeatDiv6;
	case 7:  return GridTypeBeatDiv7;
	case 8:  return GridTypeBeatDiv8;
	case 10: return GridTypeBeatDiv10;
	case 12: return GridTypeBeatDiv12;
	case 14: return GridTypeBeatDiv14;
	case 16: return GridTypeBeatDiv16;
	case 20: return GridTypeBeatDiv20;
	case 24: return GridTypeBeatDiv24;
	case 28: return GridTypeBeatDiv28;
	case 32: return GridTypeBeatDiv32;
	}
	return GridTypeBeatDiv4;
}

void
MainToolbar::toggle_step_edit_action ()
{
	if (_instance) {
		_instance->toggle_step_edit ();
	}
}

void
MainToolbar::toggle_chord_mode_action ()
{
	if (_instance) {
		_instance->toggle_chord_mode ();
	}
}

void
MainToolbar::toggle_overwrite_default_action ()
{
	if (_instance) {
		_instance->toggle_overwrite_default ();
	}
}

void
MainToolbar::toggle_overwrite_existing_action ()
{
	if (_instance) {
		_instance->toggle_overwrite_existing ();
	}
}

void
MainToolbar::toggle_overwrite_star_action ()
{
	if (_instance) {
		_instance->toggle_overwrite_star ();
	}
}

void
MainToolbar::toggle_sync_playhead_action ()
{
	if (_instance) {
		_instance->toggle_sync_playhead ();
	}
}

void
MainToolbar::toggle_jump_action ()
{
	if (_instance) {
		_instance->toggle_jump ();
	}
}

void
MainToolbar::toggle_wrap_action ()
{
	if (_instance) {
		_instance->toggle_wrap ();
	}
}

void
MainToolbar::toggle_hex_action ()
{
	if (_instance) {
		_instance->toggle_hex ();
	}
}

void
MainToolbar::cut_action ()
{
	if (_instance) {
		_instance->cut ();
	}
}

void
MainToolbar::copy_action ()
{
	if (_instance) {
		_instance->copy ();
	}
}

void
MainToolbar::paste_action ()
{
	if (_instance) {
		_instance->paste ();
	}
}

void
MainToolbar::paste_overlay_action ()
{
	if (_instance) {
		_instance->paste_overlay ();
	}
}
void
MainToolbar::beats_per_row_chosen_action (GridType st)
{
	if (_instance) {
		_instance->beats_per_row_chosen (st);
	}
}

void
MainToolbar::adjust_action (AdjustParam what, int direction)
{
	if (!_instance) {
		return;
	}
	switch (what) {
	case AdjustParam::STEP_SIZE:
		if (direction < 0) { _instance->step_size_down (); } else { _instance->step_size_up (); }
		break;
	case AdjustParam::OCTAVE:
		if (direction < 0) { _instance->octave_down (); } else { _instance->octave_up (); }
		break;
	case AdjustParam::CHANNEL:
		if (direction < 0) { _instance->channel_down (); } else { _instance->channel_up (); }
		break;
	case AdjustParam::VELOCITY:
		if (direction < 0) { _instance->velocity_down (); } else { _instance->velocity_up (); }
		break;
	case AdjustParam::DELAY:
		if (direction < 0) { _instance->delay_down (); } else { _instance->delay_up (); }
		break;
	case AdjustParam::PRECISION:
		if (direction < 0) { _instance->precision_down (); } else { _instance->precision_up (); }
		break;
	case AdjustParam::POSITION:
		if (direction < 0) { _instance->position_down (); } else { _instance->position_up (); }
		break;
	case AdjustParam::ROWS_PER_BEAT:
		_instance->rows_per_beat_step (direction);
		break;
	}
}

