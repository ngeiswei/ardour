/*
 * Copyright (C) 2015-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#include <cstdlib>
#include <sstream>
#include <algorithm>

#include <stdint.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "pbd/compose.h"
#include "pbd/xml++.h"

#include "midi++/port.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/amp.h"
#include "ardour/bundle.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midi_port.h"
#include "ardour/midiport_manager.h"
#include "ardour/monitor_processor.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/record_enable_control.h"
#include "ardour/stripable.h"
#include "ardour/session.h"
#include "ardour/session_configuration.h"
#include "ardour/track.h"

#include "faderport.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;
using namespace Glib;
using namespace std;

#include "pbd/i18n.h"

#include "pbd/abstract_ui.inc.cc" // instantiate template

FaderPort::FaderPort (Session& s)
	: MIDISurface (s, X_("PreSonus FaderPort"), X_("FaderPort"), false)
	, gui (0)
	, fader_msb (0)
	, fader_lsb (0)
	, fader_is_touched (false)
	, button_state (ButtonState (0))
	, blink_state (false)
	, rec_enable_state (false)
{
	last_encoder_time = 0;

	buttons.insert (std::make_pair (Mute, Button (*this, _("Mute"), Mute, 21)));
	buttons.insert (std::make_pair (Solo, Button (*this, _("Solo"), Solo, 22)));
	buttons.insert (std::make_pair (Rec, Button (*this, _("Rec"), Rec, 23)));
	buttons.insert (std::make_pair (Left, Button (*this, _("Left"), Left, 20)));
	buttons.insert (std::make_pair (Bank, Button (*this, _("Bank"), Bank, 19)));
	buttons.insert (std::make_pair (Right, Button (*this, _("Right"), Right, 18)));
	buttons.insert (std::make_pair (Output, Button (*this, _("Output"), Output, 17)));
	buttons.insert (std::make_pair (FP_Read, Button (*this, _("Read"), FP_Read, 13)));
	buttons.insert (std::make_pair (FP_Write, Button (*this, _("Write"), FP_Write, 14)));
	buttons.insert (std::make_pair (FP_Touch, Button (*this, _("Touch"), FP_Touch, 15)));
	buttons.insert (std::make_pair (FP_Off, Button (*this, _("Off"), FP_Off, 16)));
	buttons.insert (std::make_pair (Mix, Button (*this, _("Mix"), Mix, 12)));
	buttons.insert (std::make_pair (Proj, Button (*this, _("Proj"), Proj, 11)));
	buttons.insert (std::make_pair (Trns, Button (*this, _("Trns"), Trns, 10)));
	buttons.insert (std::make_pair (Undo, Button (*this, _("Undo"), Undo, 9)));
	buttons.insert (std::make_pair (Shift, Button (*this, _("Shift"), Shift, 5)));
	buttons.insert (std::make_pair (Punch, Button (*this, _("Punch"), Punch, 6)));
	buttons.insert (std::make_pair (User, Button (*this, _("User"), User, 7)));
	buttons.insert (std::make_pair (Loop, Button (*this, _("Loop"), Loop, 8)));
	buttons.insert (std::make_pair (Rewind, Button (*this, _("Rewind"), Rewind, 4)));
	buttons.insert (std::make_pair (Ffwd, Button (*this, _("Ffwd"), Ffwd, 3)));
	buttons.insert (std::make_pair (Stop, Button (*this, _("Stop"), Stop, 2)));
	buttons.insert (std::make_pair (Play, Button (*this, _("Play"), Play, 1)));
	buttons.insert (std::make_pair (RecEnable, Button (*this, _("RecEnable"), RecEnable, 0)));
	buttons.insert (std::make_pair (Footswitch, Button (*this, _("Footswitch"), Footswitch, -1)));
	buttons.insert (std::make_pair (FaderTouch, Button (*this, _("Fader (touch)"), FaderTouch, -1)));

	get_button (Shift).set_flash (true);
	get_button (Mix).set_flash (true);
	get_button (Proj).set_flash (true);
	get_button (Trns).set_flash (true);
	get_button (User).set_flash (true);

	get_button (Left).set_action ( std::bind (&FaderPort::left, this), true);
	get_button (Right).set_action ( std::bind (&FaderPort::right, this), true);

	get_button (Undo).set_action (std::bind (&FaderPort::undo, this), true);
	get_button (Undo).set_action (std::bind (&FaderPort::redo, this), true, ShiftDown);
	get_button (Undo).set_flash (true);

	get_button (FP_Read).set_action (std::bind (&FaderPort::read, this), true);
	get_button (FP_Read).set_action (std::bind (&FaderPort::off, this), false, LongPress);
	get_button (FP_Write).set_action (std::bind (&FaderPort::write, this), true);
	get_button (FP_Write).set_action (std::bind (&FaderPort::off, this), false, LongPress);
	get_button (FP_Touch).set_action (std::bind (&FaderPort::touch, this), true);
	get_button (FP_Touch).set_action (std::bind (&FaderPort::off, this), false, LongPress);
	get_button (FP_Off).set_action (std::bind (&FaderPort::off, this), true);

	get_button (Play).set_action (std::bind (&BasicUI::transport_play, this, true), true);
	get_button (RecEnable).set_action (std::bind (&BasicUI::rec_enable_toggle, this), true);
	/* Stop is a modifier, so we have to use its own button state to get
	   the default action (since StopDown will be set when looking for the
	   action to invoke.
	*/
	get_button (Stop).set_action (std::bind (&BasicUI::transport_stop, this), true, StopDown);
	get_button (Ffwd).set_action (std::bind (&BasicUI::ffwd, this), true);

	/* See comments about Stop above .. */
	get_button (Rewind).set_action (std::bind (&BasicUI::rewind, this), true, RewindDown);
	get_button (Rewind).set_action (std::bind (&BasicUI::goto_zero, this), true, ButtonState (RewindDown|StopDown));
	get_button (Rewind).set_action (std::bind (&BasicUI::goto_start, this, false), true, ButtonState (RewindDown|ShiftDown));

	get_button (Ffwd).set_action (std::bind (&BasicUI::ffwd, this), true);
	get_button (Ffwd).set_action (std::bind (&BasicUI::goto_end, this), true, ShiftDown);

	get_button (Punch).set_action (std::bind (&FaderPort::punch, this), true);

	get_button (Loop).set_action (std::bind (&BasicUI::loop_toggle, this), true);
	get_button (Loop).set_action (std::bind (&BasicUI::add_marker, this, string()), true, ShiftDown);

	get_button (Punch).set_action (std::bind (&BasicUI::prev_marker, this), true, ShiftDown);
	get_button (User).set_action (std::bind (&BasicUI::next_marker, this), true, ShiftDown);

	get_button (Mute).set_action (std::bind (&FaderPort::mute, this), true);
	get_button (Solo).set_action (std::bind (&FaderPort::solo, this), true);
	get_button (Rec).set_action (std::bind (&FaderPort::rec_enable, this), true);

	get_button (Output).set_action (std::bind (&FaderPort::use_master, this), true);
	get_button (Output).set_action (std::bind (&FaderPort::use_monitor, this), true, ShiftDown);

	run_event_loop ();
	port_setup ();
}

