/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2007-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2018 Ben Loftis <ben@harrisonconsoles.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <fcntl.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>
#include <iomanip>

#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>

#include <glibmm/miscutils.h>

#include "midi++/types.h"
#include "midi++/port.h"
#include "midi++/ipmidi_port.h"
#include "pbd/pthread_utils.h"
#include "pbd/error.h"
#include "pbd/memento_command.h"
#include "pbd/convert.h"

#include "ardour/audio_track.h"
#include "ardour/automation_control.h"
#include "ardour/async_midi_port.h"
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/location.h"
#include "ardour/meter.h"
#include "ardour/midi_track.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/profile.h"
#include "ardour/record_enable_control.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/track.h"
#include "ardour/types.h"
#include "ardour/audioengine.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "temporal/tempo.h"

#include "mackie_control_protocol.h"

#include "midi_byte_array.h"
#include "mackie_control_exception.h"
#include "device_profile.h"
#include "subview.h"
#include "surface_port.h"
#include "surface.h"
#include "strip.h"
#include "control_group.h"
#include "meter.h"
#include "button.h"
#include "fader.h"
#include "pot.h"

#ifndef G_SOURCE_FUNC
#define G_SOURCE_FUNC(f) ((GSourceFunc) (void (*)(void)) (f))
#endif

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace ArdourSurface::MACKIE_NAMESPACE;

#include "pbd/i18n.h"

#include "pbd/abstract_ui.inc.cc" // instantiate template

const int MACKIE_NAMESPACE::MackieControlProtocol::MODIFIER_OPTION = 0x1;
const int MACKIE_NAMESPACE::MackieControlProtocol::MODIFIER_CONTROL = 0x2;
const int MACKIE_NAMESPACE::MackieControlProtocol::MODIFIER_SHIFT = 0x4;
const int MACKIE_NAMESPACE::MackieControlProtocol::MODIFIER_CMDALT = 0x8;
const int MACKIE_NAMESPACE::MackieControlProtocol::MODIFIER_ZOOM = 0x10;
const int MACKIE_NAMESPACE::MackieControlProtocol::MODIFIER_SCRUB = 0x20;
const int MACKIE_NAMESPACE::MackieControlProtocol::MODIFIER_MARKER = 0x40;
const int MACKIE_NAMESPACE::MackieControlProtocol::MODIFIER_NUDGE = 0x80;
const int MACKIE_NAMESPACE::MackieControlProtocol::MAIN_MODIFIER_MASK = (MackieControlProtocol::MODIFIER_OPTION|
						       MackieControlProtocol::MODIFIER_CONTROL|
						       MackieControlProtocol::MODIFIER_SHIFT|
						       MackieControlProtocol::MODIFIER_CMDALT);

MACKIE_NAMESPACE::MackieControlProtocol* MACKIE_NAMESPACE::MackieControlProtocol::_instance = 0;

MackieControlProtocol::MackieControlProtocol (Session& session, const char* pname)
	: ControlProtocol (session, pname)
	, AbstractUI<MackieControlUIRequest> (name())
	, _current_initial_bank (0)
	, _timecode_last (10, '\0')
	, _sample_last (0)
	, _timecode_type (ARDOUR::AnyTime::BBT)
	, _gui (0)
	, _scrub_mode (false)
	, _flip_mode (Normal)
	, _view_mode (Mixer)
	, _current_selected_track (-1)
	, _modifier_state (0)
	, _ipmidi_base (MIDI::IPMIDIPort::lowest_ipmidi_port_default)
	, needs_ipmidi_restart (false)
	, _metering_active (true)
	, _initialized (false)
	, configuration_state (0)
	, state_version (0)
	, marker_modifier_consumed_by_button (false)
	, nudge_modifier_consumed_by_button (false)
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::MackieControlProtocol\n");

	_subview = MACKIE_NAMESPACE::SubviewFactory::instance()->create_subview(Subview::None, *this, std::shared_ptr<Stripable>());

	DeviceInfo::reload_device_info ();
	DeviceProfile::reload_device_profiles ();

	for (int i = 0; i < 9; i++) {
		_last_bank[i] = 0;
	}

	PresentationInfo::Change.connect (gui_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_presentation_info_changed, this, _1), this);

	_instance = this;

	build_button_map ();
}

MackieControlProtocol::~MackieControlProtocol()
{
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol init\n");

	for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
		(*si)->reset ();
	}

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol drop_connections ()\n");
	drop_connections ();

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol tear_down_gui ()\n");
	tear_down_gui ();

	delete configuration_state;

	/* stop event loop */
	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol BaseUI::quit ()\n");
	BaseUI::quit ();

	try {
		DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol close()\n");
		close();
	}
	catch (exception & e) {
		cout << "~MackieControlProtocol caught " << e.what() << endl;
	}
	catch (...) {
		cout << "~MackieControlProtocol caught unknown" << endl;
	}

	_instance = 0;

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::~MackieControlProtocol done\n");
}

void
MackieControlProtocol::thread_init ()
{
	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	set_thread_priority ();
}

// go to the previous track.
void
MackieControlProtocol::prev_track()
{
	if (_current_initial_bank >= 1) {
		switch_banks (_current_initial_bank - 1);
	}
}

// go to the next track.
void
MackieControlProtocol::next_track()
{
	Sorted sorted = get_sorted_stripables();
	if (_current_initial_bank + 1 < sorted.size()) {
		switch_banks (_current_initial_bank + 1);
	}
}

bool
MackieControlProtocol::stripable_is_locked_to_strip (std::shared_ptr<Stripable> r) const
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
			if ((*si)->stripable_is_locked_to_strip (r)) {
				return true;
			}
		}
	}
	return false;
}

// predicate for sort call in get_sorted_stripables
struct StripableByPresentationOrder
{
	bool operator () (const std::shared_ptr<Stripable> & a, const std::shared_ptr<Stripable> & b) const
	{
		return a->presentation_info().order() < b->presentation_info().order();
	}

	bool operator () (const Stripable & a, const Stripable & b) const
	{
		return a.presentation_info().order() < b.presentation_info().order();
	}

	bool operator () (const Stripable * a, const Stripable * b) const
	{
		return a->presentation_info().order() < b->presentation_info().order();
	}
};

MackieControlProtocol::Sorted
MackieControlProtocol::get_sorted_stripables()
{
	Sorted sorted;

	// fetch all stripables
	StripableList stripables;

	session->get_stripables (stripables, PresentationInfo::AllStripables);

	// sort in presentation order, and exclude master, control and hidden stripables
	// and any stripables that are already set.

	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {

		std::shared_ptr<Stripable> s = *it;

		if (s->presentation_info().special()) {
			continue;
		}

		/* don't include locked routes */

		if (stripable_is_locked_to_strip (s)) {
			continue;
		}

		switch (_view_mode) {
		case Mixer:
			if (!s->presentation_info().hidden()) {
				sorted.push_back (s);
			}
			break;
		case AudioTracks:
			if (is_audio_track(s) && !s->presentation_info().hidden()) {
				sorted.push_back (s);
			}
			break;
		case Busses:
			if (Profile->get_mixbus()) {
#ifdef MIXBUS
				if (s->mixbus()) {
					sorted.push_back (s);
				}
#endif
			} else {
				if (!is_track (s) && !is_vca (s) && !is_foldback_bus (s)  && !s->presentation_info().hidden()) {
					sorted.push_back (s);
				}
			}
			break;
		case MidiTracks:
			if (is_midi_track(s) && !s->presentation_info().hidden()) {
				sorted.push_back (s);
			}
			break;
		case Auxes: // in ardour, for now aux and buss are same. for mixbus, "Busses" are mixbuses, "Auxes" are ardour buses
#ifdef MIXBUS
			if (!s->mixbus() && !is_track(s) && !is_vca (s) && !is_foldback_bus (s) && !s->presentation_info().hidden())
#else
			if (!is_track (s) && !is_vca (s) && !is_foldback_bus (s) && !s->presentation_info().hidden())
#endif
			{
				sorted.push_back (s);
			}
			break;
		case Outputs:
			if (is_foldback_bus (s) && !s->presentation_info().hidden()) {
				sorted.push_back (s);
			}
			break;
		case Selected: // For example: a group (this is USER)
			if (s->is_selected() && !s->presentation_info().hidden()) {
				sorted.push_back (s);
			}
			break;
		case AudioInstr:
			if (is_vca (s)){
				sorted.push_back (s);
			}
			break;
		case Inputs:
			if (is_trigger_track (s) && !s->presentation_info().hidden()){
				sorted.push_back (s);
			}
			break;
		}
	}

	sort (sorted.begin(), sorted.end(), StripableByPresentationOrder());
	return sorted;
}

void
MackieControlProtocol::refresh_current_bank()
{
	switch_banks (_current_initial_bank, true);
}

uint32_t
MackieControlProtocol::n_strips (bool with_locked_strips) const
{
	uint32_t strip_count = 0;

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		for (Surfaces::const_iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
			strip_count += (*si)->n_strips (with_locked_strips);
		}
	}

	return strip_count;
}

