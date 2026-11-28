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

struct RowsPerBeatChoice {
	Editing::GridType grid_type;
	uint16_t          rows;
	const char*       action_name;
	const char*       menu_label;
};

/* Single source for grid types, row counts, actions, labels and step order. */
static const RowsPerBeatChoice _rows_per_beat_choices[] = {
	{ GridTypeBar,        0,  "beats-per-row-bar",            N_("1 Row/Bar") },
	{ GridTypeBeat,       1,  "beats-per-row-beat",           N_("1 Row/Beat") },
	{ GridTypeBeatDiv2,   2,  "beats-per-row-halves",         N_("2 Rows/Beat") },
	{ GridTypeBeatDiv3,   3,  "beats-per-row-thirds",         N_("3 Rows/Beat") },
	{ GridTypeBeatDiv4,   4,  "beats-per-row-quarters",       N_("4 Rows/Beat") },
	{ GridTypeBeatDiv5,   5,  "beats-per-row-fifths",         N_("5 Rows/Beat") },
	{ GridTypeBeatDiv6,   6,  "beats-per-row-sixths",         N_("6 Rows/Beat") },
	{ GridTypeBeatDiv7,   7,  "beats-per-row-sevenths",       N_("7 Rows/Beat") },
	{ GridTypeBeatDiv8,   8,  "beats-per-row-eighths",        N_("8 Rows/Beat") },
	{ GridTypeBeatDiv10, 10,  "beats-per-row-tenths",         N_("10 Rows/Beat") },
	{ GridTypeBeatDiv12, 12,  "beats-per-row-twelfths",       N_("12 Rows/Beat") },
	{ GridTypeBeatDiv14, 14,  "beats-per-row-fourteenths",    N_("14 Rows/Beat") },
	{ GridTypeBeatDiv16, 16,  "beats-per-row-asixteenthbeat", N_("16 Rows/Beat") },
	{ GridTypeBeatDiv20, 20,  "beats-per-row-twentieths",     N_("20 Rows/Beat") },
	{ GridTypeBeatDiv24, 24,  "beats-per-row-twentyfourths",  N_("24 Rows/Beat") },
	{ GridTypeBeatDiv28, 28,  "beats-per-row-twentyeighths",  N_("28 Rows/Beat") },
	{ GridTypeBeatDiv32, 32,  "beats-per-row-thirtyseconds",  N_("32 Rows/Beat") },
};

/**
 * Ardour creates the per-user keybinding file only once; new default bindings
 * added in later revisions are not merged into an existing user file.  Install
 * a default binding in memory when the loaded bindings predate the action, so
 * both the shortcut and the popup menu hint work without forcing the user to
 * reset all their shortcuts.  An existing binding (including a user-customized
 * one) always wins, except when it still uses an old default that is being
 * replaced (old_key_name).
 */
static void
ensure_default_tracker_binding (Gtkmm2ext::Bindings* bindings,
                                 const char* action_name,
                                 const char* key_name,
                                 const char* old_key_name = nullptr)
{
	Gtkmm2ext::KeyboardKey key;

	if (!bindings->is_registered (Gtkmm2ext::Bindings::Press, action_name) &&
	    !bindings->is_registered (Gtkmm2ext::Bindings::Release, action_name)) {
		if (Gtkmm2ext::KeyboardKey::make_key (key_name, key)) {
			bindings->add (key, Gtkmm2ext::Bindings::Press, action_name, 0, false);
		}
		return;
	}

	if (!old_key_name) {
		return;
	}

	Gtkmm2ext::KeyboardKey old_key;
	std::string bound_action;

	if (!Gtkmm2ext::KeyboardKey::make_key (old_key_name, old_key) ||
	    !bindings->is_bound (old_key, Gtkmm2ext::Bindings::Press, &bound_action) ||
	    bound_action != action_name ||
	    !Gtkmm2ext::KeyboardKey::make_key (key_name, key)) {
		return;
	}

	/* Do not steal the new key if the user has already given it to another
	 * action. */
	if (bindings->is_bound (key, Gtkmm2ext::Bindings::Press, &bound_action) &&
	    bound_action != action_name) {
		return;
	}

	bindings->replace (key, Gtkmm2ext::Bindings::Press, action_name, false);
}

MainToolbar* MainToolbar::_instance = nullptr;

MainToolbar::MainToolbar (TrackerEditor& te)
	: tracker_editor (te)
	, bindings (0)
	, spacing (4)
	, rows_per_beat (4)
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

	update_interpolate_sensitivity ();
}