FaderPort::~FaderPort ()
{
	all_lights_out ();

	MIDISurface::drop ();

	tear_down_gui ();

	/* stop event loop */
	DEBUG_TRACE (DEBUG::FaderPort, "BaseUI::quit ()\n");

	BaseUI::quit ();
}

void
FaderPort::all_lights_out ()
{
	for (ButtonMap::iterator b = buttons.begin(); b != buttons.end(); ++b) {
		b->second.set_led_state (false);
	}
}

FaderPort::Button&
FaderPort::get_button (ButtonID id) const
{
	ButtonMap::const_iterator b = buttons.find (id);
	assert (b != buttons.end());
	return const_cast<Button&>(b->second);
}

bool
FaderPort::button_long_press_timeout (ButtonID id)
{
	if (buttons_down.find (id) != buttons_down.end()) {
		if (get_button (id).invoke (ButtonState (LongPress|button_state), false)) {
			/* whichever button this was, we've used it ... don't invoke the
			   release action.
			*/
			consumed.insert (id);
		}
	} else {
		/* release happened and somehow we were not cancelled */
	}

	return false; /* don't get called again */
}

void
FaderPort::start_press_timeout (Button& button, ButtonID id)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (500); // milliseconds
	button.timeout_connection = timeout->connect (sigc::bind (sigc::mem_fun (*this, &FaderPort::button_long_press_timeout), id));
	timeout->attach (main_loop()->get_context());
}

