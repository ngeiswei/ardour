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
#include "ardour/track.h"

#include "audio_track_toolbar.h"
#include "grid.h"
#include "midi_track_toolbar.h"
#include "track_toolbar.h"
#include "tracker_editor.h"
#include "tracker_utils.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace Tracker;

TrackToolbar::TrackToolbar (TrackerEditor& te, TrackPattern* tp, int mti)
	: tracker_editor (te)
	, track (tp->track)
	, track_pattern (tp)
	, track_index (mti)
	, grid (te.grid)
	, spacing (2)
	, visible_delay (true)
	, automation_action_menu (0)
	, controller_menu (0)
	, _processor_automation_separator (0)
	, _processor_automation_item (0)
	, gain_automation_item (0)
	, trim_automation_item (0)
	, mute_automation_item (0)
	, pan_automation_item (0)
{
}

TrackToolbar::~TrackToolbar ()
{
	delete automation_action_menu;
	delete controller_menu;
}

void
TrackToolbar::setup ()
{
	set_spacing (2);

	setup_delay ();
	setup_automation ();

	show ();

	setup_tooltips ();
}

void
TrackToolbar::setup_delay ()
{
	// Add visible delay button (green outline when enabled)
	visible_delay_button.set_name ("page switch button");
	visible_delay_button.set_text (S_("Delay|D"));
	update_visible_delay_button ();
	visible_delay_button.show ();
	visible_delay_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &TrackToolbar::visible_delay_press), false);
	pack_start (visible_delay_button, false, false);
}

void
TrackToolbar::setup_automation ()
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
TrackToolbar::setup_tooltips ()
{
	set_tooltip (visible_delay_button, _("Toggle delay visibility"));
	set_tooltip (automation_button, _("Automation"));
}

bool
TrackToolbar::visible_delay_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_delay = !visible_delay;
	// grid.redisplay_visible_delay ();
	redisplay_grid ();
	return false;
}

void
TrackToolbar::automation_click ()
{
	if (!automation_action_menu) {
		build_automation_menu ();
	} else {
		refresh_automation_menu ();
	}

	automation_action_menu->popup (1, gtk_get_current_event_time ());
}

void
TrackToolbar::build_automation_menu ()
{
	using namespace Menu_Helpers;

	detach_menu (subplugin_menu);

	_processor_automation_separator = 0;
	_processor_automation_item = 0;

	delete automation_action_menu;
	automation_action_menu = new Menu;

	MenuList& items = automation_action_menu->items ();

	automation_action_menu->set_name ("ArdourContextMenu");

	build_show_hide_automations (items);

	setup_processor_menu_and_curves ();

	items.push_back (SeparatorElem ());
	_processor_automation_separator = &items.back ();

	items.push_back (MenuElem (_("Processor automation"), subplugin_menu));
	_processor_automation_item = &items.back ();
	_processor_automation_item->set_sensitive (true);

	update_processor_automation_item ();

	/* Add any main automation */
	build_main_automation_menu (items);
}

void
TrackToolbar::refresh_automation_menu ()
{
	/* The processor registry can change while the tracker is open (plugin
	 * insert/remove, parameter changes).  Rebuild only the processor submenu
	 * when its contents no longer match; otherwise just refresh the states
	 * of the existing check items. */
	if (processor_menu_needs_rebuild ()) {
		setup_processor_menu_and_curves ();
		update_processor_automation_item ();
	} else {
		for (const ProcessorMenuMap::value_type& item : _processor_menu_items) {
			item.second->set_active (grid.is_automation_visible (track_index, item.first));
		}
	}

	refresh_main_automation_items ();
}

void
TrackToolbar::refresh_main_automation_items ()
{
	if (gain_automation_item) {
		gain_automation_item->set_active (grid.is_gain_visible (track_index));
	}
	if (is_audio_track_toolbar () && trim_automation_item) {
		trim_automation_item->set_active (grid.is_trim_visible (track_index));
	}
	if (mute_automation_item) {
		mute_automation_item->set_active (grid.is_mute_visible (track_index));
	}
	if (pan_automation_item) {
		pan_automation_item->set_active (grid.is_pan_visible (track_index));
	}
}

