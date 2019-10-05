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

MainToolbar::MainToolbar (TrackerEditor& te)
	: tracker_editor (te)
	, bindings (0)
	, beats_per_row_strings (I18N (_beats_per_row_strings))
	, rows_per_beat (0)
	, step_edit (false)
	, octave_label (_("Octave"))
	, octave_adjustment (4, -1, 9, 1, 2)
	, octave_spinner (octave_adjustment)
	, channel_label (S_("Channel|Ch"))
	, channel_adjustment (1, 1, 16, 1, 4)
	, channel_spinner (channel_adjustment)
	, velocity_label (S_("Velocity|Vel"))
	, velocity_adjustment (64, 0, 127, 1, 4)
	, velocity_spinner (velocity_adjustment)
	, delay_label (_("Delay"))
	, delay_adjustment (0, 0, 0, 1, 4)
	, delay_spinner (delay_adjustment)
	, position_label (_("Position"))
	, position_adjustment (0, min_position, max_position, 1, 2)
	, position_spinner (position_adjustment)
	, steps_label (_("Steps"))
	  // TODO set the boundaries to not be above the number of rows
	, steps_adjustment (1, -255, 255, 1, 4)
	, steps_spinner (steps_adjustment)
{
}

void
MainToolbar::setup ()
{
	setup_layout ();
	setup_tooltips ();
	setup_beats_per_row_menu ();
	load_bindings ();
	register_actions ();
	set_data ("ardour-bindings", bindings);
	set_beats_per_row_to (GridTypeBeatDiv4);
}

void
MainToolbar::setup_layout ()
{
	set_spacing (2);

	// Add beats per row selection
	beats_per_row_selector.show ();
	pack_start (beats_per_row_selector, false, false);

	// Step edit button
	step_edit_separator.show ();
	pack_start (step_edit_separator, false, false);
	step_edit_button.set_name ("step edit button");
	step_edit_button.set_text (S_("Step Edit"));
	step_edit_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MainToolbar::step_edit_press), false);
	step_edit_button.set_active_state (step_edit ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	step_edit_button.show ();
	pack_start (step_edit_button, false, false);

	// Steps spinner
	steps_separator.show ();
	pack_start (steps_separator, false, false);
	steps_label.show ();
	pack_start (steps_label, false, false);
	steps_spinner.set_activates_default ();
	steps_spinner.show ();
	pack_start (steps_spinner, false, false);

	// Octave spinner
	octave_separator.show ();
	pack_start (octave_separator, false, false);
	octave_label.show ();
	pack_start (octave_label, false, false);
	octave_spinner.set_activates_default ();
	octave_spinner.show ();
	pack_start (octave_spinner, false, false);

	// Channel spinner
	channel_separator.show ();
	pack_start (channel_separator, false, false);
	channel_label.show ();
	pack_start (channel_label, false, false);
	channel_spinner.set_activates_default ();
	channel_spinner.show ();
	pack_start (channel_spinner, false, false);

	// Velocity spinner
	velocity_separator.show ();
	pack_start (velocity_separator, false, false);
	velocity_label.show ();
	pack_start (velocity_label, false, false);
	velocity_spinner.set_activates_default ();
	velocity_spinner.show ();
	pack_start (velocity_spinner, false, false);

	// Delay spinner
	delay_separator.show ();
	pack_start (delay_separator, false, false);
	delay_label.show ();
	pack_start (delay_label, false, false);
	delay_spinner.set_activates_default ();
	delay_spinner.show ();
	pack_start (delay_spinner, false, false);

	// Position spinner
	position_separator.show ();
	pack_start (position_separator, false, false);
	position_label.show ();
	pack_start (position_label, false, false);
	position_spinner.signal_value_changed().connect (sigc::mem_fun(*this, &MainToolbar::change_position), false);
	position_spinner.set_activates_default ();
	position_spinner.show ();
	pack_start (position_spinner, false, false);

	show ();
}

