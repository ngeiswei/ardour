/*
 * Copyright (C) 2018-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include <list>
#include <string>

#include "pbd/i18n.h"

#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "ardour/amp.h"
#include "ardour/midi_track.h"

#include "grid.h"
#include "midi_track_toolbar.h"
#include "tracker_editor.h"
#include "tracker_utils.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace Tracker;

MidiTrackToolbar::MidiTrackToolbar (TrackerEditor& te, MidiTrackPattern& mtp, int mti)
	: TrackToolbar (te, &mtp, mti)
	, midi_track (mtp.midi_track)
	, midi_track_pattern (mtp)
	, visible_note (true)
	, visible_channel (false)
	, visible_velocity (true)
{
	setup_processor_menu_and_curves ();
	setup ();
}

MidiTrackToolbar::~MidiTrackToolbar ()
{
}

void
MidiTrackToolbar::setup ()
{
	set_spacing (spacing);

	setup_rm_add_note_col ();
	setup_note ();
	setup_channel ();
	setup_velocity ();
	setup_delay ();
	setup_automation ();

	show ();

	setup_tooltips ();
}

void
MidiTrackToolbar::setup_note ()
{
	// Add visible note button
	visible_note_button.set_name ("visible note button");
	visible_note_button.set_text (S_("Note|N"));
	update_visible_note_button ();
	visible_note_button.show ();
	visible_note_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MidiTrackToolbar::visible_note_press), false);
	pack_start (visible_note_button, false, false);
}

void
MidiTrackToolbar::setup_channel ()
{
	// Add visible channel button
	visible_channel_button.set_name ("visible channel button");
	visible_channel_button.set_text (S_("Channel|C"));
	update_visible_channel_button ();
	visible_channel_button.show ();
	visible_channel_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MidiTrackToolbar::visible_channel_press), false);
	pack_start (visible_channel_button, false, false);
}

void
MidiTrackToolbar::setup_velocity ()
{
	// Add visible velocity button
	visible_velocity_button.set_name ("visible velocity button");
	visible_velocity_button.set_text (S_("Velocity|V"));
	update_visible_velocity_button ();
	visible_velocity_button.show ();
	visible_velocity_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MidiTrackToolbar::visible_velocity_press), false);
	pack_start (visible_velocity_button, false, false);
}

void
MidiTrackToolbar::setup_rm_add_note_col ()
{
	// Remove/add note column
	remove_note_column_button.set_name ("remove note column");
	remove_note_column_button.set_text (S_("Remove|-"));
	remove_note_column_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MidiTrackToolbar::remove_note_column_press), false);
	remove_note_column_button.show ();
	remove_note_column_button.set_sensitive (false);
	pack_start (remove_note_column_button, false, false);
	add_note_column_button.set_name ("add note column");
	add_note_column_button.set_text (S_("Add|+"));
	add_note_column_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &MidiTrackToolbar::add_note_column_press), false);
	add_note_column_button.show ();
	pack_start (add_note_column_button, false, false);

	// Separator
	pack_start (rm_add_note_column_separator, false, false);
	rm_add_note_column_separator.show ();
}

void
MidiTrackToolbar::setup_automation ()
{
	// Separator
	pack_start (automation_separator, false, false);
	automation_separator.show ();

	// Add automation button
	automation_button.set_name ("automation button");
	automation_button.set_text (S_("Automation|A"));
	update_automation_button ();
	automation_button.show ();
	pack_start (automation_button, false, false);
}

void
MidiTrackToolbar::setup_tooltips ()
{
	set_tooltip (visible_note_button, _("Toggle note visibility"));
	set_tooltip (visible_channel_button, _("Toggle channel visibility"));
	set_tooltip (visible_velocity_button, _("Toggle velocity visibility"));
	set_tooltip (visible_delay_button, _("Toggle delay visibility"));
	set_tooltip (remove_note_column_button, _("Remove note column"));
	set_tooltip (add_note_column_button, _("Add note column"));
	set_tooltip (automation_button, _("MIDI Controllers and Automation"));
}

bool
MidiTrackToolbar::visible_note_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_note = !visible_note;
	// grid.redisplay_visible_note ();
	// grid.redisplay_visible_channel ();
	// grid.redisplay_visible_velocity ();
	// grid.redisplay_visible_delay ();
	// grid.redisplay_visible_note_separator ();
	redisplay_grid ();
	return false;
}

bool
MidiTrackToolbar::visible_channel_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_channel = !visible_channel;
	// grid.redisplay_visible_channel ();
	redisplay_grid ();
	return false;
}

bool
MidiTrackToolbar::visible_velocity_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_velocity = !visible_velocity;
	// grid.redisplay_visible_velocity ();
	redisplay_grid ();
	return false;
}

void
MidiTrackToolbar::automation_click ()
{
	build_automation_menu ();
	automation_action_menu->popup (1, gtk_get_current_event_time ());
}

bool
MidiTrackToolbar::remove_note_column_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	midi_track_pattern.dec_ntracks ();
	redisplay_grid ();
	update_remove_note_column_button ();
	update_add_note_column_button ();

	return false;
}

bool
MidiTrackToolbar::add_note_column_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	midi_track_pattern.inc_ntracks ();
	redisplay_grid ();
	update_remove_note_column_button ();
	update_add_note_column_button ();

	return false;
}

void
MidiTrackToolbar::build_automation_menu ()
{
	TrackToolbar::build_automation_menu ();
	build_midi_automation_menu ();
}

void
MidiTrackToolbar::build_show_hide_automations (Menu_Helpers::MenuList& items)
{
	using namespace Menu_Helpers;
	items.push_back (MenuElem (_("Show All Automation"),
	                           sigc::mem_fun (*this, &MidiTrackToolbar::show_all_automation)));

	items.push_back (MenuElem (_("Show Existing Automation"),
	                           sigc::mem_fun (*this, &MidiTrackToolbar::show_existing_automation)));

	items.push_back (MenuElem (_("Hide All Automation"),
	                           sigc::mem_fun (*this, &MidiTrackToolbar::hide_all_automation)));
}

void
MidiTrackToolbar::build_midi_automation_menu ()
{
	/* Add any midi automation */

	_channel_command_menu_map.clear ();

	Menu_Helpers::MenuList& automation_items = automation_action_menu->items ();

	uint16_t selected_channels = midi_track->get_playback_channel_mask ();

	if (selected_channels !=  0) {

		automation_items.push_back (Menu_Helpers::SeparatorElem ());

		/* these 2 MIDI "command" types are semantically more like automation
		   than note data, but they are not MIDI controllers. We give them
		   special status in this menu, since they will not show up in the
		   controller list and anyone who actually knows something about MIDI
		   (!) would not expect to find them there.
		*/

		add_channel_command_menu_item (
			automation_items, _("Bender"), MidiPitchBenderAutomation, 0);
		automation_items.back ().set_sensitive (true);
		add_channel_command_menu_item (
			automation_items, _("Pressure"), MidiChannelPressureAutomation, 0);
		automation_items.back ().set_sensitive (true);

		/* now all MIDI controllers. Always offer the possibility that we will
		   rebuild the controllers menu since it might need to be updated after
		   a channel mode change or other change. Also detach it first in case
		   it has been used anywhere else.
		*/

		build_controller_menu ();

		automation_items.push_back (Menu_Helpers::MenuElem (_("Controllers"), *controller_menu));
		// TODO: add Polyphonic Pressure.  See midi_time_axis.cc.
		automation_items.back ().set_sensitive (true);
	} else {
		automation_items.push_back (
			Menu_Helpers::MenuElem (string_compose ("<i>%1</i>", _("No MIDI Channels selected"))));
		dynamic_cast<Label*> (automation_items.back ().get_child ())->set_use_markup (true);
	}
}