void
MainToolbar::setup_layout ()
{
	set_spacing (spacing);

	// Top row
	Gtk::HBox* tr = Gtk::manage (new Gtk::HBox ());
	tr->set_spacing(spacing);

	// Rows per beat selector
	beats_per_row_selector.show ();
	tr->pack_start (beats_per_row_selector, false, false);

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

	// The mode toggle buttons reuse the "mouse mode button" color scheme so
	// that they turn green when enabled, like the editor's snap button.

	// Chord mode buttom
	chord_mode_button.set_name ("mouse mode button");
	chord_mode_button.set_text (S_("Chord Mode"));
	chord_mode_button.set_related_action (ActionManager::get_action (X_("Tracker"), X_("toggle-chord-mode"), false));
	chord_mode_button.show ();
	mr->pack_start (chord_mode_button, false, false);

	// Overwrite default button
	overwrite_default_separator.show ();
	mr->pack_start (overwrite_default_separator, false, false);
	overwrite_default_button.set_name ("mouse mode button");
	overwrite_default_button.set_text (S_("Overwrite Default"));
	overwrite_default_button.set_related_action (ActionManager::get_action (X_("Tracker"), X_("toggle-overwrite-default"), false));
	overwrite_default_button.show ();
	mr->pack_start (overwrite_default_button, false, false);

	// Overwrite existing button
	overwrite_existing_separator.show ();
	mr->pack_start (overwrite_existing_separator, false, false);
	overwrite_existing_button.set_name ("mouse mode button");
	overwrite_existing_button.set_text (S_("Overwrite Existing"));
	overwrite_existing_button.set_related_action (ActionManager::get_action (X_("Tracker"), X_("toggle-overwrite-existing"), false));
	overwrite_existing_button.show ();
	mr->pack_start (overwrite_existing_button, false, false);

	// Overwrite star button
	overwrite_star_separator.show ();
	mr->pack_start (overwrite_star_separator, false, false);
	overwrite_star_button.set_name ("mouse mode button");
	overwrite_star_button.set_text (S_("Overwrite ***"));
	overwrite_star_button.set_related_action (ActionManager::get_action (X_("Tracker"), X_("toggle-overwrite-star"), false));
	overwrite_star_button.show ();
	mr->pack_start (overwrite_star_button, false, false);

	// Sync playhead
	sync_playhead_separator.show ();
	mr->pack_start (sync_playhead_separator, false, false);
	sync_playhead_button.set_name ("mouse mode button");
	sync_playhead_button.set_text (S_("Sync Playhead"));
	sync_playhead_button.set_related_action (ActionManager::get_action (X_("Tracker"), X_("toggle-sync-playhead"), false));
	sync_playhead_button.show ();
	mr->pack_start (sync_playhead_button, false, false);

	// Jump to next
	jump_separator.show ();
	mr->pack_start (jump_separator, false, false);
	jump_button.set_name ("mouse mode button");
	jump_button.set_text (S_("Jump"));
	jump_button.set_related_action (ActionManager::get_action (X_("Tracker"), X_("toggle-jump"), false));
	jump_button.show ();
	mr->pack_start (jump_button, false, false);

	// Wrap vertical move
	wrap_separator.show ();
	mr->pack_start (wrap_separator, false, false);
	wrap_button.set_name ("mouse mode button");
	wrap_button.set_text (S_("Wrap"));
	wrap_button.set_related_action (ActionManager::get_action (X_("Tracker"), X_("toggle-wrap"), false));
	wrap_button.show ();
	mr->pack_start (wrap_button, false, false);

	// Follow the current row, keeping it vertically centered
	follow_separator.show ();
	mr->pack_start (follow_separator, false, false);
	follow_button.set_name ("mouse mode button");
	follow_button.set_text (S_("Follow"));
	follow_button.set_related_action (ActionManager::get_action (X_("Tracker"), X_("toggle-follow"), false));
	follow_button.show ();
	mr->pack_start (follow_button, false, false);

	mr->show ();
	pack_start (*mr, false, false);

	show ();
}

