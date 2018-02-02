/*
    Copyright (C) 2018 Nil Geisweiller

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

#include "midi_track_toolbar.h"
#include "midi_tracker_editor.h"

#include "widgets/tooltips.h"
#include "ardour/midi_track.h"
#include "ardour/amp.h"
#include "midi++2/midi++/midnam_patch.h"

#include "pbd/i18n.h"
#include "gtkmm2ext/utils.h"

#include <list>
#include <string>

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;

MidiTrackToolbar::MidiTrackToolbar (MidiTrackerEditor& mte, MidiTrackPattern& mtp)
	: midi_tracker_editor (mte)
	, midi_track_pattern (mtp)
	, visible_note (true)
	, visible_channel (false)
	, visible_velocity (false)
	, visible_delay (false)
	, automation_action_menu (NULL)
	, controller_menu (NULL)
{
}

MidiTrackToolbar::~MidiTrackToolbar ()
{
	delete automation_action_menu;
	delete controller_menu;
}

void
MidiTrackToolbar::setup ()
{
	setup_layout ();
	setup_tooltips ();
}

void
MidiTrackToolbar::setup_layout ()
{
	set_spacing (2);

	// Add visible note button
	visible_note_button.set_name ("visible note button");
	visible_note_button.set_text (S_("Note|N"));
	update_visible_note_button ();
	visible_note_button.show ();
	visible_note_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::visible_note_press), false);
	pack_start (visible_note_button, false, false);

	// Add visible channel button
	visible_channel_button.set_name ("visible channel button");
	visible_channel_button.set_text (S_("Channel|C"));
	update_visible_channel_button ();
	visible_channel_button.show ();
	visible_channel_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::visible_channel_press), false);
	pack_start (visible_channel_button, false, false);

	// Add visible velocity button
	visible_velocity_button.set_name ("visible velocity button");
	visible_velocity_button.set_text (S_("Velocity|V"));
	update_visible_velocity_button ();
	visible_velocity_button.show ();
	visible_velocity_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::visible_velocity_press), false);
	pack_start (visible_velocity_button, false, false);

	// Add visible delay button
	visible_delay_button.set_name ("visible delay button");
	visible_delay_button.set_text (S_("Delay|D"));
	update_visible_delay_button ();
	visible_delay_button.show ();
	visible_delay_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::visible_delay_press), false);
	pack_start (visible_delay_button, false, false);

	// Add automation button
	automation_button.set_name ("automation button");
	automation_button.set_text (S_("Automation|A"));
	update_automation_button ();
	automation_button.show ();
	pack_start (automation_button, false, false);

	// Remove/add note column
	rm_add_note_column_separator.show ();
	pack_start (rm_add_note_column_separator, false, false);
	remove_note_column_button.set_name ("remove note column");
	remove_note_column_button.set_text (S_("Remove|-"));
	remove_note_column_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::remove_note_column_press), false);
	remove_note_column_button.show ();
	remove_note_column_button.set_sensitive (false);
	pack_start (remove_note_column_button, false, false);
	add_note_column_button.set_name ("add note column");
	add_note_column_button.set_text (S_("Add|+"));
	add_note_column_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackToolbar::add_note_column_press), false);
	add_note_column_button.show ();
	pack_start (add_note_column_button, false, false);

	show ();
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
MidiTrackToolbar::visible_note_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_note = !visible_note;
	midi_tracker_editor.redisplay_visible_note();
	return false;
}

bool
MidiTrackToolbar::visible_channel_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_channel = !visible_channel;
	midi_tracker_editor.redisplay_visible_channel();
	return false;
}

bool
MidiTrackToolbar::visible_velocity_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_velocity = !visible_velocity;
	midi_tracker_editor.redisplay_visible_velocity();
	return false;
}

bool
MidiTrackToolbar::visible_delay_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_delay = !visible_delay;
	midi_tracker_editor.redisplay_visible_delay();
	return false;
}

void
MidiTrackToolbar::automation_click ()
{
	build_automation_action_menu ();
	automation_action_menu->popup (1, gtk_get_current_event_time());
}

bool
MidiTrackToolbar::remove_note_column_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	midi_track_pattern.np.dec_ntracks ();
	midi_tracker_editor.redisplay_model ();
	update_remove_note_column_button ();

	return false;
}

bool
MidiTrackToolbar::add_note_column_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	midi_track_pattern.np.inc_ntracks ();
	midi_tracker_editor.redisplay_model ();
	update_remove_note_column_button ();

	return false;
}

void
MidiTrackToolbar::build_automation_action_menu ()
{
	using namespace Menu_Helpers;

	/* detach subplugin_menu from automation_action_menu before we delete automation_action_menu,
	   otherwise bad things happen (see comment for similar case in MidiTimeAxisView::build_automation_action_menu)
	*/

	detach_menu (subplugin_menu);

	delete automation_action_menu;
	automation_action_menu = new Menu;

	MenuList& items = automation_action_menu->items();

	automation_action_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All Automation"),
	                           sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::show_all_automation)));

	items.push_back (MenuElem (_("Show Existing Automation"),
	                           sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::show_existing_automation)));

	items.push_back (MenuElem (_("Hide All Automation"),
	                           sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::hide_all_automation)));

	/* Attach the plugin submenu. It may have previously been used elsewhere,
	   so it was detached above
	*/

	// TODO could be optimized, no need to rebuild everything
	setup_processor_menu_and_curves ();
	midi_tracker_editor.build_param2actrl ();
	midi_tracker_editor.update_automation_patterns ();

	if (!subplugin_menu.items().empty()) {
		items.push_back (SeparatorElem ());
		items.push_back (MenuElem (_("Processor automation"), subplugin_menu));
		items.back().set_sensitive (true);
	}

	/* Add any route automation */

	if (true) {
		items.push_back (CheckMenuElem (_("Fader"), sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::update_gain_column_visibility)));
		gain_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		gain_automation_item->set_active (midi_tracker_editor.is_gain_visible());
	}

	if (false /*trim_track*/ /* TODO: support audio track */) {
		items.push_back (CheckMenuElem (_("Trim"), sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::update_trim_column_visibility)));
		trim_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		trim_automation_item->set_active (false);
	}

	if (true /*mute_track*/) {
		items.push_back (CheckMenuElem (_("Mute"), sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::update_mute_column_visibility)));
		mute_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		mute_automation_item->set_active (midi_tracker_editor.is_mute_visible());
	}

	if (true /*pan_tracks*/) {
		items.push_back (CheckMenuElem (_("Pan"), sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::update_pan_columns_visibility)));
		pan_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		pan_automation_item->set_active (midi_tracker_editor.is_pan_visible());
	}

	/* Add any midi automation */

	_channel_command_menu_map.clear ();

	MenuList& automation_items = automation_action_menu->items();

	uint16_t selected_channels = midi_tracker_editor.midi_track()->get_playback_channel_mask();

	if (selected_channels !=  0) {

		automation_items.push_back (SeparatorElem());

		/* these 2 MIDI "command" types are semantically more like automation
		   than note data, but they are not MIDI controllers. We give them
		   special status in this menu, since they will not show up in the
		   controller list and anyone who actually knows something about MIDI
		   (!) would not expect to find them there.
		*/

		add_channel_command_menu_item (
			automation_items, _("Bender"), MidiPitchBenderAutomation, 0);
		automation_items.back().set_sensitive (true);
		add_channel_command_menu_item (
			automation_items, _("Pressure"), MidiChannelPressureAutomation, 0);
		automation_items.back().set_sensitive (true);

		/* now all MIDI controllers. Always offer the possibility that we will
		   rebuild the controllers menu since it might need to be updated after
		   a channel mode change or other change. Also detach it first in case
		   it has been used anywhere else.
		*/

		build_controller_menu ();

		automation_items.push_back (MenuElem (_("Controllers"), *controller_menu));
		automation_items.back().set_sensitive (true);
	} else {
		automation_items.push_back (
			MenuElem (string_compose ("<i>%1</i>", _("No MIDI Channels selected"))));
		dynamic_cast<Label*> (automation_items.back().get_child())->set_use_markup (true);
	}
}

