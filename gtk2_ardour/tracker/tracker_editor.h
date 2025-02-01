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

#ifndef __ardour_tracker_tracker_editor_h_
#define __ardour_tracker_tracker_editor_h_

#include <cmath>

// #include <gtkmm/box.h>
// #include <gtkmm/scrolledwindow.h>
// #include <gtkmm/table.h>

#include "gtkmm2ext/bindings.h"

#include "evoral/types.h"

#include "ardour/automation_list.h"
#include "ardour/session_handle.h"

#include "ardour_window.h"
#include "editor.h"
#include "midi_time_axis.h"
#include "region_selection.h"

#include "audio_track_toolbar.h"
#include "grid.h"
#include "grid_header.h"
#include "main_toolbar.h"
#include "midi_track_toolbar.h"

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

namespace Tracker {

class TrackerEditor : public ArdourWindow
{
public:
	TrackerEditor (ARDOUR::Session*, RegionSelection& rs);
	~TrackerEditor ();

	void setup (RegionSelection& rs);

	void resize_width ();

	void connect_track (TrackPtr track);
	void connect_midi_region (MidiRegionPtr midi_region);
	void connect_automation (AutomationControlPtr actl);

	void connect_midi_event ();
	void disconnect_midi_event ();

	sigc::connection midi_event_connection;

	ARDOUR::Session* session;

	// Reference to the unique editor
	PublicEditor& public_editor;

	// List of selected region considered at the creation of this class
	RegionSelection region_selection;

	Gtk::ScrolledWindow          scroller;
	Grid                         grid;
	MainToolbar                  main_toolbar;
	GridHeader*                  grid_header;
	Gtk::VBox                    vbox;

	/** connection used to connect to model's ContentsChanged signal */
	// TODO: what does it mean?
	PBD::ScopedConnectionList content_connections;

private:
	void set_region_selection (const RegionSelection&);
	void setup_toolbars ();
	void setup_main_toolbar ();
	void setup_grid ();
	void setup_grid_header ();
	void setup_scroller ();
	void setup_vbox ();

	// bool on_key_press_event (GdkEventKey* event);

public:
	// To not redo first time initialization when setup () is called again
	bool _first;
};

// Determine the window name of a selection of regions.
std::string window_name (RegionSelection& rs);

} // ~namespace tracker

#endif /* __ardour_tracker_tracker_editor_h_ */