void
FaderPort::handle_midi_polypressure_message (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	ButtonID id (ButtonID (tb->controller_number));
	Button& button (get_button (id));

	DEBUG_TRACE (DEBUG::FaderPort, string_compose ("button event for ID %1 press ? %2\n", (int) tb->controller_number, (tb->value ? "yes" : "no")));

	if (tb->value) {
		buttons_down.insert (id);
	} else {
		buttons_down.erase (id);
		button.timeout_connection.disconnect ();
	}

	ButtonState bs (ButtonState (0));

	switch (id) {
	case Shift:
		bs = ShiftDown;
		break;
	case Stop:
		bs = StopDown;
		break;
	case Rewind:
		bs = RewindDown;
		break;
	case FaderTouch:
		fader_is_touched = tb->value;
		if (_current_stripable) {
			std::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
			if (gain) {
				timepos_t now = timepos_t (session->engine().sample_time());
				if (tb->value) {
					gain->start_touch (now);
				} else {
					gain->stop_touch (now);
				}
			}
		}
		break;
	default:
		if (tb->value) {
			start_press_timeout (button, id);
		}
		break;
	}

	if (bs) {
		button_state = (tb->value ? ButtonState (button_state|bs) : ButtonState (button_state&~bs));
		DEBUG_TRACE (DEBUG::FaderPort, string_compose ("reset button state to %1 using %2\n", button_state, (int) bs));
	}

	if (button.uses_flash()) {
		button.set_led_state ((int)tb->value);
	}

	set<ButtonID>::iterator c = consumed.find (id);

	if (c == consumed.end()) {
		(void) button.invoke (button_state, tb->value ? true : false);
	} else {
		DEBUG_TRACE (DEBUG::FaderPort, "button was consumed, ignored\n");
		consumed.erase (c);
	}
}

void
FaderPort::handle_midi_pitchbend_message (MIDI::Parser &, MIDI::pitchbend_t pb)
{
	int delta = 1;

	if (pb >= 8192) {
		delta = -1;
	}

	//knob debouncing and hysteresis.  The presonus encoder often sends bursts of events, or goes the wrong direction
	{
		last_last_encoder_delta = last_encoder_delta;
		last_encoder_delta = delta;
		microseconds_t now = get_microseconds ();
		if ((now - last_encoder_time) < 10*1000) { //require at least 10ms interval between changes, because the device sometimes sends multiple deltas
			return;
		}
		if ((now - last_encoder_time) < 100*1000) { //avoid directional changes while "spinning", 100ms window
			if ( (delta == last_encoder_delta) && (delta == last_last_encoder_delta) ) {
				last_good_encoder_delta = delta;  //3 in a row, grudgingly accept this as the new direction
			}
			if (delta != last_good_encoder_delta) {  //otherwise ensure we keep going the same way
				delta = last_good_encoder_delta;
			}
		} else {  //we aren't yet in a spin window, just assume this move is really what we want
			//NOTE:  if you are worried about where these get initialized, here it is.
			last_last_encoder_delta = delta;
			last_encoder_delta = delta;
		}
		last_encoder_time = now;
		last_good_encoder_delta = delta;
	}

	if (_current_stripable) {

		ButtonState trim_modifier;
		ButtonState width_modifier;

		trim_modifier = ShiftDown;
		width_modifier = ButtonState (0);

		if ((button_state & trim_modifier) == trim_modifier ) {    // mod+encoder = input trim
			std::shared_ptr<AutomationControl> trim = _current_stripable->trim_control ();
			if (trim) {
				float val = accurate_coefficient_to_dB (trim->get_value());
				val += delta * .5f; // use 1/2 dB Steps -20..+20
				trim->set_value (dB_to_coefficient (val), Controllable::UseGroup);
			}
		} else if (width_modifier && ((button_state & width_modifier) == width_modifier)) {
			pan_width (delta);

		} else {  // pan/balance
			pan_azimuth (delta);
		}
	}
}

