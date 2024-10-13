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

#include "track_toolbar.h"
#include "midi_track_toolbar.h"
#include "audio_track_toolbar.h"
#include "tracker_editor.h"
#include "grid.h"
#include "tracker_utils.h"

#include "widgets/tooltips.h"
#include "ardour/track.h"
#include "ardour/amp.h"

#include "pbd/i18n.h"
#include "gtkmm2ext/utils.h"

#include <list>
#include <string>

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace Tracker;

TrackToolbar::TrackToolbar (TrackerEditor& te, TrackPattern* tp, size_t mti)
	: tracker_editor (te)
	, track (tp->track)
	, track_pattern (tp)
	, track_index (mti)
	, grid (te.grid)
	, spacing (2)
	, visible_delay (true)
	, automation_action_menu (0)
	, controller_menu (0)
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
TrackToolbar::setup_label ()
{
	// Add label
	std::string label = track->name();
	label += ":\t";
	Gtk::Label* mtt_label = new Gtk::Label (label.c_str());
	mtt_label->show ();
	pack_start (*mtt_label, false, false);
}

void
TrackToolbar::setup_delay ()
{
	// Add visible delay button
	visible_delay_button.set_name ("visible delay button");
	visible_delay_button.set_text (S_("Delay|D"));
	update_visible_delay_button ();
	visible_delay_button.show ();
	visible_delay_button.signal_button_press_event().connect (sigc::mem_fun(*this, &TrackToolbar::visible_delay_press), false);
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
TrackToolbar::visible_delay_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_delay = !visible_delay;
	// grid.redisplay_visible_delay();
	redisplay_grid();
	return false;
}

void
TrackToolbar::automation_click ()
{
	build_automation_menu ();
	automation_action_menu->popup (1, gtk_get_current_event_time());
}

void
TrackToolbar::build_automation_menu ()
{
	using namespace Menu_Helpers;

	detach_menu (subplugin_menu);

	delete automation_action_menu;
	automation_action_menu = new Menu;

	MenuList& items = automation_action_menu->items();

	automation_action_menu->set_name ("ArdourContextMenu");

	build_show_hide_automations (items);

	/* Attach the plugin submenu. It may have previously been used elsewhere,
	   so it was detached above
	*/

	// TODO could be optimized, no need to rebuild everything!
	setup_processor_menu_and_curves ();

	if (!subplugin_menu.items().empty()) {
		items.push_back (SeparatorElem ());
		items.push_back (MenuElem (_("Processor automation"), subplugin_menu));
		items.back().set_sensitive (true);
	}

	/* Add any route automation */

	if (true) {
		items.push_back (CheckMenuElem (_("Fader"), sigc::bind (sigc::mem_fun (grid, &Grid::update_gain_column_visibility), track_index)));
		gain_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		gain_automation_item->set_active (grid.is_gain_visible(track_index));
	}

	if (is_audio_track_toolbar() /*trim_track*/) {
		items.push_back (CheckMenuElem (_("Trim"), sigc::bind (sigc::mem_fun (grid, &Grid::update_trim_column_visibility), track_index)));
		trim_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		trim_automation_item->set_active (grid.is_trim_visible(track_index));
	}

	if (true /*mute_track*/) {
		items.push_back (CheckMenuElem (_("Mute"), sigc::bind (sigc::mem_fun (grid, &Grid::update_mute_column_visibility), track_index)));
		mute_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		mute_automation_item->set_active (grid.is_mute_visible(track_index));
	}

	if (true /*pan_tracks*/) {
		items.push_back (CheckMenuElem (_("Pan"), sigc::bind (sigc::mem_fun (grid, &Grid::update_pan_columns_visibility), track_index)));
		pan_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		pan_automation_item->set_active (grid.is_pan_visible(track_index));
	}
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
	_subplugin_menu_map.clear ();
	subplugin_menu.items().clear ();
	track->foreach_processor (sigc::mem_fun (*this, &TrackToolbar::add_processor_to_subplugin_menu));
}

void
TrackToolbar::add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor> p)
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

		ProcessorAutomationNode* pauno = 0;
		CheckMenuItem* mitem;

		std::string name = processor->describe_parameter (*i);

		if (name == X_("hidden")) {
			continue;
		}

		items.push_back (CheckMenuElem (name));
		mitem = dynamic_cast<CheckMenuItem*> (&items.back());

		_subplugin_menu_map[*i] = mitem;

		if (TrackerUtils::is_in(*i, has_visible_automation)) {
			mitem->set_active(true);
		}

		if ((pauno = find_processor_automation_node (processor, *i)) == 0) {
			/* new item */
			pauno = new ProcessorAutomationNode (*i, mitem);
			rai->columns.push_back (pauno);
		} else {
			pauno->menu_item = mitem;
		}

		mitem->signal_toggled().connect (sigc::bind (sigc::mem_fun(*this, &TrackToolbar::processor_menu_item_toggled), rai, pauno));
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
}

void
TrackToolbar::processor_menu_item_toggled (ProcessorAutomationInfo* rai, ProcessorAutomationNode* pauno)
{
	const bool showit = pauno->menu_item->get_active();

	if (pauno->column == 0)
		grid.add_processor_automation_column (track_index, rai->processor, pauno->param);

	grid.set_automation_column_visible (track_index, pauno->param, pauno->column, showit);

	/* now trigger a redisplay */
	redisplay_grid ();
}