void
MidiTrackToolbar::build_controller_menu ()
{
	using namespace Menu_Helpers;

	if (controller_menu) {
		/* For some reason an already built controller menu cannot be attached
		   so let's rebuild it */
		delete controller_menu;
	}

	controller_menu = new Menu; // explicitly managed by us
	MenuList& items (controller_menu->items());

	/* create several "top level" menu items for sets of controllers (16 at a
	   time), and populate each one with a submenu for each controller+channel
	   combination covering the currently selected channels for this track
	*/

	const uint16_t selected_channels = midi_tracker_editor.midi_track()->get_playback_channel_mask();

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
	boost::shared_ptr<MasterDeviceNames> device_names = midi_tracker_editor.get_device_names();

	if (device_names && !device_names->controls().empty()) {
		/* Controllers names available in midnam file, generate fancy menu */
		unsigned n_items  = 0;
		unsigned n_groups = 0;

		/* TODO: This is not correct, should look up the currently applicable ControlNameList
		   and only build a menu for that one. */
		for (MasterDeviceNames::ControlNameLists::const_iterator l = device_names->controls().begin();
		     l != device_names->controls().end(); ++l) {
			boost::shared_ptr<ControlNameList> name_list = l->second;
			Menu*                              ctl_menu  = NULL;

			for (ControlNameList::Controls::const_iterator c = name_list->controls().begin();
			     c != name_list->controls().end();) {
				const uint16_t ctl = c->second->number();
				if (ctl != MIDI_CTL_MSB_BANK && ctl != MIDI_CTL_LSB_BANK) {
					/* Skip bank select controllers since they're handled specially */
					if (n_items == 0) {
						/* Create a new submenu */
						ctl_menu = manage (new Menu);
					}

					MenuList& ctl_items (ctl_menu->items());
					if (chn_cnt > 1) {
						add_multi_channel_controller_item(ctl_items, ctl, c->second->name());
					} else {
						add_single_channel_controller_item(ctl_items, ctl, c->second->name());
					}
				}

				++c;
				if (ctl_menu && (++n_items == 16 || c == name_list->controls().end())) {
					/* Submenu has 16 items or we're done, add it to controller menu and reset */
					items.push_back(
						MenuElem(string_compose(_("Controllers %1-%2"),
						                        (16 * n_groups), (16 * n_groups) + n_items - 1),
						         *ctl_menu));
					ctl_menu = NULL;
					n_items  = 0;
					++n_groups;
				}
			}
		}
	} else {
		/* No controllers names, generate generic numeric menu */
		for (int i = 0; i < 127; i += 16) {
			Menu*     ctl_menu = manage (new Menu);
			MenuList& ctl_items (ctl_menu->items());

			for (int ctl = i; ctl < i+16; ++ctl) {
				if (ctl == MIDI_CTL_MSB_BANK || ctl == MIDI_CTL_LSB_BANK) {
					/* Skip bank select controllers since they're handled specially */
					continue;
				}

				if (chn_cnt > 1) {
					add_multi_channel_controller_item(
						ctl_items, ctl, string_compose(_("Controller %1"), ctl));
				} else {
					add_single_channel_controller_item(
						ctl_items, ctl, string_compose(_("Controller %1"), ctl));
				}
			}

			/* Add submenu for this block of controllers to controller menu */
			items.push_back (
				MenuElem (string_compose (_("Controllers %1-%2"), i, i + 15),
				          *ctl_menu));
		}
	}
}