void
FaderPort::handle_midi_controller_message (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	bool was_fader = false;

	if (tb->controller_number == 0x0) {
		fader_msb = tb->value;
		was_fader = true;
	} else if (tb->controller_number == 0x20) {
		fader_lsb = tb->value;
		was_fader = true;
	}

	if (was_fader) {
		if (_current_stripable) {
			std::shared_ptr<AutomationControl> gain = _current_stripable->gain_control ();
			if (gain) {
				int ival = (fader_msb << 7) | fader_lsb;
				float val = gain->interface_to_internal (ival/16383.0);
				/* even though the faderport only controls a
				   single stripable at a time, allow the fader to
				   modify the group, if appropriate.
				*/
				_current_stripable->gain_control()->set_value (val, Controllable::UseGroup);
			}
		}
	}
}

void
FaderPort::handle_midi_sysex (MIDI::Parser &p, MIDI::byte *buf, size_t sz)
{
        DEBUG_TRACE (DEBUG::FaderPort, string_compose ("sysex message received, size = %1\n", sz));

	if (sz < 17) {
		return;
	}

	if (buf[2] != 0x7f ||
	    buf[3] != 0x06 ||
	    buf[4] != 0x02 ||
	    buf[5] != 0x0 ||
	    buf[6] != 0x1 ||
	    buf[7] != 0x06 ||
	    buf[8] != 0x02 ||
	    buf[9] != 0x0 ||
	    buf[10] != 0x01 ||
	    buf[11] != 0x0) {
		return;
	}

	DEBUG_TRACE (DEBUG::FaderPort, "FaderPort identified via MIDI Device Inquiry response\n");

	/* put it into native mode */

	MIDI::byte native[3];
	native[0] = 0x91;
	native[1] = 0x00;
	native[2] = 0x64;

	MIDISurface::write (native, 3);

	all_lights_out ();

	/* catch up on state */

	/* make sure that rec_enable_state is consistent with current device state */
	get_button (RecEnable).set_led_state (rec_enable_state);

	map_transport_state ();
	map_recenable_state ();
}

int
FaderPort::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::FaderPort, string_compose("Faderport::set_active init with yn: '%1' while active = %2\n", yn, active()));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		if (device_acquire ()) {
			return -1;
		}

	} else {
		/* Control Protocol Manager never calls us with false, but
		 * insteads destroys us.
		 */
	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::FaderPort, string_compose("Faderport::set_active done with yn: '%1'\n", yn));

	return 0;
}

bool
FaderPort::periodic ()
{
	if (!_current_stripable) {
		return true;
	}

	ARDOUR::AutoState gain_state = _current_stripable->gain_control()->automation_state();

	if (gain_state == ARDOUR::Touch || gain_state == ARDOUR::Play) {
		map_gain ();
	}

	return true;
}

void
FaderPort::stop_blinking (ButtonID id)
{
	blinkers.remove (id);
	get_button (id).set_led_state (false);
}

void
FaderPort::start_blinking (ButtonID id)
{
	blinkers.push_back (id);
	get_button (id).set_led_state (true);
}

bool
FaderPort::blink ()
{
	blink_state = !blink_state;

	for (Blinkers::iterator b = blinkers.begin(); b != blinkers.end(); b++) {
		get_button(*b).set_led_state (blink_state);
	}

	map_recenable_state ();

	return true;
}

void
FaderPort::map_recenable_state ()
{
	/* special case for RecEnable because its status can change as a
	 * confluence of unrelated parameters: (a) session rec-enable state (b)
	 * rec-enabled tracks. So we don't add the button to the blinkers list,
	 * we just call this:
	 *
	 *  * from the blink callback
	 *  * when the session tells us about a status change
	 *
	 * We do the last one so that the button changes state promptly rather
	 * than waiting for the next blink callback. The change in "blinking"
	 * based on having record-enabled tracks isn't urgent, and that happens
	 * during the blink callback.
	 */

	bool onoff;

	switch (session->record_status()) {
	case Disabled:
		onoff = false;
		break;
	case Enabled:
		onoff = blink_state;
		break;
	case Recording:
		if (session->have_rec_enabled_track ()) {
			onoff = true;
		} else {
			onoff = blink_state;
		}
		break;
	default:
		return; /* stupid compilers */
	}

	if (onoff != rec_enable_state) {
		get_button(RecEnable).set_led_state (onoff);
		rec_enable_state = onoff;
	}
}