void
MainToolbar::setup_tooltips ()
{
	set_tooltip (beats_per_row_selector, _("Beats per row"));
	step_edit_button.set_tooltip_text (_("Toggle step editing"));
	octave_spinner.set_tooltip_text (_("Default octave"));
	channel_spinner.set_tooltip_text (_("Default channel"));
	velocity_spinner.set_tooltip_text (_("Default velocity"));
	delay_spinner.set_tooltip_text (_("Default delay"));
	position_spinner.set_tooltip_text (_("Position from the numerical separator changed when step editing automation. Place value = base^(position-1)"));
	steps_spinner.set_tooltip_text (_("Step size"));
}

void
MainToolbar::setup_beats_per_row_menu ()
{
	using namespace Gtk::Menu_Helpers;

	// TODO GridTypeBar is not yet supported
	// beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBar - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBar)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeat - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeat)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv2 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv2)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv3 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv3)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv4 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv4)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv5 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv5)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv6 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv6)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv7 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv7)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv8 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv8)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv10 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv10)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv12 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv12)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv14 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv14)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv16 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv16)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv20 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv20)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv24 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv24)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv28 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv28)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)GridTypeBeatDiv32 - (int)GridTypeBar], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (GridType) GridTypeBeatDiv32)));

	set_size_request_to_display_given_text (beats_per_row_selector, beats_per_row_strings, COMBO_TRIANGLE_WIDTH, 2);
}

void
MainToolbar::load_bindings ()
{
	bindings = Gtkmm2ext::Bindings::get_bindings (X_("Main Toolbar"));
}

void
MainToolbar::register_actions ()
{
	Glib::RefPtr<Gtk::ActionGroup> beats_per_row_actions = ActionManager::create_action_group (bindings, X_("BeatsPerRow"));
	Gtk::RadioAction::Group beats_per_row_choice_group;

	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirtyseconds"), _("Beats Per Row to Thirty Seconds"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv32)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyeighths"), _("Beats Per Row to Twenty Eighths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv28)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyfourths"), _("Beats Per Row to Twenty Fourths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv24)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentieths"), _("Beats Per Row to Twentieths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv20)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-asixteenthbeat"), _("Beats Per Row to Sixteenths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv16)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fourteenths"), _("Beats Per Row to Fourteenths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv14)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twelfths"), _("Beats Per Row to Twelfths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv12)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-tenths"), _("Beats Per Row to Tenths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv10)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-eighths"), _("Beats Per Row to Eighths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv8)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sevenths"), _("Beats Per Row to Sevenths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv7)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixths"), _("Beats Per Row to Sixths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv6)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fifths"), _("Beats Per Row to Fifths"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv5)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-quarters"), _("Beats Per Row to Quarters"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv4)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirds"), _("Beats Per Row to Thirds"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv3)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-halves"), _("Beats Per Row to Halves"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeatDiv2)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-beat"), _("Beats Per Row to Beat"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBeat)));
	ActionManager::register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-bar"), _("Beats Per Row to Bar"),
	                                      (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::GridTypeBar)));
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
	const char* action = 0;
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
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("BeatsPerRow"), action);

	if (act) {
		Glib::RefPtr<Gtk::RadioAction> ract = Glib::RefPtr<Gtk::RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "Tracker::MainToolbar::beats_per_row_chosen could not find action to match type.") << endmsg;
		return Glib::RefPtr<Gtk::RadioAction>();
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

	if (ract && ract->get_active()) {
		set_beats_per_row_to (st);

		// TODO: alternatively send signal to TrackerEditor
		tracker_editor.grid.redisplay_grid_direct_call ();
	}
}

void
MainToolbar::set_beats_per_row_to (GridType st)
{
	unsigned int snap_ind = (int)st - (int)GridTypeBar;

	std::string str = beats_per_row_strings[snap_ind];

	if (str != beats_per_row_selector.get_text()) {
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

	step_edit = !step_edit;
	step_edit_button.set_active_state (step_edit ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	// TODO: possibly replace by signal
	tracker_editor.grid.redisplay_grid_direct_call();
	if (step_edit) {
		tracker_editor.grid.set_underline_current_step_edit_cell ();
	} else {
		tracker_editor.grid.unset_underline_current_step_edit_cell ();
	}

	return false;
}

void
MainToolbar::change_position ()
{
	// TODO: possibly replace by signal
	if (step_edit) {
		tracker_editor.grid.unset_underline_current_step_edit_cell ();
		tracker_editor.grid.set_underline_current_step_edit_cell ();
	}
}