/** Set up the processor menu for the current set of processors, and
 *  display automation curves for any parameters which have data.
 */
void
MidiTrackToolbar::setup_processor_menu_and_curves ()
{
	_subplugin_menu_map.clear ();
	subplugin_menu.items().clear ();
	midi_tracker_editor.route->foreach_processor (sigc::mem_fun (*this, &MidiTrackToolbar::add_processor_to_subplugin_menu));
}

void
MidiTrackToolbar::add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor> p)
{
	boost::shared_ptr<ARDOUR::Processor> processor (p.lock ());

	if (!processor || !processor->display_to_user ()) {
		return;
	}

	// /* we use this override to veto the Amp processor from the plugin menu,
	//    as its automation lane can be accessed using the special "Fader" menu
	//    option
	// */

	if (boost::dynamic_pointer_cast<Amp> (processor) != 0) {
		return;
	}

	using namespace Menu_Helpers;
	ProcessorAutomationInfo *rai;
	std::list<ProcessorAutomationInfo*>::iterator x;

	const std::set<Evoral::Parameter>& automatable = processor->what_can_be_automated ();

	if (automatable.empty()) {
		return;
	}

	for (x = processor_automation.begin(); x != processor_automation.end(); ++x) {
		if ((*x)->processor == processor) {
			break;
		}
	}

	if (x == processor_automation.end()) {
		rai = new ProcessorAutomationInfo (processor);
		processor_automation.push_back (rai);
	} else {
		rai = *x;
	}

	/* any older menu was deleted at the top of processors_changed()
	   when we cleared the subplugin menu.
	*/

	rai->menu = manage (new Menu);
	MenuList& items = rai->menu->items();
	rai->menu->set_name ("ArdourContextMenu");

	items.clear ();

	std::set<Evoral::Parameter> has_visible_automation;
	// AutomationTimeAxisView::what_has_visible_automation (processor, has_visible_automation);

	for (std::set<Evoral::Parameter>::const_iterator i = automatable.begin(); i != automatable.end(); ++i) {

		ProcessorAutomationNode* pan = NULL;
		CheckMenuItem* mitem;

		std::string name = processor->describe_parameter (*i);

		if (name == X_("hidden")) {
			continue;
		}

		items.push_back (CheckMenuElem (name));
		mitem = dynamic_cast<CheckMenuItem*> (&items.back());

		_subplugin_menu_map[*i] = mitem;

		if (is_in(*i, has_visible_automation)) {
			mitem->set_active(true);
		}

		if ((pan = find_processor_automation_node (processor, *i)) == 0) {
			/* new item */
			pan = new ProcessorAutomationNode (*i, mitem);
			rai->columns.push_back (pan);
		} else {
			pan->menu_item = mitem;
		}

		mitem->signal_toggled().connect (sigc::bind (sigc::mem_fun(*this, &MidiTrackToolbar::processor_menu_item_toggled), rai, pan));
	}

	if (items.size() == 0) {
		return;
	}

	/* add the menu for this processor, because the subplugin
	   menu is always cleared at the top of processors_changed().
	   this is the result of some poor design in gtkmm and/or
	   GTK+.
	*/

	subplugin_menu.items().push_back (MenuElem (processor->name(), *rai->menu));
	rai->valid = true;
}