void
FaderPort::map_transport_state ()
{
	get_button (Loop).set_led_state (session->get_play_loop());

	float ts = get_transport_speed();

	if (ts == 0) {
		stop_blinking (Play);
	} else if (fabs (ts) == 1.0) {
		stop_blinking (Play);
		get_button (Play).set_led_state (true);
	} else {
		start_blinking (Play);
	}

	get_button (Stop).set_led_state (stop_button_onoff());
	get_button (Rewind).set_led_state (rewind_button_onoff ());
	get_button (Ffwd).set_led_state (ffwd_button_onoff());
}

void
FaderPort::parameter_changed (string what)
{
	if (what == "punch-in" || what == "punch-out") {
		bool in = session->config.get_punch_in ();
		bool out = session->config.get_punch_out ();
		if (in && out) {
			get_button (Punch).set_led_state (true);
			blinkers.remove (Punch);
		} else if (in || out) {
			start_blinking (Punch);
		} else {
			stop_blinking (Punch);
		}
	}
}

XMLNode&
FaderPort::get_state () const
{
	XMLNode& node (MIDISurface::get_state());

	/* Save action state for Mix, Proj, Trns and User buttons, since these
	 * are user controlled. We can only save named-action operations, since
	 * internal functions are just pointers to functions and hard to
	 * serialize without enumerating them all somewhere.
	 */

	node.add_child_nocopy (get_button (Mix).get_state());
	node.add_child_nocopy (get_button (Proj).get_state());
	node.add_child_nocopy (get_button (Trns).get_state());
	node.add_child_nocopy (get_button (User).get_state());
	node.add_child_nocopy (get_button (Footswitch).get_state());

	return node;
}

int
FaderPort::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist;

	if (MIDISurface::set_state (node, version)) {
		return -1;
	}

	for (XMLNodeList::const_iterator n = node.children().begin(); n != node.children().end(); ++n) {
		if ((*n)->name() == X_("Button")) {
			int32_t xid;
			if (!(*n)->get_property (X_("id"), xid)) {
				continue;
			}
			ButtonMap::iterator b = buttons.find (ButtonID (xid));
			if (b == buttons.end()) {
				continue;
			}
			b->second.set_state (**n);
		}
	}

	return 0;
}

std::string
FaderPort::input_port_name () const
{
#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Ableton
	   and may change in time. This is part of how CoreMIDI works.
	*/
	return X_("system:midi_capture_1319078870");
#else
	return X_("FaderPort MIDI 1 (in)");
#endif
}

std::string
FaderPort::output_port_name () const
{
#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Ableton
	   and may change in time. This is part of how CoreMIDI works.
	*/
	return X_("system:midi_playback_3409210341");
#else
	return X_("FaderPort MIDI 1 (out)");
#endif
}

void
FaderPort::run_event_loop ()
{
	DEBUG_TRACE (DEBUG::FaderPort, "start event loop\n");
	BaseUI::run ();
}

void
FaderPort::stop_event_loop ()
{
	DEBUG_TRACE (DEBUG::FaderPort, "stop event loop\n");
	BaseUI::quit ();
}

int
FaderPort::begin_using_device()
{
	DEBUG_TRACE (DEBUG::FaderPort, "begin using device\n");

	connect_session_signals ();

	Glib::RefPtr<Glib::TimeoutSource> blink_timeout = Glib::TimeoutSource::create (200); // milliseconds
	blink_connection = blink_timeout->connect (sigc::mem_fun (*this, &FaderPort::blink));
	blink_timeout->attach (main_loop()->get_context());

	Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (100); // milliseconds
	periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this, &FaderPort::periodic));
	periodic_timeout->attach (main_loop()->get_context());

	if (MIDISurface::begin_using_device ()) {
		return -1;
	}

	DEBUG_TRACE (DEBUG::FaderPort, "sending device inquiry message...\n");

	/* send device inquiry */

	MIDI::byte buf[6];

	buf[0] = 0xf0;
	buf[1] = 0x7e;
	buf[2] = 0x7f;
	buf[3] = 0x06;
	buf[4] = 0x01;
	buf[5] = 0xf7;

	MIDISurface::write (buf, 6);

	return 0;
}

