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

#ifndef __ardour_tracker_time_header_h_
#define __ardour_tracker_time_header_h_

#include <ytkmm/table.h>

#include "widgets/ardour_button.h"

namespace Tracker {

class TrackerEditor;

/**
 * Widget placed on top of the Time column.  Currently it holds the Step Edit
 * button, which spans the width of the time column and the height of the track
 * headers.
 */
class TimeHeader : public Gtk::Table
{
public:
	explicit TimeHeader (TrackerEditor& te);
	~TimeHeader ();

	/** Update the Step Edit button to reflect the given step-editing mode. */
	void set_step_edit (bool);

	TrackerEditor& tracker_editor;
	ArdourWidgets::ArdourButton step_edit_button;

private:
	bool step_edit_press (GdkEventButton* ev);
};

} // ~namespace tracker

#endif /* __ardour_tracker_time_header_h_ */