void
MidiTrackToolbar::processor_menu_item_toggled (ProcessorAutomationInfo* rai, ProcessorAutomationNode* pan)
{
	const bool showit = pan->menu_item->get_active();

	if (pan->column == 0)
		midi_tracker_editor.add_processor_automation_column (rai->processor, pan->what);

	if (showit)
		midi_tracker_editor.visible_automation_columns.insert (pan->column);
	else
		midi_tracker_editor.visible_automation_columns.erase (pan->column);

	/* now trigger a redisplay */
	midi_tracker_editor.redisplay_model ();
}

void
MidiTrackToolbar::add_channel_command_menu_item (Menu_Helpers::MenuList& items,
                                                 const std::string&      label,
                                                 AutomationType          auto_type,
                                                 uint8_t                 cmd)
{
	using namespace Menu_Helpers;

	/* count the number of selected channels because we will build a different menu
	   structure if there is more than 1 selected.
	 */

	const uint16_t selected_channels = midi_tracker_editor.midi_track()->get_playback_channel_mask();
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

		Menu* chn_menu = manage (new Menu);
		MenuList& chn_items (chn_menu->items());
		Evoral::Parameter param_without_channel (auto_type, 0, cmd);

		/* add a couple of items to hide/show all of them */

		chn_items.push_back (
			MenuElem (_("Hide all channels"),
			          sigc::bind (sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::change_all_channel_tracks_visibility),
			                      false, param_without_channel)));
		chn_items.push_back (
			MenuElem (_("Show all channels"),
			          sigc::bind (sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::change_all_channel_tracks_visibility),
			                      true, param_without_channel)));

		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {

				/* for each selected channel, add a menu item for this controller */

				Evoral::Parameter fully_qualified_param (auto_type, chn, cmd);
				chn_items.push_back (
					CheckMenuElem (string_compose (_("Channel %1"), chn+1),
					               sigc::bind (sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::update_automation_column_visibility),
					                           fully_qualified_param)));

				bool visible = midi_tracker_editor.is_automation_visible(fully_qualified_param);

				CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&chn_items.back());
				_channel_command_menu_map[fully_qualified_param] = cmi;
				cmi->set_active (visible);
			}
		}

		/* now create an item in the parent menu that has the per-channel list as a submenu */

		items.push_back (MenuElem (label, *chn_menu));

	} else {

		/* just one channel - create a single menu item for this command+channel combination*/

		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {

				Evoral::Parameter fully_qualified_param (auto_type, chn, cmd);
				items.push_back (
					CheckMenuElem (label,
					               sigc::bind (sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::update_automation_column_visibility),
					                           fully_qualified_param)));

				bool visible = midi_tracker_editor.is_automation_visible(fully_qualified_param);

				CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&items.back());
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
MidiTrackToolbar::add_single_channel_controller_item(Menu_Helpers::MenuList& ctl_items,
                                                     int                     ctl,
                                                     const std::string&      name)
{
	using namespace Menu_Helpers;

	const uint16_t selected_channels = midi_tracker_editor.midi_track()->get_playback_channel_mask();
	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			ctl_items.push_back (
				CheckMenuElem (
					string_compose ("<b>%1</b>: %2 [%3]", ctl, name, int (chn + 1)),
					sigc::bind (
						sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::update_automation_column_visibility),
						fully_qualified_param)));
			dynamic_cast<Label*> (ctl_items.back().get_child())->set_use_markup (true);

			bool visible = midi_tracker_editor.is_automation_visible(fully_qualified_param);

			CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&ctl_items.back());
			_controller_menu_map[fully_qualified_param] = cmi;
			cmi->set_active (visible);

			/* one channel only */
			break;
		}
	}
}