void
MainToolbar::setup_tooltips ()
{
	set_tooltip (beats_per_row_selector, _("Beats per row"));
	chord_mode_button.set_tooltip_text (_("Any note entered while a note is still pressed gets entered at the same row"));
	overwrite_default_button.set_tooltip_text (_("MIDI input events overwrite default channel and velocity"));
	overwrite_existing_button.set_tooltip_text (_("Input events overwrite existing channel, velocity and delay"));
	overwrite_star_button.set_tooltip_text (_("Cell containing more than one event, i.e. displayed as ***, can be overwriten. Otherwise, they are read only, and only zooming in, till there is at most one event per cell, make them writable."));
	sync_playhead_button.set_tooltip_text (_("Synchronize current row and playhead"));
	jump_button.set_tooltip_text (_("Jump to the next event on the same row or column while moving the cursor or step editing. If no such event exist, then fallback to regular movement."));
	wrap_button.set_tooltip_text (_("Wrap vertical move"));
	follow_button.set_tooltip_text (_("Keep the current row vertically centered while the cursor moves"));
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
	for (const RowsPerBeatChoice& choice : _rows_per_beat_choices) {
		// One row per bar is registered for completeness but is not yet supported.
		if (choice.grid_type != GridTypeBar) {
			beats_per_row_selector.append (
				ActionManager::get_action (X_("BeatsPerRow"), choice.action_name));
		}
	}

	set_size_request_to_display_given_text (beats_per_row_selector, _("32 Rows/Beat"), COMBO_TRIANGLE_WIDTH, 2);
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

	for (const RowsPerBeatChoice& choice : _rows_per_beat_choices) {
		Glib::RefPtr<Gtk::RadioAction> action = ActionManager::register_radio_action (
			beats_per_row_actions, beats_per_row_choice_group,
			choice.action_name, _(choice.menu_label),
			sigc::bind (sigc::ptr_fun (&MainToolbar::beats_per_row_chosen_action), choice.grid_type));
		action->set_short_label (_(choice.menu_label));
	}

	// Keyboard shortcuts for the tracker editor.
	Glib::RefPtr<Gtk::ActionGroup> tracker_actions = ActionManager::create_action_group (bindings, X_("Tracker"));

	// Mode toggles.  Pure modes need no callback: their ToggleAction owns the
	// state.  Callbacks below are only for modes with side effects.
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-step-edit"), _("Toggle Step Edit"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::toggle_step_edit));
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-chord-mode"), _("Toggle Chord Mode"), sigc::slot<void> ());
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-overwrite-default"), _("Toggle Overwrite Default"), sigc::slot<void> ());
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-overwrite-existing"), _("Toggle Overwrite Existing"), sigc::slot<void> ());
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-overwrite-star"), _("Toggle Overwrite ***"), sigc::slot<void> ());
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-sync-playhead"), _("Toggle Sync Playhead"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::toggle_sync_playhead));
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-jump"), _("Toggle Jump"), sigc::slot<void> ());
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-wrap"), _("Toggle Wrap"), sigc::slot<void> ());
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-follow"), _("Toggle Follow"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::toggle_follow));
	ActionManager::register_toggle_action (tracker_actions, X_("toggle-hex"), _("Toggle Hexadecimal"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::toggle_hex));

	// Preserve the tracker's original defaults.
	ActionManager::get_toggle_action (X_("Tracker"), X_("toggle-overwrite-default"))->set_active (true);
	ActionManager::get_toggle_action (X_("Tracker"), X_("toggle-hex"))->set_active (true);

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
	ActionManager::register_action (tracker_actions, X_("cut"), _("Cut"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::cut));
	ActionManager::register_action (tracker_actions, X_("copy"), _("Copy"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::copy));
	ActionManager::register_action (tracker_actions, X_("paste"), _("Paste"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::paste));
	ActionManager::register_action (tracker_actions, X_("paste-overlay"), _("Paste Overlay"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::paste_overlay));
	ActionManager::register_action (tracker_actions, X_("interpolate"), _("Interpolate"), sigc::bind (sigc::ptr_fun (&MainToolbar::call_instance_method), &MainToolbar::interpolate));

	// New defaults for existing user key files (see
	// ensure_default_tracker_binding()).
	ensure_default_tracker_binding (bindings, "Tracker/toggle-follow", "Secondary-f");
	ensure_default_tracker_binding (bindings, "Tracker/interpolate", "Primary-l");

	// Rows-per-beat used to be Primary-r/Primary-Tertiary-r, which conflicted
	// with the main editor's redo shortcut.  Move the old default out of the
	// way for users whose key file still has it.
	ensure_default_tracker_binding (bindings, "Tracker/increase-rows-per-beat", "Primary-b", "Primary-r");
	ensure_default_tracker_binding (bindings, "Tracker/decrease-rows-per-beat", "Primary-Tertiary-b", "Primary-Tertiary-r");
}

Glib::RefPtr<Gtk::RadioAction>
MainToolbar::beats_per_row_action (GridType st)
{
	for (const RowsPerBeatChoice& choice : _rows_per_beat_choices) {
		if (choice.grid_type == st) {
			return ActionManager::get_radio_action (X_("BeatsPerRow"), choice.action_name, false);
		}
	}

	fatal << string_compose (_("programming error: %1: %2"), "Tracker: impossible beats-per-row", (int) st) << endmsg;
	abort (); /*NOTREACHED*/
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
	for (const RowsPerBeatChoice& choice : _rows_per_beat_choices) {
		if (choice.grid_type != st) {
			continue;
		}

		const int32_t divisions = tracker_editor.public_editor.get_grid_beat_divisions (st);
		rows_per_beat = (st == GridTypeBar) ? 0 : divisions;

		const std::string label = _(choice.menu_label);
		if (label != beats_per_row_selector.get_text ()) {
			beats_per_row_selector.set_text (label);
		}
		return;
	}
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
	if (step_edit ()) {
		tracker_editor.grid.unset_underline_current_step_edit_cell ();
		tracker_editor.grid.set_underline_current_step_edit_cell ();
	}
}