ProcessorAutomationNode*
TrackToolbar::find_processor_automation_node (boost::shared_ptr<Processor> processor, Evoral::Parameter param)
{
	for (std::list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {

		if ((*i)->processor == processor) {

			for (std::vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
				if ((*ii)->param == param) {
					return *ii;
				}
			}
		}
	}

	return 0;
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
	// Gain
	gain_automation_item->set_active (true);
	grid.update_gain_column_visibility (track_index);

	// Trim
	if (is_audio_track_toolbar()) {
		trim_automation_item->set_active (true);
		grid.update_trim_column_visibility (track_index);
	}

	// Mute
	mute_automation_item->set_active (true);
	grid.update_mute_column_visibility (track_index);

	// Pan
	pan_automation_item->set_active (true);
	grid.update_pan_columns_visibility (track_index);
}

void
TrackToolbar::show_existing_main_automations ()
{
	// Gain
	bool gain_visible = !track_pattern->is_empty(Evoral::Parameter(GainAutomation));
	gain_automation_item->set_active (gain_visible);
	grid.update_gain_column_visibility (track_index);

	// Trim
	if (is_audio_track_toolbar()) {
		bool trim_visible = !track_pattern->is_empty(Evoral::Parameter(GainAutomation));
		trim_automation_item->set_active (trim_visible);
		grid.update_gain_column_visibility (track_index);
	}

	// Mute
	bool mute_visible = !track_pattern->is_empty(Evoral::Parameter(MuteAutomation));
	mute_automation_item->set_active (mute_visible);
	grid.update_mute_column_visibility (track_index);

	// Pan
	bool pan_visible = false;
	std::set<Evoral::Parameter> const & pan_params = track->pannable()->what_can_be_automated ();
	for (std::set<Evoral::Parameter>::const_iterator p = pan_params.begin(); p != pan_params.end(); ++p) {
		if (!track_pattern->is_empty(*p)) {
			pan_visible = true;
			break;
		}
	}
	pan_automation_item->set_active (pan_visible);
	grid.update_pan_columns_visibility (track_index);
}

void
TrackToolbar::hide_main_automations ()
{
	// Gain
	gain_automation_item->set_active (false);
	grid.update_gain_column_visibility (track_index);

	// Trim
	if (is_audio_track_toolbar()) {
		trim_automation_item->set_active (false);
		grid.update_trim_column_visibility (track_index);
	}

	// Mute
	mute_automation_item->set_active (false);
	grid.update_mute_column_visibility (track_index);

	// Pan
	pan_automation_item->set_active (false);
	grid.update_pan_columns_visibility (track_index);
}

void
TrackToolbar::show_all_processor_automations ()
{
	for (std::list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin();
	     i != processor_automation.end(); ++i) {
		for (std::vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
			size_t& column = (*ii)->column;
			if (column == 0)
				grid.add_processor_automation_column (track_index, (*i)->processor, (*ii)->param);

			// Still no column available, skip
			if (column == 0)
				continue;

			grid.set_automation_column_visible (track_index, (*ii)->param, column, true);

			(*ii)->menu_item->set_active (true);
		}
	}
}

void
TrackToolbar::show_existing_processor_automations ()
{
	for (std::list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin();
	     i != processor_automation.end(); ++i) {
		for (std::vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
			size_t& column = (*ii)->column;
			bool exist = !track_pattern->is_empty((*ii)->param);

			// Create automation column if necessary
			if (exist) {
				if (column == 0)
					grid.add_processor_automation_column (track_index, (*i)->processor, (*ii)->param);
			}

			// Still no column available, skip
			if (column == 0)
				continue;

			grid.set_automation_column_visible (track_index, (*ii)->param, column, exist);

			(*ii)->menu_item->set_active (exist);
		}
	}
}

void
TrackToolbar::hide_processor_automations ()
{
	for (std::list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin();
	     i != processor_automation.end(); ++i) {
		for (std::vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
			size_t column = (*ii)->column;
			if (column != 0) {
				grid.set_automation_column_visible (track_index, (*ii)->param, column, false);
				(*ii)->menu_item->set_active (false);
			}
		}
	}
}

void
TrackToolbar::update_visible_delay_button()
{
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
TrackToolbar::update_automation_button()
{
	automation_button.signal_clicked.connect (sigc::mem_fun(*this, &TrackToolbar::automation_click));
}

bool
TrackToolbar::is_midi_track_toolbar() const
{
	return (bool)midi_track_toolbar();
}

bool
TrackToolbar::is_audio_track_toolbar() const
{
	return (bool)audio_track_toolbar();
}

const MidiTrackToolbar*
TrackToolbar::midi_track_toolbar() const
{
	return dynamic_cast<const MidiTrackToolbar*>(this);
}

MidiTrackToolbar*
TrackToolbar::midi_track_toolbar()
{
	return dynamic_cast<MidiTrackToolbar*>(this);
}

const AudioTrackToolbar*
TrackToolbar::audio_track_toolbar() const
{
	return dynamic_cast<const AudioTrackToolbar*>(this);
}

AudioTrackToolbar*
TrackToolbar::audio_track_toolbar()
{
	return dynamic_cast<AudioTrackToolbar*>(this);
}

int
TrackToolbar::get_min_width() const
{
	int width = visible_delay_button.get_width() + spacing +
		+ automation_separator.get_width() + spacing
		+ automation_button.get_width();
	return width;
}

void
TrackToolbar::redisplay_grid()
{
	tracker_editor.grid.redisplay_grid_direct_call ();
}