int
MackieControlProtocol::switch_banks (uint32_t initial, bool force)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("switch banking to start at %1 force ? %2 current = %3\n", initial, force, _current_initial_bank));

	if (initial == _current_initial_bank && !force) {
		/* everything is as it should be */
		return 0;
	}

	Sorted sorted = get_sorted_stripables();
	uint32_t strip_cnt = n_strips (false); // do not include locked strips
					       // in this count

	if (initial >= sorted.size() && !force) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("bank target %1 exceeds route range %2\n",
		                                                   _current_initial_bank, sorted.size()));
		/* too high, we can't get there */
		return -1;
	}

	if (sorted.size() <= strip_cnt && _current_initial_bank == 0 && !force) {
		/* no banking - not enough stripables to fill all strips and we're
		 * not at the first one.
		 */
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("less routes (%1) than strips (%2) and we're at the end already (%3)\n",
		                                                   sorted.size(), strip_cnt, _current_initial_bank));
		return -1;
	}

	_current_initial_bank = initial;
	_current_selected_track = -1;

	// Map current bank of stripables onto each surface(+strip)

	if (_current_initial_bank < sorted.size()) {

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("switch to %1, %2, available stripables %3 on %4 surfaces\n",
								   _current_initial_bank, strip_cnt, sorted.size(),
								   surfaces.size()));

		// link stripables to strips

		Sorted::iterator r = sorted.begin() + _current_initial_bank;

		{
			Glib::Threads::Mutex::Lock lm (surfaces_lock);
			for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
				vector<std::shared_ptr<Stripable> > stripables;
				uint32_t added = 0;

				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("surface has %1 unlocked strips\n", (*si)->n_strips (false)));

				for (; r != sorted.end() && added < (*si)->n_strips (false); ++r, ++added) {
					stripables.push_back (*r);
				}

				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("give surface %1 stripables\n", stripables.size()));

				(*si)->map_stripables (stripables);
			}
		}

	} else {
		/* all strips need to be reset */
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("clear all strips, bank target %1  is outside route range %2\n",
		                                                   _current_initial_bank, sorted.size()));
		{
			Glib::Threads::Mutex::Lock lm (surfaces_lock);
			for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
				vector<std::shared_ptr<Stripable> > stripables;
				/* pass in an empty stripables list, so that all strips will be reset */
				(*si)->map_stripables (stripables);
			}
		}
		return -1;
	}

	/* current bank has not been saved */
	session->set_dirty();

	return 0;
}

int
MackieControlProtocol::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose("MackieControlProtocol::set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		/* start event loop */

		BaseUI::run ();

		connect_session_signals ();

		if (!_device_info.name().empty()) {
			set_device (_device_info.name(), true);
		}

		/* set up periodic task for timecode display and metering and automation
		 */

		// set different refresh time for qcon and standard mackie MCU

		int iTimeCodeRefreshTime = 100; // default value for mackie MCU (100ms)
		int iStripDisplayRefreshTime = 10; // default value for Mackie MCU (10ms)

		if(_device_info.is_qcon()){
			// set faster timecode display refresh speed (55ms)
			iTimeCodeRefreshTime = 55;
			// set slower refresh time on qcon than on mackie (15ms)
			iStripDisplayRefreshTime = 15;
		}

		Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (iTimeCodeRefreshTime); // milliseconds
		periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::periodic));
		periodic_timeout->attach (main_loop()->get_context());

		/* periodic task used to update strip displays */

		Glib::RefPtr<Glib::TimeoutSource> redisplay_timeout = Glib::TimeoutSource::create (iStripDisplayRefreshTime); // milliseconds
		redisplay_connection = redisplay_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::redisplay));
		redisplay_timeout->attach (main_loop()->get_context());

		notify_transport_state_changed ();

	} else {

		BaseUI::quit ();
		close ();

	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose("MackieControlProtocol::set_active done with yn: '%1'\n", yn));

	return 0;
}

bool
MackieControlProtocol::hui_heartbeat ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->hui_heartbeat ();
	}

	return true;
}

bool
MackieControlProtocol::periodic ()
{
	if (!active()) {
		return false;
	}

	if (!_initialized) {
		/* wait for higher-frequency redisplay() callback to initialize
		 * us
		 */
		return true;
	}

	update_timecode_display ();

	PBD::microseconds_t now_usecs = PBD::get_microseconds ();

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->periodic (now_usecs);
		}
	}

	return true;
}

bool
MackieControlProtocol::redisplay ()
{
	if (!active()) {
		return false;
	}

	if (needs_ipmidi_restart) {
		ipmidi_restart ();
		return true;
	}

	if (!_initialized) {
		initialize();
	}

	PBD::microseconds_t now = PBD::get_microseconds ();

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->redisplay (now, false);
		}
	}

	return true;
}

void
MackieControlProtocol::update_timecode_beats_led()
{
	if (!_device_info.has_timecode_display()) {
		return;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose("MackieControlProtocol::update_timecode_beats_led(): %1\n", _timecode_type));
	switch (_timecode_type) {
		case ARDOUR::AnyTime::BBT:
			update_global_led (Led::Beats, on);
			update_global_led (Led::Timecode, off);
			break;
		case ARDOUR::AnyTime::Timecode:
			update_global_led (Led::Timecode, on);
			update_global_led (Led::Beats, off);
			break;
		default:
			ostringstream os;
			os << "Unknown Anytime::Type " << _timecode_type;
			throw runtime_error (os.str());
	}
}

void
MackieControlProtocol::update_global_button (int id, MACKIE_NAMESPACE::LedState ls)
{
	std::shared_ptr<MACKIE_NAMESPACE::Surface> surface;

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (!_master_surface) {
			return;
		}

		if (!_device_info.has_global_controls()) {
			return;
		}
		// surface needs to be master surface
		surface = _master_surface;
	}

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (id);
	if (x != surface->controls_by_device_independent_id.end()) {
		Button * button = dynamic_cast<Button*> (x->second);
		surface->write (button->set_state (ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Button %1 not found\n", id));
	}
}

void
MackieControlProtocol::update_global_led (int id, MACKIE_NAMESPACE::LedState ls)
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	if (surfaces.empty()) {
		return;
	}

	if (!_device_info.has_global_controls()) {
		return;
	}
	std::shared_ptr<MACKIE_NAMESPACE::Surface> surface = _master_surface;

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (id);

	if (x != surface->controls_by_device_independent_id.end()) {
		Led * led = dynamic_cast<Led*> (x->second);
		DEBUG_TRACE (DEBUG::MackieControl, "Writing LedState\n");
		surface->write (led->set_state (ls));
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Led %1 not found\n", id));
	}
}

void
MackieControlProtocol::device_ready ()
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("device ready init (active=%1)\n", active()));
	// Clear the surface so that any left over control from other programs are reset.
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->zero_all ();
		}
	}
	update_surfaces ();
	set_subview_mode (MACKIE_NAMESPACE::Subview::None, std::shared_ptr<Stripable>());
	set_flip_mode (Normal);
}

// send messages to surface to set controls to correct values
void
MackieControlProtocol::update_surfaces()
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieControlProtocol::update_surfaces() init (active=%1)\n", active()));
	if (!active()) {
		return;
	}

	// do the initial bank switch to connect signals
	// _current_initial_bank is initialised by set_state
	(void) switch_banks (_current_initial_bank, true);

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::update_surfaces() finished\n");
}

void
MackieControlProtocol::initialize()
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}

		if (!_master_surface) {
			return;
		}

		if (!_master_surface->active ()) {
			return;
		}

		// sometimes the jog wheel is a pot
		if (_device_info.has_jog_wheel()) {
			_master_surface->blank_jog_ring ();
		}
	}

	// update global buttons and displays

	notify_record_state_changed();
	notify_transport_state_changed();
	update_timecode_beats_led();

	_initialized = true;
}

void
MackieControlProtocol::connect_session_signals()
{
	// receive routes added
	session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_routes_added, this, _1), this);
	// receive VCAs added
	session->vca_manager().VCAAdded.connect(session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_vca_added, this, _1), this);

	// receive record state toggled
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_record_state_changed, this), this);
	// receive transport state changed
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_loop_state_changed, this), this);
	// receive punch-in and punch-out
	Config->ParameterChanged.connect(session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_parameter_changed, this, _1), this);
	// receive rude solo changed
	session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_solo_active_changed, this, _1), this);

	session->MonitorBusAddedOrRemoved.connect (session_connections, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::notify_monitor_added_or_removed, this), this);

	// make sure remote id changed signals reach here
	// see also notify_stripable_added
	Sorted sorted = get_sorted_stripables();
}

void
MackieControlProtocol::set_profile (const string& profile_name)
{
	map<string,DeviceProfile>::iterator d = DeviceProfile::device_profiles.find (profile_name);

	if (d == DeviceProfile::device_profiles.end()) {
		_device_profile = DeviceProfile (profile_name);
		return;
	}

	_device_profile = d->second;
}

int
MackieControlProtocol::set_device_info (const string& device_name)
{
	map<string,DeviceInfo>::iterator d = DeviceInfo::device_info.find (device_name);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("new device chosen %1\n", device_name));

	if (d == DeviceInfo::device_info.end()) {
		return -1;
	}

	_device_info = d->second;

	return 0;
}