bool
MainToolbar::mode_active (const char* action_name)
{
	Glib::RefPtr<Gtk::ToggleAction> action = ActionManager::get_toggle_action (X_("Tracker"), action_name, false);
	return action && action->get_active ();
}

void
MainToolbar::toggle_step_edit ()
{
	tracker_editor.grid.redisplay_grid_direct_call ();
	if (step_edit ()) {
		tracker_editor.connect_midi_event ();
		tracker_editor.grid.set_underline_current_step_edit_cell ();
	} else {
		tracker_editor.disconnect_midi_event ();
		tracker_editor.grid.unset_underline_current_step_edit_cell ();
	}
}

void
MainToolbar::toggle_sync_playhead ()
{
	if (sync_playhead ()) {
		tracker_editor.grid.connect_clock ();
	} else {
		tracker_editor.grid.disconnect_clock ();
	}
}

void
MainToolbar::toggle_follow ()
{
	if (follow ()) {
		// Center the current row immediately, without waiting for the next
		// cursor move.
		tracker_editor.grid.scroll_to_current_row ();
	}
}

void
MainToolbar::toggle_hex ()
{
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
MainToolbar::interpolate ()
{
	tracker_editor.grid.interpolate_selection ();
}

void
MainToolbar::update_interpolate_sensitivity ()
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Tracker"), X_("interpolate"), false);
	if (act) {
		act->set_sensitive (tracker_editor.grid._subgrid_selector.has_selection ());
	}
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

	update_interpolate_sensitivity ();

	static const char* const action_names[] = { "cut", "copy", "paste", "paste-overlay", "interpolate" };
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

	// Separator, then the mode toggles (Hexadecimal plus the edit/playback mode buttons),
	// stacked right below each other without a further separator.
	Gtk::SeparatorMenuItem* separator = Gtk::manage (new Gtk::SeparatorMenuItem ());
	separator->show ();
	clipboard_menu->append (*separator);

	struct ModeItem {
		const char* action;
		const char* label;
	};

	const ModeItem mode_items[] = {
		{ "toggle-hex", _("Hexadecimal") },
		{ "toggle-chord-mode", S_("Chord Mode") },
		{ "toggle-overwrite-default", S_("Overwrite Default") },
		{ "toggle-overwrite-existing", S_("Overwrite Existing") },
		{ "toggle-overwrite-star", S_("Overwrite ***") },
		{ "toggle-sync-playhead", S_("Sync Playhead") },
		{ "toggle-jump", S_("Jump") },
		{ "toggle-wrap", S_("Wrap") },
		{ "toggle-follow", S_("Follow") },
	};

	for (size_t i = 0; i < sizeof (mode_items) / sizeof (mode_items[0]); ++i) {
		Glib::RefPtr<Gtk::ToggleAction> act = ActionManager::get_toggle_action (X_("Tracker"), mode_items[i].action, false);
		if (!act) {
			continue;
		}

		Gtk::MenuItem* item = Gtk::manage (act->create_menu_item ());
		item->set_label (mode_items[i].label);
		act->set_accel_group (ActionManager::ui_manager->get_accel_group ());

		// Give the item the same accel path as its action so that its shortcut is
		// shown right-aligned, after refreshing the AccelMap entry from the current
		// binding so that the hint reflects any change made in the Keyboard Shortcuts
		// window.
		if (bindings) {
			Gtkmm2ext::Bindings::Operation op;
			Gtkmm2ext::KeyboardKey key = bindings->get_binding_for_action (act, op);
			if (!(key == Gtkmm2ext::KeyboardKey::null_key ())) {
				Gtk::AccelMap::change_entry (act->get_accel_path (), key.key (), (Gdk::ModifierType) key.state (), true);
			}
		}
		item->set_accel_path (act->get_accel_path ());

		item->show ();
		clipboard_menu->append (*item);
	}
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
MainToolbar::rows_per_beat_step (int direction)
{
	const size_t nchoices = sizeof (_rows_per_beat_choices) / sizeof (_rows_per_beat_choices[0]);
	size_t idx = 0;
	for (size_t i = 0; i < nchoices; i++) {
		if (_rows_per_beat_choices[i].rows == rows_per_beat) {
			idx = i;
			break;
		}
	}
	const int new_idx = std::min ((int) nchoices - 1, std::max (0, (int) idx + direction));
	if ((size_t) new_idx != idx) {
		set_rows_per_beat_to (_rows_per_beat_choices[new_idx].grid_type);
	}
}

void
MainToolbar::call_instance_method (void (MainToolbar::*fn) ())
{
	if (_instance) {
		(_instance->*fn) ();
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

