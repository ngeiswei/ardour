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

#ifndef __ardour_tracker_tracker_editor_h_
#define __ardour_tracker_tracker_editor_h_

// TODO: remove useless includes

#include <cmath>

#include <boost/bimap/bimap.hpp>

#include <gtkmm/table.h>
#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>

#include "gtkmm2ext/bindings.h"

#include "evoral/types.hpp"

#include "ardour/session_handle.h"
#include "ardour/automation_list.h"

#include "ardour_window.h"
#include "midi_time_axis.h"
#include "region_selection.h"

#include "tracker_types.h"
#include "main_toolbar.h"
#include "midi_track_toolbar.h"
#include "tracker_grid.h"

namespace Evoral {
	template<typename Time> class Note;
};

namespace MIDI {
namespace Name {
class MasterDeviceNames;
class CustomDeviceMode;
}
}

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
	class Processor;
};

class AutomationTimeAxisView;

class TrackerEditor : public ArdourWindow
{
public:
	TrackerEditor(ARDOUR::Session*, RegionSelection& rs);
	~TrackerEditor();

	boost::shared_ptr<ARDOUR::MidiModel> to_model (boost::shared_ptr<ARDOUR::MidiRegion> midi_region);

	boost::shared_ptr<MIDI::Name::MasterDeviceNames> get_device_names();
	void resize_width ();

	ARDOUR::Session* session;

	// Reference to the unique editor
	PublicEditor& public_editor;

	// List of selected region considered at the creation of this class
	RegionSelection region_selection;

	// Hold tracks, models, views and patterns for each midi track
	std::vector<boost::shared_ptr<ARDOUR::MidiModel> > midi_models; // NEXT TODO: probably doesn't need this
	std::vector<MidiTimeAxisView*> midi_time_axis_views; // NEXT TODO: probably doesn't need this

	Gtk::ScrolledWindow          scroller;
	Gtk::Table                   buttons;
	TrackerGrid                  grid;
	MainToolbar                  main_toolbar;
	std::vector<MidiTrackToolbar*> midi_track_toolbars; // TODO: maybe replace that with a map from track to toolbar
	Gtk::VBox                    vbox;

	/** connection used to connect to model's ContentsChanged signal */
	PBD::ScopedConnectionList content_connections;

private:
	void setup_toolbars ();
	void setup_main_toolbar ();
	void setup_midi_track_toolbars ();
	void setup_grid ();
	void setup_scroller ();
};


// Determine the window name of a selection of regions.
std::string window_name(RegionSelection& rs);

#endif /* __ardour_tracker_tracker_editor_h_ */