int
MackieControlProtocol::set_device (const string& device_name, bool force)
{
	if (device_name == device_info().name() && !force) {
		/* already using that device, nothing to do */
		return 0;
	}
	/* get state from the current setup, and make sure it is stored in
	   the configuration_states node so that if we switch back to this device,
	   we will have its state available.
	*/

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		if (!surfaces.empty()) {
			update_configuration_state ();
		}
	}

	if (set_device_info (device_name)) {
		return -1;
	}

	clear_surfaces ();
	port_connection.disconnect ();
	hui_connection.disconnect ();

	if (_device_info.device_type() == DeviceInfo::HUI) {
		Glib::RefPtr<Glib::TimeoutSource> hui_timeout = Glib::TimeoutSource::create (1000); // milliseconds
		hui_connection = hui_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::hui_heartbeat));
		hui_timeout->attach (main_loop()->get_context());
	}

	if (!_device_info.uses_ipmidi()) {
		/* notice that the handler for this will execute in our event
		   loop, not in the thread where the
		   PortConnectedOrDisconnected signal is emitted.
		*/
		ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, std::bind (&MackieControlProtocol::connection_handler, this, _1, _2, _3, _4, _5), this);
	}

	build_button_map();

	if (create_surfaces ()) {
		return -1;
	}

	DeviceChanged ();

	return 0;
}

gboolean
ArdourSurface::MACKIE_NAMESPACE::ipmidi_input_handler (GIOChannel*, GIOCondition condition, void *data)
{
	MackieControlProtocol::ipMIDIHandler* ipm = static_cast<MackieControlProtocol::ipMIDIHandler*>(data);
	return ipm->mcp->midi_input_handler (Glib::IOCondition (condition), ipm->port);
}

int
MackieControlProtocol::create_surfaces ()
{
	string device_name;
	surface_type_t stype = mcu; // type not yet determined

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Create %1 surfaces for %2\n", 1 + _device_info.extenders(), _device_info.name()));

	if (!_device_info.uses_ipmidi()) {
		_input_bundle.reset (new ARDOUR::Bundle (_("Mackie Control In"), true));
		_output_bundle.reset (new ARDOUR::Bundle (_("Mackie Control Out"), false));
	} else {
		_input_bundle.reset ();
		_output_bundle.reset ();

	}
	for (uint32_t n = 0; n < 1 + _device_info.extenders(); ++n) {
		bool is_master = false;

		if (n == _device_info.master_position()) {
			is_master = true;
			if (_device_info.extenders() == 0) {
				device_name = _device_info.name();
			} else {
#ifdef UF8
				device_name = X_("SSL-UFx");
#else
				device_name = X_("mackie control");
#endif
			}

		}

		if (!is_master) {
			device_name = string_compose (X_("mackie control ext %1"), n+1);
		}

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Port Name for surface %1 is %2\n", n, device_name));

		std::shared_ptr<MACKIE_NAMESPACE::Surface> surface;

		if (is_master) {
			stype = mcu;
		} else {
			stype = ext;
		}
		try {
			surface.reset (new MACKIE_NAMESPACE::Surface (*this, device_name, n, stype));
		} catch (...) {
			return -1;
		}

		if (configuration_state) {
			XMLNode* this_device = 0;
			XMLNodeList const& devices = configuration_state->children();
			for (XMLNodeList::const_iterator d = devices.begin(); d != devices.end(); ++d) {
				XMLProperty const * prop = (*d)->property (X_("name"));
				if (prop && prop->value() == _device_info.name()) {
					this_device = *d;
					break;
				}
			}
			if (this_device) {
				XMLNode* snode = this_device->child (X_("Surfaces"));
				if (snode) {
					surface->set_state (*snode, state_version);
				}
			}
		}

		{
			Glib::Threads::Mutex::Lock lm (surfaces_lock);
			if (is_master) {
				_master_surface = surface;
			}
			surfaces.push_back (surface);
		}

		if (!_device_info.uses_ipmidi()) {

			_input_bundle->add_channel (
				"",
				ARDOUR::DataType::MIDI,
				session->engine().make_port_name_non_relative (surface->port().input_port().name())
				);

			_output_bundle->add_channel (
				"",
				ARDOUR::DataType::MIDI,
				session->engine().make_port_name_non_relative (surface->port().output_port().name())
				);
		}

		MIDI::Port& input_port (surface->port().input_port());
		AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*> (&input_port);

		if (asp) {

			/* async MIDI port */

			asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &MackieControlProtocol::midi_input_handler), &input_port));
			asp->xthread().attach (main_loop()->get_context());

		} else {

			/* ipMIDI port, no IOSource method at this time */

			int fd;

			if ((fd = input_port.selectable ()) >= 0) {

				GIOChannel* ioc = g_io_channel_unix_new (fd);
				surface->input_source = g_io_create_watch (ioc, GIOCondition (G_IO_IN|G_IO_HUP|G_IO_ERR));

				/* make surface's input source now hold the
				 * only reference on the IO channel
				 */
				g_io_channel_unref (ioc);

				/* hack up an object so that in the callback from the event loop
				   we have both the MackieControlProtocol and the input port.

				   If we were using C++ for this stuff we wouldn't need this
				   but a nasty, not-fixable bug in the binding between C
				   and C++ makes it necessary to avoid C++ for the IO
				   callback setup.
				*/

				ipMIDIHandler* ipm = new ipMIDIHandler (); /* we will leak this sizeof(pointer)*2 sized object */
				ipm->mcp = this;
				ipm->port = &input_port;

				g_source_set_callback (surface->input_source, G_SOURCE_FUNC (ipmidi_input_handler), ipm, NULL);
				g_source_attach (surface->input_source, main_loop()->get_context()->gobj());
			}
		}
	}

	if (!_device_info.uses_ipmidi()) {
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->port().reconnect ();
		}
	}

	session->BundleAddedOrRemoved ();

	assert (_master_surface);

	return 0;
}

void
MackieControlProtocol::close()
{
	port_connection.disconnect ();
	session_connections.drop_connections ();
	stripable_connections.drop_connections ();
	periodic_connection.disconnect ();

	clear_surfaces();
}

/** Ensure that the configuration_state XML node contains an up-to-date
 *  copy of the state node the current device. If configuration_state already
 *  contains a state node for the device, it will deleted and replaced.
 */
void
MackieControlProtocol::update_configuration_state () const
{
	/* CALLER MUST HOLD SURFACES LOCK */

	if (!configuration_state) {
		configuration_state = new XMLNode (X_("Configurations"));
	}

	XMLNode* devnode = new XMLNode (X_("Configuration"));
	devnode->set_property (X_("name"), _device_info.name());

	configuration_state->remove_nodes_and_delete (X_("name"), _device_info.name());
	configuration_state->add_child_nocopy (*devnode);

	XMLNode* snode = new XMLNode (X_("Surfaces"));

	for (auto const & s : surfaces) {
		snode->add_child_nocopy (s->get_state());
	}

	devnode->add_child_nocopy (*snode);
}

XMLNode&
MackieControlProtocol::get_state() const
{
	XMLNode& node (ControlProtocol::get_state());

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::get_state init\n");

	// add current bank
	node.set_property (X_("bank"), _current_initial_bank);

	// ipMIDI base port (possibly not used)
	node.set_property (X_("ipmidi-base"), _ipmidi_base);

	node.set_property (X_("device-profile"), _device_profile.name());
	node.set_property (X_("device-name"), _device_info.name());

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		update_configuration_state ();
	}

	/* force a copy of the _surfaces_state node, because we want to retain ownership */
	node.add_child_copy (*configuration_state);

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::get_state done\n");

	return node;
}

bool
MackieControlProtocol::profile_exists (string const & name) const
{
	return DeviceProfile::device_profiles.find (name) != DeviceProfile::device_profiles.end();
}

int
MackieControlProtocol::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieControlProtocol::set_state: active %1\n", active()));

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	uint16_t ipmidi_base;
	if (node.get_property (X_("ipmidi-base"), ipmidi_base)) {
		set_ipmidi_base (ipmidi_base);
	}

	uint32_t bank = 0;
	// fetch current bank
	node.get_property (X_("bank"), bank);

	std::string device_name;
	if (node.get_property (X_("device-name"), device_name)) {
		set_device_info (device_name);
	}

	std::string device_profile_name;
	if (node.get_property (X_("device-profile"), device_profile_name)) {
		if (device_profile_name.empty()) {
			string default_profile_name;

			/* start by looking for a user-edited profile for the current device name */

			default_profile_name = DeviceProfile::name_when_edited (_device_info.name());

			if (!profile_exists (default_profile_name)) {

				/* no user-edited profile for this device name, so try the user-edited default profile */

				default_profile_name = DeviceProfile::name_when_edited (DeviceProfile::default_profile_name);

				if (!profile_exists (default_profile_name)) {

					/* no user-edited version, so just try the device name */

					default_profile_name = _device_info.name();

					if (!profile_exists (default_profile_name)) {

						/* no generic device specific profile, just try the fixed default */
						default_profile_name = DeviceProfile::default_profile_name;
					}
				}
			}

			set_profile (default_profile_name);

		} else {
			if (profile_exists (device_profile_name)) {
				set_profile (device_profile_name);
			} else {
				set_profile (DeviceProfile::default_profile_name);
			}
		}
	}

	XMLNode* dnode = node.child (X_("Configurations"));

	delete configuration_state;
	configuration_state = 0;

	if (dnode) {
		configuration_state = new XMLNode (*dnode);
		state_version = version;
	}

	(void) switch_banks (bank, true);

	DEBUG_TRACE (DEBUG::MackieControl, "MackieControlProtocol::set_state done\n");

	return 0;
}

