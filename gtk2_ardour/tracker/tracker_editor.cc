/*
 * Copyright (C) 2015-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include <cctype>
#include <cmath>
#include <map>

#include <boost/algorithm/string.hpp>

#include "pbd/file_utils.h"
#include "pbd/memento_command.h"

#include <ytkmm/cellrenderercombo.h>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "evoral/Note.h"
#include "evoral/midi_util.h"

#include "ardour/amp.h"
#include "ardour/midi_model.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "axis_view.h"
#include "editor.h"
#include "midi_region_view.h"
#include "note_player.h"
#include "ui_config.h"

#include "midi_track_toolbar.h"
#include "tracker_editor.h"
#include "tracker_utils.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;

namespace Tracker {

///////////////////
// TrackerEditor //
///////////////////

// TODO: fix grid focus (use get_focus () to gather information on which widget
//       has a current focus)

TrackerEditor::TrackerEditor (Session* s, RegionSelection& rs)
	: ArdourWindow ("")
	, session (s)
	, public_editor (dynamic_cast<RouteTimeAxisView&> (rs.front ()->get_time_axis_view ()).editor ())
	, region_selection (rs)
	, grid (*this)
	, main_toolbar (*this)
	, grid_header (0)
	, _first (true)
{
	set_session (s);

	signal_map ().connect (sigc::mem_fun (*this, &TrackerEditor::map_handler));
	signal_unmap ().connect (sigc::mem_fun (*this, &TrackerEditor::unmap_handler));
}

TrackerEditor::~TrackerEditor ()
{
	delete grid_header;
}

void TrackerEditor::setup (RegionSelection& rs)
{
	set_region_selection (rs);
	set_title (window_name (rs));

	setup_grid ();
	setup_scroller ();
	setup_toolbars ();
	setup_grid_header ();

	grid.redisplay_grid_direct_call ();

	setup_vbox ();

	set_size_request (-1, 800);

	set_focus (grid);
	// raise (); // TODO: crashes ardour, maybe it would be effective otherwise for getting the focus

	show ();

	if (_first)
		grid.setup_init_cursor ();

	// To align header and grid
	grid.redisplay_grid_direct_call ();

	_first = false;
}

void
TrackerEditor::connect_track (TrackPtr track)
{
	track->playlist ()->ContentsChanged.connect (content_connections, invalidator (*this),
	                                             boost::bind (&Grid::redisplay_grid_connect_call, &grid),
	                                             gui_context ());

	// When the track's color changes, immediately refresh the surrounding
	// left/right separator columns so their background stays in sync.
	track->presentation_info ().PropertyChanged.connect (content_connections, invalidator (*this),
	                                                     [this, track] (const PBD::PropertyChange& what_changed) {
		                                                     if (what_changed.contains (Properties::color)) {
			                                                     grid.redisplay_left_right_separator_columns (track);
		                                                     }
	                                                     },
	                                                     gui_context ());
}

void
TrackerEditor::connect_midi_region (MidiRegionPtr midi_region)
{
	// TODO: optimize, maybe could call a more direct method than
	// redisplay_grid_connect_call.

	// Changing midi content re-render the grid
	midi_region->model ()->ContentsChanged.connect (content_connections, invalidator (*this),
	                                                boost::bind (&Grid::redisplay_grid_connect_call, &grid),
	                                                gui_context ());

	// Changing the region time zone re-render the grid
	midi_region->PropertyChanged.connect (content_connections, invalidator (*this),
	                                      boost::bind (&Grid::redisplay_grid_connect_call, &grid),
	                                      gui_context ());
}

void
TrackerEditor::connect_automation (AutomationControlPtr actl)
{
	// TODO: call a more direct redisplay method than redisplay_grid to speed up redisplay
	AutomationListPtr alist = actl->alist ();
	if (alist) {
		alist->StateChanged.connect (content_connections, invalidator (*this),
		                             std::bind (&Grid::redisplay_grid_connect_call, &grid),
		                             gui_context ());
		alist->InterpolationChanged.connect (content_connections, invalidator (*this),
		                                     std::bind (&Grid::redisplay_grid_connect_call, &grid),
		                                     gui_context ());
	}
}

void
TrackerEditor::connect_midi_event ()
{
	grid.set_step_editing_current_track ();
	midi_event_connection = Glib::signal_timeout().connect (sigc::mem_fun (grid, &Grid::step_editing_check_midi_event), 20);
}

void
TrackerEditor::disconnect_midi_event ()
{
	grid.unset_step_editing_current_track ();
	midi_event_connection.disconnect ();
}

void
TrackerEditor::map_handler ()
{
	// Resume step-editing MIDI input when the window becomes visible again.
	// The step-edit cursor underline will be redrawn by the next cursor move.
	if (main_toolbar.step_edit) {
		connect_midi_event ();
	}
}

void
TrackerEditor::unmap_handler ()
{
	// Stop step-editing MIDI input while the window is not visible.
	disconnect_midi_event ();
}

void
TrackerEditor::resize_width ()
{
	// Resize directly to the natural (preferred) width of the content instead
	// of collapsing to 1px and letting GTK re-expand.  The old 1px trick made
	// the window visibly flash whenever a column was added or removed.
	Gtk::Widget* child = get_child ();
	if (!child) {
		// The child (vbox) is added in setup_vbox(), which runs after the
		// first redisplay_grid_direct_call() in TrackerEditor::setup().
		return;
	}
	Gtk::Requisition req = child->size_request ();
	int width, height;
	get_size (width, height);
	resize (req.width, height);
}

void
TrackerEditor::set_region_selection (const RegionSelection& rs)
{
	region_selection = rs;
	// Replace by the following code if you want to add up the new selection instead of replace the previous one
	// for (RegionSelection::const_iterator it = rs.begin (); it != rs.end (); ++it) {
	// 	region_selection.add (*it);
	// }
}

void
TrackerEditor::setup_grid ()
{
	grid.setup ();
}

void
TrackerEditor::setup_toolbars ()
{
	if (!_first) {
		return;
	}

	main_toolbar.setup ();
}

void
TrackerEditor::setup_grid_header ()
{
	if (grid_header) {
		grid_header->setup_track_headers ();
	} else {
		grid_header = new GridHeader (*this);
	}
}

void
TrackerEditor::setup_scroller ()
{
	if (!_first) {
		return;
	}

	scroller.add (grid);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	scroller.show ();
}

void
TrackerEditor::setup_vbox ()
{
	if (!_first) {
		return;
	}

	vbox.show ();
	vbox.set_spacing (6);
	vbox.set_border_width (6);
	vbox.pack_start (main_toolbar, false, false);
	vbox.pack_start (*grid_header, false, false);
	vbox.pack_start (scroller, true, true);
	add (vbox);
}

string
window_name (RegionSelection& rs)
{
	string wn ("Tracker Editor:");
	static const unsigned wn_max_size = 64;
	for (RegionSelection::const_iterator it = rs.begin (); it != rs.end (); ++it) {
		wn += " ";
		if (wn.size () <= wn_max_size) {
			RegionPtr region = (*it)->region ();
			wn += region->name ();
		} else {
			wn += "...";
			break;
		}
	}
	return wn;
}

} // ~namespace tracker