void
MidiTrackToolbar::build_controller_menu ()
{
	using namespace Menu_Helpers;

	/* For some reason an already built controller menu cannot be attached
	   so let's rebuild it */
	delete controller_menu;

	// TODO: compare with lastest ardour changes to update if necessary

	controller_menu = new Menu; // explicitly managed by us
	Menu_Helpers::MenuList& items (controller_menu->items ());

	/* create several "top level" menu items for sets of controllers (16 at a
	   time), and populate each one with a submenu for each controller+channel
	   combination covering the currently selected channels for this track
	*/

	const uint16_t selected_channels = midi_track->get_playback_channel_mask ();

	/* count the number of selected channels because we will build a different menu
	   structure if there is more than 1 selected.
	*/

	int chn_cnt = 0;
	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {
			if (++chn_cnt > 1) {
				break;
			}
		}
	}

	using namespace MIDI::Name;
	std::shared_ptr<MasterDeviceNames> device_names = get_device_names ();

	if (device_names && !device_names->controls ().empty ()) {
		/* Controllers names available in midnam file, generate fancy menu */
		unsigned n_items  = 0;
		unsigned n_groups = 0;

		/* TODO: This is not correct, should look up the currently applicable ControlNameList
		   and only build a menu for that one. */
		for (MasterDeviceNames::ControlNameLists::const_iterator l = device_names->controls ().begin ();
		     l != device_names->controls ().end (); ++l) {
			std::shared_ptr<ControlNameList> name_list = l->second;
			Menu*                            ctl_menu  = 0;

			for (ControlNameList::Controls::const_iterator c = name_list->controls ().begin ();
			     c != name_list->controls ().end ();) {
				const uint16_t ctl = c->second->number ();
				if (ctl != MIDI_CTL_MSB_BANK && ctl != MIDI_CTL_LSB_BANK) {
					/* Skip bank select controllers since they're handled specially */
					if (n_items == 0) {
						/* Create a new submenu */
						ctl_menu = Gtk::manage (new Menu);
					}

					Menu_Helpers::MenuList& ctl_items (ctl_menu->items ());
					if (chn_cnt > 1) {
						add_multi_channel_controller_item (ctl_items, ctl, c->second->name ());
					} else {
						add_single_channel_controller_item (ctl_items, ctl, c->second->name ());
					}
				}

				++c;
				if (ctl_menu && (++n_items == 16 || c == name_list->controls ().end ())) {
					/* Submenu has 16 items or we're done, add it to controller menu and reset */
					items.push_back (
						Menu_Helpers::MenuElem (string_compose (_("Controllers %1-%2"),
						                                        (16 * n_groups), (16 * n_groups) + n_items - 1),
						                        *ctl_menu));
					ctl_menu = 0;
					n_items  = 0;
					++n_groups;
				}
			}
		}
	} else {
		/* No controllers names, generate generic numeric menu */
		for (int i = 0; i < 127; i += 16) {
			Menu*     ctl_menu = Gtk::manage (new Menu);
			Menu_Helpers::MenuList& ctl_items (ctl_menu->items ());

			for (int ctl = i; ctl < i+16; ++ctl) {
				if (ctl == MIDI_CTL_MSB_BANK || ctl == MIDI_CTL_LSB_BANK) {
					/* Skip bank select controllers since they're handled specially */
					continue;
				}

				if (chn_cnt > 1) {
					add_multi_channel_controller_item (
						ctl_items, ctl, string_compose (_("Controller %1"), ctl));
				} else {
					add_single_channel_controller_item (
						ctl_items, ctl, string_compose (_("Controller %1"), ctl));
				}
			}

			/* Add submenu for this block of controllers to controller menu */
			items.push_back (
				Menu_Helpers::MenuElem (string_compose (_("Controllers %1-%2"), i, i + 15),
				                        *ctl_menu));
		}
	}
}