void
TrackToolbar::update_processor_automation_item ()
{
	const bool has_processors = !subplugin_menu.items ().empty ();

	if (_processor_automation_separator) {
		_processor_automation_separator->set_visible (has_processors);
	}
	if (_processor_automation_item) {
		_processor_automation_item->set_visible (has_processors);
	}
}

void
TrackToolbar::build_main_automation_menu (Menu_Helpers::MenuList& items)
{
	build_main_automation_menu_item (items, _("Fader"), &Grid::update_gain_column_visibility, grid.is_gain_visible (track_index), gain_automation_item);

	if (is_audio_track_toolbar () /*trim_track*/) {
		build_main_automation_menu_item (items, _("Trim"), &Grid::update_trim_column_visibility, grid.is_trim_visible (track_index), trim_automation_item);
	}

	build_main_automation_menu_item (items, _("Mute"), &Grid::update_mute_column_visibility, grid.is_mute_visible (track_index), mute_automation_item);

	build_main_automation_menu_item (items, _("Pan"), &Grid::update_pan_columns_visibility, grid.is_pan_visible (track_index), pan_automation_item);
}

Gtk::CheckMenuItem*
TrackToolbar::build_main_automation_menu_item (Menu_Helpers::MenuList& items, const std::string& label, void (Grid::*update) (int), bool active, Gtk::CheckMenuItem*& slot)
{
	using namespace Menu_Helpers;

	items.push_back (CheckMenuElem (label, sigc::bind (sigc::mem_fun (grid, update), track_index)));
	Gtk::CheckMenuItem* item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());

	// Publish the new item before set_active (): set_active () emits toggled
	// when the state actually changes, and the handler reads this member.  If
	// it were still pointing at the item of the previous (already deleted)
	// menu, that would be a use-after-free.
	slot = item;

	item->set_active (active);
	return item;
}

void
TrackToolbar::build_show_hide_automations (Menu_Helpers::MenuList& items)
{
	using namespace Menu_Helpers;
	items.push_back (MenuElem (_("Show All Automation"),
	                           sigc::mem_fun (*this, &TrackToolbar::show_all_automation)));

	items.push_back (MenuElem (_("Show Existing Automation"),
	                           sigc::mem_fun (*this, &TrackToolbar::show_existing_automation)));

	items.push_back (MenuElem (_("Hide All Automation"),
	                           sigc::mem_fun (*this, &TrackToolbar::hide_all_automation)));
}

/** Set up the processor menu for the current set of processors, and
 *  display automation curves for any parameters which have data.
 */
void
TrackToolbar::setup_processor_menu_and_curves ()
{
	_processor_menu_items.clear ();
	subplugin_menu.items ().clear ();

	for (const auto& id_pattern : track_pattern->track_all_automations_pattern.id_to_processor_automation_pattern) {
		add_processor_to_subplugin_menu (id_pattern.second.get ());
	}
}

bool
TrackToolbar::processor_menu_needs_rebuild () const
{
	size_t count = 0;

	for (const auto& id_pattern : track_pattern->track_all_automations_pattern.id_to_processor_automation_pattern) {
		ProcessorPtr processor = track->processor_by_id (id_pattern.first);
		if (!processor || !processor->display_to_user () || std::dynamic_pointer_cast<Amp> (processor)) {
			continue;
		}

		const ProcessorAutomationPattern* pattern = id_pattern.second.get ();
		if (pattern->param_to_actl.empty ()) {
			continue;
		}

		for (const ParamAutomationControlMap::value_type& param_actl : pattern->param_to_actl) {
			const std::string& name = pattern->get_name (param_actl.first);
			if (name == X_("hidden")) {
				continue;
			}

			const IDParameter id_param (pattern->id (), param_actl.first);
			ProcessorMenuMap::const_iterator item = _processor_menu_items.find (id_param);
			if (item == _processor_menu_items.end () ||
			    item->second->get_label ().raw () != name) {
				return true;
			}
			++count;
		}
	}

	return count != _processor_menu_items.size ();
}

