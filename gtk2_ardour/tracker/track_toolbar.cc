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

ProcessorAutomationNode::ProcessorAutomationNode (ProcessorPtr proc, Evoral::Parameter param, Gtk::CheckMenuItem* mitem)
	: processor(proc), parameter (param), menu_item (mitem), column (0)
{
}

std::string
ProcessorAutomationNode::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << indent << "parameter[" << parameter << "] name = " << processor->describe_parameter (parameter) << std::endl
	   << indent << "menu_item = " << menu_item << std::endl
	   << indent << "column = " << column;
	return ss.str ();
}

ProcessorAutomationInfo::ProcessorAutomationInfo (ProcessorPtr i)
	: processor (i), menu (0)
{
}

std::string
ProcessorAutomationInfo::to_string(const std::string& indent) const
{
	std::stringstream ss;
	std::string indent2 = indent + "  ";
	ss << indent << "processor = " << processor << std::endl
	   << indent << "menu = " << menu << std::endl
	   << indent << "columns:" << std::endl
	   << indent2 << "size = " << columns.size();
	std::string indent3 = indent2 + "  ";
	for (size_t i = 0; i < columns.size(); i++)
		ss << std::endl << indent2 << "processor automation node[" << i << "]:"
		   << std::endl << columns[i]->to_string(indent3);
	return ss.str ();
}

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
	std::string label = track->name ();
	label += ":\t";
	Gtk::Label* mtt_label = Gtk::manage (new Gtk::Label (label.c_str ()));
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
	build_automation_menu ();
	automation_action_menu->popup (1, gtk_get_current_event_time ());
}

void
TrackToolbar::build_automation_menu ()
{
	using namespace Menu_Helpers;

	detach_menu (subplugin_menu);

	delete automation_action_menu;
	automation_action_menu = new Menu;

	MenuList& items = automation_action_menu->items ();

	automation_action_menu->set_name ("ArdourContextMenu");

	build_show_hide_automations (items);

	/* Attach the plugin submenu. It may have previously been used elsewhere,
	   so it was detached above
	*/

	// TODO could be optimized, no need to rebuild everything!
	setup_processor_menu_and_curves ();

	if (!subplugin_menu.items ().empty ()) {
		items.push_back (SeparatorElem ());
		items.push_back (MenuElem (_("Processor automation"), subplugin_menu));
		items.back ().set_sensitive (true);
	}

	/* Add any main automation */
	build_main_automation_menu (items);
}