int
FaderPort::stop_using_device ()
{
	blink_connection.disconnect ();
	selection_connection.disconnect ();
	stripable_connections.drop_connections ();
	periodic_connection.disconnect ();

	return 0;
}

bool
FaderPort::Button::invoke (FaderPort::ButtonState bs, bool press)
{
	DEBUG_TRACE (DEBUG::FaderPort, string_compose ("invoke button %1 for %2 state %3%4%5\n", id, (press ? "press":"release"), hex, bs, dec));

	ToDoMap::iterator x;

	if (press) {
		if ((x = on_press.find (bs)) == on_press.end()) {
			DEBUG_TRACE (DEBUG::FaderPort, string_compose ("no press action for button %1 state %2 @ %3 in %4\n", id, bs, this, &on_press));
			return false;
		}
	} else {
		if ((x = on_release.find (bs)) == on_release.end()) {
			DEBUG_TRACE (DEBUG::FaderPort, string_compose ("no release action for button %1 state %2 @%3 in %4\n", id, bs, this, &on_release));
			return false;
		}
	}

	switch (x->second.type) {
	case NamedAction:
		if (!x->second.action_name.empty()) {
			fp.access_action (x->second.action_name);
			return true;
		}
		break;
	case InternalFunction:
		if (x->second.function) {
			x->second.function ();
			return true;
		}
	}

	return false;
}

void
FaderPort::Button::set_action (string const& name, bool when_pressed, FaderPort::ButtonState bs)
{
	ToDo todo;

	todo.type = NamedAction;

	if (when_pressed) {
		if (name.empty()) {
			on_press.erase (bs);
		} else {
			DEBUG_TRACE (DEBUG::FaderPort, string_compose ("set button %1 to action %2 on press + %3\n", id, name, bs));
			todo.action_name = name;
			on_press[bs] = todo;
		}
	} else {
		if (name.empty()) {
			on_release.erase (bs);
		} else {
			DEBUG_TRACE (DEBUG::FaderPort, string_compose ("set button %1 to action %2 on release + %3\n", id, name, bs));
			todo.action_name = name;
			on_release[bs] = todo;
		}
	}
}

string
FaderPort::Button::get_action (bool press, FaderPort::ButtonState bs)
{
	ToDoMap::iterator x;

	if (press) {
		if ((x = on_press.find (bs)) == on_press.end()) {
			return string();
		}
		if (x->second.type != NamedAction) {
			return string ();
		}
		return x->second.action_name;
	} else {
		if ((x = on_release.find (bs)) == on_release.end()) {
			return string();
		}
		if (x->second.type != NamedAction) {
			return string ();
		}
		return x->second.action_name;
	}
}

void
FaderPort::Button::set_action (std::function<void()> f, bool when_pressed, FaderPort::ButtonState bs)
{
	ToDo todo;
	todo.type = InternalFunction;

	if (when_pressed) {
		DEBUG_TRACE (DEBUG::FaderPort, string_compose ("set button %1 (%2) @ %5 to some functor on press + %3 in %4\n", id, name, bs, &on_press, this));
		todo.function = f;
		on_press[bs] = todo;
	} else {
		DEBUG_TRACE (DEBUG::FaderPort, string_compose ("set button %1 (%2) @ %5 to some functor on release + %3\n", id, name, bs, this));
		todo.function = f;
		on_release[bs] = todo;
	}
}

void
FaderPort::Button::set_led_state (bool onoff)
{
	if (out < 0) {
		/* fader button ID - no LED */
		return;
	}

	MIDI::byte buf[3];
	buf[0] = 0xa0;
	buf[1] = out;
	buf[2] = onoff ? 1 : 0;

	/* C++ can be irritating */

	MIDISurface* ms (&fp);
	ms->write (buf, 3);
}

