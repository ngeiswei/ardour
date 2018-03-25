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
using Timecode::BBT_Time;

//////////
// TODO //
//////////
//
// - [ ] Make sure including audio tracks in the selection doesn't crash
//
// - [ ] Wrap the whole code in a Tracker namespace
//
// - [ ] Add shortcut for parameters, steps, etc
//
// - [ ] Make the non-editing cursor visible
//
// - [ ] Add copy/move, etc notes
//
// - [ ] Use Ardour's logger instead of stdout
//
// - [ ] Support ardour shortcut such as playing the song, etc
//
// - [ ] Add piano keyboard display (see gtk_pianokeyboard.h)
//
// - [ ] Support audio tracks and trim automation
//
// - [ ] Support multiple tracks and regions.
//
// - [ ] Support tab in horizontal_move_cursor (for multi-track grid)

///////////////////
// TrackerEditor //
///////////////////

TrackerEditor::TrackerEditor (ARDOUR::Session* s, RegionSelection& rs)
	: ArdourWindow (dynamic_cast<MidiRegionView*>(rs.front())->midi_region()->name())
	, session(s)
	, public_editor(dynamic_cast<MidiRegionView*>(rs.front())->midi_view ()->editor ())
	, region_selection(rs)
	, grid (*this)
	, main_toolbar (*this)
{
	// TODO: merely for debugging
	for (RegionSelection::const_iterator it = region_selection.begin();
	     it != region_selection.end(); ++it) {
		MidiTimeAxisView* mtav = dynamic_cast<MidiRegionView*>(*it)->midi_view();
		std::cout << "&public_editor = " << &public_editor << std::endl;
		std::cout << "&(mtav->editor()) = " << &(mtav->editor()) << std::endl;
		// As observed route is different per track
		std::cout << "mtav->_route.get() = " << mtav->_route.get() << std::endl;
		// Is device_names per track?
		std::cout << "mtav->get_device_names () = " << mtav->get_device_names ().get() << std::endl;
	}

	/* We do not handle nested sources/regions. Caller should have tackled this */

	// TODO: what to do about that!!!
	// if (region->max_source_level() > 0) {
	// 	throw failed_constructor();
	// }

	set_session (s);

	// Build regions, tracks, midi_models, midi_time_axis_views and routes
	// TODO: for now we assume that there is one region per track
	for (RegionSelection::const_iterator it = region_selection.begin();
	     it != region_selection.end(); ++it) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*it);
		if (mrv) {
			boost::shared_ptr<ARDOUR::MidiRegion> midi_region = mrv->midi_region();
			boost::shared_ptr<ARDOUR::MidiTrack> midi_track = mrv->midi_view()->midi_track();
			boost::shared_ptr<ARDOUR::MidiModel> midi_model = midi_region->midi_source(0)->model();
			MidiTimeAxisView* midi_time_axis_view = mrv->midi_view();
			// TODO: a midi track is also a route, maybe we don't need that!
			boost::shared_ptr<ARDOUR::Route> route = midi_time_axis_view->_route;

			// Make changing midi content re-render the grid
			midi_model->ContentsChanged.connect (content_connections, invalidator (*this),
			                                     boost::bind (&TrackerGrid::redisplay_model, &grid), gui_context());

			// Make changing the region time zone re-render the grid
			midi_region->RegionPropertyChanged.connect (content_connections, invalidator (*this),
			                                       boost::bind (&TrackerGrid::redisplay_model, &grid), gui_context());

			midi_regions.push_back(midi_region);
			midi_tracks.push_back(midi_track);
			midi_models.push_back(midi_model);
			midi_time_axis_views.push_back(midi_time_axis_view);
			routes.push_back(route);
		}
	}

	build_param2actrls ();
	build_patterns ();
	setup_grid ();
	setup_scroller ();
	setup_toolbars ();

	grid.redisplay_model ();

	vbox.show ();

	vbox.set_spacing (6);
	vbox.set_border_width (6);
	vbox.pack_start (main_toolbar, false, false);
	for (std::vector<MidiTrackToolbar*>::iterator it = midi_track_toolbars.begin(); it != midi_track_toolbars.end(); ++it) {
		std::cout << "vbox.pack_start [" << &(**it) << "]" << std::endl;
		vbox.pack_start (**it, false, false);
	}
	vbox.pack_start (scroller, true, true);

	add (vbox);
	set_size_request (-1, 400);
}

TrackerEditor::~TrackerEditor ()
{
	for (std::vector<MidiTrackPattern*>::iterator it = mtps.begin(); it != mtps.end(); ++it)
		delete *it;
	for (std::vector<MidiTrackToolbar*>::iterator it = midi_track_toolbars.begin(); it != midi_track_toolbars.end(); ++it)
		delete *it;
}

boost::shared_ptr<MIDI::Name::MasterDeviceNames>
TrackerEditor::get_device_names ()
{
	// TODO: check whether the device names are per track and upgrade this
	// function to take the track in input
	return midi_time_axis_views.front()->get_device_names ();
}