void
TrackToolbar::add_processor_to_subplugin_menu (ProcessorAutomationPattern* pattern)
{
	using namespace Menu_Helpers;

	ProcessorPtr processor = track->processor_by_id (pattern->id ());
	if (!processor || !processor->display_to_user () || std::dynamic_pointer_cast<Amp> (processor)) {
		return;
	}

	if (pattern->param_to_actl.empty ()) {
		return;
	}

	Menu* menu = Gtk::manage (new Menu);
	menu->set_name ("ArdourContextMenu");
	MenuList& items = menu->items ();

	for (const ParamAutomationControlMap::value_type& param_actl : pattern->param_to_actl) {
		const Evoral::Parameter& param = param_actl.first;
		const std::string& name = pattern->get_name (param);
		if (name == X_("hidden")) {
			continue;
		}

		items.push_back (CheckMenuElem (name));
		CheckMenuItem* mitem = dynamic_cast<CheckMenuItem*> (&items.back ());
		const IDParameter id_param (pattern->id (), param);
		_processor_menu_items[id_param] = mitem;

		mitem->signal_toggled ().connect (
			sigc::bind (sigc::mem_fun (*this, &TrackToolbar::processor_menu_item_toggled), id_param));
		mitem->set_active (grid.is_automation_visible (track_index, id_param));
	}

	if (!items.empty ()) {
		subplugin_menu.items ().push_back (MenuElem (processor->name (), *menu));
	}
}

void
TrackToolbar::processor_menu_item_toggled (IDParameter id_param)
{
	const bool showit = _processor_menu_items[id_param]->get_active ();
	ProcessorPtr processor = track->processor_by_id (id_param.first);
	if (!processor) {
		return;
	}

	const int column = grid.add_processor_automation_column (
		track_index, processor, id_param.second);
	if (column == 0) {
		return;
	}

	grid.set_automation_column_visible (track_index, id_param, column, showit);
	redisplay_grid ();
}

// Show all automation.
//
// TODO: this menu needs to be persistent between sessions. Should also be
// fixed for the track/piano roll view.
void
TrackToolbar::show_all_automation ()
{
	show_all_main_automations ();
	show_all_processor_automations ();

	redisplay_grid ();
}

void
TrackToolbar::show_existing_automation ()
{
	show_existing_main_automations ();
	show_existing_processor_automations ();

	redisplay_grid ();
}

void
TrackToolbar::hide_all_automation ()
{
	hide_main_automations ();
	hide_processor_automations ();

	redisplay_grid ();
}

void
TrackToolbar::show_all_main_automations ()
{
	apply_main_automation_visibility (true);
}

void
TrackToolbar::apply_main_automation_visibility (bool visible)
{
	gain_automation_item->set_active (visible);
	grid.update_gain_column_visibility (track_index);

	if (is_audio_track_toolbar ()) {
		trim_automation_item->set_active (visible);
		grid.update_trim_column_visibility (track_index);
	}

	mute_automation_item->set_active (visible);
	grid.update_mute_column_visibility (track_index);

	pan_automation_item->set_active (visible);
	grid.update_pan_columns_visibility (track_index);
}