string
MackieControlProtocol::format_bbt_timecode (samplepos_t now_sample)
{
	Temporal::BBT_Time bbt_time;

	bbt_time = Temporal::TempoMap::fetch()->bbt_at (timepos_t (now_sample));

	// The Mackie protocol spec is built around a BBT time display of
	//
	// digits:     888/88/88/888
	// semantics:  BBB/bb/ss/ttt
	//
	// The third field is "subdivisions" which is a concept found in Logic
	// but not present in Ardour. Instead Ardour displays a 4 digit tick
	// count, which we need to spread across the 5 digits of ss/ttt.

	ostringstream os;

	os << setw(3) << setfill('0') << bbt_time.bars;
	os << setw(2) << setfill('0') << bbt_time.beats;
	os << ' ';
	os << setw(1) << setfill('0') << bbt_time.ticks / 1000;
	os << setw(3) << setfill('0') << bbt_time.ticks % 1000;

	return os.str();
}

string
MackieControlProtocol::format_timecode_timecode (samplepos_t now_sample)
{
	Timecode::Time timecode;
	session->timecode_time (now_sample, timecode);

	// According to the Logic docs
	// digits: 888/88/88/888
	// Timecode mode: Hours/Minutes/Seconds/Samples
	ostringstream os;
	os << setw(2) << setfill('0') << timecode.hours;
	os << ' ';
	os << setw(2) << setfill('0') << timecode.minutes;
	os << setw(2) << setfill('0') << timecode.seconds;
	os << ' ';
	os << setw(2) << setfill('0') << timecode.frames;

	return os.str();
}

void
MackieControlProtocol::update_timecode_display()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	if (surfaces.empty()) {
		return;
	}

	std::shared_ptr<MACKIE_NAMESPACE::Surface> surface = _master_surface;

	if (surface->type() != mcu || !_device_info.has_timecode_display() || !surface->active ()) {
		return;
	}

	string timecode;

	// do assignment here so current_sample is fixed
	samplepos_t current_sample = session->transport_sample();
	// For large jumps in play head possition do full reset
	int moved = (current_sample - _sample_last) / session->sample_rate ();
	if (moved) {
		DEBUG_TRACE (DEBUG::MackieControl, "Timecode reset\n");
		_timecode_last = string (10, '\0');
	}
	_sample_last = current_sample;

	switch (_timecode_type) {
	case ARDOUR::AnyTime::BBT:
		timecode = format_bbt_timecode (current_sample);
		break;
	case ARDOUR::AnyTime::Timecode:
		timecode = format_timecode_timecode (current_sample);
		break;
	default:
		return;
	}

	// only write the timecode string to the MCU if it's changed
	// since last time. This is to reduce midi bandwidth used.
	if (timecode != _timecode_last) {
		surface->display_timecode (timecode, _timecode_last);
		_timecode_last = timecode;
	}
}

///////////////////////////////////////////
// Session signals
///////////////////////////////////////////

void MackieControlProtocol::notify_parameter_changed (std::string const & p)
{
	if (p == "punch-in") {
		update_global_button (Button::Drop, session->config.get_punch_in() ? flashing : off);
	} else if (p == "punch-out") {
		update_global_button (Button::Replace, session->config.get_punch_out() ? flashing : off);
	} else if (p == "clicking") {
		update_global_button (Button::Click, Config->get_clicking());
	} else if (p == "follow-edits") {
		/* we can't respond to this at present, because "follow-edits"
		 * is  a property of the (G)UI configuration object, to which we
		 * have no access. For now, this means that the lit state of
		 * this button (if there is one) won't reflect the setting.
		 */

		//update_global_button (Button::Enter, session->config.get_follow_edits() ? on : off);
	} else if (p == "external-sync") {
		update_global_button (Button::Cancel, session->config.get_external_sync() ? on : off);
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("parameter changed: %1\n", p));
	}
}

void
MackieControlProtocol::notify_vca_added (ARDOUR::VCAList& vl)
{
	refresh_current_bank ();
}

// RouteList is the set of Routes that have just been added
void
MackieControlProtocol::notify_routes_added (ARDOUR::RouteList & rl)
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}
	}

	// currently assigned banks are less than the full set of
	// strips, so activate the new strip now.

	refresh_current_bank();

	// otherwise route added, but current bank needs no updating
}

void
MackieControlProtocol::notify_monitor_added_or_removed ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);
	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->master_monitor_may_have_changed ();
	}
}

void
MackieControlProtocol::notify_solo_active_changed (bool active)
{
	std::shared_ptr<MACKIE_NAMESPACE::Surface> surface;

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}

		surface = _master_surface;
	}

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (Led::RudeSolo);
	if (x != surface->controls_by_device_independent_id.end()) {
		Led* rude_solo = dynamic_cast<Led*> (x->second);
		if (rude_solo) {
			update_global_button (Button::ClearSolo, active);
			surface->write (rude_solo->set_state (active ? flashing : off));
		}
	}
}

void
MackieControlProtocol::notify_presentation_info_changed (PBD::PropertyChange const & what_changed)
{
	PBD::PropertyChange order_or_hidden;

	order_or_hidden.add (Properties::hidden);
	order_or_hidden.add (Properties::order);

	if (!what_changed.contains (order_or_hidden)) {
		return;
	}

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		if (surfaces.empty()) {
			return;
		}
	}

	refresh_current_bank();
}

///////////////////////////////////////////
// Transport signals
///////////////////////////////////////////

void
MackieControlProtocol::notify_loop_state_changed()
{
	update_global_button (Button::Loop, session->get_play_loop());
}

void
MackieControlProtocol::notify_transport_state_changed()
{
	if (!_device_info.has_global_controls()) {
		return;
	}

	// switch various play and stop buttons on / off
	update_global_button (Button::Loop, loop_button_onoff ());
	update_global_button (Button::Play, play_button_onoff ());
	update_global_button (Button::Stop, stop_button_onoff ());
	update_global_button (Button::Rewind, rewind_button_onoff ());
	update_global_button (Button::Ffwd, ffwd_button_onoff ());

	// sometimes a return to start leaves time code at old time
	_timecode_last = string (10, '\0');

	notify_metering_state_changed ();
}

void
MackieControlProtocol::notify_metering_state_changed()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->notify_metering_state_changed ();
	}
}

void
MackieControlProtocol::notify_record_state_changed ()
{
	if (!_device_info.has_global_controls()) {
		return;
	}

	std::shared_ptr<MACKIE_NAMESPACE::Surface> surface;

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		if (surfaces.empty()) {
			return;
		}
		surface = _master_surface;
	}

	/* rec is a tristate */

	map<int,Control*>::iterator x = surface->controls_by_device_independent_id.find (Button::Record);
	if (x != surface->controls_by_device_independent_id.end()) {
		Button * rec = dynamic_cast<Button*> (x->second);
		if (rec) {
			LedState ls;

			switch (session->record_status()) {
			case Disabled:
				DEBUG_TRACE (DEBUG::MackieControl, "record state changed to disabled, LED off\n");
				ls = off;
				break;
			case Recording:
				DEBUG_TRACE (DEBUG::MackieControl, "record state changed to recording, LED on\n");
				ls = on;
				break;
			case Enabled:

				if(_device_info.is_qcon()){
					// For qcon the rec button is two state only (on/off)
					DEBUG_TRACE (DEBUG::MackieControl, "record state changed to enabled, LED on (QCon)\n");
					ls = on;
					break;

				}
				else{
					// For standard Mackie MCU the record LED is flashing
					DEBUG_TRACE (DEBUG::MackieControl, "record state changed to enabled, LED flashing\n");
					ls = flashing;
					break;
				}

				break;
			}

			surface->write (rec->set_state (ls));
		}
	}
}

list<std::shared_ptr<ARDOUR::Bundle> >
MackieControlProtocol::bundles ()
{
	list<std::shared_ptr<ARDOUR::Bundle> > b;

	if (_input_bundle) {
		b.push_back (_input_bundle);
		b.push_back (_output_bundle);
	}

	return b;
}

void
MackieControlProtocol::do_request (MackieControlUIRequest* req)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("doing request type %1\n", req->type));
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
MackieControlProtocol::stop ()
{
	BaseUI::quit ();

	return 0;
}

void
MackieControlProtocol::update_led (MACKIE_NAMESPACE::Surface& surface, MACKIE_NAMESPACE::Button& button, MACKIE_NAMESPACE::LedState ls)
{
	if (ls != none) {
		surface.port().write (button.set_state (ls));
	}
}