/** Add a submenu with 1 item per channel for a controller on many channels. */
void
MidiTrackToolbar::add_multi_channel_controller_item(Menu_Helpers::MenuList& ctl_items,
                                                    int                     ctl,
                                                    const std::string&      name)
{
	using namespace Menu_Helpers;

	const uint16_t selected_channels = midi_tracker_editor.midi_track()->get_playback_channel_mask();

	Menu* chn_menu = manage (new Menu);
	MenuList& chn_items (chn_menu->items());

	/* add a couple of items to hide/show this controller on all channels */

	Evoral::Parameter param_without_channel (MidiCCAutomation, 0, ctl);
	chn_items.push_back (
		MenuElem (_("Hide all channels"),
		          sigc::bind (sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::change_all_channel_tracks_visibility),
		                      false, param_without_channel)));
	chn_items.push_back (
		MenuElem (_("Show all channels"),
		          sigc::bind (sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::change_all_channel_tracks_visibility),
		                      true, param_without_channel)));

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			/* for each selected channel, add a menu item for this controller */

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			chn_items.push_back (
				CheckMenuElem (string_compose (_("Channel %1"), chn+1),
				               sigc::bind (sigc::mem_fun (midi_tracker_editor, &MidiTrackerEditor::update_automation_column_visibility),
				                           fully_qualified_param)));

			bool visible = midi_tracker_editor.is_automation_visible(fully_qualified_param);

			CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&chn_items.back());
			_controller_menu_map[fully_qualified_param] = cmi;
			cmi->set_active (visible);
		}
	}

	/* add the per-channel menu to the list of controllers, with the name of the controller */
	ctl_items.push_back (MenuElem (string_compose ("<b>%1</b>: %2", ctl, name),
	                               *chn_menu));
	dynamic_cast<Label*> (ctl_items.back().get_child())->set_use_markup (true);
}

ProcessorAutomationNode*
MidiTrackToolbar::find_processor_automation_node (boost::shared_ptr<Processor> processor, Evoral::Parameter what)
{
	for (std::list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {

		if ((*i)->processor == processor) {

			for (std::vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
				if ((*ii)->what == what) {
					return *ii;
				}
			}
		}
	}

	return 0;
}

CheckMenuItem* MidiTrackToolbar::automation_child_menu_item(const Evoral::Parameter& param)
{
	ParameterMenuMap::iterator cmm_it = _controller_menu_map.find(param);
	ParameterMenuMap::iterator ccmm_it = _channel_command_menu_map.find(param);
	CheckMenuItem* mitem = NULL;
	if (cmm_it != _controller_menu_map.end())
		mitem = cmm_it->second;
	else if (ccmm_it != _channel_command_menu_map.end())
		mitem = ccmm_it->second;
	return mitem;
}

void
MidiTrackToolbar::update_visible_note_button()
{
	visible_note_button.set_active_state (visible_note ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_visible_channel_button()
{
	visible_channel_button.set_active_state (visible_channel ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_visible_velocity_button()
{
	visible_velocity_button.set_active_state (visible_velocity ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_visible_delay_button()
{
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
MidiTrackToolbar::update_automation_button()
{
	automation_button.signal_clicked.connect (sigc::mem_fun(*this, &MidiTrackToolbar::automation_click));
}

void
MidiTrackToolbar::update_remove_note_column_button ()
{
	remove_note_column_button.set_sensitive (midi_track_pattern.np.nreqtracks < midi_track_pattern.np.ntracks);
}
