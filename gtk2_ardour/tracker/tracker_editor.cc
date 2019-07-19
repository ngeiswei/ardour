/*
    Copyright (C) 2015-2018 Nil Geisweiller

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

#include <cctype>
#include <cmath>
#include <map>

#include <boost/algorithm/string.hpp>

#include <gtkmm/cellrenderercombo.h>

#include "pbd/file_utils.h"
#include "pbd/memento_command.h"

#include "evoral/midi_util.h"
#include "evoral/Note.hpp"

#include "ardour/amp.h"
#include "ardour/beats_samples_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/midi_track.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_playlist.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ui_config.h"
#include "note_player.h"
#include "widgets/tooltips.h"
#include "axis_view.h"
#include "editor.h"
#include "midi_region_view.h"

#include "pbd/i18n.h"

#include "tracker_utils.h"
#include "midi_track_toolbar.h"
#include "tracker_editor.h"

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

// TODO: fix grid focus (use get_focus() to gather information on which widget
//       has a current focus)

TrackerEditor::TrackerEditor (Session* s, RegionSelection& rs)
	: ArdourWindow ("")
	, session(s)
	, public_editor(dynamic_cast<RouteTimeAxisView&>(rs.front()->get_time_axis_view ()).editor ())
	, region_selection (rs)
	, grid (*this)
	, main_toolbar (*this)
	, grid_header (0)
{
	set_session (s);
}

TrackerEditor::~TrackerEditor ()
{
	delete grid_header;
}

void TrackerEditor::setup (RegionSelection& rs)
{
	// VVT: support commulative calls of TrackerEditor::setup

	region_selection = rs;
	set_title (window_name(rs));

	setup_grid ();
	setup_scroller ();
	setup_toolbars ();
	setup_grid_header ();

	grid.redisplay_model ();

	vbox.show ();

	vbox.set_spacing (6);
	vbox.set_border_width (6);
	vbox.pack_start (main_toolbar, false, false);
	vbox.pack_start (*grid_header, false, false);
	vbox.pack_start (scroller, true, true);

	add (vbox);
	set_size_request (-1, 400);

	set_focus(grid);
	// raise(); // TODO: crashes ardour, maybe it would be effective otherwise for getting the focus

	show();

	// To align header and grid
	grid.redisplay_model ();
}

boost::shared_ptr<MidiModel>
TrackerEditor::to_model (boost::shared_ptr<MidiRegion> midi_region)
{
	return midi_region->midi_source(0)->model();
}

void
TrackerEditor::connect_midi_region (boost::shared_ptr<ARDOUR::MidiRegion> midi_region)
{
	// Changing midi content re-render the grid
	boost::shared_ptr<ARDOUR::MidiModel> midi_model = midi_region->midi_source(0)->model();
	midi_model->ContentsChanged.connect (content_connections, invalidator (*this),
	                                     boost::bind (&Grid::redisplay_model, &grid), gui_context());

	// Changing the region time zone re-render the grid
	midi_region->RegionPropertyChanged.connect (content_connections, invalidator (*this),
	                                            boost::bind (&Grid::redisplay_model, &grid), gui_context());
}

void
TrackerEditor::connect_automation (boost::shared_ptr<AutomationControl> actrl)
{
	// TODO: call a more direct redisplay method than redisplay_model to speed up redisplay
	boost::shared_ptr<AutomationList> alist = actrl->alist();
	alist->StateChanged.connect (content_connections, invalidator (*this), boost::bind (&Grid::redisplay_model, &grid), gui_context());
	alist->InterpolationChanged.connect (content_connections, invalidator (*this), boost::bind (&Grid::redisplay_model, &grid), gui_context());
}

void
TrackerEditor::resize_width()
{
	int width, height;
	get_size(width, height);
	resize(1, height);
}

void
TrackerEditor::setup_grid ()
{
	grid.setup();
}

void
TrackerEditor::setup_toolbars ()
{
	main_toolbar.setup ();
}

void
TrackerEditor::setup_grid_header()
{
	delete grid_header;
	grid_header = new GridHeader (*this);
}

void
TrackerEditor::setup_scroller ()
{
	scroller.add (grid);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	scroller.show ();
}

string
window_name(RegionSelection& rs)
{
	string wn("Tracker Editor:");
	static const unsigned wn_max_size = 64;
	for (RegionSelection::const_iterator it = rs.begin(); it != rs.end(); ++it) {
		wn += " ";
		if (wn.size() <= wn_max_size) {
			boost::shared_ptr<ARDOUR::Region> region = (*it)->region();
			wn += region->name();
		} else {
			wn += "...";
			break;
		}
	}
	return wn;
}

} // ~namespace tracker