int
FaderPort::Button::set_state (XMLNode const& node)
{
	int32_t xid;
	if (!node.get_property ("id", xid) || xid != id) {
		return -1;
	}

	typedef pair<string,FaderPort::ButtonState> state_pair_t;
	vector<state_pair_t> state_pairs;

	state_pairs.push_back (make_pair (string ("plain"), ButtonState (0)));
	state_pairs.push_back (make_pair (string ("shift"), ShiftDown));
	state_pairs.push_back (make_pair (string ("long"), LongPress));

	for (vector<state_pair_t>::const_iterator sp = state_pairs.begin(); sp != state_pairs.end(); ++sp) {

		string propname = sp->first + X_("-press");
		string value;
		if (node.get_property (propname.c_str(), value)) {
			set_action (value, true, sp->second);
		}

		propname = sp->first + X_("-release");
		if (node.get_property (propname.c_str(), value)) {
			set_action (value, false, sp->second);
		}
	}

	return 0;
}

XMLNode&
FaderPort::Button::get_state () const
{
	XMLNode* node = new XMLNode (X_("Button"));

	node->set_property (X_("id"), to_string<int32_t>(id));

	ToDoMap::const_iterator x;
	ToDo null;
	null.type = NamedAction;

	typedef pair<string,FaderPort::ButtonState> state_pair_t;
	vector<state_pair_t> state_pairs;

	state_pairs.push_back (make_pair (string ("plain"), ButtonState (0)));
	state_pairs.push_back (make_pair (string ("shift"), ShiftDown));
	state_pairs.push_back (make_pair (string ("long"), LongPress));

	for (vector<state_pair_t>::const_iterator sp = state_pairs.begin(); sp != state_pairs.end(); ++sp) {
		if ((x = on_press.find (sp->second)) != on_press.end()) {
			if (x->second.type == NamedAction) {
				node->set_property (string (sp->first + X_("-press")).c_str(), x->second.action_name);
			}
		}

		if ((x = on_release.find (sp->second)) != on_release.end()) {
			if (x->second.type == NamedAction) {
				node->set_property (string (sp->first + X_("-release")).c_str(), x->second.action_name);
			}
		}
	}

	return *node;
}

void
FaderPort::stripable_selection_changed ()
{
	set_current_stripable (ControlProtocol::first_selected_stripable());
}

void
FaderPort::drop_current_stripable ()
{
	if (_current_stripable) {
		if (_current_stripable == session->monitor_out()) {
			set_current_stripable (session->master_out());
		} else {
			set_current_stripable (std::shared_ptr<Stripable>());
		}
	}
}

void
FaderPort::set_current_stripable (std::shared_ptr<Stripable> r)
{
	stripable_connections.drop_connections ();

	_current_stripable = r;

	/* turn this off. It will be turned on back on in use_master() or
	   use_monitor() as appropriate.
	*/
	get_button(Output).set_led_state (false);

	if (_current_stripable) {
		_current_stripable->DropReferences.connect (stripable_connections, MISSING_INVALIDATOR, std::bind (&FaderPort::drop_current_stripable, this), this);

		_current_stripable->mute_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, std::bind (&FaderPort::map_mute, this), this);
		_current_stripable->solo_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, std::bind (&FaderPort::map_solo, this), this);

		std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track> (_current_stripable);
		if (t) {
			t->rec_enable_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, std::bind (&FaderPort::map_recenable, this), this);
		}

		std::shared_ptr<AutomationControl> control = _current_stripable->gain_control ();
		if (control) {
			control->Changed.connect (stripable_connections, MISSING_INVALIDATOR, std::bind (&FaderPort::map_gain, this), this);
			control->alist()->automation_state_changed.connect (stripable_connections, MISSING_INVALIDATOR, std::bind (&FaderPort::map_auto, this), this);
		}

		std::shared_ptr<MonitorProcessor> mp = _current_stripable->monitor_control();
		if (mp) {
			mp->cut_control()->Changed.connect (stripable_connections, MISSING_INVALIDATOR, std::bind (&FaderPort::map_cut, this), this);
		}
	}

	//ToDo: subscribe to the fader automation modes so we can light the LEDs

	map_stripable_state ();
}