void
MackieControlProtocol::build_button_map ()
{
	/* this maps our device-independent button codes to the methods that handle them.
	 */

#define DEFINE_BUTTON_HANDLER(b,p,r) button_map.insert (pair<Button::ID,ButtonHandlers> ((b), ButtonHandlers ((p),(r))));

	if(!button_map.empty()) {
		button_map.clear();
	}

	build_device_specific_button_map();

	DEFINE_BUTTON_HANDLER (Button::Track, &MackieControlProtocol::track_press, &MackieControlProtocol::track_release);
	DEFINE_BUTTON_HANDLER (Button::Send, &MackieControlProtocol::send_press, &MackieControlProtocol::send_release);
	DEFINE_BUTTON_HANDLER (Button::Pan, &MackieControlProtocol::pan_press, &MackieControlProtocol::pan_release);
	DEFINE_BUTTON_HANDLER (Button::Plugin, &MackieControlProtocol::plugin_press, &MackieControlProtocol::plugin_release);
	DEFINE_BUTTON_HANDLER (Button::Eq, &MackieControlProtocol::eq_press, &MackieControlProtocol::eq_release);
	DEFINE_BUTTON_HANDLER (Button::Dyn, &MackieControlProtocol::dyn_press, &MackieControlProtocol::dyn_release);
	DEFINE_BUTTON_HANDLER (Button::Left, &MackieControlProtocol::left_press, &MackieControlProtocol::left_release);
	DEFINE_BUTTON_HANDLER (Button::Right, &MackieControlProtocol::right_press, &MackieControlProtocol::right_release);
	DEFINE_BUTTON_HANDLER (Button::ChannelLeft, &MackieControlProtocol::channel_left_press, &MackieControlProtocol::channel_left_release);
	DEFINE_BUTTON_HANDLER (Button::ChannelRight, &MackieControlProtocol::channel_right_press, &MackieControlProtocol::channel_right_release);
	DEFINE_BUTTON_HANDLER (Button::Flip, &MackieControlProtocol::flip_press, &MackieControlProtocol::flip_release);
	DEFINE_BUTTON_HANDLER (Button::View, &MackieControlProtocol::view_press, &MackieControlProtocol::view_release);
	DEFINE_BUTTON_HANDLER (Button::NameValue, &MackieControlProtocol::name_value_press, &MackieControlProtocol::name_value_release);
	DEFINE_BUTTON_HANDLER (Button::TimecodeBeats, &MackieControlProtocol::timecode_beats_press, &MackieControlProtocol::timecode_beats_release);
//	DEFINE_BUTTON_HANDLER (Button::F1, &MackieControlProtocol::F1_press, &MackieControlProtocol::F1_release);
//	DEFINE_BUTTON_HANDLER (Button::F2, &MackieControlProtocol::F2_press, &MackieControlProtocol::F2_release);
//	DEFINE_BUTTON_HANDLER (Button::F3, &MackieControlProtocol::F3_press, &MackieControlProtocol::F3_release);
//	DEFINE_BUTTON_HANDLER (Button::F4, &MackieControlProtocol::F4_press, &MackieControlProtocol::F4_release);
//	DEFINE_BUTTON_HANDLER (Button::F5, &MackieControlProtocol::F5_press, &MackieControlProtocol::F5_release);
//	DEFINE_BUTTON_HANDLER (Button::F6, &MackieControlProtocol::F6_press, &MackieControlProtocol::F6_release);
//	DEFINE_BUTTON_HANDLER (Button::F7, &MackieControlProtocol::F7_press, &MackieControlProtocol::F7_release);
//	DEFINE_BUTTON_HANDLER (Button::F8, &MackieControlProtocol::F8_press, &MackieControlProtocol::F8_release);
	DEFINE_BUTTON_HANDLER (Button::MidiTracks, &MackieControlProtocol::miditracks_press, &MackieControlProtocol::miditracks_release);
	DEFINE_BUTTON_HANDLER (Button::Inputs, &MackieControlProtocol::inputs_press, &MackieControlProtocol::inputs_release);
	DEFINE_BUTTON_HANDLER (Button::AudioTracks, &MackieControlProtocol::audiotracks_press, &MackieControlProtocol::audiotracks_release);
	DEFINE_BUTTON_HANDLER (Button::AudioInstruments, &MackieControlProtocol::audioinstruments_press, &MackieControlProtocol::audioinstruments_release);
	DEFINE_BUTTON_HANDLER (Button::Aux, &MackieControlProtocol::aux_press, &MackieControlProtocol::aux_release);
	DEFINE_BUTTON_HANDLER (Button::Busses, &MackieControlProtocol::busses_press, &MackieControlProtocol::busses_release);
	DEFINE_BUTTON_HANDLER (Button::Outputs, &MackieControlProtocol::outputs_press, &MackieControlProtocol::outputs_release);
	DEFINE_BUTTON_HANDLER (Button::User, &MackieControlProtocol::user_press, &MackieControlProtocol::user_release);
	DEFINE_BUTTON_HANDLER (Button::Shift, &MackieControlProtocol::shift_press, &MackieControlProtocol::shift_release);
	DEFINE_BUTTON_HANDLER (Button::Option, &MackieControlProtocol::option_press, &MackieControlProtocol::option_release);
	DEFINE_BUTTON_HANDLER (Button::Ctrl, &MackieControlProtocol::control_press, &MackieControlProtocol::control_release);
	DEFINE_BUTTON_HANDLER (Button::CmdAlt, &MackieControlProtocol::cmd_alt_press, &MackieControlProtocol::cmd_alt_release);
	DEFINE_BUTTON_HANDLER (Button::Read, &MackieControlProtocol::read_press, &MackieControlProtocol::read_release);
	DEFINE_BUTTON_HANDLER (Button::Write, &MackieControlProtocol::write_press, &MackieControlProtocol::write_release);
	DEFINE_BUTTON_HANDLER (Button::Trim, &MackieControlProtocol::trim_press, &MackieControlProtocol::trim_release);
	DEFINE_BUTTON_HANDLER (Button::Touch, &MackieControlProtocol::touch_press, &MackieControlProtocol::touch_release);
	DEFINE_BUTTON_HANDLER (Button::Latch, &MackieControlProtocol::latch_press, &MackieControlProtocol::latch_release);
	DEFINE_BUTTON_HANDLER (Button::Grp, &MackieControlProtocol::grp_press, &MackieControlProtocol::grp_release);
	DEFINE_BUTTON_HANDLER (Button::Save, &MackieControlProtocol::save_press, &MackieControlProtocol::save_release);
	DEFINE_BUTTON_HANDLER (Button::Undo, &MackieControlProtocol::undo_press, &MackieControlProtocol::undo_release);
	DEFINE_BUTTON_HANDLER (Button::Cancel, &MackieControlProtocol::cancel_press, &MackieControlProtocol::cancel_release);
	DEFINE_BUTTON_HANDLER (Button::Enter, &MackieControlProtocol::enter_press, &MackieControlProtocol::enter_release);
	DEFINE_BUTTON_HANDLER (Button::Marker, &MackieControlProtocol::marker_press, &MackieControlProtocol::marker_release);
	DEFINE_BUTTON_HANDLER (Button::Nudge, &MackieControlProtocol::nudge_press, &MackieControlProtocol::nudge_release);
	DEFINE_BUTTON_HANDLER (Button::Loop, &MackieControlProtocol::loop_press, &MackieControlProtocol::loop_release);
	DEFINE_BUTTON_HANDLER (Button::Drop, &MackieControlProtocol::drop_press, &MackieControlProtocol::drop_release);
	DEFINE_BUTTON_HANDLER (Button::Replace, &MackieControlProtocol::replace_press, &MackieControlProtocol::replace_release);
	DEFINE_BUTTON_HANDLER (Button::Click, &MackieControlProtocol::click_press, &MackieControlProtocol::click_release);
	DEFINE_BUTTON_HANDLER (Button::ClearSolo, &MackieControlProtocol::clearsolo_press, &MackieControlProtocol::clearsolo_release);
	DEFINE_BUTTON_HANDLER (Button::Rewind, &MackieControlProtocol::rewind_press, &MackieControlProtocol::rewind_release);
	DEFINE_BUTTON_HANDLER (Button::Ffwd, &MackieControlProtocol::ffwd_press, &MackieControlProtocol::ffwd_release);
	DEFINE_BUTTON_HANDLER (Button::Stop, &MackieControlProtocol::stop_press, &MackieControlProtocol::stop_release);
	DEFINE_BUTTON_HANDLER (Button::Play, &MackieControlProtocol::play_press, &MackieControlProtocol::play_release);
	DEFINE_BUTTON_HANDLER (Button::Record, &MackieControlProtocol::record_press, &MackieControlProtocol::record_release);
	DEFINE_BUTTON_HANDLER (Button::CursorUp, &MackieControlProtocol::cursor_up_press, &MackieControlProtocol::cursor_up_release);
	DEFINE_BUTTON_HANDLER (Button::CursorDown, &MackieControlProtocol::cursor_down_press, &MackieControlProtocol::cursor_down_release);
	DEFINE_BUTTON_HANDLER (Button::CursorLeft, &MackieControlProtocol::cursor_left_press, &MackieControlProtocol::cursor_left_release);
	DEFINE_BUTTON_HANDLER (Button::CursorRight, &MackieControlProtocol::cursor_right_press, &MackieControlProtocol::cursor_right_release);
	DEFINE_BUTTON_HANDLER (Button::Zoom, &MackieControlProtocol::zoom_press, &MackieControlProtocol::zoom_release);
	DEFINE_BUTTON_HANDLER (Button::Scrub, &MackieControlProtocol::scrub_press, &MackieControlProtocol::scrub_release);
	DEFINE_BUTTON_HANDLER (Button::UserA, &MackieControlProtocol::user_a_press, &MackieControlProtocol::user_a_release);
	DEFINE_BUTTON_HANDLER (Button::UserB, &MackieControlProtocol::user_b_press, &MackieControlProtocol::user_b_release);
	DEFINE_BUTTON_HANDLER (Button::MasterFaderTouch, &MackieControlProtocol::master_fader_touch_press, &MackieControlProtocol::master_fader_touch_release);
}