void
TrackToolbar::build_main_automation_menu (Menu_Helpers::MenuList& items)
{
	using namespace Menu_Helpers;

	if (true) {
		items.push_back (CheckMenuElem (_("Fader"), sigc::bind (sigc::mem_fun (grid, &Grid::update_gain_column_visibility), track_index)));
		gain_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		gain_automation_item->set_active (grid.is_gain_visible (track_index));
	}

	if (is_audio_track_toolbar () /*trim_track*/) {
		items.push_back (CheckMenuElem (_("Trim"), sigc::bind (sigc::mem_fun (grid, &Grid::update_trim_column_visibility), track_index)));
		trim_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		trim_automation_item->set_active (grid.is_trim_visible (track_index));
	}

	if (true /*mute_track*/) {
		items.push_back (CheckMenuElem (_("Mute"), sigc::bind (sigc::mem_fun (grid, &Grid::update_mute_column_visibility), track_index)));
		mute_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		mute_automation_item->set_active (grid.is_mute_visible (track_index));
	}

	if (true /*pan_tracks*/) {
		items.push_back (CheckMenuElem (_("Pan"), sigc::bind (sigc::mem_fun (grid, &Grid::update_pan_columns_visibility), track_index)));
		pan_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		pan_automation_item->set_active (grid.is_pan_visible (track_index));
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
	subplugin_menu.items ().clear ();
	track->foreach_processor (sigc::mem_fun (*this, &TrackToolbar::add_processor_to_subplugin_menu));
}

void
TrackToolbar::add_processor_to_subplugin_menu (std::weak_ptr<ARDOUR::Processor> p)
{
	using namespace Menu_Helpers;

	ProcessorPtr processor (p.lock ());

	if (!processor || !processor->display_to_user ()) {
		return;
	}

	// /* we use this override to veto the Amp processor from the plugin menu,
	//    as its automation lane can be accessed using the special "Fader" menu
	//    option
	// */

	if (std::dynamic_pointer_cast<Amp> (processor) != 0) {
		return;
	}

	// Abort if nothing is automatable
	const ParameterSet& automatable = processor->what_can_be_automated ();
	if (automatable.empty ()) {
		return;
	}

	// Find the processor automation info corresponding to the given
	// processor if it exists, otherwise construct it first.
	ProcessorAutomationInfo *pai;
	ProcessorAutomationInfoSeqIt it = find_processor_automation_info (processor);
	if (it == processor_automations.end ()) {
		pai = new ProcessorAutomationInfo (processor);
		processor_automations.push_back (pai);
	} else {
		pai = *it;
	}

	// Any older menu was deleted at the top of processors_changed ()
	// when we cleared the subplugin menu.
	pai->menu = Gtk::manage (new Menu);
	pai->menu->set_name ("ArdourContextMenu");
	MenuList& items = pai->menu->items ();
	items.clear ();

	for (ParameterSetConstIt param_it = automatable.begin (); param_it != automatable.end (); ++param_it) {

		std::string name = processor->describe_parameter (*param_it);

		if (name == X_("hidden")) { // TODO: What does that mean?
			continue;
		}

		items.push_back (CheckMenuElem (name));
		CheckMenuItem* mitem = dynamic_cast<CheckMenuItem*> (&items.back ());

		_subplugin_menu_map[*param_it] = mitem;

		ProcessorAutomationNode* pauno = nullptr;
		if ((pauno = find_processor_automation_node (processor, *param_it)) == 0) {
			/* new item */
			pauno = new ProcessorAutomationNode (processor, *param_it, mitem);
			pai->columns.push_back (pauno);
		} else {
			pauno->menu_item = mitem;
		}

		mitem->signal_toggled ().connect (sigc::bind (sigc::mem_fun (*this, &TrackToolbar::processor_menu_item_toggled), pai, pauno));
		Evoral::Parameter param(*param_it);
		IDParameter id_param(processor->id(), param);
		bool visible = grid.is_automation_visible (track_index, id_param);
		mitem->set_active (visible);
	}

	if (items.size () == 0) {
		return;
	}

	/* add the menu for this processor, because the subplugin
	   menu is always cleared at the top of processors_changed ().
	   this is the result of some poor design in gtkmm and/or
	   GTK+.
	*/

	subplugin_menu.items ().push_back (MenuElem (processor->name (), *pai->menu));
}

void
TrackToolbar::processor_menu_item_toggled (ProcessorAutomationInfo* pai, ProcessorAutomationNode* pauno)
{
	const bool showit = pauno->menu_item->get_active ();

	if (pauno->column == 0) {
		grid.add_processor_automation_column (track_index, pai->processor, pauno->parameter);
	}

	IDParameter id_param (pauno->processor->id(), pauno->parameter);
	grid.set_automation_column_visible (track_index, id_param, pauno->column, showit);

	/* now trigger a redisplay */
	redisplay_grid ();
}

ProcessorAutomationInfoSeqIt
TrackToolbar::find_processor_automation_info (ProcessorPtr processor)
{
	ProcessorAutomationInfoSeqIt it;
	for (it = processor_automations.begin (); it != processor_automations.end (); ++it) {
		if ((*it)->processor == processor) {
			break;
		}
	}
	return it;
}

ProcessorAutomationInfoSeqConstIt
TrackToolbar::find_processor_automation_info (ProcessorPtr processor) const
{
	ProcessorAutomationInfoSeqConstIt it;
	for (it = processor_automations.begin (); it != processor_automations.end (); ++it) {
		if ((*it)->processor == processor) {
			break;
		}
	}
	return it;
}

ProcessorAutomationNode*
TrackToolbar::find_processor_automation_node (ProcessorPtr processor, Evoral::Parameter param)
{
	ProcessorAutomationInfoSeqIt i = find_processor_automation_info (processor);
	for (ProcessorAutomationNodeSeqIt ii = (*i)->columns.begin (); ii != (*i)->columns.end (); ++ii) {
		if ((*ii)->parameter == param) {
			return *ii;
		}
	}
	return nullptr;
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
	if (is_audio_track_toolbar ()) {
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
	IDParameter gain_id_param(PBD::ID (0), Evoral::Parameter (GainAutomation));
	bool gain_visible = !track_pattern->is_empty (gain_id_param);
	gain_automation_item->set_active (gain_visible);
	grid.update_gain_column_visibility (track_index);

	// Trim
	if (is_audio_track_toolbar ()) {
		IDParameter trim_id_param(PBD::ID (0), Evoral::Parameter (TrimAutomation));
		bool trim_visible = !track_pattern->is_empty (trim_id_param);
		trim_automation_item->set_active (trim_visible);
		grid.update_trim_column_visibility (track_index);
	}

	// Mute
	IDParameter mute_id_param(PBD::ID (0), Evoral::Parameter (MuteAutomation));
	bool mute_visible = !track_pattern->is_empty (mute_id_param);
	mute_automation_item->set_active (mute_visible);
	grid.update_mute_column_visibility (track_index);

	// Pan
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
TrackToolbar::hide_main_automations ()
{
	// Gain
	gain_automation_item->set_active (false);
	grid.update_gain_column_visibility (track_index);

	// Trim
	if (is_audio_track_toolbar ()) {
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
	for (ProcessorAutomationInfoSeqIt i = processor_automations.begin (); i != processor_automations.end (); ++i) {
		for (ProcessorAutomationNodeSeqIt ii = (*i)->columns.begin (); ii != (*i)->columns.end (); ++ii) {
			int& column = (*ii)->column;
			if (column == 0) {
				grid.add_processor_automation_column (track_index, (*i)->processor, (*ii)->parameter);
			}

			// Still no column available, skip
			if (column == 0) {
				continue;
			}

			IDParameter id_param((*ii)->processor->id(), (*ii)->parameter);
			grid.set_automation_column_visible (track_index, id_param, column, true);

			(*ii)->menu_item->set_active (true);
		}
	}
}

void
TrackToolbar::show_existing_processor_automations ()
{
	for (ProcessorAutomationInfoSeqIt i = processor_automations.begin (); i != processor_automations.end (); ++i) {
		for (ProcessorAutomationNodeSeqIt ii = (*i)->columns.begin (); ii != (*i)->columns.end (); ++ii) {
			int& column = (*ii)->column;
			IDParameter id_param((*i)->processor->id (), (*ii)->parameter);
			bool exist = !track_pattern->is_empty (id_param);

			// Create automation column if necessary
			if (exist) {
				if (column == 0) {
					grid.add_processor_automation_column (track_index, (*i)->processor, (*ii)->parameter);
				}
			}

			// Still no column available, skip
			if (column == 0) {
				continue;
			}

			grid.set_automation_column_visible (track_index, id_param, column, exist);

			(*ii)->menu_item->set_active (exist);
		}
	}
}

void
TrackToolbar::hide_processor_automations ()
{
	for (ProcessorAutomationInfoSeqIt i = processor_automations.begin (); i != processor_automations.end (); ++i) {
		for (ProcessorAutomationNodeSeqIt ii = (*i)->columns.begin (); ii != (*i)->columns.end (); ++ii) {
			int column = (*ii)->column;
			if (column != 0) {
				IDParameter id_param((*ii)->processor->id(), (*ii)->parameter);
				grid.set_automation_column_visible (track_index, id_param, column, false);
				(*ii)->menu_item->set_active (false);
			}
		}
	}
}

void
TrackToolbar::update_visible_delay_button ()
{
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
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