void
FaderPort::map_auto ()
{
	/* Under no circumstances send a message to "enable" the LED state of
	 * the Off button, because this will disable the fader.
	 */

	std::shared_ptr<AutomationControl> control = _current_stripable->gain_control ();
	const AutoState as = control->automation_state ();

	switch (as) {
		case ARDOUR::Play:
			get_button (FP_Read).set_led_state (true);
			get_button (FP_Write).set_led_state (false);
			get_button (FP_Touch).set_led_state (false);
		break;
		case ARDOUR::Write:
			get_button (FP_Read).set_led_state (false);
			get_button (FP_Write).set_led_state (true);
			get_button (FP_Touch).set_led_state (false);
		break;
		case ARDOUR::Touch:
		case ARDOUR::Latch: // XXX
			get_button (FP_Read).set_led_state (false);
			get_button (FP_Write).set_led_state (false);
			get_button (FP_Touch).set_led_state (true);
		break;
		case ARDOUR::Off:
			get_button (FP_Read).set_led_state (false);
			get_button (FP_Write).set_led_state (false);
			get_button (FP_Touch).set_led_state (false);
		break;
	}

}


void
FaderPort::map_cut ()
{
	std::shared_ptr<MonitorProcessor> mp = _current_stripable->monitor_control();

	if (mp) {
		bool yn = mp->cut_all ();
		if (yn) {
			start_blinking (Mute);
		} else {
			stop_blinking (Mute);
		}
	} else {
		stop_blinking (Mute);
	}
}

void
FaderPort::map_mute ()
{
	if (_current_stripable) {
		if (_current_stripable->mute_control()->muted()) {
			stop_blinking (Mute);
			get_button (Mute).set_led_state (true);
		} else if (_current_stripable->mute_control()->muted_by_others_soloing () || _current_stripable->mute_control()->muted_by_masters()) {
			start_blinking (Mute);
		} else {
			stop_blinking (Mute);
		}
	} else {
		stop_blinking (Mute);
	}
}

void
FaderPort::map_solo ()
{
	if (_current_stripable) {
		get_button (Solo).set_led_state (_current_stripable->solo_control()->soloed());
	} else {
		get_button (Solo).set_led_state (false);
	}
}

void
FaderPort::map_recenable ()
{
	std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track> (_current_stripable);
	if (t) {
		get_button (Rec).set_led_state (t->rec_enable_control()->get_value());
	} else {
		get_button (Rec).set_led_state (false);
	}
}

void
FaderPort::map_gain ()
{
	if (fader_is_touched) {
		/* Do not send fader moves while the user is touching the fader */
		return;
	}

	if (!_current_stripable) {
		return;
	}

	std::shared_ptr<AutomationControl> control = _current_stripable->gain_control ();
	double val;

	if (!control) {
		val = 0.0;
	} else {
		val = control->internal_to_interface (control->get_value ());
	}

	/* Faderport sends fader position with range 0..16384 (though some of
	 * the least-significant bits at the top end are missing - it may only
	 * get to 1636X or so).
	 *
	 * But ... position must be sent in the range 0..1023.
	 *
	 * Thanks, Obama.
	 */

	int ival = (int) lrintf (val * 1023.0);

	/* MIDI normalization requires that we send two separate messages here,
	 * not one single 6 byte one.
	 */

	MIDI::byte buf[3];

	buf[0] = 0xb0;
	buf[1] = 0x0;
	buf[2] = ival >> 7;

	MIDISurface::write (buf, 3);

	buf[1] = 0x20;
	buf[2] = ival & 0x7f;

	MIDISurface::write (buf, 3);
}

void
FaderPort::map_stripable_state ()
{
	if (!_current_stripable) {
		stop_blinking (Mute);
		stop_blinking (Solo);
		get_button (Rec).set_led_state (false);
	} else {
		map_solo ();
		map_recenable ();
		map_gain ();
		map_auto ();

		if (_current_stripable == session->monitor_out()) {
			map_cut ();
		} else {
			map_mute ();
		}
	}
}

void
FaderPort::set_action (ButtonID id, std::string const& action_name, bool on_press, ButtonState bs)
{
	get_button(id).set_action (action_name, on_press, bs);
}

string
FaderPort::get_action (ButtonID id, bool press, ButtonState bs)
{
	return get_button(id).get_action (press, bs);
}


void
FaderPort::notify_record_state_changed ()
{
	map_recenable_state ();
}

void
FaderPort::notify_transport_state_changed()
{
	map_transport_state ();
}

void
FaderPort::notify_loop_state_changed ()
{
	map_transport_state ();
}