void
TrackerEditor::build_param2actrl (Parameter2AutomationControl& param2actrl,
                                  boost::shared_ptr<ARDOUR::MidiTrack> midi_track,
                                  boost::shared_ptr<ARDOUR::MidiModel> midi_model,
                                  boost::shared_ptr<ARDOUR::Route> route)
{
	// In case this is a rebuild
	param2actrl.clear();

	// Gain
	param2actrl[Evoral::Parameter(GainAutomation)] =  route->gain_control();
	automation_connect(param2actrl, Evoral::Parameter(GainAutomation));

	// Mute
	param2actrl[Evoral::Parameter(MuteAutomation)] =  route->mute_control();
	automation_connect(param2actrl, Evoral::Parameter(MuteAutomation));

	// Pan
	set<Evoral::Parameter> const & pan_params = route->pannable()->what_can_be_automated ();
	for (set<Evoral::Parameter>::const_iterator p = pan_params.begin(); p != pan_params.end(); ++p) {
		param2actrl[*p] = route->pannable()->automation_control(*p);
		automation_connect(param2actrl, *p);
	}

	// Midi
	const set<Evoral::Parameter> midi_params = midi_track->midi_playlist()->contained_automation();
	for (set<Evoral::Parameter>::const_iterator i = midi_params.begin(); i != midi_params.end(); ++i)
		param2actrl[*i] = midi_model->automation_control(*i);

	// Processors
	route->foreach_processor (sigc::bind (sigc::mem_fun (*this, &TrackerEditor::add_processor_to_param2actrl), param2actrl));
}

void
TrackerEditor::build_param2actrls ()
{
	for (unsigned i = 0; i < midi_tracks.size(); ++i) {
		Parameter2AutomationControl param2actrl;
		build_param2actrl (param2actrl, midi_tracks[i], midi_models[i], routes[i]);
		param2actrls.push_back(param2actrl);
	}
}

void
TrackerEditor::add_processor_to_param2actrl(boost::weak_ptr<ARDOUR::Processor> p, Parameter2AutomationControl& p2a)
{
	boost::shared_ptr<ARDOUR::Processor> processor (p.lock ());

	if (!processor || !processor->display_to_user ())
		return;

	const std::set<Evoral::Parameter>& automatable = processor->what_can_be_automated ();
	for (std::set<Evoral::Parameter>::const_iterator ait = automatable.begin(); ait != automatable.end(); ++ait) {
		p2a[*ait] = boost::dynamic_pointer_cast<AutomationControl>(processor->control(*ait));
		automation_connect(p2a, *ait);
	}
}

void
TrackerEditor::automation_connect (const Parameter2AutomationControl& p2a, const Evoral::Parameter& param)
{
	Parameter2AutomationControl::const_iterator it = p2a.find(param);
	boost::shared_ptr<AutomationList> alist = it->second->alist();
	alist->StateChanged.connect (content_connections, invalidator (*this), boost::bind (&TrackerGrid::redisplay_model, &grid), gui_context());
	alist->InterpolationChanged.connect (content_connections, invalidator (*this), boost::bind (&TrackerGrid::redisplay_model, &grid), gui_context());
}

void
TrackerEditor::resize_width()
{
	int width, height;
	get_size(width, height);
	resize(1, height);
}

void
TrackerEditor::build_patterns ()
{
	for (unsigned i = 0; i < midi_regions.size(); i++) {
		MidiTrackPattern* mtp = new MidiTrackPattern(_session, midi_regions[i]);
		mtps.push_back(mtp);
	}

	update_automation_patterns ();
}

void
TrackerEditor::update_automation_patterns ()
{
	for (unsigned i = 0; i < param2actrls.size(); i++) {
		MidiTrackPattern* mtp = mtps[i];
		// Insert automation controls in the automation patterns
		for (Parameter2AutomationControl::const_iterator it = param2actrls[i].begin(); it != param2actrls[i].end(); ++it) {
			// Midi automation are attached to the region, not the track
			if (TrackerUtils::is_region_automation (it->first))
				mtp->rap.insert(it->second);
			else
				mtp->tap.insert(it->second);
		}
	}
}

void
TrackerEditor::setup_toolbars ()
{
	for (unsigned i = 0; i < midi_tracks.size(); i++) {
		Parameter2AutomationControl& param2actrl = param2actrls[i];
		boost::shared_ptr<ARDOUR::MidiTrack> midi_track = midi_tracks[i];
		boost::shared_ptr<ARDOUR::MidiModel> midi_model = midi_models[i];
		boost::shared_ptr<ARDOUR::Route> route = routes[i];
		MidiTrackToolbar* mttb = new MidiTrackToolbar (*this, param2actrl, midi_track, midi_model, route, *mtps[i]);
		// TODO replace that by emplace_back when supports C++11
		midi_track_toolbars.push_back(mttb);
	}

	main_toolbar.setup ();
	setup_midi_track_toolbars ();
}

void
TrackerEditor::setup_midi_track_toolbars ()
{
	for (unsigned i = 0; i < midi_track_toolbars.size(); i++) {
		std::cout << "TrackerEditor::setup_midi_track_toolbars () midi_track_toolbars[" << i << "] = " << midi_track_toolbars[i] << std::endl;
		midi_track_toolbars[i]->setup_processor_menu_and_curves ();
		midi_track_toolbars[i]->setup ();
	}
}

void
TrackerEditor::setup_grid ()
{
	grid.mtp = mtps.front();
	grid.route = routes.front();
	grid.setup ();
}

void
TrackerEditor::setup_scroller ()
{
	scroller.add (grid);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	scroller.show ();
}