void
MidiTrackToolbar::add_channel_command_menu_item (Menu_Helpers::MenuList& items,
                                                 const std::string&      label,
                                                 AutomationType          automation_type,
                                                 uint8_t                 cmd)
{
	using namespace Menu_Helpers;

	/* count the number of selected channels because we will build a different menu
	   structure if there is more than 1 selected.
	 */

	const uint16_t selected_channels = midi_track->get_playback_channel_mask ();
	int chn_cnt = 0;

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {
			if (++chn_cnt > 1) {
				break;
			}
		}
	}

	if (chn_cnt > 1) {

		/* multiple channels - create a submenu, with 1 item per channel */

		Menu* chn_menu = Gtk::manage (new Menu);
		Menu_Helpers::MenuList& chn_items (chn_menu->items ());
		Evoral::Parameter param_without_channel (automation_type, 0, cmd);

		/* add a couple of items to hide/show all of them */

		chn_items.push_back (
			Menu_Helpers::MenuElem (_("Hide all channels"),
			                        sigc::bind (sigc::mem_fun (grid, &Grid::change_all_channel_tracks_visibility),
			                                    track_index, false, param_without_channel)));
		chn_items.push_back (
			Menu_Helpers::MenuElem (_("Show all channels"),
			                        sigc::bind (sigc::mem_fun (grid, &Grid::change_all_channel_tracks_visibility),
			                                    track_index, true, param_without_channel)));

		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {

				/* for each selected channel, add a menu item for this controller */

				Evoral::Parameter fully_qualified_param (automation_type, chn, cmd);
				IDParameter id_param (PBD::ID (0), fully_qualified_param);
				chn_items.push_back (
					CheckMenuElem (string_compose (_("Channel %1"), chn+1),
					               sigc::bind (sigc::mem_fun (grid, &Grid::update_automation_column_visibility),
					                           track_index, id_param)));

				bool visible = grid.is_automation_visible (track_index, id_param);

				CheckMenuItem* cmi = static_cast<CheckMenuItem*> (&chn_items.back ());
				_channel_command_menu_map[fully_qualified_param] = cmi;
				cmi->set_active (visible);
			}
		}

		/* now create an item in the parent menu that has the per-channel list as a submenu */

		items.push_back (Menu_Helpers::MenuElem (label, *chn_menu));

	} else {

		/* just one channel - create a single menu item for this command+channel combination*/

		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {

				Evoral::Parameter fully_qualified_param (automation_type, chn, cmd);
				IDParameter id_param (PBD::ID (0), fully_qualified_param);
				items.push_back (
					CheckMenuElem (label,
					               sigc::bind (sigc::mem_fun (grid, &Grid::update_automation_column_visibility),
					                           track_index, id_param)));

				bool visible = grid.is_automation_visible (track_index, id_param);

				CheckMenuItem* cmi = static_cast<CheckMenuItem*> (&items.back ());
				_channel_command_menu_map[fully_qualified_param] = cmi;
				cmi->set_active (visible);

				/* one channel only */
				break;
			}
		}
	}
}

