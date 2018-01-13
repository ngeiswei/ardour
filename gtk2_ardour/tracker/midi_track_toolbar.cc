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

#include "midi_track_toolbar.h"
#include "midi_tracker_editor.h"

#include "widgets/tooltips.h"

#include "pbd/i18n.h"

MidiTrackToolbar::MidiTrackToolbar (MidiTrackerEditor& mte)
	: midi_tracker_editor (mte)
	, visible_note (true)
	, visible_channel (false)
	, visible_velocity (false)
	, visible_delay (false)
{
}

void
MidiTrackToolbar::setup ()
{
	setup_layout ();
	setup_tooltips ();
}

void
MidiTrackToolbar::setup_layout ()
{
	set_spacing (2);

	// Add visible note button
	visible_note_button.set_name ("visible note button");
	visible_note_button.set_text (S_("Note|N"));
	update_visible_note_button ();
	visible_note_button.show ();
	visible_note_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::visible_note_press), false);
	pack_start (visible_note_button, false, false);

	// Add visible channel button
	visible_channel_button.set_name ("visible channel button");
	visible_channel_button.set_text (S_("Channel|C"));
	update_visible_channel_button ();
	visible_channel_button.show ();
	visible_channel_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::visible_channel_press), false);
	pack_start (visible_channel_button, false, false);

	// Add visible velocity button
	visible_velocity_button.set_name ("visible velocity button");
	visible_velocity_button.set_text (S_("Velocity|V"));
	update_visible_velocity_button ();
	visible_velocity_button.show ();
	visible_velocity_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::visible_velocity_press), false);
	pack_start (visible_velocity_button, false, false);

	// Add visible delay button
	visible_delay_button.set_name ("visible delay button");
	visible_delay_button.set_text (S_("Delay|D"));
	update_visible_delay_button ();
	visible_delay_button.show ();
	visible_delay_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::visible_delay_press), false);
	pack_start (visible_delay_button, false, false);

	// Add automation button
	automation_button.set_name ("automation button");
	automation_button.set_text (S_("Automation|A"));
	update_automation_button ();
	automation_button.show ();
	pack_start (automation_button, false, false);

	// Remove/add note column
	rm_add_note_column_separator.show ();
	pack_start (rm_add_note_column_separator, false, false);
	remove_note_column_button.set_name ("remove note column");
	remove_note_column_button.set_text (S_("Remove|-"));
	remove_note_column_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::remove_note_column_press), false);
	remove_note_column_button.show ();
	remove_note_column_button.set_sensitive (false);
	pack_start (remove_note_column_button, false, false);
	add_note_column_button.set_name ("add note column");
	add_note_column_button.set_text (S_("Add|+"));
	add_note_column_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::add_note_column_press), false);
	add_note_column_button.show ();
	pack_start (add_note_column_button, false, false);

	show ();
}

void
MidiTrackToolbar::setup_tooltips ()
{
	set_tooltip (visible_note_button, _("Toggle note visibility"));
	set_tooltip (visible_channel_button, _("Toggle channel visibility"));
	set_tooltip (visible_velocity_button, _("Toggle velocity visibility"));
	set_tooltip (visible_delay_button, _("Toggle delay visibility"));
	set_tooltip (remove_note_column_button, _("Remove note column"));
	set_tooltip (add_note_column_button, _("Add note column"));
	set_tooltip (automation_button, _("MIDI Controllers and Automation"));
}

bool
MidiTrackToolbar::visible_note_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_note = !visible_note;
	midi_tracker_editor.redisplay_visible_note();
	return false;
}

bool
MidiTrackToolbar::visible_channel_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_channel = !visible_channel;
	midi_tracker_editor.redisplay_visible_channel();
	return false;
}

bool
MidiTrackToolbar::visible_velocity_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_velocity = !visible_velocity;
	midi_tracker_editor.redisplay_visible_velocity();
	return false;
}

bool
MidiTrackToolbar::visible_delay_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_delay = !visible_delay;
	midi_tracker_editor.redisplay_visible_delay();
	return false;
}

void
MidiTrackToolbar::automation_click ()
{
	// TODO: move this in MidiTrackToolbar
	midi_tracker_editor.build_automation_action_menu ();
	midi_tracker_editor.automation_action_menu->popup (1, gtk_get_current_event_time());
}

bool
MidiTrackToolbar::remove_note_column_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	midi_tracker_editor.mtp->np.dec_ntracks (); // TODO: better have a ref to mtp
	midi_tracker_editor.redisplay_model ();
	update_remove_note_column_button ();

	return false;
}

bool
MidiTrackToolbar::add_note_column_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	midi_tracker_editor.mtp->np.inc_ntracks (); // TODO: better have a ref to mtp
	midi_tracker_editor.redisplay_model ();
	update_remove_note_column_button ();

	return false;
}

void
MidiTrackToolbar::update_visible_note_button()
{
	visible_note_button.set_active_state (visible_note ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_visible_channel_button()
{
	visible_channel_button.set_active_state (visible_channel ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_visible_velocity_button()
{
	visible_velocity_button.set_active_state (visible_velocity ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_visible_delay_button()
{
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_automation_button()
{
	automation_button.signal_clicked.connect (sigc::mem_fun(*this, &MidiTrackToolbar::automation_click));
}

void
MidiTrackToolbar::update_remove_note_column_button ()
{
	// TODO: have a reference of mtp
	remove_note_column_button.set_sensitive (midi_tracker_editor.mtp->np.nreqtracks < midi_tracker_editor.mtp->np.ntracks);
}
