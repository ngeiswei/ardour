/*
 * Copyright (C) 2005-2011 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <vector>

#include <ytkmm/box.h>
#include <ytkmm/adjustment.h>
#include <ytkmm/eventbox.h>
#include <ytkmm/arrow.h>
#include <ytkmm/togglebutton.h>
#include <ytkmm/button.h>

#include "ardour/session_handle.h"

#include "enums.h"

class Panner2d;
class Panner2dWindow;
class StereoPanner;
class MonoPanner;

namespace ARDOUR {
	class Session;
	class Panner;
	class PannerShell;
	class Delivery;
	class AutomationControl;
}

namespace Gtk {
	class CheckMenuItem;
	class Menu;
	class Menuitem;
}

class PannerUI : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	PannerUI (ARDOUR::Session*);
	~PannerUI ();

	virtual void set_panner (std::shared_ptr<ARDOUR::PannerShell>, std::shared_ptr<ARDOUR::Panner>);

	void panshell_changed ();

	void update_pan_sensitive ();
	void update_gain_sensitive ();

	void set_width (Width);
	void setup_pan ();
	void set_available_panners(std::map<std::string,std::string>);
	void set_send_drawing_mode (bool);

	void effective_pan_display ();

	void set_meter_strip_name (std::string name);

	void on_size_allocate (Gtk::Allocation &);

	static void setup_slider_pix ();

private:
	friend class MixerStrip;
	friend class TriggerStrip;
	friend class SendUI;

	std::shared_ptr<ARDOUR::PannerShell> _panshell;
	std::shared_ptr<ARDOUR::Panner> _panner;
	PBD::ScopedConnectionList connections;
	PBD::ScopedConnectionList _pan_control_connections;

	bool in_pan_update;
	int _current_nouts;
	int _current_nins;
	std::string _current_uri;
	bool _send_mode;

	Panner2d*       twod_panner; ///< 2D panner, or 0
	Panner2dWindow* big_window;

	Gtk::VBox           pan_bar_packer;
	Gtk::VBox           pan_vbox;
	Gtk::VBox           poswidth_box;
	Width              _width;

	StereoPanner*  _stereo_panner;
	MonoPanner*    _mono_panner;

	bool _ignore_width_change;
	bool _ignore_position_change;
	void width_adjusted ();
	void show_width ();
	void position_adjusted ();
	void show_position ();

	Gtk::Menu* pan_astate_menu;
	Gtk::Menu* pan_astyle_menu;

	Gtk::ToggleButton pan_automation_state_button;

	void pan_value_changed (uint32_t which);
	void build_astate_menu ();
	void build_astyle_menu ();

	void hide_pans ();

	void panner_moved (int which);
	void panner_bypass_toggled ();

	gint start_pan_touch (GdkEventButton*);
	gint end_pan_touch (GdkEventButton*);

	bool pan_button_event (GdkEventButton*);

	Gtk::Menu* pan_menu;
	Gtk::CheckMenuItem* bypass_menu_item;
	Gtk::CheckMenuItem* send_link_menu_item;
	void build_pan_menu ();
	void pan_reset ();
	void pan_bypass_toggle ();
	void pan_link_toggle ();
	void pan_edit ();
	void pan_set_custom_type (std::string type);

	void pan_automation_state_changed();
	gint pan_automation_state_button_event (GdkEventButton *);

	void start_touch (std::weak_ptr<ARDOUR::AutomationControl>);
	void stop_touch (std::weak_ptr<ARDOUR::AutomationControl>);

	std::map<std::string,std::string> _panner_list;
	bool _suspend_menu_callbacks;
};