void
TrackToolbar::show_existing_main_automations ()
{
	show_existing_main_automation (gain_automation_item, IDParameter (PBD::ID (0), Evoral::Parameter (GainAutomation)), &Grid::update_gain_column_visibility);

	if (is_audio_track_toolbar ()) {
		show_existing_main_automation (trim_automation_item, IDParameter (PBD::ID (0), Evoral::Parameter (TrimAutomation)), &Grid::update_trim_column_visibility);
	}

	show_existing_main_automation (mute_automation_item, IDParameter (PBD::ID (0), Evoral::Parameter (MuteAutomation)), &Grid::update_mute_column_visibility);

	// Pan: visible iff any pannable parameter is non-empty
	bool pan_visible = false;
	ParameterSet const & pan_params = track->pannable ()->what_can_be_automated ();
	for (ParameterSetConstIt p = pan_params.begin (); p != pan_params.end (); ++p) {
		IDParameter pan_id_param(PBD::ID (0), *p);
		if (!track_pattern->is_empty (pan_id_param)) {
			pan_visible = true;
			break;
		}
	}
	pan_automation_item->set_active (pan_visible);
	grid.update_pan_columns_visibility (track_index);
}

void
TrackToolbar::show_existing_main_automation (Gtk::CheckMenuItem* item, const IDParameter& id_param, void (Grid::*update) (int))
{
	item->set_active (!track_pattern->is_empty (id_param));
	(grid.*update) (track_index);
}

void
TrackToolbar::hide_main_automations ()
{
	apply_main_automation_visibility (false);
}

void
TrackToolbar::show_all_processor_automations ()
{
	set_all_processor_automation_visibility (true, false);
}

void
TrackToolbar::show_existing_processor_automations ()
{
	set_all_processor_automation_visibility (true, true);
}

void
TrackToolbar::hide_processor_automations ()
{
	set_all_processor_automation_visibility (false, false);
}

void
TrackToolbar::set_all_processor_automation_visibility (bool visible, bool only_existing)
{
	for (const ProcessorMenuMap::value_type& item : _processor_menu_items) {
		const IDParameter& id_param = item.first;
		const bool show = visible && (!only_existing || !track_pattern->is_empty (id_param));
		ProcessorPtr processor = track->processor_by_id (id_param.first);
		if (!processor) {
			continue;
		}

		int column = 0;
		Grid::IndexParamBimap::right_const_iterator it =
			grid.col2params[track_index].right.find (id_param);
		if (it != grid.col2params[track_index].right.end ()) {
			column = it->second;
		}

		if (show && column == 0) {
			column = grid.add_processor_automation_column (
				track_index, processor, id_param.second);
		}
		if (column == 0) {
			item.second->set_active (false);
			continue;
		}

		grid.set_automation_column_visible (track_index, id_param, column, show);
		item.second->set_active (show);
	}
}

void
TrackToolbar::update_visible_delay_button ()
{
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ImplicitActive : Gtkmm2ext::Off);
}

void
TrackToolbar::update_automation_button ()
{
	automation_button.signal_clicked.connect (sigc::mem_fun (*this, &TrackToolbar::automation_click));
}

bool
TrackToolbar::is_midi_track_toolbar () const
{
	return (bool)midi_track_toolbar ();
}

bool
TrackToolbar::is_audio_track_toolbar () const
{
	return (bool)audio_track_toolbar ();
}

const MidiTrackToolbar*
TrackToolbar::midi_track_toolbar () const
{
	return dynamic_cast<const MidiTrackToolbar*> (this);
}

MidiTrackToolbar*
TrackToolbar::midi_track_toolbar ()
{
	return dynamic_cast<MidiTrackToolbar*> (this);
}

const AudioTrackToolbar*
TrackToolbar::audio_track_toolbar () const
{
	return dynamic_cast<const AudioTrackToolbar*> (this);
}

AudioTrackToolbar*
TrackToolbar::audio_track_toolbar ()
{
	return dynamic_cast<AudioTrackToolbar*> (this);
}

int
TrackToolbar::get_min_width () const
{
	int width = visible_delay_button.get_width () + spacing +
		+ automation_separator.get_width () + spacing
		+ automation_button.get_width ();
	return width;
}

void
TrackToolbar::redisplay_grid ()
{
	tracker_editor.grid.redisplay_grid_direct_call ();
}
