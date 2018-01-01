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

#include "main_toolbar.h"
#include "midi_tracker_editor.h"

#include "actions.h"

#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "pbd/i18n.h"
#include "pbd/error.h"

using namespace Editing;
using namespace PBD;

#define COMBO_TRIANGLE_WIDTH 25

static const gchar *_beats_per_row_strings[] = {
	N_("Beats/128"),
	N_("Beats/64"),
	N_("Beats/32"),
	N_("Beats/28"),
	N_("Beats/24"),
	N_("Beats/20"),
	N_("Beats/16"),
	N_("Beats/14"),
	N_("Beats/12"),
	N_("Beats/10"),
	N_("Beats/8"),
	N_("Beats/7"),
	N_("Beats/6"),
	N_("Beats/5"),
	N_("Beats/4"),
	N_("Beats/3"),
	N_("Beats/2"),
	N_("Beats"),
	N_("Bars"),
	0
};

MainToolbar::MainToolbar (MidiTrackerEditor& mte)
	: midi_tracker_editor (mte)
	, myactions (X_("Tracking"))
	, beats_per_row_strings (I18N (_beats_per_row_strings))
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
	, position_adjustment (0, -5, 5, 1, 2)
	, position_spinner (position_adjustment)
	, steps_label (_("Steps"))
	  // TODO set the boundaries to not be above the number of rows
	, steps_adjustment (4, -255, 255, 1, 4)
	, steps_spinner (steps_adjustment)
{
}

void
MainToolbar::setup ()
{
	setup_layout ();
	setup_tooltips ();
	setup_beats_per_row_menu ();
	register_actions ();
	set_beats_per_row_to (SnapToBeatDiv4);
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
	position_spinner.set_activates_default ();
	position_spinner.show ();
	pack_start (position_spinner, false, false);

	show ();
}

void
MainToolbar::setup_tooltips ()
{
	set_tooltip (beats_per_row_selector, _("Beats per row"));
	// TODO: add step edit button tooltip
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

	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv128 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv128)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv64 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv64)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv32 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv32)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv28 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv28)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv24 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv24)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv20 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv20)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv16 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv16)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv14 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv14)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv12 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv12)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv10 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv10)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv8 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv8)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv7 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv7)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv6 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv6)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv5 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv5)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv4 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv4)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv3 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv3)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv2 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeatDiv2)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeat - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBeat)));
	// TODO SnapToBar is not yet supported
	// beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBar - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_selection_done), (SnapType) SnapToBar)));

	set_size_request_to_display_given_text (beats_per_row_selector, beats_per_row_strings, COMBO_TRIANGLE_WIDTH, 2);
}

void
MainToolbar::register_actions ()
{
	Glib::RefPtr<Gtk::ActionGroup> beats_per_row_actions = myactions.create_action_group (X_("BeatsPerRow"));
	Gtk::RadioAction::Group beats_per_row_choice_group;

	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-onetwentyeighths"), _("Beats Per Row to One Twenty Eighths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv128)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixtyfourths"), _("Beats Per Row to Sixty Fourths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv64)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirtyseconds"), _("Beats Per Row to Thirty Seconds"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv32)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyeighths"), _("Beats Per Row to Twenty Eighths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv28)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyfourths"), _("Beats Per Row to Twenty Fourths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv24)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentieths"), _("Beats Per Row to Twentieths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv20)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-asixteenthbeat"), _("Beats Per Row to Sixteenths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv16)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fourteenths"), _("Beats Per Row to Fourteenths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv14)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twelfths"), _("Beats Per Row to Twelfths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv12)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-tenths"), _("Beats Per Row to Tenths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv10)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-eighths"), _("Beats Per Row to Eighths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv8)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sevenths"), _("Beats Per Row to Sevenths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv7)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixths"), _("Beats Per Row to Sixths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv6)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fifths"), _("Beats Per Row to Fifths"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv5)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-quarters"), _("Beats Per Row to Quarters"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv4)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirds"), _("Beats Per Row to Thirds"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv3)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-halves"), _("Beats Per Row to Halves"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeatDiv2)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-beat"), _("Beats Per Row to Beat"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBeat)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-bar"), _("Beats Per Row to Bar"), (sigc::bind (sigc::mem_fun(*this, &MainToolbar::beats_per_row_chosen), Editing::SnapToBar)));
}