void
MackieControlProtocol::build_device_specific_button_map()
{
	/* this maps our device-dependent button codes to the methods that handle them.
	 */

#define DEFINE_BUTTON_HANDLER(b,p,r) button_map.insert (pair<Button::ID,ButtonHandlers> ((b), ButtonHandlers ((p),(r))));

	if (_device_info.is_platformMp()) {
		DEFINE_BUTTON_HANDLER (Button::Marker, &MackieControlProtocol::flip_window_press, &MackieControlProtocol::flip_window_release);
	}

	if(_device_info.is_proG2()) {
		DEFINE_BUTTON_HANDLER (Button::View, &MackieControlProtocol::user_press, &MackieControlProtocol::user_release);
		DEFINE_BUTTON_HANDLER (Button::Trim, &MackieControlProtocol::send_press, &MackieControlProtocol::send_release);
		DEFINE_BUTTON_HANDLER (Button::Touch, &MackieControlProtocol::open_press, &MackieControlProtocol::open_release);
		DEFINE_BUTTON_HANDLER (Button::Latch, &MackieControlProtocol::flip_window_press, &MackieControlProtocol::flip_window_release);
		DEFINE_BUTTON_HANDLER (Button::Save, &MackieControlProtocol::prog2_vst_press, &MackieControlProtocol::prog2_vst_release);
		DEFINE_BUTTON_HANDLER (Button::Undo, &MackieControlProtocol::master_press, &MackieControlProtocol::master_release);
		DEFINE_BUTTON_HANDLER (Button::Cancel, &MackieControlProtocol::prog2_clear_solo_press, &MackieControlProtocol::prog2_clear_solo_release);
		DEFINE_BUTTON_HANDLER (Button::Enter, &MackieControlProtocol::shift_press, &MackieControlProtocol::shift_release);
		DEFINE_BUTTON_HANDLER (Button::Marker, &MackieControlProtocol::prog2_left_press, &MackieControlProtocol::prog2_left_release);
		DEFINE_BUTTON_HANDLER (Button::Nudge, &MackieControlProtocol::prog2_right_press, &MackieControlProtocol::prog2_right_release);
		DEFINE_BUTTON_HANDLER (Button::Replace, &MackieControlProtocol::prev_marker_press, &MackieControlProtocol::prev_marker_release);
		DEFINE_BUTTON_HANDLER (Button::Click, &MackieControlProtocol::prog2_marker_press, &MackieControlProtocol::prog2_marker_release);
		DEFINE_BUTTON_HANDLER (Button::ClearSolo, &MackieControlProtocol::next_marker_press, &MackieControlProtocol::next_marker_release);
		DEFINE_BUTTON_HANDLER (Button::Shift, &MackieControlProtocol::prog2_undo_press, &MackieControlProtocol::prog2_undo_release);
		DEFINE_BUTTON_HANDLER (Button::Option, &MackieControlProtocol::redo_press, &MackieControlProtocol::redo_release);
		DEFINE_BUTTON_HANDLER (Button::Ctrl, &MackieControlProtocol::prog2_save_press, &MackieControlProtocol::prog2_save_release);
	}
}

void
MackieControlProtocol::handle_button_event (MACKIE_NAMESPACE::Surface& surface, MACKIE_NAMESPACE::Button& button, MACKIE_NAMESPACE::ButtonState bs)
{
	Button::ID button_id = button.bid();

	if  (bs != press && bs != release) {
		update_led (surface, button, none);
		return;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Handling %1 for button %2 (%3)\n", (bs == press ? "press" : "release"), button.id(),
							   Button::id_to_name (button.bid())));

	/* check profile first */

	string action = _device_profile.get_button_action (button.bid(), _modifier_state);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("device profile returned [%1] for that button\n", action));

	if (!action.empty()) {

		if (action.find ('/') != string::npos) { /* good chance that this is really an action */

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Looked up action for button %1 with modifier %2, got [%3]\n",
									   button.bid(), _modifier_state, action));

			/* if there is a bound action for this button, and this is a press event,
			   carry out the action. If its a release event, do nothing since we
			   don't bind to them at all but don't want any other handling to
			   occur either.
			*/
			if (bs == press) {
				update_led (surface, button, on);
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("executing action %1\n", action));
				access_action (action);
			} else {
				update_led (surface, button, off);
			}
			return;

		} else {

			/* "action" is more likely to be a button name. We use this to
			 * allow remapping buttons to different (builtin) functionality
			 * associated with an existing button. This is similar to the
			 * way that (for example) Nuendo moves the "Shift" function to
			 * the "Enter" key of the MCU Pro.
			 */

			int bid = Button::name_to_id (action);

			if (bid < 0) {
				DEBUG_TRACE (DEBUG::MackieControl, string_compose ("apparent button name %1 not found\n", action));
				return;
			}

			button_id = (Button::ID) bid;
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("handling button %1 as if it was %2 (%3)\n", Button::id_to_name (button.bid()), button_id, Button::id_to_name (button_id)));
		}
	}

	/* Now that we have the correct (maybe remapped) button ID, do these
	 * checks on it.
	 */

	if ((button_id != Button::Marker) && (modifier_state() & MODIFIER_MARKER)) {
		marker_modifier_consumed_by_button = true;
	}

	if ((button_id != Button::Nudge) && (modifier_state() & MODIFIER_NUDGE)) {
		nudge_modifier_consumed_by_button = true;
	}

	/* lookup using the device-INDEPENDENT button ID */

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("now looking up button ID %1\n", button_id));

	ButtonMap::iterator b = button_map.find (button_id);

	if (b != button_map.end()) {

		ButtonHandlers& bh (b->second);

		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("button found in map, now invoking %1\n", (bs == press ? "press" : "release")));

		switch  (bs) {
		case press:
			surface.write (button.set_state ((this->*(bh.press)) (button)));
			break;
		case release:
			surface.write (button.set_state ((this->*(bh.release)) (button)));
			break;
		default:
			break;
		}
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("no button handlers for button ID %1 (device ID %2)\n",
								   button.bid(), button.id()));
		error << string_compose ("no button handlers for button ID %1 (device ID %2)\n",
					 button.bid(), button.id()) << endmsg;
	}
}

bool
MackieControlProtocol::midi_input_handler (IOCondition ioc, MIDI::Port* port)
{
	if (ioc & ~IO_IN) {
		DEBUG_TRACE (DEBUG::MackieControl, "MIDI port closed\n");
		return false;
	}

	if (ioc & IO_IN) {

		// DEBUG_TRACE (DEBUG::MackieControl, string_compose ("something happened on  %1\n", port->name()));

		/* Devices using regular JACK MIDI ports will need to have
		   the x-thread FIFO drained to avoid burning endless CPU.

		   Devices using ipMIDI have port->selectable() as the same
		   file descriptor that data arrives on, so doing this
		   for them will simply throw all incoming data away.
		*/

		if (!_device_info.uses_ipmidi()) {
			AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*>(port);
			if (asp) {
				asp->clear ();
			}
		}

		// DEBUG_TRACE (DEBUG::MackieControl, string_compose ("data available on %1\n", port->name()));
		samplepos_t now = session->engine().sample_time();
		port->parse (now);
	}

	return true;
}

void
MackieControlProtocol::clear_ports ()
{
	if (_input_bundle) {
		_input_bundle->remove_channels ();
		_output_bundle->remove_channels ();
	}
}

void
MackieControlProtocol::notify_subview_stripable_deleted ()
{
	/* return to global/mixer view */
	_subview->notify_subview_stripable_deleted();
	set_view_mode (Mixer);
}

bool
MackieControlProtocol::redisplay_subview_mode ()
{
	Surfaces copy; /* can't hold surfaces lock while calling Strip::subview_mode_changed */

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		copy = surfaces;
	}

	for (Surfaces::iterator s = copy.begin(); s != copy.end(); ++s) {
		(*s)->subview_mode_changed ();
	}

	/* don't call this again from a timeout */
	return false;
}