/** Add a single menu item for a controller on one channel. */
void
MidiTrackToolbar::add_single_channel_controller_item (Menu_Helpers::MenuList& ctl_items,
                                                      int                     ctl,
                                                      const std::string&      name)
{
	using namespace Menu_Helpers;

	const uint16_t selected_channels = midi_track->get_playback_channel_mask ();
	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			IDParameter id_param (PBD::ID (0), fully_qualified_param);
			ctl_items.push_back (
				CheckMenuElem (
					string_compose ("<b>%1</b>: %2 [%3]", ctl, name, int (chn + 1)),
					sigc::bind (
						sigc::mem_fun (grid, &Grid::update_automation_column_visibility),
						track_index, id_param)));
			dynamic_cast<Label*> (ctl_items.back ().get_child ())->set_use_markup (true);

			bool visible = grid.is_automation_visible (track_index, id_param);

			CheckMenuItem* cmi = static_cast<CheckMenuItem*> (&ctl_items.back ());
			_controller_menu_map[fully_qualified_param] = cmi;
			cmi->set_active (visible);

			/* one channel only */
			break;
		}
	}
}

/** Add a submenu with 1 item per channel for a controller on many channels. */
void
MidiTrackToolbar::add_multi_channel_controller_item (Menu_Helpers::MenuList& ctl_items,
                                                     int                     ctl,
                                                     const std::string&      name)
{
	using namespace Menu_Helpers;

	const uint16_t selected_channels = midi_track->get_playback_channel_mask ();

	Menu* chn_menu = Gtk::manage (new Menu);
	Menu_Helpers::MenuList& chn_items (chn_menu->items ());

	/* add a couple of items to hide/show this controller on all channels */

	Evoral::Parameter param_without_channel (MidiCCAutomation, 0, ctl);
	chn_items.push_back (
		Menu_Helpers::MenuElem (_("Hide all channels"),
		                        sigc::bind (sigc::mem_fun (grid, &Grid::change_all_channel_tracks_visibility),
		                                    track_index, false, param_without_channel)));
	chn_items.push_back (
		Menu_Helpers::MenuElem (_("Show all channels"),
		                        sigc::bind (sigc::mem_fun (grid, &Grid::change_all_channel_tracks_visibility),
		                                    track_index, true, param_without_channel)));

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			/* for each selected channel, add a menu item for this controller */

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			IDParameter id_param (PBD::ID (0), fully_qualified_param);
			chn_items.push_back (
				CheckMenuElem (string_compose (_("Channel %1"), chn+1),
				               sigc::bind (sigc::mem_fun (grid, &Grid::update_automation_column_visibility),
				                           track_index, id_param)));

			bool visible = grid.is_automation_visible (track_index, id_param);

			CheckMenuItem* cmi = static_cast<CheckMenuItem*> (&chn_items.back ());
			_controller_menu_map[fully_qualified_param] = cmi;
			cmi->set_active (visible);
		}
	}

	/* add the per-channel menu to the list of controllers, with the name of the controller */
	ctl_items.push_back (Menu_Helpers::MenuElem (string_compose ("<b>%1</b>: %2", ctl, name),
	                                             *chn_menu));
	dynamic_cast<Label*> (ctl_items.back ().get_child ())->set_use_markup (true);
}