void
MainToolbar::beats_per_row_selection_done (SnapType st)
{
	Glib::RefPtr<Gtk::RadioAction> ract = beats_per_row_action (st);
	if (ract) {
		ract->set_active ();
	}
}

Glib::RefPtr<Gtk::RadioAction>
MainToolbar::beats_per_row_action (SnapType st)
{
	const char* action = 0;
	Glib::RefPtr<Gtk::Action> act;

	switch (st) {
	case Editing::SnapToBeatDiv128:
		action = "beats-per-row-onetwentyeighths";
		break;
	case Editing::SnapToBeatDiv64:
		action = "beats-per-row-sixtyfourths";
		break;
	case Editing::SnapToBeatDiv32:
		action = "beats-per-row-thirtyseconds";
		break;
	case Editing::SnapToBeatDiv28:
		action = "beats-per-row-twentyeighths";
		break;
	case Editing::SnapToBeatDiv24:
		action = "beats-per-row-twentyfourths";
		break;
	case Editing::SnapToBeatDiv20:
		action = "beats-per-row-twentieths";
		break;
	case Editing::SnapToBeatDiv16:
		action = "beats-per-row-asixteenthbeat";
		break;
	case Editing::SnapToBeatDiv14:
		action = "beats-per-row-fourteenths";
		break;
	case Editing::SnapToBeatDiv12:
		action = "beats-per-row-twelfths";
		break;
	case Editing::SnapToBeatDiv10:
		action = "beats-per-row-tenths";
		break;
	case Editing::SnapToBeatDiv8:
		action = "beats-per-row-eighths";
		break;
	case Editing::SnapToBeatDiv7:
		action = "beats-per-row-sevenths";
		break;
	case Editing::SnapToBeatDiv6:
		action = "beats-per-row-sixths";
		break;
	case Editing::SnapToBeatDiv5:
		action = "beats-per-row-fifths";
		break;
	case Editing::SnapToBeatDiv4:
		action = "beats-per-row-quarters";
		break;
	case Editing::SnapToBeatDiv3:
		action = "beats-per-row-thirds";
		break;
	case Editing::SnapToBeatDiv2:
		action = "beats-per-row-halves";
		break;
	case Editing::SnapToBeat:
		action = "beats-per-row-beat";
		break;
	case Editing::SnapToBar:
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
MainToolbar::beats_per_row_chosen (SnapType st)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	Glib::RefPtr<Gtk::RadioAction> ract = beats_per_row_action (st);

	if (ract && ract->get_active()) {
		set_beats_per_row_to (st);
	}
}

void
MainToolbar::set_beats_per_row_to (SnapType st)
{
	unsigned int snap_ind = (int)st - (int)SnapToBeatDiv128;

	std::string str = beats_per_row_strings[snap_ind];

	if (str != beats_per_row_selector.get_text()) {
		beats_per_row_selector.set_text (str);
	}

	switch (st) {
	case SnapToBeatDiv128: rows_per_beat = 128; break;
	case SnapToBeatDiv64: rows_per_beat = 64; break;
	case SnapToBeatDiv32: rows_per_beat = 32; break;
	case SnapToBeatDiv28: rows_per_beat = 28; break;
	case SnapToBeatDiv24: rows_per_beat = 24; break;
	case SnapToBeatDiv20: rows_per_beat = 20; break;
	case SnapToBeatDiv16: rows_per_beat = 16; break;
	case SnapToBeatDiv14: rows_per_beat = 14; break;
	case SnapToBeatDiv12: rows_per_beat = 12; break;
	case SnapToBeatDiv10: rows_per_beat = 10; break;
	case SnapToBeatDiv8: rows_per_beat = 8; break;
	case SnapToBeatDiv7: rows_per_beat = 7; break;
	case SnapToBeatDiv6: rows_per_beat = 6; break;
	case SnapToBeatDiv5: rows_per_beat = 5; break;
	case SnapToBeatDiv4: rows_per_beat = 4; break;
	case SnapToBeatDiv3: rows_per_beat = 3; break;
	case SnapToBeatDiv2: rows_per_beat = 2; break;
	case SnapToBeat: rows_per_beat = 1; break;
	case SnapToBar: rows_per_beat = 0; break;
	default:
		/* relax */
		break;
	}

	// TODO: alternatively send signal to MidiTrackerEditor
	midi_tracker_editor.redisplay_model ();
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

	return false;
}