bool
MackieControlProtocol::set_subview_mode (MACKIE_NAMESPACE::Subview::Mode sm, std::shared_ptr<Stripable> r)
{
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("set subview mode %1 with stripable %2, current flip mode %3\n", sm, (r ? r->name() : string ("null")), _flip_mode));

	if (_flip_mode != Normal) {
		set_flip_mode (Normal);
	}

	std::string reason_why_subview_not_possible = "";
	if (!_subview->subview_mode_would_be_ok (sm, r, reason_why_subview_not_possible)) {

		DEBUG_TRACE (DEBUG::MackieControl, "subview mode not OK\n");

		if (r) {

			Glib::Threads::Mutex::Lock lm (surfaces_lock);

			if (!surfaces.empty()) {
				if (!reason_why_subview_not_possible.empty()) {
					surfaces.front()->display_message_for (reason_why_subview_not_possible, 1000);
					if (_subview->subview_mode() != MACKIE_NAMESPACE::Subview::None) {
						/* redisplay current subview mode after
						   that message goes away.
						*/
						Glib::RefPtr<Glib::TimeoutSource> redisplay_timeout = Glib::TimeoutSource::create (1000); // milliseconds
						redisplay_timeout->connect (sigc::mem_fun (*this, &MackieControlProtocol::redisplay_subview_mode));
						redisplay_timeout->attach (main_loop()->get_context());
					}
				}
			}
		}

		return false;
	}

	_subview = MACKIE_NAMESPACE::SubviewFactory::instance()->create_subview(sm, *this, r);
	/* Catch the current subview stripable going away */
	if (_subview->subview_stripable()) {
		_subview->subview_stripable()->DropReferences.connect (_subview->subview_stripable_connections(), MISSING_INVALIDATOR,
													std::bind (&MackieControlProtocol::notify_subview_stripable_deleted, this),
													this);
	}

	redisplay_subview_mode ();
	_subview->update_global_buttons();

	return true;
}

void
MackieControlProtocol::set_view_mode (ViewMode m)
{
	if (_flip_mode != Normal) {
		set_flip_mode (Normal);
	}
	ViewMode old_view_mode = _view_mode;

	_view_mode = m;
	_last_bank[old_view_mode] = _current_initial_bank;

	if (switch_banks(_last_bank[m], true)) {
		_view_mode = old_view_mode;
		return;
	}

	/* leave subview mode, whatever it was */
	DEBUG_TRACE (DEBUG::MackieControl, "\t\t\tsubview mode reset in MackieControlProtocol::set_view_mode \n");
	set_subview_mode (MACKIE_NAMESPACE::Subview::None, std::shared_ptr<Stripable>());
	display_view_mode ();
}

void
MackieControlProtocol::display_view_mode ()
{
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);

		for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
			(*s)->update_view_mode_display (true);
		}
	}

	update_global_button (Button::View, (view_mode() == MackieControlProtocol::Mixer) ? on : off);
}

void
MackieControlProtocol::set_flip_mode (FlipMode fm)
{
	if (fm == Normal) {
		update_global_button (Button::Flip, off);
	} else {
		update_global_button (Button::Flip, on);
	}

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	_flip_mode = fm;

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->update_flip_mode_display ();
	}
}

void
MackieControlProtocol::set_master_on_surface_strip (uint32_t surface, uint32_t strip_number)
{
	force_special_stripable_to_strip (session->master_out(), surface, strip_number);
}

void
MackieControlProtocol::set_monitor_on_surface_strip (uint32_t surface, uint32_t strip_number)
{
	force_special_stripable_to_strip (session->monitor_out(), surface, strip_number);
}

void
MackieControlProtocol::force_special_stripable_to_strip (std::shared_ptr<Stripable> r, uint32_t surface, uint32_t strip_number)
{
	if (!r) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		if ((*s)->number() == surface) {
			Strip* strip = (*s)->nth_strip (strip_number);
			if (strip) {
				strip->set_stripable (session->master_out());
				strip->lock_controls ();
			}
		}
	}
}

void
MackieControlProtocol::check_fader_automation_state ()
{
	fader_automation_connections.drop_connections ();

	std::shared_ptr<Stripable> r = first_selected_stripable ();

	if (!r) {
		update_global_button (Button::Read, off);
		update_global_button (Button::Write, off);
		update_global_button (Button::Touch, off);
		update_global_button (Button::Trim, off);
		update_global_button (Button::Latch, off);
		update_global_button (Button::Grp, on);
		return;
	}

	r->gain_control()->alist()->automation_state_changed.connect (fader_automation_connections,
	                                                              MISSING_INVALIDATOR,
	                                                              std::bind (&MackieControlProtocol::update_fader_automation_state, this),
	                                                              this);

	update_fader_automation_state ();
}

void
MackieControlProtocol::update_fader_automation_state ()
{
	std::shared_ptr<Stripable> r = first_selected_stripable ();

	if (!r) {
		update_global_button (Button::Read, off);
		update_global_button (Button::Write, off);
		update_global_button (Button::Touch, off);
		update_global_button (Button::Trim, off);
		update_global_button (Button::Latch, off);
		update_global_button (Button::Grp, on);
		return;
	}

	switch (r->gain_control()->automation_state()) {
	case Off:
		update_global_button (Button::Read, off);
		update_global_button (Button::Write, off);
		update_global_button (Button::Touch, off);
		update_global_button (Button::Trim, off);
		update_global_button (Button::Latch, off);
		update_global_button (Button::Grp, on);
		break;
	case Play:
		update_global_button (Button::Read, on);
		update_global_button (Button::Write, off);
		update_global_button (Button::Touch, off);
		update_global_button (Button::Trim, off);
		update_global_button (Button::Latch, off);
		update_global_button (Button::Grp, off);
		break;
	case Write:
		update_global_button (Button::Read, off);
		update_global_button (Button::Write, on);
		update_global_button (Button::Touch, off);
		update_global_button (Button::Trim, off);
		update_global_button (Button::Latch, off);
		update_global_button (Button::Grp, off);
		break;
	case Touch:
		update_global_button (Button::Read, off);
		update_global_button (Button::Write, off);
		update_global_button (Button::Touch, on);
		update_global_button (Button::Trim, off);
		update_global_button (Button::Latch, off);
		update_global_button (Button::Grp, off);
		break;
	case Latch:
		update_global_button (Button::Read, off);
		update_global_button (Button::Write, off);
		update_global_button (Button::Touch, off);
		update_global_button (Button::Trim, off);
		update_global_button (Button::Latch, on);
		update_global_button (Button::Grp, off);
		break;
	}
}

samplepos_t
MackieControlProtocol::transport_sample() const
{
	return session->transport_sample();
}

void
MackieControlProtocol::add_down_select_button (int surface, int strip)
{
	_down_select_buttons.insert ((surface<<8)|(strip&0xf));
}

void
MackieControlProtocol::remove_down_select_button (int surface, int strip)
{
	DownButtonList::iterator x = find (_down_select_buttons.begin(), _down_select_buttons.end(), (uint32_t) (surface<<8)|(strip&0xf));
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("removing surface %1 strip %2 from down select buttons\n", surface, strip));
	if (x != _down_select_buttons.end()) {
		_down_select_buttons.erase (x);
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("surface %1 strip %2 not found in down select buttons\n",
								   surface, strip));
	}
}

void
MackieControlProtocol::select_range (uint32_t pressed)
{
	StripableList stripables;

	pull_stripable_range (_down_select_buttons, stripables, pressed);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("select range: found %1 stripables, first = %2\n", stripables.size(),
	                                                   (stripables.empty() ? "null" : stripables.front()->name())));

	if (stripables.empty()) {
		return;
	}

	if (stripables.size() == 1 && ControlProtocol::last_selected().size() == 1 && stripables.front()->is_selected()) {
		/* cancel selection for one and only selected stripable */
		toggle_stripable_selection (stripables.front());
	} else {
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {

			if (main_modifier_state() == MODIFIER_SHIFT) {
				toggle_stripable_selection (*s);
			} else {
				if (s == stripables.begin()) {
					set_stripable_selection (*s);
				} else {
					add_stripable_to_selection (*s);
				}
			}
		}
	}
}

void
MackieControlProtocol::add_down_button (AutomationType a, int surface, int strip)
{
	DownButtonMap::iterator m = _down_buttons.find (a);

	if (m == _down_buttons.end()) {
		_down_buttons[a] = DownButtonList();
	}

	_down_buttons[a].insert ((surface<<8)|(strip&0xf));
}

void
MackieControlProtocol::remove_down_button (AutomationType a, int surface, int strip)
{
	DownButtonMap::iterator m = _down_buttons.find (a);

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("removing surface %1 strip %2 from down buttons for %3\n", surface, strip, (int) a));

	if (m == _down_buttons.end()) {
		return;
	}

	DownButtonList& l (m->second);
	DownButtonList::iterator x = find (l.begin(), l.end(), (surface<<8)|(strip&0xf));

	if (x != l.end()) {
		l.erase (x);
	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("surface %1 strip %2 not found in down buttons for %3\n",
								   surface, strip, (int) a));
	}
}

MackieControlProtocol::ControlList
MackieControlProtocol::down_controls (AutomationType p, uint32_t pressed)
{
	ControlList controls;
	StripableList stripables;

	DownButtonMap::iterator m = _down_buttons.find (p);

	if (m == _down_buttons.end()) {
		return controls;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("looking for down buttons for %1, got %2\n",
							   p, m->second.size()));

	pull_stripable_range (m->second, stripables, pressed);

	switch (p) {
	case GainAutomation:
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			controls.push_back ((*s)->gain_control());
		}
		break;
	case SoloAutomation:
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			controls.push_back ((*s)->solo_control());
		}
		break;
	case MuteAutomation:
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			controls.push_back ((*s)->mute_control());
		}
		break;
	case RecEnableAutomation:
		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			std::shared_ptr<AutomationControl> ac = (*s)->rec_enable_control();
			if (ac) {
				controls.push_back (ac);
			}
		}
		break;
	default:
		break;
	}

	return controls;

}