CheckMenuItem* MidiTrackToolbar::automation_child_menu_item (const IDParameter& id_param)
{
	ParameterMenuMap::iterator cmm_it = _controller_menu_map.find (id_param.second);
	ParameterMenuMap::iterator ccmm_it = _channel_command_menu_map.find (id_param.second);
	CheckMenuItem* mitem = nullptr;
	if (cmm_it != _controller_menu_map.end ()) {
		mitem = cmm_it->second;
	} else if (ccmm_it != _channel_command_menu_map.end ()) {
		mitem = ccmm_it->second;
	}
	return mitem;
}

// Show all automation, with the exception of midi automations, only show the
// existing one, because there are too many.
//
// TODO: this menu needs to be persistent between sessions. Should also be
// fixed for the track/piano roll view.
void
MidiTrackToolbar::show_all_automation ()
{
	show_all_main_automations ();
	show_existing_midi_automations ();
	show_all_processor_automations ();

	redisplay_grid ();
}

void
MidiTrackToolbar::show_existing_automation ()
{
	show_existing_main_automations ();
	show_existing_midi_automations ();
	show_existing_processor_automations ();

	redisplay_grid ();
}

void
MidiTrackToolbar::hide_all_automation ()
{
	hide_main_automations ();
	hide_midi_automations ();
	hide_processor_automations ();

	redisplay_grid ();
}

