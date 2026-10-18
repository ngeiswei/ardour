/*
 * Copyright (C) 2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include "pbd/i18n.h"

#include "time_header.h"
#include "tracker_editor.h"

using namespace Tracker;

/**
 * Widget placed on top of the Time column.  It holds the Step Edit button,
 * spanning the time column width and the track headers' height.
 */

TimeHeader::TimeHeader (TrackerEditor& te)
	: tracker_editor (te)
{
	// Reuse the red "record enable button" color scheme.
	step_edit_button.set_name ("record enable button");
	step_edit_button.set_text (S_("Step Edit"));
	step_edit_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &TimeHeader::step_edit_press), false);
	step_edit_button.set_active_state (tracker_editor.main_toolbar.step_edit ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	step_edit_button.show ();

	// Fill the (single-cell) table so the button spans the whole time header
	// area, which GridHeader::align() sizes to the time column.
	attach (step_edit_button, 0, 1, 0, 1);
}

TimeHeader::~TimeHeader ()
{
}

bool
TimeHeader::step_edit_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		return true;
	}

	tracker_editor.main_toolbar.toggle_step_edit ();
	return false;
}

void
TimeHeader::set_step_edit (bool step_edit)
{
	step_edit_button.set_active_state (step_edit ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}