struct ButtonRangeSorter {
    bool operator() (const uint32_t& a, const uint32_t& b) {
	    return (a>>8) < (b>>8) // a.surface < b.surface
		    ||
		    ((a>>8) == (b>>8) && (a&0xf) < (b&0xf)); // a.surface == b.surface && a.strip < b.strip
    }
};

void
MackieControlProtocol::pull_stripable_range (DownButtonList& down, StripableList& selected, uint32_t pressed)
{
	ButtonRangeSorter cmp;

	if (down.empty()) {
		return;
	}

	list<uint32_t> ldown;
	ldown.insert (ldown.end(), down.begin(), down.end());
	ldown.sort (cmp);

	uint32_t first = ldown.front();
	uint32_t last = ldown.back ();

	uint32_t first_surface = first>>8;
	uint32_t first_strip = first&0xf;

	uint32_t last_surface = last>>8;
	uint32_t last_strip = last&0xf;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("PRR %5 in list %1.%2 - %3.%4\n", first_surface, first_strip, last_surface, last_strip,
							   down.size()));

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {

		if ((*s)->number() >= first_surface && (*s)->number() <= last_surface) {

			uint32_t fs;
			uint32_t ls;

			if ((*s)->number() == first_surface) {
				fs = first_strip;
			} else {
				fs = 0;
			}

			if ((*s)->number() == last_surface) {
				ls = last_strip;
				ls += 1;
			} else {
				ls = (*s)->n_strips ();
			}

			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("adding strips for surface %1 (%2 .. %3)\n",
									   (*s)->number(), fs, ls));

			for (uint32_t n = fs; n < ls; ++n) {
				Strip* strip = (*s)->nth_strip (n);
				std::shared_ptr<Stripable> r = strip->stripable();
				if (r) {
					if (global_index_locked (*strip) == pressed) {
						selected.push_front (r);
					} else {
						selected.push_back (r);
					}
				}
			}
		}
	}

}

void
MackieControlProtocol::set_ipmidi_base (int16_t portnum)
{
	/* this will not be saved without a session save, so .. */

	session->set_dirty ();

	_ipmidi_base = portnum;

	/* if the current device uses ipMIDI we need
	   to restart.
	*/

	if (active() && _device_info.uses_ipmidi()) {
		needs_ipmidi_restart = true;
	}
}

int
MackieControlProtocol::ipmidi_restart ()
{
	clear_surfaces ();
	if (create_surfaces ()) {
		return -1;
	}
	(void) switch_banks (_current_initial_bank, true);
	needs_ipmidi_restart = false;
	return 0;
}

void
MackieControlProtocol::clear_surfaces ()
{
	clear_ports ();

	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		_master_surface.reset ();
		surfaces.clear ();
	}
}

void
MackieControlProtocol::set_touch_sensitivity (int sensitivity)
{
	sensitivity = min (9, sensitivity);
	sensitivity = max (0, sensitivity);

	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		(*s)->set_touch_sensitivity (sensitivity);
	}
}

void
MackieControlProtocol::recalibrate_faders ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (auto const& s : surfaces) {
		s->recalibrate_faders ();
	}
}

void
MackieControlProtocol::toggle_backlight ()
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (auto const& s : surfaces) {
		s->toggle_backlight ();
	}
}

std::shared_ptr<MACKIE_NAMESPACE::Surface>
MackieControlProtocol::get_surface_by_raw_pointer (void* ptr) const
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (auto const& s : surfaces) {
		if (s.get() == (MACKIE_NAMESPACE::Surface*) ptr) {
			return s;
		}
	}

	return std::shared_ptr<MACKIE_NAMESPACE::Surface> ();
}

std::shared_ptr<MACKIE_NAMESPACE::Surface>
MackieControlProtocol::nth_surface (uint32_t n) const
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s, --n) {
		if (n == 0) {
			return *s;
		}
	}

	return std::shared_ptr<MACKIE_NAMESPACE::Surface> ();
}

void
MackieControlProtocol::connection_handler (std::weak_ptr<ARDOUR::Port> wp1, std::string name1, std::weak_ptr<ARDOUR::Port> wp2, std::string name2, bool yn)
{
	Surfaces scopy;
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		scopy = surfaces;
	}

	for (Surfaces::const_iterator s = scopy.begin(); s != scopy.end(); ++s) {
		if ((*s)->connection_handler (wp1, name1, wp2, name2, yn)) {
			ConnectionChange (*s);
			break;
		}
	}
}

bool
MackieControlProtocol::is_track (std::shared_ptr<Stripable> r) const
{
	return std::dynamic_pointer_cast<Track>(r) != 0;
}

bool
MackieControlProtocol::is_audio_track (std::shared_ptr<Stripable> r) const
{
	return std::dynamic_pointer_cast<AudioTrack>(r) != 0;
}

bool
MackieControlProtocol::is_midi_track (std::shared_ptr<Stripable> r) const
{
	return std::dynamic_pointer_cast<MidiTrack>(r) != 0;
}

bool
MackieControlProtocol::is_trigger_track (std::shared_ptr<Stripable> r) const
{
	std::shared_ptr<Track> trk = std::dynamic_pointer_cast<Track>(r);
	return (trk && (r)->presentation_info ().trigger_track ());
}

bool
MackieControlProtocol::is_foldback_bus (std::shared_ptr<Stripable> r) const
{
	return ((r)->presentation_info ().flags () & PresentationInfo::FoldbackBus);
}

bool
MackieControlProtocol::is_vca (std::shared_ptr<Stripable> r) const
{
	return std::dynamic_pointer_cast<VCA>(r) != 0;
}

bool
MackieControlProtocol::has_instrument (std::shared_ptr<Stripable> r) const
{
	std::shared_ptr<MidiTrack> mt = std::dynamic_pointer_cast<MidiTrack>(r);
	return mt && mt->the_instrument();
}

bool
MackieControlProtocol::is_mapped (std::shared_ptr<Stripable> r) const
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		if ((*s)->stripable_is_mapped (r)) {
			return true;
		}
	}

	return false;
}

void
MackieControlProtocol::stripable_selection_changed ()
{
	//this function is called after the stripable selection is "stable", so this is the place to check surface selection state
	{
		Glib::Threads::Mutex::Lock lm (surfaces_lock);
		for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
			(*si)->update_strip_selection ();
		}
	}

	/* if we are following the Gui, find the selected strips and map them here */
	if (_device_info.single_fader_follows_selection()) {

		Sorted sorted = get_sorted_stripables();

		{
			Glib::Threads::Mutex::Lock lm (surfaces_lock);
			Sorted::iterator r = sorted.begin();
			for (Surfaces::iterator si = surfaces.begin(); si != surfaces.end(); ++si) {
				vector<std::shared_ptr<Stripable> > stripables;
				uint32_t added = 0;

				for (; r != sorted.end() && added < (*si)->n_strips (false); ++r, ++added) {
					if ((*r)->is_selected()) {
						stripables.push_back (*r);
					}
				}

				(*si)->map_stripables (stripables);
			}
		}
		return;
	}

	std::shared_ptr<Stripable> s = first_selected_stripable ();
	if (s) {
		check_fader_automation_state ();

		/* It is possible that first_selected_route() may return null if we
		 * are no longer displaying/mapping that route. In that case,
		 * we will exit subview mode. If first_selected_route() is
		 * null, and subview mode is not None, then the first call to
		 * set_subview_mode() will fail, and we will reset to None.
		 */

		if (!set_subview_mode (_subview->subview_mode(), s)) {
			set_subview_mode (MACKIE_NAMESPACE::Subview::None, std::shared_ptr<Stripable>());
		}
	}
	else {
		// none selected or not on surface
		set_subview_mode(MACKIE_NAMESPACE::Subview::None, std::shared_ptr<Stripable>());
	}
}

std::shared_ptr<Stripable>
MackieControlProtocol::first_selected_stripable () const
{
	std::shared_ptr<Stripable> s = ControlProtocol::first_selected_stripable();

	if (s) {
		/* check it is on one of our surfaces */

		if (is_mapped (s)) {
			return s;
		}

		/* stripable is not mapped. thus, the currently selected stripable is
		 * not on the surfaces, and so from our perspective, there is
		 * no currently selected stripable.
		 */

		s.reset ();
	}

	return s; /* may be null */
}

uint32_t
MackieControlProtocol::global_index (MACKIE_NAMESPACE::Strip& strip)
{
	Glib::Threads::Mutex::Lock lm (surfaces_lock);
	return global_index_locked (strip);
}

uint32_t
MackieControlProtocol::global_index_locked (MACKIE_NAMESPACE::Strip& strip)
{
	uint32_t global = 0;

	for (Surfaces::const_iterator s = surfaces.begin(); s != surfaces.end(); ++s) {
		if ((*s).get() == strip.surface()) {
			return global + strip.index();
		}
		global += (*s)->n_strips ();
	}

	return global;
}

void
MackieControlProtocol::set_automation_state (AutoState as)
{
	std::shared_ptr<Stripable> r = first_selected_stripable ();

	if (!r) {
		return;
	}

	std::shared_ptr<AutomationControl> ac = r->gain_control();

	if (!ac) {
		return;
	}

	ac->set_automation_state (as);
}
