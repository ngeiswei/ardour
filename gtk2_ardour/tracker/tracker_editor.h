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
#include "midi_track_pattern.h"
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

	// Build parameter to automation control map for the give midi track
	void build_param2actrl (Parameter2AutomationControl& param2actrl,
	                        boost::shared_ptr<ARDOUR::MidiTrack> midi_track,
	                        boost::shared_ptr<ARDOUR::MidiModel> midi_model);

	// Build parameter to automation control map for all track
	void build_param2actrls ();
	void add_processor_to_param2actrl (boost::weak_ptr<ARDOUR::Processor> processor, Parameter2AutomationControl& p2a);

	/**
	 * Instantiate mtps with all MidiTrackPatterns
	 */
	void build_patterns ();

	void update_automation_patterns ();
	boost::shared_ptr<MIDI::Name::MasterDeviceNames> get_device_names();
	void automation_connect (const Parameter2AutomationControl& p2a, const Evoral::Parameter&);
	void resize_width ();

	ARDOUR::Session* session;

	// Reference to the unique editor
	PublicEditor& public_editor;

	// List of selected region considered at the creation of this class
	RegionSelection region_selection;

	// Hold regions, tracks, models, views and patterns for each midi track
	std::vector<boost::shared_ptr<ARDOUR::MidiRegion> > midi_regions;
	std::vector<boost::shared_ptr<ARDOUR::MidiTrack> > midi_tracks;
	std::vector<boost::shared_ptr<ARDOUR::MidiModel> > midi_models;
	std::vector<MidiTimeAxisView*> midi_time_axis_views;
	std::vector<MidiTrackPattern*> mtps;

	// Parameter to AutomationControl for each midi track
	std::vector<Parameter2AutomationControl> param2actrls;

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

#endif /* __ardour_tracker_tracker_editor_h_ */