void
MidiTrackToolbar::show_existing_midi_automations ()
{
	const ParameterSet params = midi_track->midi_playlist ()->contained_automation ();
	for (ParameterSetConstIt p = params.begin (); p != params.end (); ++p) {
		IDParameter id_param (/* MIDI PBD::ID is 0*/ PBD::ID (0), *p);
		Grid::IndexParamBimap::right_const_iterator it = grid.col2params[track_index].right.find (id_param);
		int column = (it == grid.col2params[track_index].right.end ()) || (it->second == 0) ?
			grid.add_midi_automation_column (track_index, *p) : it->second;

		// Still no column available, skip
		if (column == 0) {
			continue;
		}

		grid.set_automation_column_visible (track_index, id_param, column, true);
	}
}

void
MidiTrackToolbar::hide_midi_automations ()
{
	std::set<int> to_remove;
	for (std::set<int>::iterator it = grid.visible_automation_columns.begin ();
	     it != grid.visible_automation_columns.end (); ++it) {
		int column = *it;
		Grid::IndexParamBimap::left_const_iterator c2p_it = grid.col2params[track_index].left.find (column);
		if (c2p_it == grid.col2params[track_index].left.end ()) {
			continue;
		}

		IDParameter id_param = c2p_it->second;
		CheckMenuItem* mitem = automation_child_menu_item (id_param);

		if (mitem) {
			to_remove.insert (column);
		}
	}
	for (std::set<int>::iterator it = to_remove.begin ();
	     it != to_remove.end (); ++it)
		grid.visible_automation_columns.erase (*it);
}

void
MidiTrackToolbar::update_visible_note_button ()
{
	visible_note_button.set_active_state (visible_note ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_visible_channel_button ()
{
	visible_channel_button.set_active_state (visible_channel ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_visible_velocity_button ()
{
	visible_velocity_button.set_active_state (visible_velocity ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_remove_note_column_button ()
{
	remove_note_column_button.set_sensitive (midi_track_pattern.get_nreqtracks () < midi_track_pattern.get_ntracks ());
}

void
MidiTrackToolbar::update_add_note_column_button ()
{
	add_note_column_button.set_sensitive (midi_track_pattern.get_ntracks () < MAX_NUMBER_OF_NOTE_TRACKS_PER_TRACK);
}

void
MidiTrackToolbar::update_automation_button ()
{
	automation_button.signal_clicked.connect (sigc::mem_fun (*this, &MidiTrackToolbar::automation_click));
}

void
MidiTrackToolbar::print_widths () const
{
	std::cout << "visible_note_button.get_width () = " << visible_note_button.get_width () << std::endl;
	std::cout << "visible_channel_button.get_width () = " << visible_channel_button.get_width () << std::endl;
	std::cout << "visible_velocity_button.get_width () = " << visible_velocity_button.get_width () << std::endl;
	std::cout << "remove_note_column_button.get_width () = " << remove_note_column_button.get_width () << std::endl;
	std::cout << "add_note_column_button.get_width () = " << add_note_column_button.get_width () << std::endl;
	std::cout << "rm_add_note_column_separator.get_width () = " << rm_add_note_column_separator.get_width () << std::endl;
	std::cout << "automation_separator.get_width () = " << automation_separator.get_width () << std::endl;
	std::cout << "automation_button.get_width () = " << automation_button.get_width () << std::endl;
}

int
MidiTrackToolbar::get_min_width () const
{
	int width = visible_note_button.get_width () + spacing +
		visible_channel_button.get_width () + spacing +
		visible_velocity_button.get_width () + spacing +
		remove_note_column_button.get_width () + spacing +
		add_note_column_button.get_width () + spacing +
		rm_add_note_column_separator.get_width () + spacing +
		visible_delay_button.get_width () + spacing +
		automation_separator.get_width () + spacing +
		automation_button.get_width ();

	return width;
}

std::shared_ptr<MIDI::Name::MasterDeviceNames>
MidiTrackToolbar::get_device_names ()
{
	return midi_track_pattern.get_device_names ();
}
