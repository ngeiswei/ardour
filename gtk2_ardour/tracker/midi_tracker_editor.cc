/*
    Copyright (C) 2015-2017 Nil Geisweiller

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
#include "midi_tracker_editor.h"
#include "note_player.h"
#include "widgets/tooltips.h"
#include "axis_view.h"
#include "editor.h"

#include "pbd/i18n.h"

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

///////////////////////
// MidiTrackerEditor //
///////////////////////

static const gchar *_beats_per_row_strings[] = {
	N_("Beats/128"),
	N_("Beats/64"),
	N_("Beats/32"),
	N_("Beats/28"),
	N_("Beats/24"),
	N_("Beats/20"),
	N_("Beats/16"),
	N_("Beats/14"),
	N_("Beats/12"),
	N_("Beats/10"),
	N_("Beats/8"),
	N_("Beats/7"),
	N_("Beats/6"),
	N_("Beats/5"),
	N_("Beats/4"),
	N_("Beats/3"),
	N_("Beats/2"),
	N_("Beats"),
	N_("Bars"),
	0
};

#define COMBO_TRIANGLE_WIDTH 25

const std::string MidiTrackerEditor::note_off_str = "===";
const std::string MidiTrackerEditor::undefined_str = "***";

MidiTrackerEditor::MidiTrackerEditor (ARDOUR::Session* s, MidiTimeAxisView* mtv, boost::shared_ptr<ARDOUR::Route> rou, boost::shared_ptr<MidiRegion> reg, boost::shared_ptr<MidiTrack> tr)
	: ArdourWindow (reg->name())
	, automation_action_menu(0)
	, controller_menu (0)
	, gain_column (0)
	, mute_column (0)
	, midi_time_axis_view(mtv)
	, route (rou)
	, nrows (0)
	, edit_rowidx (-1)
	, edit_tracknum (-1)
	, edit_colnum (-1)
	, editing_editable (NULL)
	, myactions (X_("Tracking"))
	, visible_note (true)
	, visible_channel (false)
	, visible_velocity (false)
	, visible_delay (false)
	, step_edit (false)
	, octave_label (_("Octave"))
	, octave_adjustment (4, -1, 9, 1, 2)
	, octave_spinner (octave_adjustment)
	, channel_label (S_("Channel|Ch"))
	, channel_adjustment (1, 1, 16, 1, 4)
	, channel_spinner (channel_adjustment)
	, velocity_label (S_("Velocity|Vel"))
	, velocity_adjustment (64, 0, 127, 1, 4)
	, velocity_spinner (velocity_adjustment)
	, delay_label (_("Delay"))
	, delay_adjustment (0, 0, 0, 1, 4)
	, delay_spinner (delay_adjustment)
	, position_label (_("Position"))
	, position_adjustment (0, -5, 5, 1, 2)
	, position_spinner (position_adjustment)
	, steps_label (_("Steps"))
	  // TODO set the boundaries to not be above the number of rows
	, steps_adjustment (4, -255, 255, 1, 4)
	, steps_spinner (steps_adjustment)
	, region (reg)
	, track (tr)
	, midi_model (region->midi_source(0)->model())

{
	/* We do not handle nested sources/regions. Caller should have tackled this */

	if (reg->max_source_level() > 0) {
		throw failed_constructor();
	}

	set_session (s);

	// Fill _pan_param_types
	_pan_param_types.insert(PanAzimuthAutomation);
	_pan_param_types.insert(PanElevationAutomation);
	_pan_param_types.insert(PanWidthAutomation);
	_pan_param_types.insert(PanFrontBackAutomation);
	_pan_param_types.insert(PanLFEAutomation);

	// Beats per row combo
	beats_per_row_strings =  I18N (_beats_per_row_strings);
	build_beats_per_row_menu ();

	register_actions ();

	setup_processor_menu_and_curves ();
	build_param2actrl ();
	build_pattern ();

	setup_tooltips ();
	setup_toolbar ();
	setup_pattern ();
	setup_scroller ();

	set_beats_per_row_to (SnapToBeatDiv4);

	redisplay_model ();

	midi_model->ContentsChanged.connect (content_connections, invalidator (*this),
	                                     boost::bind (&MidiTrackerEditor::redisplay_model, this), gui_context());
	region->RegionPropertyChanged.connect (content_connections, invalidator (*this),
	                                       boost::bind (&MidiTrackerEditor::redisplay_model, this), gui_context());
	
	vbox.show ();

	vbox.set_spacing (6);
	vbox.set_border_width (6);
	vbox.pack_start (toolbar, false, false);
	vbox.pack_start (scroller, true, true);

	add (vbox);
	set_size_request (-1, 400);
}

MidiTrackerEditor::~MidiTrackerEditor ()
{
	delete np;
	delete tap;
	delete rap;
	delete automation_action_menu;
	delete controller_menu;
}

////////////////
// Automation //
////////////////

MidiTrackerEditor::ProcessorAutomationNode*
MidiTrackerEditor::find_processor_automation_node (boost::shared_ptr<Processor> processor, Evoral::Parameter what)
{
	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {

		if ((*i)->processor == processor) {

			for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
				if ((*ii)->what == what) {
					return *ii;
				}
			}
		}
	}

	return 0;
}

CheckMenuItem* MidiTrackerEditor::automation_child_menu_item(const Evoral::Parameter& param)
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

bool
MidiTrackerEditor::is_pan_type (const Evoral::Parameter& param) const {
	return _pan_param_types.find((ARDOUR::AutomationType)param.type()) != _pan_param_types.end();
}

size_t
MidiTrackerEditor::select_available_automation_column ()
{
	// Find the next available column
	if (available_automation_columns.empty()) {
		std::cout << "Warning: no more available automation column" << std::endl;
		return 0;
	}
	std::set<size_t>::iterator it = available_automation_columns.begin();
	size_t column = *it;
	available_automation_columns.erase(it);

	return column;
}

size_t
MidiTrackerEditor::add_main_automation_column (const Evoral::Parameter& param)
{
	// Select the next available column
	size_t column = select_available_automation_column ();
	if (column == 0)
		return column;

	// Associate that column to the parameter
	col2param.insert(ColParamBimap::value_type(column, param));

	// Set the column title
	string name = is_pan_type(param) ?
		route->panner()->describe_parameter (param)
		: route->describe_parameter (param);
	view.get_column(column)->set_title (name);

	return column;
}

size_t
MidiTrackerEditor::add_midi_automation_column (const Evoral::Parameter& param)
{
	// If not in param2actrl, add it.
	if (!param2actrl[param]) {
		param2actrl[param] = is_region_automation (param) ? midi_model->automation_control(param, true) : route->automation_control(param, true); 
		AutomationPattern* ap = get_automation_pattern (param);
		ap->insert(param2actrl[param]);
	}

	// Select the next available column
	size_t column = select_available_automation_column ();
	if (column == 0)
		return column;

	// Associate that column to the parameter
	col2param.insert(ColParamBimap::value_type(column, param));

	// Set the column title
	view.get_column(column)->set_title (route->describe_parameter (param));

	return column;
}

/**
 * Add an AutomationTimeAxisView to display automation for a processor's
 * parameter.
 */
void
MidiTrackerEditor::add_processor_automation_column (boost::shared_ptr<Processor> processor, const Evoral::Parameter& what)
{
	ProcessorAutomationNode* pan;

	if ((pan = find_processor_automation_node (processor, what)) == 0) {
		/* session state may never have been saved with new plugin */
		error << _("programming error: ")
		      << string_compose (X_("processor automation column for %1:%2/%3/%4 not registered with track!"),
		                         processor->name(), what.type(), (int) what.channel(), what.id() )
		      << endmsg;
		abort(); /*NOTREACHED*/
		return;
	}

	if (pan->column) {
		return;
	}

	// Find the next available column
	pan->column = select_available_automation_column ();
	if (!pan->column) {
		return;
	}

	// Associate that column to the parameter
	col2param.insert(ColParamBimap::value_type(pan->column, what));

	// Set the column title
	string name = processor->describe_parameter (what);
	view.get_column(pan->column)->set_title (name);
}

void
MidiTrackerEditor::show_all_processor_automations ()
{
	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
			size_t& column = (*ii)->column;
			if (column == 0)
				add_processor_automation_column ((*i)->processor, (*ii)->what);

			// Still no column available, skip
			if (column == 0)
				continue;

			visible_automation_columns.insert (column);

			(*ii)->menu_item->set_active (true);
		}
	}
}

// Show all automation, with the exception of midi automations, only show the
// existing one, because there are too many.
//
// TODO: this menu needs to be persistent between sessions. Should also be
// fixed for the track/piano roll view.
void
MidiTrackerEditor::show_all_automation ()
{
	show_all_main_automations ();
	show_existing_midi_automations ();
	show_all_processor_automations ();

	redisplay_model ();
}

void
MidiTrackerEditor::show_existing_midi_automations ()
{
	const set<Evoral::Parameter> params = midi_track()->midi_playlist()->contained_automation();
	for (set<Evoral::Parameter>::const_iterator p = params.begin(); p != params.end(); ++p) {
		ColParamBimap::right_const_iterator it = col2param.right.find(*p);
		size_t column = (it == col2param.right.end()) || (it->second == 0) ?
			add_midi_automation_column (*p) : it->second;

		// Still no column available, skip
		if (column == 0)
			continue;

		visible_automation_columns.insert (column);
	}
}

void
MidiTrackerEditor::show_existing_processor_automations ()
{
	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
			size_t& column = (*ii)->column;
			bool exist = param2actrl[(*ii)->what]->list()->size() > 0;

			// Create automation column if necessary
			if (exist) {
				if (column == 0)
					add_processor_automation_column ((*i)->processor, (*ii)->what);
			}

			// Still no column available, skip
			if (column == 0)
				continue;

			if (exist)
				visible_automation_columns.insert (column);
			else
				visible_automation_columns.erase (column);

			(*ii)->menu_item->set_active (exist);
		}
	}
}

void
MidiTrackerEditor::show_existing_automation ()
{
	show_existing_main_automations ();
	show_existing_midi_automations ();
	show_existing_processor_automations ();

	redisplay_model ();
}

void
MidiTrackerEditor::hide_midi_automations ()
{
	std::set<size_t> to_remove;
	for (std::set<size_t>::iterator it = visible_automation_columns.begin();
	     it != visible_automation_columns.end(); it++) {
		size_t column = *it;
		ColParamBimap::left_const_iterator c2p_it = col2param.left.find(column);
		if (c2p_it == col2param.left.end())
			continue;

		Evoral::Parameter param = c2p_it->second;
		CheckMenuItem* mitem = automation_child_menu_item(param);

		if (mitem)
			to_remove.insert(column);
	}
	for (std::set<size_t>::iterator it = to_remove.begin();
	     it != to_remove.end(); it++)
		visible_automation_columns.erase (*it);
}

void
MidiTrackerEditor::hide_processor_automations ()
{
	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
			size_t column = (*ii)->column;
			if (column != 0) {
				visible_automation_columns.erase (column);
				(*ii)->menu_item->set_active (false);
			}
		}
	}
}

void
MidiTrackerEditor::hide_all_automation ()
{
	hide_main_automations ();
	hide_midi_automations ();
	hide_processor_automations ();

	redisplay_model ();
}

/** Set up the processor menu for the current set of processors, and
 *  display automation curves for any parameters which have data.
 */
void
MidiTrackerEditor::setup_processor_menu_and_curves ()
{
	_subplugin_menu_map.clear ();
	subplugin_menu.items().clear ();
	route->foreach_processor (sigc::mem_fun (*this, &MidiTrackerEditor::add_processor_to_subplugin_menu));
}

void
MidiTrackerEditor::add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor> p)
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
	list<ProcessorAutomationInfo*>::iterator x;

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

		string name = processor->describe_parameter (*i);

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
			pan = new ProcessorAutomationNode (*i, mitem, *this);
			rai->columns.push_back (pan);
		} else {
			pan->menu_item = mitem;
		}

		mitem->signal_toggled().connect (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::processor_menu_item_toggled), rai, pan));
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
MidiTrackerEditor::processor_menu_item_toggled (MidiTrackerEditor::ProcessorAutomationInfo* rai, MidiTrackerEditor::ProcessorAutomationNode* pan)
{
	const bool showit = pan->menu_item->get_active();

	if (pan->column == 0)
		add_processor_automation_column (rai->processor, pan->what);

	if (showit)
		visible_automation_columns.insert (pan->column);
	else
		visible_automation_columns.erase (pan->column);

	/* now trigger a redisplay */
	redisplay_model ();
}

void
MidiTrackerEditor::build_automation_action_menu ()
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
	                           sigc::mem_fun (*this, &MidiTrackerEditor::show_all_automation)));

	items.push_back (MenuElem (_("Show Existing Automation"),
	                           sigc::mem_fun (*this, &MidiTrackerEditor::show_existing_automation)));

	items.push_back (MenuElem (_("Hide All Automation"),
	                           sigc::mem_fun (*this, &MidiTrackerEditor::hide_all_automation)));

	/* Attach the plugin submenu. It may have previously been used elsewhere,
	   so it was detached above
	*/

	// TODO could be optimized, no need to rebuild everything
	setup_processor_menu_and_curves ();
	build_param2actrl ();
	update_automation_patterns ();

	if (!subplugin_menu.items().empty()) {
		items.push_back (SeparatorElem ());
		items.push_back (MenuElem (_("Processor automation"), subplugin_menu));
		items.back().set_sensitive (true);
	}

	/* Add any route automation */

	if (true) {
		items.push_back (CheckMenuElem (_("Fader"), sigc::mem_fun (*this, &MidiTrackerEditor::update_gain_column_visibility)));
		gain_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		gain_automation_item->set_active (is_gain_visible());
	}

	if (false /*trim_track*/ /* TODO: support audio track */) {
		items.push_back (CheckMenuElem (_("Trim"), sigc::mem_fun (*this, &MidiTrackerEditor::update_trim_column_visibility)));
		trim_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		trim_automation_item->set_active (false);
	}

	if (true /*mute_track*/) {
		items.push_back (CheckMenuElem (_("Mute"), sigc::mem_fun (*this, &MidiTrackerEditor::update_mute_column_visibility)));
		mute_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		mute_automation_item->set_active (is_mute_visible());
	}

	if (true /*pan_tracks*/) {
		items.push_back (CheckMenuElem (_("Pan"), sigc::mem_fun (*this, &MidiTrackerEditor::update_pan_columns_visibility)));
		pan_automation_item = dynamic_cast<CheckMenuItem*> (&items.back ());
		pan_automation_item->set_active (is_pan_visible());
	}

	/* Add any midi automation */

	_channel_command_menu_map.clear ();

	MenuList& automation_items = automation_action_menu->items();

	uint16_t selected_channels = midi_track()->get_playback_channel_mask();

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
MidiTrackerEditor::add_channel_command_menu_item (Menu_Helpers::MenuList& items,
                                                  const string&           label,
                                                  AutomationType          auto_type,
                                                  uint8_t                 cmd)
{
	using namespace Menu_Helpers;

	/* count the number of selected channels because we will build a different menu
	   structure if there is more than 1 selected.
	 */

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();
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
			          sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::change_all_channel_tracks_visibility),
			                      false, param_without_channel)));
		chn_items.push_back (
			MenuElem (_("Show all channels"),
			          sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::change_all_channel_tracks_visibility),
			                      true, param_without_channel)));

		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {

				/* for each selected channel, add a menu item for this controller */

				Evoral::Parameter fully_qualified_param (auto_type, chn, cmd);
				chn_items.push_back (
					CheckMenuElem (string_compose (_("Channel %1"), chn+1),
					               sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::update_automation_column_visibility),
					                           fully_qualified_param)));

				bool visible = is_automation_visible(fully_qualified_param);

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
					               sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::update_automation_column_visibility),
					                           fully_qualified_param)));

				bool visible = is_automation_visible(fully_qualified_param);

				CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&items.back());
				_channel_command_menu_map[fully_qualified_param] = cmi;
				cmi->set_active (visible);

				/* one channel only */
				break;
			}
		}
	}
}

void
MidiTrackerEditor::change_all_channel_tracks_visibility (bool yn, Evoral::Parameter param)
{
	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (param.type(), chn, param.id());
			CheckMenuItem* menu = automation_child_menu_item (fully_qualified_param);

			if (menu) {
				menu->set_active (yn);
			}
		}
	}
}

/** Toggle an automation column for a fully-specified Parameter (type,channel,id)
 *  Will add column if necessary.
 */
void
MidiTrackerEditor::update_automation_column_visibility (const Evoral::Parameter& param)
{
	// Find menu item associated to this parameter
	CheckMenuItem* mitem = automation_child_menu_item(param);
	assert (mitem);
	const bool showit = mitem->get_active();

	// Find the column associated to this parameter, assign one if necessary
	ColParamBimap::right_const_iterator it = col2param.right.find(param);
	size_t column = (it == col2param.right.end()) || (it->second == 0) ?
		add_midi_automation_column (param) : it->second;

	// Still no column available, skip
	if (column == 0)
		return;

	if (showit)
		visible_automation_columns.insert (column);
	else
		visible_automation_columns.erase (column);

	/* now trigger a redisplay */
	redisplay_model ();
}

void
MidiTrackerEditor::build_controller_menu ()
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

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();

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
	boost::shared_ptr<MasterDeviceNames> device_names = get_device_names();

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

boost::shared_ptr<MIDI::Name::MasterDeviceNames>
MidiTrackerEditor::get_device_names ()
{
	return midi_time_axis_view->get_device_names ();
}


/** Add a single menu item for a controller on one channel. */
void
MidiTrackerEditor::add_single_channel_controller_item(Menu_Helpers::MenuList& ctl_items,
                                                      int                     ctl,
                                                      const std::string&      name)
{
	using namespace Menu_Helpers;

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();
	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			ctl_items.push_back (
				CheckMenuElem (
					string_compose ("<b>%1</b>: %2 [%3]", ctl, name, int (chn + 1)),
					sigc::bind (
						sigc::mem_fun (*this, &MidiTrackerEditor::update_automation_column_visibility),
						fully_qualified_param)));
			dynamic_cast<Label*> (ctl_items.back().get_child())->set_use_markup (true);

			bool visible = is_automation_visible(fully_qualified_param);

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
MidiTrackerEditor::add_multi_channel_controller_item(Menu_Helpers::MenuList& ctl_items,
                                                     int                     ctl,
                                                     const std::string&      name)
{
	using namespace Menu_Helpers;

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();

	Menu* chn_menu = manage (new Menu);
	MenuList& chn_items (chn_menu->items());

	/* add a couple of items to hide/show this controller on all channels */

	Evoral::Parameter param_without_channel (MidiCCAutomation, 0, ctl);
	chn_items.push_back (
		MenuElem (_("Hide all channels"),
		          sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::change_all_channel_tracks_visibility),
		                      false, param_without_channel)));
	chn_items.push_back (
		MenuElem (_("Show all channels"),
		          sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::change_all_channel_tracks_visibility),
		                      true, param_without_channel)));

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			/* for each selected channel, add a menu item for this controller */

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			chn_items.push_back (
				CheckMenuElem (string_compose (_("Channel %1"), chn+1),
				               sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::update_automation_column_visibility),
				                           fully_qualified_param)));

			bool visible = is_automation_visible(fully_qualified_param);

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

bool
MidiTrackerEditor::is_automation_visible(const Evoral::Parameter& param) const
{
	ColParamBimap::right_const_iterator it = col2param.right.find(param);
	return it != col2param.right.end() &&
		visible_automation_columns.find(it->second) != visible_automation_columns.end();
}

bool
MidiTrackerEditor::is_gain_visible() const
{
	return visible_automation_columns.find(gain_column)
		!= visible_automation_columns.end();
};

bool
MidiTrackerEditor::is_mute_visible() const
{
	return visible_automation_columns.find(mute_column)
		!= visible_automation_columns.end();
};

bool
MidiTrackerEditor::is_pan_visible() const
{
	bool visible = not pan_columns.empty();
	for (std::vector<size_t>::const_iterator it = pan_columns.begin(); it != pan_columns.end(); ++it) {
		visible = visible_automation_columns.find(*it) != visible_automation_columns.end();
		if (not visible)
			break;
	}
	return visible;
};

void
MidiTrackerEditor::update_gain_column_visibility ()
{
	const bool showit = gain_automation_item->get_active();

	if (gain_column == 0)
		gain_column = add_main_automation_column(Evoral::Parameter(GainAutomation));

	// Still no column available, abort
	if (gain_column == 0)
		return;
	
	if (showit)
		visible_automation_columns.insert (gain_column);
	else
		visible_automation_columns.erase (gain_column);

	/* now trigger a redisplay */
	redisplay_model ();
}

void
MidiTrackerEditor::update_trim_column_visibility ()
{
	// bool const showit = trim_automation_item->get_active();

	// if (showit != string_is_affirmative (trim_track->gui_property ("visible"))) {
	// 	trim_track->set_marked_for_display (showit);

	// 	/* now trigger a redisplay */

	// 	if (!no_redraw) {
	// 		 _route->gui_changed (X_("visible_tracks"), (void *) 0); /* EMIT_SIGNAL */
	// 	}
	// }
}

void
MidiTrackerEditor::update_mute_column_visibility ()
{
	const bool showit = mute_automation_item->get_active();

	if (mute_column == 0)
		mute_column = add_main_automation_column(Evoral::Parameter(MuteAutomation));

	// Still no column available, abort
	if (mute_column == 0)
		return;

	if (showit)
		visible_automation_columns.insert (mute_column);
	else
		visible_automation_columns.erase (mute_column);

	/* now trigger a redisplay */
	redisplay_model ();
}

void
MidiTrackerEditor::update_pan_columns_visibility ()
{
	const bool showit = pan_automation_item->get_active();

	if (pan_columns.empty()) {
		set<Evoral::Parameter> const & params = route->panner()->what_can_be_automated ();
		for (set<Evoral::Parameter>::const_iterator p = params.begin(); p != params.end(); ++p) {
			pan_columns.push_back(add_main_automation_column(*p));
		}
	}

	// Still no column available, abort
	if (pan_columns.empty())
		return;

	for (std::vector<size_t>::const_iterator it = pan_columns.begin(); it != pan_columns.end(); ++it) {
		if (showit)
			visible_automation_columns.insert (*it);
		else
			visible_automation_columns.erase (*it);
	}

	/* now trigger a redisplay */
	redisplay_model ();
}

void
MidiTrackerEditor::show_all_main_automations ()
{
	// Gain
	gain_automation_item->set_active (true);
	update_gain_column_visibility ();

	// Mute
	mute_automation_item->set_active (true);
	update_mute_column_visibility ();

	// Pan
	pan_automation_item->set_active (true);
	update_pan_columns_visibility ();
}

bool
MidiTrackerEditor::has_pan_automation() const
{
	for (std::set<AutomationType>::const_iterator it = _pan_param_types.begin();
	     it != _pan_param_types.end(); ++it) {
		Parameter2AutomationControl::const_iterator pac_it = param2actrl.find(Evoral::Parameter(*it));
		if (pac_it != param2actrl.end() && pac_it->second->list()->size() > 0)
			return true;
	}
	return false;
}

void
MidiTrackerEditor::show_existing_main_automations ()
{
	// Gain
	bool gain_visible = param2actrl[Evoral::Parameter(GainAutomation)]->list()->size() > 0;
	gain_automation_item->set_active (gain_visible);
	update_gain_column_visibility ();

	// Mute
	bool mute_visible = param2actrl[Evoral::Parameter(MuteAutomation)]->list()->size() > 0;
	mute_automation_item->set_active (mute_visible);
	update_mute_column_visibility ();

	// Pan
	bool pan_visible = false;
	set<Evoral::Parameter> const & pan_params = route->pannable()->what_can_be_automated ();
	for (set<Evoral::Parameter>::const_iterator p = pan_params.begin(); p != pan_params.end(); ++p) {
		if (param2actrl[*p]->list()->size() > 0) {
			pan_visible = true;
			break;
		}
	}
	pan_automation_item->set_active (pan_visible);
	update_pan_columns_visibility ();
}

void
MidiTrackerEditor::hide_main_automations ()
{
	// Gain
	gain_automation_item->set_active (false);
	update_gain_column_visibility ();

	// Mute
	mute_automation_item->set_active (false);
	update_mute_column_visibility ();

	// Pan
	pan_automation_item->set_active (false);
	update_pan_columns_visibility ();
}

/////////////////////////
// Other (to sort out) //
/////////////////////////

void
MidiTrackerEditor::register_actions ()
{
	Glib::RefPtr<ActionGroup> beats_per_row_actions = myactions.create_action_group (X_("BeatsPerRow"));
	RadioAction::Group beats_per_row_choice_group;

	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-onetwentyeighths"), _("Beats Per Row to One Twenty Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv128)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixtyfourths"), _("Beats Per Row to Sixty Fourths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv64)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirtyseconds"), _("Beats Per Row to Thirty Seconds"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv32)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyeighths"), _("Beats Per Row to Twenty Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv28)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyfourths"), _("Beats Per Row to Twenty Fourths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv24)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentieths"), _("Beats Per Row to Twentieths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv20)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-asixteenthbeat"), _("Beats Per Row to Sixteenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv16)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fourteenths"), _("Beats Per Row to Fourteenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv14)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twelfths"), _("Beats Per Row to Twelfths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv12)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-tenths"), _("Beats Per Row to Tenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv10)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-eighths"), _("Beats Per Row to Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv8)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sevenths"), _("Beats Per Row to Sevenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv7)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixths"), _("Beats Per Row to Sixths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv6)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fifths"), _("Beats Per Row to Fifths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv5)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-quarters"), _("Beats Per Row to Quarters"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv4)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirds"), _("Beats Per Row to Thirds"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv3)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-halves"), _("Beats Per Row to Halves"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv2)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-beat"), _("Beats Per Row to Beat"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeat)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-bar"), _("Beats Per Row to Bar"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBar)));
}

void
MidiTrackerEditor::resize_width()
{
	int width, height;
	get_size(width, height);
	resize(1, height);
}

void
MidiTrackerEditor::redisplay_visible_note()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS; i++)
		view.get_column(note_colnum(i))->set_visible(i < np->ntracks ? visible_note : false);
	visible_note_button.set_active_state (visible_note ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	// Keep the window width is kept to its minimum
	resize_width();
}

int
MidiTrackerEditor::note_colnum(int tracknum)
{
	return tracknum*4 + NOTE_COLNUM;
}

bool
MidiTrackerEditor::visible_note_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_note = !visible_note;
	redisplay_visible_note();
	return false;
}

void
MidiTrackerEditor::redisplay_visible_channel()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS; i++)
		view.get_column(note_channel_colnum(i))->set_visible(i < np->ntracks ? visible_channel : false);
	visible_channel_button.set_active_state (visible_channel ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	// Keep the window width is kept to its minimum
	resize_width();
}

int
MidiTrackerEditor::note_channel_colnum(int tracknum)
{
	return tracknum*4 + CHANNEL_COLNUM;
}

bool
MidiTrackerEditor::visible_channel_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_channel = !visible_channel;
	redisplay_visible_channel();
	return false;
}

void
MidiTrackerEditor::redisplay_visible_velocity()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS; i++)
		view.get_column(note_velocity_colnum(i))->set_visible(i < np->ntracks ? visible_velocity : false);
	visible_velocity_button.set_active_state (visible_velocity ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	// Keep the window width is kept to its minimum
	resize_width();
}

int
MidiTrackerEditor::note_velocity_colnum(int tracknum)
{
	return tracknum*4 + VELOCITY_COLNUM;
}

bool
MidiTrackerEditor::visible_velocity_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_velocity = !visible_velocity;
	redisplay_visible_velocity();
	return false;
}

void
MidiTrackerEditor::redisplay_visible_delay()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS; i++)
		view.get_column(note_delay_colnum(i))->set_visible(i < np->ntracks ? visible_delay : false);
	redisplay_visible_automation_delay ();
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	// Keep the window width is kept to its minimum
	resize_width();
}

int
MidiTrackerEditor::note_delay_colnum(int tracknum)
{
	return tracknum*4 + DELAY_COLNUM;
}

bool
MidiTrackerEditor::visible_delay_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_delay = !visible_delay;
	redisplay_visible_delay();
	return false;
}

void
MidiTrackerEditor::redisplay_visible_automation()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS; i++) {
		size_t col = automation_colnum(i);
		bool is_visible = is_in(col, visible_automation_columns);
		view.get_column(col)->set_visible(is_visible);
	}
	redisplay_visible_automation_delay();

	// Keep the window width is kept to its minimum
	resize_width();
}

int
MidiTrackerEditor::automation_colnum(int tracknum)
{
	return automation_col_offset + 2 * tracknum;
}

void
MidiTrackerEditor::redisplay_visible_automation_delay()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS; i++) {
		size_t col = automation_delay_colnum(i);
		bool is_visible = visible_delay && is_in(col - 1, visible_automation_columns);
		view.get_column(col)->set_visible(is_visible);
	}

	// Keep the window width is kept to its minimum
	resize_width();
}

int
MidiTrackerEditor::automation_delay_colnum(int tracknum)
{
	return automation_col_offset + 2 * tracknum + 1;
}

void
MidiTrackerEditor::automation_click ()
{
	build_automation_action_menu ();
	automation_action_menu->popup (1, gtk_get_current_event_time());
}

void
MidiTrackerEditor::update_remove_note_column_button ()
{
	remove_note_column_button.set_sensitive (np->nreqtracks < np->ntracks);
}

bool
MidiTrackerEditor::remove_note_column_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	np->dec_ntracks ();
	redisplay_model ();
	update_remove_note_column_button ();

	return false;
}

bool
MidiTrackerEditor::add_note_column_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	np->inc_ntracks ();
	redisplay_model ();
	update_remove_note_column_button ();

	return false;
}

bool
MidiTrackerEditor::step_edit_press (GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	step_edit = !step_edit;
	step_edit_button.set_active_state (step_edit ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	return false;
}

void
MidiTrackerEditor::redisplay_model ()
{
	if (editing_editable)
		return;

	if (_session) {

		np->set_rows_per_beat(rows_per_beat);
		np->update();
		delay_spinner.get_adjustment()->set_lower(np->delay_ticks_min());
		delay_spinner.get_adjustment()->set_upper(np->delay_ticks_max());

		tap->set_rows_per_beat(rows_per_beat);
		tap->update();

		rap->set_rows_per_beat(rows_per_beat);
		rap->update();

		TreeModel::Row row;

		// Make sure that midi and automation regions start at the same sample
		assert (np->sample_at_row(0) == tap->sample_at_row(0));
		assert (np->sample_at_row(0) == rap->sample_at_row(0));

		nrows = std::max(std::max(np->nrows, tap->nrows), rap->nrows);

		std::string beat_background_color = UIConfiguration::instance().color_str ("tracker editor: beat background");
		std::string bar_background_color = UIConfiguration::instance().color_str ("tracker editor: bar background");
		std::string background_color = UIConfiguration::instance().color_str ("tracker editor: background");
		std::string blank_foreground_color = UIConfiguration::instance().color_str ("tracker editor: blank foreground");
		std::string active_foreground_color = UIConfiguration::instance().color_str ("tracker editor: active foreground");
		std::string passive_foreground_color = UIConfiguration::instance().color_str ("tracker editor: passive foreground");

		// Fill rows
		TreeModel::Children::iterator row_it = model->children().begin();
		for (uint32_t irow = 0; irow < nrows; irow++) {
			// Get existing row, or create one if it does exist
			if (row_it == model->children().end())
				row_it = model->append();
			row = *row_it++;

			Temporal::Beats row_beats = np->beats_at_row(irow);
			Temporal::Beats row_relative_beats = np->region_relative_beats_at_row(irow);
			uint32_t row_sample = np->sample_at_row(irow);

			// Time
			Timecode::BBT_Time row_bbt;
			_session->bbt_time(row_sample, row_bbt);
			stringstream ss;
			print_padded(ss, row_bbt);
			row[columns.time] = ss.str();

			// If the row is on a bar, beat or otherwise, the color differs
			row[columns._background_color] =
				(row_beats == row_beats.round_up_to_beat() ?
				 (row_bbt.beats == 1 ? bar_background_color : beat_background_color)
				 : background_color);

			// Render midi notes pattern
			size_t ntracks = np->ntracks;
			if (ntracks > MAX_NUMBER_OF_NOTE_TRACKS) {
				// TODO: use Ardour's logger instead of stdout
				std::cout << "Warning: Number of note tracks needed for "
				          << "the tracker interface is too high, "
				          << "some notes might be discarded" << std::endl;
				ntracks = MAX_NUMBER_OF_NOTE_TRACKS;
			}
			for (size_t i = 0; i < ntracks; i++) {

				// Fill with blank
				row[columns.note_name[i]] = "----";
				row[columns.channel[i]] = "--";
				row[columns.velocity[i]] = "---";
				row[columns.delay[i]] = "-----";

				// Grey out infoless cells
				row[columns._note_foreground_color[i]] = blank_foreground_color;
				row[columns._channel_foreground_color[i]] = blank_foreground_color;
				row[columns._velocity_foreground_color[i]] = blank_foreground_color;
				row[columns._delay_foreground_color[i]] = blank_foreground_color;

				// Reset keeping track of the on and off notes
				row[columns._off_note[i]] = NULL;
				row[columns._on_note[i]] = NULL;

				size_t off_notes_count = np->off_notes[i].count(irow);
				size_t on_notes_count = np->on_notes[i].count(irow);

				if (on_notes_count > 0 || off_notes_count > 0) {
					if (np->is_displayable(irow, i)) {
						// Notes off
						NotePattern::RowToNotes::const_iterator i_off = np->off_notes[i].find(irow);
						if (i_off != np->off_notes[i].end()) {
							NoteTypePtr note = i_off->second;
							row[columns.note_name[i]] = note_off_str;
							row[columns._note_foreground_color[i]] = active_foreground_color;
							int64_t delay_ticks = np->region_relative_delay_ticks(note->end_time(), irow);
							if (delay_ticks != 0) {
								row[columns.delay[i]] = to_string (delay_ticks);
								row[columns._delay_foreground_color[i]] = active_foreground_color;
							}
							// Keep the note off around for playing and editing
							row[columns._off_note[i]] = note;
						}

						// Notes on
						NotePattern::RowToNotes::const_iterator i_on = np->on_notes[i].find(irow);
						if (i_on != np->on_notes[i].end()) {
							NoteTypePtr note = i_on->second;
							row[columns.channel[i]] = to_string (note->channel() + 1);
							row[columns.note_name[i]] = ParameterDescriptor::midi_note_name (note->note());
							row[columns.velocity[i]] = to_string ((int)note->velocity());
							row[columns._note_foreground_color[i]] = active_foreground_color;
							row[columns._channel_foreground_color[i]] = active_foreground_color;
							row[columns._velocity_foreground_color[i]] = active_foreground_color;

							int64_t delay_ticks = np->region_relative_delay_ticks(note->time(), irow);
							if (delay_ticks != 0) {
								row[columns.delay[i]] = to_string (delay_ticks);
								row[columns._delay_foreground_color[i]] = active_foreground_color;
							}
							// Keep the note around for playing and editing
							row[columns._on_note[i]] = note;
						}
					} else {
						// Too many notes, not displayable
						row[columns.note_name[i]] = undefined_str;
						row[columns._note_foreground_color[i]] = active_foreground_color;
					}
				}
			}

			// Render automation pattern
			for (ColParamBimap::left_const_iterator cp_it = col2param.left.begin(); cp_it != col2param.left.end(); ++cp_it) {
				size_t col_idx = cp_it->first;
				ColAutoTrackBimap::left_const_iterator ca_it = col2autotrack.left.find(col_idx);
				size_t i = ca_it->second;
				const Evoral::Parameter& param = cp_it->second;
				AutomationPattern* ap = get_automation_pattern (param);
				const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
				size_t auto_count = r2at.count(irow);

				if (i >= MAX_NUMBER_OF_AUTOMATION_TRACKS) {
					std::cout << "Warning: Number of automation tracks needed for the tracker interface is too high, some automations might be discarded" << std::endl;
					continue;
				}

				row[columns._automation_delay_foreground_color[i]] = blank_foreground_color;

				// Fill with blank
				row[columns.automation[i]] = "---";
				row[columns.automation_delay[i]] = "-----";

				if (auto_count > 0) {
					if (ap->is_displayable(irow, param)) {
						AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(irow);
						if (auto_it != r2at.end()) {
							double aval = (*auto_it->second)->value;
							row[columns.automation[i]] = to_string (aval);
							double awhen = (*auto_it->second)->when;
							int64_t delay_ticks = is_region_automation (param) ?
								rap->region_relative_delay_ticks(Temporal::Beats(awhen), irow) : tap->delay_ticks((samplepos_t)awhen, irow);
							if (delay_ticks != 0) {
								row[columns.automation_delay[i]] = to_string (delay_ticks);
								row[columns._automation_delay_foreground_color[i]] = active_foreground_color;
							}
							// Keep the automation iterator around for editing it
							row[columns._automation[i]] = auto_it->second; // TODO not sure it is useful yet
						}
					} else {
						row[columns.automation[i]] = undefined_str;
					}
					row[columns._automation_foreground_color[i]] = active_foreground_color;
				} else {
					// Interpolation
					double inter_auto_val = 0;
					if (param2actrl[param]) {
						boost::shared_ptr<AutomationList> alist = param2actrl[param]->alist();
						// We need to use ControlList::rt_safe_eval instead of ControlList::eval, otherwise the lock inside eval
						// interferes with the lock inside ControlList::erase. Though if mark_dirty is called outside of the scope
						// of the WriteLock in ControlList::erase and such, then eval can be used.
						bool ok;
						double awhen = is_region_automation (param) ? row_relative_beats.to_double() : row_sample;
						inter_auto_val = alist->rt_safe_eval(awhen, ok);
					}
					row[columns.automation[i]] = to_string (inter_auto_val);
					row[columns._automation_foreground_color[i]] = passive_foreground_color;
				}
			}
		}

		// Remove unused rows
		for (; row_it != model->children().end();)
			row_it = model->erase(row_it);
	}
	view.set_model (model);

	// In case tracks have been added or removed
	redisplay_visible_note();
	redisplay_visible_channel();
	redisplay_visible_velocity();
	redisplay_visible_delay();
	redisplay_visible_automation();
}

/////////////////////
// Edit Pattern    //
/////////////////////

uint32_t
MidiTrackerEditor::get_row_index(const std::string& path)
{
	return get_row_index (TreeModel::Path (path));
}

uint32_t
MidiTrackerEditor::get_row_index(const TreeModel::Path& path)
{
	return path.front();
}

boost::shared_ptr<MidiTrackerEditor::NoteType>
MidiTrackerEditor::get_on_note(int rowidx)
{
	return get_on_note(TreeModel::Path (1U, rowidx));
}

boost::shared_ptr<MidiTrackerEditor::NoteType>
MidiTrackerEditor::get_on_note(const std::string& path)
{
	return get_on_note(TreeModel::Path (path));
}

boost::shared_ptr<MidiTrackerEditor::NoteType>
MidiTrackerEditor::get_on_note(const TreeModel::Path& path)
{
	TreeModel::iterator iter = model->get_iter (path);
	if (!iter)
		return NoteTypePtr();
	return (*iter)[columns._on_note[edit_tracknum]];
}

boost::shared_ptr<MidiTrackerEditor::NoteType>
MidiTrackerEditor::get_off_note(int rowidx)
{
	return get_off_note(TreeModel::Path ((TreeModel::Path::size_type)1, (TreeModel::Path::value_type)rowidx));
}

boost::shared_ptr<MidiTrackerEditor::NoteType>
MidiTrackerEditor::get_off_note(const std::string& path)
{
	return get_off_note (TreeModel::Path (path));
}

boost::shared_ptr<MidiTrackerEditor::NoteType>
MidiTrackerEditor::get_off_note(const TreeModel::Path& path)
{
	TreeModel::iterator iter = model->get_iter (path);
	if (!iter)
		return NoteTypePtr();
	return (*iter)[columns._off_note[edit_tracknum]];
}

boost::shared_ptr<MidiTrackerEditor::NoteType>
MidiTrackerEditor::get_note(const std::string& path)
{
	return get_note (TreeModel::Path (path));
}

boost::shared_ptr<MidiTrackerEditor::NoteType>
MidiTrackerEditor::get_note(const TreeModel::Path& path)
{
	NoteTypePtr note = get_on_note(path);
	if (!note)
		note = get_off_note(path);
	return note;
}

void
MidiTrackerEditor::editing_note_started (CellEditable* ed, const string& path, int tracknum)
{
	edit_colnum = note_colnum (tracknum);
	editing_started (ed, path, tracknum);

	if (ed && step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::step_editing_note_key_press), false);
		}
	}
}

void
MidiTrackerEditor::editing_note_channel_started (CellEditable* ed, const string& path, int tracknum)
{
	edit_colnum = note_channel_colnum (tracknum);
	editing_started (ed, path, tracknum);

	if (ed && step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::step_editing_note_channel_key_press), false);
		}
	}
}

void
MidiTrackerEditor::editing_note_velocity_started (CellEditable* ed, const string& path, int tracknum)
{
	edit_colnum = note_velocity_colnum (tracknum);
	editing_started (ed, path, tracknum);

	if (ed && step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::step_editing_note_velocity_key_press), false);
		}
	}
}

void
MidiTrackerEditor::editing_note_delay_started (CellEditable* ed, const string& path, int tracknum)
{
	edit_colnum = note_delay_colnum (tracknum);
	editing_started (ed, path, tracknum);

	if (ed && step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::step_editing_note_delay_key_press), false);
		}
	}
}

void
MidiTrackerEditor::editing_automation_started (CellEditable* ed, const string& path, int tracknum)
{
	edit_colnum = automation_colnum (tracknum);
	editing_started (ed, path, tracknum);

	if (ed && step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::step_editing_automation_key_press), false);
		}
	}
}

void
MidiTrackerEditor::editing_automation_delay_started (CellEditable* ed, const string& path, int tracknum)
{
	edit_colnum = automation_delay_colnum (tracknum);
	editing_started (ed, path, tracknum);

	if (ed && step_edit) {
		Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ed);
		if (e) {
			e->signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::step_editing_automation_delay_key_press), false);
		}
	}
}

void
MidiTrackerEditor::editing_started (CellEditable* ed, const string& path, int tracknum)
{
	edit_path = TreePath (path);
	edit_rowidx = get_row_index (edit_path);
	edit_tracknum = tracknum;
	editing_editable = ed;
}

void
MidiTrackerEditor::clear_editables ()
{
	edit_path.clear ();
	edit_rowidx = -1;
	edit_tracknum = -1;
	edit_colnum = -1;
	editing_editable = NULL;

	redisplay_model ();
}

void
MidiTrackerEditor::editing_canceled ()
{
	clear_editables ();
}

uint8_t
MidiTrackerEditor::parse_pitch (std::string text) const
{
	// Add default octave, if the octave digit is missing
	if (!text.empty() && !std::isdigit(*text.rbegin()))
		text += to_string(octave_spinner.get_value_as_int());

	// Parse the note per se
	return ParameterDescriptor::midi_note_num(text);
}

void
MidiTrackerEditor::note_edited (const std::string& path, const std::string& text)
{
	std::string norm_text = boost::erase_all_copy(text, " ");
	bool is_del = norm_text.empty();
	bool is_off = !is_del and (norm_text[0] == note_off_str[0]);
	uint8_t pitch = parse_pitch (norm_text);
	bool is_on = pitch <= 127;

	// Can't edit ***
	if (not np->is_displayable(edit_rowidx, edit_tracknum)) {
		clear_editables ();
		return;
	}

	if (is_on) {
		set_on_note (pitch, edit_rowidx, edit_tracknum);
	} else if (is_off) {
		set_off_note (edit_rowidx, edit_tracknum);
	} else if (is_del) {
		delete_note (edit_rowidx, edit_tracknum);
	}

	clear_editables ();
}

void
MidiTrackerEditor::set_on_note (uint8_t pitch, int rowidx, int tracknum)
{
	// Abort if the new pitch is invalid
	if (127 < pitch)
		return;

	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);

	int delay = delay_spinner.get_value_as_int();
	uint8_t chan = channel_spinner.get_value_as_int() - 1;
	uint8_t vel = velocity_spinner.get_value_as_int();

	MidiModel::NoteDiffCommand* cmd = NULL;

	if (on_note) {
		// Change the pitch of the on note
		char const * opname = _("change note");
		cmd = midi_model->new_note_diff_command (opname);
		cmd->change (on_note, MidiModel::NoteDiffCommand::NoteNumber, pitch);
	} else if (off_note) {
		// Replace off note by another (non-off) note. Calculate the start
		// time and length of the new on note.
		Temporal::Beats start = off_note->end_time();
		Temporal::Beats end = np->next_off(rowidx, edit_tracknum);
		Temporal::Beats length = end - start;
		// Build note using defaults
		NoteTypePtr new_note(new NoteType(chan, start, length, pitch, vel));
		char const * opname = _("add note");
		cmd = midi_model->new_note_diff_command (opname);
		cmd->add (new_note);
		// Pre-emptively add the note in np to so that it knows in
		// which track it is supposed to be.
		np->add (edit_tracknum, new_note);
	} else {
		// Create a new on note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = np->region_relative_beats_at_row(rowidx, delay);
		NoteTypePtr prev_note = np->find_prev(rowidx, edit_tracknum);
		Temporal::Beats prev_start;
		Temporal::Beats prev_end;
		if (prev_note) {
			prev_start = prev_note->time();
			prev_end = prev_note->end_time();
		}

		char const * opname = _("add note");
		cmd = midi_model->new_note_diff_command (opname);
		// Only update the length the previous note if the new on note
		// is shortening it.
		if (prev_note) {
			if (here <= prev_end) {
				Temporal::Beats new_length = here - prev_start;
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, new_length);
			}
		}

		// Create the new note using the defaults. Calculate the start
		// and length of the new note
		Temporal::Beats end = np->next_off(rowidx, edit_tracknum);
		Temporal::Beats length = end - here;
		NoteTypePtr new_note(new NoteType(chan, here, length, pitch, vel));
		cmd->add (new_note);
		// Pre-emptively add the note in np to so that it knows in
		// which track it is supposed to be.
		np->add (edit_tracknum, new_note);
	}

	// Apply note changes
	if (cmd)
		apply_command (cmd);
}

void
MidiTrackerEditor::set_off_note (int rowidx, int tracknum)
{
	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);

	int delay = delay_spinner.get_value_as_int();

	MidiModel::NoteDiffCommand* cmd = NULL;

	if (on_note) {
		// Replace the on note by an off note, that is remove the on note
		char const * opname = _("delete note");
		cmd = midi_model->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is no off note, update the length of the preceding node
		// to match the new off note (smart off note).
		if (!off_note) {
			NoteTypePtr prev_note = np->find_prev(rowidx, edit_tracknum);
			if (prev_note) {
				Temporal::Beats length = on_note->time() - prev_note->time();
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else {
		// Create a new off note in an empty cell
		// Fetch useful information for most cases
		Temporal::Beats here = np->region_relative_beats_at_row(rowidx, delay);
		NoteTypePtr prev_note = np->find_prev(rowidx, edit_tracknum);
		Temporal::Beats prev_start;
		Temporal::Beats prev_end;
		if (prev_note) {
			prev_start = prev_note->time();
			prev_end = prev_note->end_time();
		}

		// Update the length the previous note to match the new off
		// note no matter what (smart off note).
		if (prev_note) {
			Temporal::Beats new_length = here - prev_start;
			char const * opname = _("resize note");
			cmd = midi_model->new_note_diff_command (opname);
			cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, new_length);
		}
	}

	// Apply note changes
	if (cmd)
		apply_command (cmd);
}

void
MidiTrackerEditor::delete_note (int rowidx, int tracknum)
{
	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);

	MidiModel::NoteDiffCommand* cmd = NULL;

	if (on_note) {
		// Delete on note and change
		char const * opname = _("delete note");
		cmd = midi_model->new_note_diff_command (opname);
		cmd->remove (on_note);

		// If there is an off note, update the length of the preceding note
		// to match the next note or the end of the region.
		if (off_note) {
			NoteTypePtr prev_note = np->find_prev(rowidx, edit_tracknum);
			if (prev_note) {
				// Calculate the length of the previous note
				Temporal::Beats start = prev_note->time();
				Temporal::Beats end = np->next_off(rowidx, edit_tracknum);
				Temporal::Beats length = end - start;
				cmd->change (prev_note, MidiModel::NoteDiffCommand::Length, length);
			}
		}
	} else if (off_note) {
		// Update the length of the corresponding on note so the off note
		// matches the next note or the end of the region.
		Temporal::Beats start = off_note->time();
		Temporal::Beats end = np->next_off(rowidx, edit_tracknum);
		Temporal::Beats length = end - start;
		char const * opname = _("resize note");
		cmd = midi_model->new_note_diff_command (opname);
		cmd->change (off_note, MidiModel::NoteDiffCommand::Length, length);
	}

	// Apply note changes
	if (cmd)
		apply_command (cmd);
}

void
MidiTrackerEditor::note_channel_edited (const std::string& path, const std::string& text)
{
	NoteTypePtr note = get_on_note(path);
	if (text.empty() || !note) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	if (!np->is_displayable(edit_rowidx, edit_tracknum)) {
		clear_editables ();
		return;
	}

	int  ch;
	if (sscanf (text.c_str(), "%d", &ch) == 1) {
		ch--;  // Adjust for zero-based counting
		set_note_channel(note, ch);
	}

	clear_editables ();
}

void
MidiTrackerEditor::set_note_channel (NoteTypePtr note, int ch)
{
	if (!note)
		return;

	if (0 <= ch && ch < 16 && ch != note->channel()) {
		char const* opname = _("change note channel");

		// Define change command
		MidiModel::NoteDiffCommand* cmd = midi_model->new_note_diff_command (opname);
		cmd->change (note, MidiModel::NoteDiffCommand::Channel, ch);

		// Apply change command
		apply_command (cmd);
	}
}

void
MidiTrackerEditor::note_velocity_edited (const std::string& path, const std::string& text)
{
	NoteTypePtr note = get_on_note(path);
	if (text.empty() || !note) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	if (!np->is_displayable(edit_rowidx, edit_tracknum)) {
		clear_editables ();
		return;
	}

	int  vel;
	// Parse the edited velocity and set the note velocity
	if (sscanf (text.c_str(), "%d", &vel) == 1) {
		set_note_velocity (note, vel);
	}

	clear_editables ();
}

void
MidiTrackerEditor::set_note_velocity (NoteTypePtr note, int vel)
{
	if (!note)
		return;

	// Change if within acceptable boundaries and different than the previous
	// velocity
	if (0 <= vel && vel <= 127 && vel != note->velocity()) {
		char const* opname = _("change note velocity");

		// Define change command
		MidiModel::NoteDiffCommand* cmd = midi_model->new_note_diff_command (opname);
		cmd->change (note, MidiModel::NoteDiffCommand::Velocity, vel);

		// Apply change command
		apply_command (cmd);
	}
}

void
MidiTrackerEditor::note_delay_edited (const std::string& path, const std::string& text)
{
	// Can't edit ***
	if (!np->is_displayable(edit_rowidx, edit_tracknum)) {
		clear_editables ();
		return;
	}

	// Parse the edited delay and set note delay
	int delay;
	if (!text.empty() and sscanf (text.c_str(), "%d", &delay) == 1) {
		set_note_delay (delay, edit_rowidx, edit_tracknum);
	}

	clear_editables ();
}

void
MidiTrackerEditor::set_note_delay (int delay, int rowidx, int tracknum)
{
	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);
	if (!on_note && !off_note)
		return;

	char const* opname = _("change note delay");
	MidiModel::NoteDiffCommand* cmd = midi_model->new_note_diff_command (opname);

	// Check if within acceptable boundaries
	if (delay < np->delay_ticks_min() || np->delay_ticks_max() < delay)
		return;

	if (on_note) {
		// Modify the start time and length according to the new on note delay

		// Change start time according to new delay
		int delta = delay - np->region_relative_delay_ticks(on_note->time(), rowidx);
		Temporal::Beats relative_beats = Temporal::Beats::ticks(delta);
		Temporal::Beats new_start = on_note->time() + relative_beats;
		// Make sure the new_start is still within the visible region
		if (new_start < np->start_beats) {
			new_start = np->start_beats;
			relative_beats = new_start - on_note->time();
		}
		cmd->change (on_note, MidiModel::NoteDiffCommand::StartTime, new_start);

		// Adjust length so that the end time doesn't change
		Temporal::Beats new_length = on_note->length() - relative_beats;
		cmd->change (on_note, MidiModel::NoteDiffCommand::Length, new_length);

		if (off_note) {
			// There is an off note at the same row. Adjust its length as well.
			Temporal::Beats new_off_length = off_note->length() + relative_beats;
			cmd->change (off_note, MidiModel::NoteDiffCommand::Length, new_off_length);
		}
	}
	else if (off_note) {
		// There is only an off note. Modify its length accoding to the new off
		// note delay.
		int delta = delay - np->region_relative_delay_ticks(off_note->end_time(), rowidx);
		Temporal::Beats relative_beats = Temporal::Beats::ticks(delta);
		Temporal::Beats new_length = off_note->length() + relative_beats;
		// Make sure the off note is after the on note
		if (new_length < Temporal::Beats::ticks(1))
			new_length = Temporal::Beats::ticks(1);
		// Make sure the off note doesn't go beyong the limit of the region
		if (np->end_beats < off_note->time() + new_length)
			new_length = np->end_beats - off_note->time();
		cmd->change (off_note, MidiModel::NoteDiffCommand::Length, new_length);
	}

	apply_command (cmd);
}

void MidiTrackerEditor::play_note(uint8_t pitch)
{
	uint8_t event[3];
	uint8_t chan = channel_spinner.get_value_as_int() - 1;
	uint8_t vel = velocity_spinner.get_value_as_int();
	event[0] = (MIDI_CMD_NOTE_ON | chan);
	event[1] = pitch;
	event[2] = vel;
	track->write_immediate_event (3, event);
}

void MidiTrackerEditor::release_note(uint8_t pitch)
{
	uint8_t event[3];
	uint8_t chan = channel_spinner.get_value_as_int() - 1;
	event[0] = (MIDI_CMD_NOTE_OFF | chan);
	event[1] = pitch;
	event[2] = 0;
	track->write_immediate_event (3, event);
}

bool MidiTrackerEditor::is_region_automation (const Evoral::Parameter& param) const
{
	return ARDOUR::parameter_is_midi((AutomationType)param.type());
}

Evoral::Parameter MidiTrackerEditor::get_parameter (int tracknum)
{
	ColAutoTrackBimap::right_const_iterator ac_it = col2autotrack.right.find(tracknum);
	if (ac_it == col2autotrack.right.end())
		return Evoral::Parameter();
	size_t edited_colnum = ac_it->second;
	ColParamBimap::left_const_iterator it = col2param.left.find(edited_colnum);
	if (it == col2param.left.end())
		return Evoral::Parameter();
	const Evoral::Parameter& param = it->second;
	return param;
}

boost::shared_ptr<AutomationList> MidiTrackerEditor::get_alist (const Evoral::Parameter& param)
{
	if (!param)
		return NULL;
	boost::shared_ptr<ARDOUR::AutomationControl> actrl = param2actrl[param];
	boost::shared_ptr<AutomationList> alist = actrl->alist();
	return alist;
}

AutomationPattern* MidiTrackerEditor::get_automation_pattern (const Evoral::Parameter& param)
{
	if (!param)
		return NULL;
	return is_region_automation (param) ? (AutomationPattern*)rap : (AutomationPattern*)tap;
}

void
MidiTrackerEditor::automation_edited (const std::string& path, const std::string& text)
{
	bool is_del = text.empty();
	double nval;
	if (!is_del and sscanf (text.c_str(), "%lg", &nval) != 1) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	Evoral::Parameter param = get_parameter (edit_tracknum);
	AutomationPattern* ap = get_automation_pattern (param);
	if (!ap || not ap->is_displayable(edit_rowidx, param)) {
		clear_editables ();
		return;
	}

	if (is_del)
		delete_automation (edit_rowidx, edit_tracknum);
	else
		set_automation (nval, edit_rowidx, edit_tracknum);

	clear_editables ();
}

std::pair<double, bool>
MidiTrackerEditor::get_automation_value (int rowidx, int tracknum)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_parameter (tracknum);
	AutomationPattern* ap = get_automation_pattern (param);
	if (!ap)
		return std::make_pair(0.0, false);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);
	if (auto_it != r2at.end()) {
		double aval = (*auto_it->second)->value;
		return std::make_pair(aval, true);
	}
	return std::make_pair(0.0, false);
}

void
MidiTrackerEditor::set_automation (double val, int rowidx, int tracknum)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_parameter (tracknum);
	boost::shared_ptr<AutomationList> alist = get_alist (param);
	if (!alist)
		return;

	// Clamp nval to its range
	boost::shared_ptr<ARDOUR::AutomationControl> actrl = param2actrl[param];
	val = clamp (val, actrl->lower (), actrl->upper ());

	// Find the control iterator to change
	AutomationPattern* ap = get_automation_pattern (param);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);

	// Save state for undo
	XMLNode& before = alist->get_state ();

	// If no existing value, insert one
	if (auto_it == r2at.end()) {
		int delay = delay_spinner.get_value_as_int ();
		Temporal::Beats row_relative_beats = ap->region_relative_beats_at_row(rowidx, delay);
		uint32_t row_sample = ap->sample_at_row(rowidx, delay);
		double awhen = is_region_automation (param) ? row_relative_beats.to_double() : row_sample;
		if (alist->editor_add (awhen, val, false)) {
			XMLNode& after = alist->get_state ();
			register_automation_undo (alist, _("add automation event"), before, after);
		}
		return;
	}

	// Change existing value
	double awhen = (*auto_it->second)->when;
	alist->modify (auto_it->second, awhen, val);
	XMLNode& after = alist->get_state ();
	register_automation_undo (alist, _("change automation event"), before, after);
}

void
MidiTrackerEditor::delete_automation(int rowidx, int tracknum)
{
	Evoral::Parameter param = get_parameter (tracknum);
	boost::shared_ptr<AutomationList> alist = get_alist (param);
	if (!alist)
		return;

	// Find the control iterator to change
	AutomationPattern* ap = get_automation_pattern (param);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);

	// Save state for undo
	XMLNode& before = alist->get_state ();

	alist->erase (auto_it->second);
	XMLNode& after = alist->get_state ();
	register_automation_undo (alist, _("delete automation event"), before, after);
}

void
MidiTrackerEditor::automation_delay_edited (const std::string& path, const std::string& text)
{
	int delay = 0;
	// Parse the edited delay
	if (!text.empty() and sscanf (text.c_str(), "%d", &delay) != 1) {
		clear_editables ();
		return;
	}

	// Check if within acceptable boundaries
	if (delay < np->delay_ticks_min() || np->delay_ticks_max() < delay) {
		clear_editables ();
		return;
	}

	// Can't edit ***
	Evoral::Parameter param = get_parameter (edit_tracknum);
	AutomationPattern* ap = get_automation_pattern (param);
	if (!ap || !ap->is_displayable(edit_rowidx, param)) {
		clear_editables ();
		return;
	}

	set_automation_delay (delay, edit_rowidx, edit_tracknum);

	clear_editables ();
}

std::pair<int, bool>
MidiTrackerEditor::get_automation_delay (int rowidx, int tracknum)
{
	// Find the parameter to automate
	Evoral::Parameter param = get_parameter (tracknum);
	AutomationPattern* ap = get_automation_pattern (param);
	if (!ap)
		return std::make_pair(0, false);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);
	if (auto_it != r2at.end()) {
		double awhen = (*auto_it->second)->when;
		int delay_ticks = is_region_automation (param) ?
			rap->region_relative_delay_ticks(Temporal::Beats(awhen), rowidx) : tap->delay_ticks((samplepos_t)awhen, rowidx);
		return std::make_pair(delay_ticks, true);
	}
	return std::make_pair(0, false);
}

void
MidiTrackerEditor::set_automation_delay (int delay, int rowidx, int tracknum)
{
	// Check if within acceptable boundaries
	if (delay < np->delay_ticks_min() || np->delay_ticks_max() < delay)
		return;

	Temporal::Beats row_relative_beats = tap->region_relative_beats_at_row(rowidx, delay);
	uint32_t row_sample = tap->sample_at_row(rowidx, delay);

	// Find the parameter to change delay
	Evoral::Parameter param = get_parameter (edit_tracknum);
	boost::shared_ptr<AutomationList> alist = get_alist (param);
	if (!alist)
		return;

	// Find the control iterator to change
	AutomationPattern* ap = get_automation_pattern (param);

	const AutomationPattern::RowToAutomationIt& r2at = ap->automations[param];
	AutomationPattern::RowToAutomationIt::const_iterator auto_it = r2at.find(rowidx);

	// If no existing value, abort
	if (auto_it == r2at.end())
		return;

	// Change existing delay
	XMLNode& before = alist->get_state ();
	double awhen = is_region_automation (param) ?
		(row_relative_beats < ap->start_beats ? ap->start_beats : row_relative_beats).to_double()
		: row_sample;
	alist->modify (auto_it->second, awhen, (*auto_it->second)->value);
	XMLNode& after = alist->get_state ();
	register_automation_undo (alist, _("change automation event delay"), before, after);
}

void
MidiTrackerEditor::register_automation_undo (boost::shared_ptr<AutomationList> alist, const std::string& opname, XMLNode& before, XMLNode& after)
{
	PublicEditor& editor = midi_time_axis_view->editor ();
	editor.begin_reversible_command (opname);
	_session->add_command (new MementoCommand<ARDOUR::AutomationList> (*alist.get (), &before, &after));
	editor.commit_reversible_command ();
	_session->set_dirty ();
}

void
MidiTrackerEditor::apply_command (MidiModel::NoteDiffCommand* cmd)
{
	// Apply change command
	midi_model->apply_command (_session, cmd);

	// reset edit info, since we're done
	// TODO: is this really necessary since clear_editables does that
	edit_tracknum = -1;
}

/////////////////////////
// Other (sort out)    //
/////////////////////////

bool
MidiTrackerEditor::is_midi_track () const
{
	return boost::dynamic_pointer_cast<MidiTrack>(route) != 0;
}

boost::shared_ptr<MidiTrack>
MidiTrackerEditor::midi_track() const
{
	return boost::dynamic_pointer_cast<MidiTrack>(route);
}

void
MidiTrackerEditor::build_param2actrl ()
{
	// Gain
	param2actrl[Evoral::Parameter(GainAutomation)] =  route->gain_control();
	connect(Evoral::Parameter(GainAutomation));

	// Mute
	param2actrl[Evoral::Parameter(MuteAutomation)] =  route->mute_control();
	connect(Evoral::Parameter(MuteAutomation));

	// Pan
	set<Evoral::Parameter> const & pan_params = route->pannable()->what_can_be_automated ();
	for (set<Evoral::Parameter>::const_iterator p = pan_params.begin(); p != pan_params.end(); ++p) {
		param2actrl[*p] = route->pannable()->automation_control(*p);
		connect(*p);
	}

	// Midi
	const set<Evoral::Parameter> midi_params = midi_track()->midi_playlist()->contained_automation();
	for (set<Evoral::Parameter>::const_iterator i = midi_params.begin(); i != midi_params.end(); ++i)
		param2actrl[*i] = midi_model->automation_control(*i);

	// Processors
	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->columns.begin(); ii != (*i)->columns.end(); ++ii) {
			param2actrl[(*ii)->what] = boost::dynamic_pointer_cast<AutomationControl>((*i)->processor->control((*ii)->what));
			connect((*ii)->what);
		}
	}
}

void
MidiTrackerEditor::connect (const Evoral::Parameter& param)
{
	boost::shared_ptr<AutomationList> alist = param2actrl[param]->alist();
	alist->StateChanged.connect (content_connections, invalidator (*this), boost::bind (&MidiTrackerEditor::redisplay_model, this), gui_context());
	alist->InterpolationChanged.connect (content_connections, invalidator (*this), boost::bind (&MidiTrackerEditor::redisplay_model, this), gui_context());
}

void
MidiTrackerEditor::setup_time_column()
{
	TreeViewColumn* viewcolumn_time  = new TreeViewColumn (_("Time"), columns.time);
	CellRenderer* cellrenderer_time = viewcolumn_time->get_first_cell_renderer ();		
	viewcolumn_time->add_attribute(cellrenderer_time->property_cell_background (), columns._background_color);
	view.append_column (*viewcolumn_time);
}

void
MidiTrackerEditor::setup_note_column (size_t i)
{
	string note_str(_("Note"));

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_note = new TreeViewColumn (note_str.c_str(), columns.note_name[i]);
	CellRendererText* cellrenderer_note = dynamic_cast<CellRendererText*> (viewcolumn_note->get_first_cell_renderer ());

	// Link to color attributes
	viewcolumn_note->add_attribute(cellrenderer_note->property_cell_background (), columns._background_color);
	viewcolumn_note->add_attribute(cellrenderer_note->property_foreground (), columns._note_foreground_color[i]);

	// Link to editing methods
	cellrenderer_note->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::editing_note_started), i));
	cellrenderer_note->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiTrackerEditor::editing_canceled));
	cellrenderer_note->signal_edited().connect (sigc::mem_fun (*this, &MidiTrackerEditor::note_edited));
	cellrenderer_note->property_editable() = true;

	view.append_column (*viewcolumn_note);
}

void
MidiTrackerEditor::setup_note_channel_column (size_t i)
{
	string ch_str(S_("Channel|Ch"));

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_channel = new TreeViewColumn (ch_str.c_str(), columns.channel[i]);
	CellRendererText* cellrenderer_channel = dynamic_cast<CellRendererText*> (viewcolumn_channel->get_first_cell_renderer ());

	// Link to color attribute
	viewcolumn_channel->add_attribute(cellrenderer_channel->property_cell_background (), columns._background_color);
	viewcolumn_channel->add_attribute(cellrenderer_channel->property_foreground (), columns._channel_foreground_color[i]);

	// Link to editing methods
	cellrenderer_channel->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::editing_note_channel_started), i));
	cellrenderer_channel->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiTrackerEditor::editing_canceled));
	cellrenderer_channel->signal_edited().connect (sigc::mem_fun (*this, &MidiTrackerEditor::note_channel_edited));
	cellrenderer_channel->property_editable() = true;

	view.append_column (*viewcolumn_channel);
}

void
MidiTrackerEditor::setup_note_velocity_column (size_t i)
{
	string vel_str(S_("Velocity|Vel"));

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_velocity = new TreeViewColumn (vel_str.c_str(), columns.velocity[i]);
	CellRendererText* cellrenderer_velocity = dynamic_cast<CellRendererText*> (viewcolumn_velocity->get_first_cell_renderer ());

	// Link to color attribute
	viewcolumn_velocity->add_attribute(cellrenderer_velocity->property_cell_background (), columns._background_color);
	viewcolumn_velocity->add_attribute(cellrenderer_velocity->property_foreground (), columns._velocity_foreground_color[i]);

	// Link to editing methods
	cellrenderer_velocity->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::editing_note_velocity_started), i));
	cellrenderer_velocity->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiTrackerEditor::editing_canceled));
	cellrenderer_velocity->signal_edited().connect (sigc::mem_fun (*this, &MidiTrackerEditor::note_velocity_edited));
	cellrenderer_velocity->property_editable() = true;

	view.append_column (*viewcolumn_velocity);
}

void
MidiTrackerEditor::setup_note_delay_column (size_t i)
{
	string delay_str(_("Delay"));

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_delay = new TreeViewColumn (delay_str.c_str(), columns.delay[i]);
	CellRendererText* cellrenderer_delay = dynamic_cast<CellRendererText*> (viewcolumn_delay->get_first_cell_renderer ());

	// Link to color attribute
	viewcolumn_delay->add_attribute(cellrenderer_delay->property_cell_background (), columns._background_color);
	viewcolumn_delay->add_attribute(cellrenderer_delay->property_foreground (), columns._delay_foreground_color[i]);

	// Link to editing methods
	cellrenderer_delay->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::editing_note_delay_started), i));
	cellrenderer_delay->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiTrackerEditor::editing_canceled));
	cellrenderer_delay->signal_edited().connect (sigc::mem_fun (*this, &MidiTrackerEditor::note_delay_edited));
	cellrenderer_delay->property_editable() = true;

	view.append_column (*viewcolumn_delay);
}

void
MidiTrackerEditor::setup_automation_column (size_t i)
{
	stringstream ss_automation;
	ss_automation << "A" << i;

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_automation = new TreeViewColumn (_(ss_automation.str().c_str()), columns.automation[i]);
	CellRendererText* cellrenderer_automation = dynamic_cast<CellRendererText*> (viewcolumn_automation->get_first_cell_renderer ());

	// Link to color attributes
	viewcolumn_automation->add_attribute(cellrenderer_automation->property_cell_background (), columns._background_color);
	viewcolumn_automation->add_attribute(cellrenderer_automation->property_foreground (), columns._automation_foreground_color[i]);

	// Link to editing methods
	cellrenderer_automation->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::editing_automation_started), i));
	cellrenderer_automation->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiTrackerEditor::editing_canceled));
	cellrenderer_automation->signal_edited().connect (sigc::mem_fun (*this, &MidiTrackerEditor::automation_edited));
	cellrenderer_automation->property_editable() = true;

	size_t column = view.get_columns().size();
	view.append_column (*viewcolumn_automation);
	col2autotrack.insert(ColAutoTrackBimap::value_type(column, i));
	available_automation_columns.insert(column);
	view.get_column(column)->set_visible (false);
}

void
MidiTrackerEditor::setup_automation_delay_column (size_t i)
{
	stringstream ss_automation_delay;
	ss_automation_delay << _("Delay");

	// TODO be careful of potential memory leaks
	TreeViewColumn* viewcolumn_automation_delay = new TreeViewColumn (_(ss_automation_delay.str().c_str()), columns.automation_delay[i]);
	CellRendererText* cellrenderer_automation_delay = dynamic_cast<CellRendererText*> (viewcolumn_automation_delay->get_first_cell_renderer ());

	// Link to color attributes
	viewcolumn_automation_delay->add_attribute(cellrenderer_automation_delay->property_cell_background (), columns._background_color);
	viewcolumn_automation_delay->add_attribute(cellrenderer_automation_delay->property_foreground (), columns._automation_delay_foreground_color[i]);

	// Link to editing methods
	cellrenderer_automation_delay->signal_editing_started().connect (sigc::bind (sigc::mem_fun (*this, &MidiTrackerEditor::editing_automation_delay_started), i));
	cellrenderer_automation_delay->signal_editing_canceled().connect (sigc::mem_fun (*this, &MidiTrackerEditor::editing_canceled));
	cellrenderer_automation_delay->signal_edited().connect (sigc::mem_fun (*this, &MidiTrackerEditor::automation_delay_edited));
	cellrenderer_automation_delay->property_editable() = true;

	size_t column = view.get_columns().size();
	view.append_column (*viewcolumn_automation_delay);
	view.get_column(column)->set_visible (false);
}

void
MidiTrackerEditor::vertical_move_cursor (int steps)
{
	TreeModel::Path path = edit_path;
	wrap_around_move (path, steps);
	TreeViewColumn* col = view.get_column (edit_colnum);
	view.set_cursor (path, *col, true);
}

void MidiTrackerEditor::wrap_around_move (TreeModel::Path& path, int steps) const
{
	path[0] += steps;
	path[0] %= nrows;
	if (path[0] < 0)
		path[0] += nrows;
}

void
MidiTrackerEditor::horizontal_move_cursor (int steps, bool tab)
{
	// TODO support tab == true
	int colnum = edit_colnum;
	const int n_col = view.get_columns().size();
	TreeViewColumn* col;

	while (steps < 0) {
		--colnum;
		if (colnum < 1)
			colnum = n_col - 1;
		col = view.get_column (colnum);
		if (col->get_visible ())
			++steps;
	}
	while (0 < steps) {
		++colnum;
		if (n_col <= colnum)
			colnum = 1;         // colnum 0 is time
		col = view.get_column (colnum);
		if (col->get_visible ())
			--steps;
	}
	TreeModel::Path path = edit_path;
	view.set_cursor (path, *col, true);
}

uint8_t
MidiTrackerEditor::pitch (uint8_t semitones, int octave)
{
	return (uint8_t)(octave + 1) * 12 + semitones;
}

bool MidiTrackerEditor::move_cursor_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	case GDK_Up:
	case GDK_uparrow:
		vertical_move_cursor(-1);
		ret = true;
		break;
	case GDK_Down:
	case GDK_downarrow:
		vertical_move_cursor(1);
		ret = true;
		break;
	case GDK_Left:
	case GDK_leftarrow:
		horizontal_move_cursor(-1);
		ret = true;
		break;
	case GDK_Right:
	case GDK_rightarrow:
		horizontal_move_cursor(1);
		ret = true;
		break;
	case GDK_Tab:
		horizontal_move_cursor(1, true);
		ret = true;
		break;
	}

	return ret;
}

int
MidiTrackerEditor::digit_key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
	// Num keys
	case GDK_0:
	case GDK_parenright:
		return 0;
	case GDK_1:
	case GDK_exclam:
		return 1;
	case GDK_2:
	case GDK_at:
		return 2;
	case GDK_3:
	case GDK_numbersign:
		return 3;
	case GDK_4:
	case GDK_dollar:
		return 4;
	case GDK_5:
	case GDK_percent:
		return 5;
	case GDK_6:
	case GDK_caret:
		return 6;
	case GDK_7:
	case GDK_ampersand:
		return 7;
	case GDK_8:
	case GDK_asterisk:
		return 8;
	case GDK_9:
	case GDK_parenleft:
		return 9;
	default:
		return -1;
	}
}

uint8_t
MidiTrackerEditor::pitch_key (GdkEventKey* ev)
{
	int octave = octave_spinner.get_value_as_int();

	switch (ev->keyval) {
	case GDK_z:                 // C
	case GDK_Z:
		return pitch (0, octave);
	case GDK_s:                 // C#
	case GDK_S:
		return pitch (1, octave);
	case GDK_x:                 // D
	case GDK_X:
		return pitch (2, octave);
	case GDK_d:                 // D#
	case GDK_D:
		return pitch (3, octave);
	case GDK_c:                 // E
	case GDK_C:
		return pitch (4, octave);
	case GDK_v:                 // F
	case GDK_V:
		return pitch (5, octave);
	case GDK_g:                 // F#
	case GDK_G:
		return pitch (6, octave);
	case GDK_b:                 // G
	case GDK_B:
		return pitch (7, octave);
	case GDK_h:                 // G#
	case GDK_H:
		return pitch (8, octave);
	case GDK_n:                 // A
	case GDK_N:
		return pitch (9, octave);
	case GDK_j:                 // A#
	case GDK_J:
		return pitch (10, octave);
	case GDK_m:                 // B
	case GDK_M:
		return pitch (11, octave);
	case GDK_q:                 // C+1
	case GDK_Q:
	case GDK_comma:
	case GDK_less:
		return pitch (0, octave + 1);
	case GDK_2:                 // C#+1
	case GDK_at:
	case GDK_l:
	case GDK_L:
		return pitch (1, octave + 1);
	case GDK_w:                 // D+1
	case GDK_W:
	case GDK_period:
	case GDK_greater:
		return pitch (2, octave + 1);
	case GDK_3:                 // D#+1
	case GDK_numbersign:
	case GDK_semicolon:
	case GDK_colon:
		return pitch (3, octave + 1);
		break;
	case GDK_e:                 // E+1
	case GDK_E:
	case GDK_slash:
	case GDK_question:
		return pitch (4, octave + 1);
	case GDK_r:                 // F+1
	case GDK_R:
		return pitch (5, octave + 1);
	case GDK_5:                 // F#+1
	case GDK_percent:
		return pitch (6, octave + 1);
	case GDK_t:                 // G+1
	case GDK_T:
		return pitch (7, octave + 1);
	case GDK_6:                 // G#+1
	case GDK_caret:
		return pitch (8, octave + 1);
	case GDK_y:                 // A+1
	case GDK_Y:
		return pitch (9, octave + 1);
	case GDK_7:                 // A#+1
	case GDK_ampersand:
		return pitch (10, octave + 1);
	case GDK_u:                 // B+1
	case GDK_U:
		return pitch (11, octave + 1);
	case GDK_i:                 // C+2
	case GDK_I:
		return pitch (0, octave + 2);
	case GDK_9:                 // C#+2
	case GDK_parenleft:
		return pitch (1, octave + 2);
	case GDK_o:                 // D+2
	case GDK_O:
		return pitch (2, octave + 2);
	case GDK_0:                 // D#+2
	case GDK_parenright:
		return pitch (3, octave + 2);
	case GDK_p:                 // E+2
	case GDK_P:
		return pitch (4, octave + 2);
	case GDK_bracketleft:       // F+2
	case GDK_braceleft:
		return pitch (5, octave + 2);
	default:
		return -1;
	}
}

bool
MidiTrackerEditor::step_editing_note_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// On notes
	case GDK_z:                 // C
	case GDK_Z:
	case GDK_s:                 // C#
	case GDK_S:
	case GDK_x:                 // D
	case GDK_X:
	case GDK_d:                 // D#
	case GDK_D:
	case GDK_c:                 // E
	case GDK_C:
	case GDK_v:                 // F
	case GDK_V:
	case GDK_g:                 // F#
	case GDK_G:
	case GDK_b:                 // G
	case GDK_B:
	case GDK_h:                 // G#
	case GDK_H:
	case GDK_n:                 // A
	case GDK_N:
	case GDK_j:                 // A#
	case GDK_J:
	case GDK_m:                 // B
	case GDK_M:
	case GDK_q:                 // C+1
	case GDK_Q:
	case GDK_comma:
	case GDK_less:
	case GDK_2:                 // C#+1
	case GDK_at:
	case GDK_l:
	case GDK_L:
	case GDK_w:                 // D+1
	case GDK_W:
	case GDK_period:
	case GDK_greater:
	case GDK_3:                 // D#+1
	case GDK_numbersign:
	case GDK_semicolon:
	case GDK_colon:
	case GDK_e:                 // E+1
	case GDK_E:
	case GDK_slash:
	case GDK_question:
	case GDK_r:                 // F+1
	case GDK_R:
	case GDK_5:                 // F#+1
	case GDK_percent:
	case GDK_t:                 // G+1
	case GDK_T:
	case GDK_6:                 // G#+1
	case GDK_caret:
	case GDK_y:                 // A+1
	case GDK_Y:
	case GDK_7:                 // A#+1
	case GDK_ampersand:
	case GDK_u:                 // B+1
	case GDK_U:
	case GDK_i:                 // C+2
	case GDK_I:
	case GDK_9:                 // C#+2
	case GDK_parenleft:
	case GDK_o:                 // D+2
	case GDK_O:
	case GDK_0:                 // D#+2
	case GDK_parenright:
	case GDK_p:                 // E+2
	case GDK_P:
	case GDK_bracketleft:       // F+2
	case GDK_braceleft:
		ret = step_editing_set_on_note (pitch_key (ev), edit_rowidx, edit_tracknum);
		break;

	// Off note
	case GDK_equal:
	case GDK_plus:
	case GDK_Caps_Lock:
		ret = step_editing_set_off_note (edit_rowidx, edit_tracknum);
		break;

	// Delete note
	case GDK_BackSpace:
	case GDK_Delete:
		ret = step_editing_delete_note (edit_rowidx, edit_tracknum);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
		ret = move_cursor_key_press (ev);
		break;

	// Other key not passed to the default entry handler
	case GDK_a:
	case GDK_A:
	case GDK_f:
	case GDK_F:
	case GDK_k:
	case GDK_K:
	case GDK_apostrophe:
	case GDK_quotedbl:
	case GDK_1:
	case GDK_exclam:
	case GDK_4:
	case GDK_dollar:
	case GDK_8:
	case GDK_asterisk:
	case GDK_minus:
	case GDK_underscore:
	case GDK_bracketright:
	case GDK_braceright:
		ret = true;

	default:
		break;
	}

	return ret;
}

bool MidiTrackerEditor::step_editing_set_on_note (uint8_t pitch, int rowidx, int tracknum)
{
	play_note(pitch);
	set_on_note (pitch, rowidx, tracknum);
	int steps = steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
	return true;
}

bool MidiTrackerEditor::step_editing_set_off_note (int rowidx, int tracknum)
{
	set_off_note (rowidx, tracknum);
	int steps = steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
	return true;
}

bool MidiTrackerEditor::step_editing_delete_note (int rowidx, int tracknum)
{
	delete_note (rowidx, tracknum);
	int steps = steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
	return true;
}

bool
MidiTrackerEditor::step_editing_note_channel_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_note_channel (digit_key_press (ev), edit_rowidx, edit_tracknum);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
MidiTrackerEditor::step_editing_set_note_channel (int digit, int rowidx, int tracknum)
{
	boost::shared_ptr<MidiTrackerEditor::NoteType> note = get_on_note(rowidx);
	if (note) {
		int ch = note->channel();
		int position = position_spinner.get_value_as_int();
		int new_ch = change_digit (ch + 1, digit, position);
		set_note_channel (note, new_ch - 1);
	}

	int steps = steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
	return true;
}

bool
MidiTrackerEditor::step_editing_note_velocity_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_note_velocity (digit_key_press (ev), edit_rowidx, edit_tracknum);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
MidiTrackerEditor::step_editing_set_note_velocity (int digit, int rowidx, int tracknum)
{
	boost::shared_ptr<MidiTrackerEditor::NoteType> note = get_on_note(rowidx);
	if (note) {
		int vel = note->velocity();
		int position = position_spinner.get_value_as_int();
		int new_vel = change_digit (vel, digit, position);
		set_note_velocity (note, new_vel);
	}
	int steps = steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);
	return true;
}

bool
MidiTrackerEditor::step_editing_note_delay_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_note_delay (digit_key_press (ev), edit_rowidx, edit_tracknum);
		break;

	// Minus
	case GDK_minus:
	case GDK_underscore:
		ret = step_editing_set_note_delay (-1, edit_rowidx, edit_tracknum);
		break;

	// Plus
	case GDK_plus:
	case GDK_equal:
		ret = step_editing_set_note_delay (100, edit_rowidx, edit_tracknum);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
MidiTrackerEditor::step_editing_set_note_delay (int digit, int rowidx, int tracknum)
{
	int position = position_spinner.get_value_as_int();
	int steps = steps_spinner.get_value_as_int();
	NoteTypePtr on_note = get_on_note(rowidx);
	NoteTypePtr off_note = get_off_note(rowidx);
	if (!on_note && !off_note) {
		vertical_move_cursor (steps);
		return true;
	}

	// Fetch delay
	int old_delay = on_note ? np->region_relative_delay_ticks(on_note->time(), edit_rowidx)
		: np->region_relative_delay_ticks(off_note->end_time(), edit_rowidx);

	// Update delay
	int new_delay = change_digit_or_sign(old_delay, digit, position);
	set_note_delay (new_delay, edit_rowidx, edit_tracknum);

	// Move the cursor
	vertical_move_cursor (steps);

	return true;
}

bool
MidiTrackerEditor::step_editing_automation_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_automation (digit_key_press (ev), edit_rowidx, edit_tracknum);
		break;

	// Minus
	case GDK_minus:
	case GDK_underscore:
		ret = step_editing_set_automation (-1, edit_rowidx, edit_tracknum);
		break;

	// Plus
	case GDK_plus:
	case GDK_equal:
		ret = step_editing_set_automation (100, edit_rowidx, edit_tracknum);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
MidiTrackerEditor::step_editing_set_automation (int digit, int rowidx, int tracknum)
{
	std::pair<double, bool> val_def = get_automation_value(rowidx, tracknum);
	double oval = val_def.first;

	// Set new value
	int position = position_spinner.get_value_as_int();
	double nval = change_digit_or_sign (oval, digit, position);
	set_automation (nval, rowidx, tracknum);

	// Move cursor
	int steps = steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);

	return true;
}

bool
MidiTrackerEditor::step_editing_automation_delay_key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {

	// Num keys
	case GDK_0:
	case GDK_parenright:
	case GDK_1:
	case GDK_exclam:
	case GDK_2:
	case GDK_at:
	case GDK_3:
	case GDK_numbersign:
	case GDK_4:
	case GDK_dollar:
	case GDK_5:
	case GDK_percent:
	case GDK_6:
	case GDK_caret:
	case GDK_7:
	case GDK_ampersand:
	case GDK_8:
	case GDK_asterisk:
	case GDK_9:
	case GDK_parenleft:
		ret = step_editing_set_automation_delay (digit_key_press (ev), edit_rowidx, edit_tracknum);
		break;

	// Minus
	case GDK_minus:
	case GDK_underscore:
		ret = step_editing_set_automation_delay (-1, edit_rowidx, edit_tracknum);
		break;

	// Plus
	case GDK_plus:
	case GDK_equal:
		ret = step_editing_set_automation_delay (100, edit_rowidx, edit_tracknum);
		break;

	// Cursor movements
	case GDK_Up:
	case GDK_uparrow:
	case GDK_Down:
	case GDK_downarrow:
	case GDK_Left:
	case GDK_leftarrow:
	case GDK_Right:
	case GDK_rightarrow:
	case GDK_Tab:
		ret = move_cursor_key_press (ev);
		break;

	default:
		break;
	}

	return ret;
}

bool
MidiTrackerEditor::step_editing_set_automation_delay (int digit, int rowidx, int tracknum)
{
	std::pair<int, bool> val_def = get_automation_delay (rowidx, tracknum);
	int old_delay = val_def.first;

	// Set new value
	if (val_def.second) {
		int position = position_spinner.get_value_as_int();
		int new_delay = change_digit_or_sign (old_delay, digit, position);
		set_automation_delay (new_delay, rowidx, tracknum);
	}

	// Move cursor
	int steps = steps_spinner.get_value_as_int();
	vertical_move_cursor (steps);

	return true;
}

bool
MidiTrackerEditor::key_press (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {
	// On notes
	case GDK_z:                 // C
	case GDK_Z:
	case GDK_s:                 // C#
	case GDK_S:
	case GDK_x:                 // D
	case GDK_X:
	case GDK_d:                 // D#
	case GDK_D:
	case GDK_c:                 // E
	case GDK_C:
	case GDK_v:                 // F
	case GDK_V:
	case GDK_g:                 // F#
	case GDK_G:
	case GDK_b:                 // G
	case GDK_B:
	case GDK_h:                 // G#
	case GDK_H:
	case GDK_n:                 // A
	case GDK_N:
	case GDK_j:                 // A#
	case GDK_J:
	case GDK_m:                 // B
	case GDK_M:
	case GDK_q:                 // C+1
	case GDK_Q:
	case GDK_comma:
	case GDK_less:
	case GDK_2:                 // C#+1
	case GDK_at:
	case GDK_l:
	case GDK_L:
	case GDK_w:                 // D+1
	case GDK_W:
	case GDK_period:
	case GDK_greater:
	case GDK_3:                 // D#+1
	case GDK_numbersign:
	case GDK_semicolon:
	case GDK_colon:
	case GDK_e:                 // E+1
	case GDK_E:
	case GDK_slash:
	case GDK_question:
	case GDK_r:                 // F+1
	case GDK_R:
	case GDK_5:                 // F#+1
	case GDK_percent:
	case GDK_t:                 // G+1
	case GDK_T:
	case GDK_6:                 // G#+1
	case GDK_caret:
	case GDK_y:                 // A+1
	case GDK_Y:
	case GDK_7:                 // A#+1
	case GDK_ampersand:
	case GDK_u:                 // B+1
	case GDK_U:
	case GDK_i:                 // C+2
	case GDK_I:
	case GDK_9:                 // C#+2
	case GDK_parenleft:
	case GDK_o:                 // D+2
	case GDK_O:
	case GDK_0:                 // D#+2
	case GDK_parenright:
	case GDK_p:                 // E+2
	case GDK_P:
	case GDK_bracketleft:       // F+2
	case GDK_braceleft:
		play_note (pitch_key (ev));
		ret = true;
		break;
	}

	return ret;
}

bool
MidiTrackerEditor::key_release (GdkEventKey* ev)
{
	bool ret = false;

	switch (ev->keyval) {
	// On notes
	case GDK_z:                 // C
	case GDK_Z:
	case GDK_s:                 // C#
	case GDK_S:
	case GDK_x:                 // D
	case GDK_X:
	case GDK_d:                 // D#
	case GDK_D:
	case GDK_c:                 // E
	case GDK_C:
	case GDK_v:                 // F
	case GDK_V:
	case GDK_g:                 // F#
	case GDK_G:
	case GDK_b:                 // G
	case GDK_B:
	case GDK_h:                 // G#
	case GDK_H:
	case GDK_n:                 // A
	case GDK_N:
	case GDK_j:                 // A#
	case GDK_J:
	case GDK_m:                 // B
	case GDK_M:
	case GDK_q:                 // C+1
	case GDK_Q:
	case GDK_comma:
	case GDK_less:
	case GDK_2:                 // C#+1
	case GDK_at:
	case GDK_l:
	case GDK_L:
	case GDK_w:                 // D+1
	case GDK_W:
	case GDK_period:
	case GDK_greater:
	case GDK_3:                 // D#+1
	case GDK_numbersign:
	case GDK_semicolon:
	case GDK_colon:
	case GDK_e:                 // E+1
	case GDK_E:
	case GDK_slash:
	case GDK_question:
	case GDK_r:                 // F+1
	case GDK_R:
	case GDK_5:                 // F#+1
	case GDK_percent:
	case GDK_t:                 // G+1
	case GDK_T:
	case GDK_6:                 // G#+1
	case GDK_caret:
	case GDK_y:                 // A+1
	case GDK_Y:
	case GDK_7:                 // A#+1
	case GDK_ampersand:
	case GDK_u:                 // B+1
	case GDK_U:
	case GDK_i:                 // C+2
	case GDK_I:
	case GDK_9:                 // C#+2
	case GDK_parenleft:
	case GDK_o:                 // D+2
	case GDK_O:
	case GDK_0:                 // D#+2
	case GDK_parenright:
	case GDK_p:                 // E+2
	case GDK_P:
	case GDK_bracketleft:       // F+2
	case GDK_braceleft:
		release_note (pitch_key (ev));
		ret = true;
		break;
	}

	return ret;
}

bool
MidiTrackerEditor::button_event (GdkEventButton* ev)
{
	// TODO: understand why get_path_at_pos does not work
	// if (ev->button == 1) {
		TreeModel::Path path;
		TreeViewColumn* col;
		int cell_x, cell_y;
		view.get_path_at_pos(ev->x, ev->y, path, col, cell_x, cell_y);
		int colnum = GPOINTER_TO_UINT (col->get_data (X_("colnum")));
		std::cout << "ev->button = " << ev->button
		          << ", ev->x = " << ev->x
		          << ", ev->y = " << ev->y
		          << ", ev->x_root = " << ev->x_root
		          << ", ev->y_root = " << ev->y_root << std::endl;
		std::cout << "ev->window = " << ev->window
		          << ", get_bin_window()->gobj() = "
		          << view.get_bin_window()->gobj() << endl;
		std::cout << "path = " << path << ", colnum = " << colnum
		          << ", cell_x = " << cell_x << ", cell_y = " << cell_y
		          << endl;

		int x, y;
		int bx, by;
		view.get_pointer (x, y);
		view.convert_widget_to_bin_window_coords (x, y, bx, by);
		view.get_path_at_pos (bx, by, path);
		std::cout << "x = " << x << ", y = " << y
		          << ", bx = " << bx << ", by = " << by
		          << ", path-2 = " << path << std::endl;

	// }
	return true;
}

bool
MidiTrackerEditor::scroll_event (GdkEventScroll* ev)
{
	// TODO change values if editing is active, otherwise scroll.
	return false;               // Silence compiler
}

void
MidiTrackerEditor::build_pattern ()
{
	np = new NotePattern(_session, region, midi_model);
	tap = new TrackAutomationPattern(_session, region);
	rap = new RegionAutomationPattern(_session, region);

	update_automation_patterns ();
}

void
MidiTrackerEditor::update_automation_patterns ()
{
	// Insert automation controls in the automation patterns
	for (Parameter2AutomationControl::const_iterator it = param2actrl.begin(); it != param2actrl.end(); ++it) {
		// Midi automation are attached to the region, not the track
		if (is_region_automation (it->first))
			rap->insert(it->second);
		else
			tap->insert(it->second);
	}
}

void
MidiTrackerEditor::setup_pattern ()
{
	model = ListStore::create (columns);
	view.set_model (model);

	setup_time_column();

	// Instantiate note tracks
	for (size_t i = 0; i < MAX_NUMBER_OF_NOTE_TRACKS; i++) {
		setup_note_column(i);
		setup_note_channel_column(i);
		setup_note_velocity_column(i);
		setup_note_delay_column(i);
	}

	automation_col_offset = view.get_columns().size();

	// Instantiate automation tracks
	for (size_t i = 0; i < MAX_NUMBER_OF_AUTOMATION_TRACKS; i++) {
		setup_automation_column(i);
		setup_automation_delay_column(i);
	}

	// Connect to key press events
	view.signal_key_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::key_press), false);
	view.signal_key_release_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::key_release), false);

	// Connect to mouse button events
	//
	// Disabled for now because it doesn't work as expected
	//
	// view.signal_button_press_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::button_event), false);
	// view.signal_scroll_event().connect (sigc::mem_fun (*this, &MidiTrackerEditor::scroll_event), false);

	view.set_headers_visible (true);
	view.set_rules_hint (true);
	view.set_grid_lines (TREE_VIEW_GRID_LINES_BOTH);
	view.get_selection()->set_mode (SELECTION_NONE);
	view.set_enable_search(false);

	view.show ();
}

void
MidiTrackerEditor::setup_toolbar ()
{
	toolbar.set_spacing (2);

	// Add beats per row selection
	beats_per_row_selector.show ();
	toolbar.pack_start (beats_per_row_selector, false, false);

	// Add visible note button
	visible_note_button.set_name ("visible note button");
	visible_note_button.set_text (S_("Note|N"));
	visible_note_button.set_active_state (visible_note ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_note_button.show ();
	visible_note_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_note_press), false);
	toolbar.pack_start (visible_note_button, false, false);

	// Add visible channel button
	visible_channel_button.set_name ("visible channel button");
	visible_channel_button.set_text (S_("Channel|C"));
	visible_channel_button.set_active_state (visible_channel ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_channel_button.show ();
	visible_channel_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_channel_press), false);
	toolbar.pack_start (visible_channel_button, false, false);

	// Add visible velocity button
	visible_velocity_button.set_name ("visible velocity button");
	visible_velocity_button.set_text (S_("Velocity|V"));
	visible_velocity_button.set_active_state (visible_velocity ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_velocity_button.show ();
	visible_velocity_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_velocity_press), false);
	toolbar.pack_start (visible_velocity_button, false, false);

	// Add visible delay button
	visible_delay_button.set_name ("visible delay button");
	visible_delay_button.set_text (S_("Delay|D"));
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_delay_button.show ();
	visible_delay_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_delay_press), false);
	toolbar.pack_start (visible_delay_button, false, false);

	// Add automation button
	automation_button.set_name ("automation button");
	automation_button.set_text (S_("Automation|A"));
	automation_button.signal_clicked.connect (sigc::mem_fun(*this, &MidiTrackerEditor::automation_click));
	automation_button.show ();
	toolbar.pack_start (automation_button, false, false);

	// Remove/add note column
	rm_add_note_column_separator.show ();
	toolbar.pack_start (rm_add_note_column_separator, false, false);
	remove_note_column_button.set_name ("remove note column");
	remove_note_column_button.set_text (S_("Remove|-"));
	remove_note_column_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::remove_note_column_press), false);
	remove_note_column_button.show ();
	remove_note_column_button.set_sensitive (false);
	toolbar.pack_start (remove_note_column_button, false, false);
	add_note_column_button.set_name ("add note column");
	add_note_column_button.set_text (S_("Add|+"));
	add_note_column_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::add_note_column_press), false);
	add_note_column_button.show ();
	toolbar.pack_start (add_note_column_button, false, false);

	// Step edit button
	step_edit_separator.show ();
	toolbar.pack_start (step_edit_separator, false, false);
	step_edit_button.set_name ("step edit button");
	step_edit_button.set_text (S_("Step Edit"));
	step_edit_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::step_edit_press), false);
	step_edit_button.show ();
	toolbar.pack_start (step_edit_button, false, false);

	// Steps spinner
	steps_separator.show ();
	toolbar.pack_start (steps_separator, false, false);
	steps_label.show ();
	toolbar.pack_start (steps_label, false, false);
	steps_spinner.set_activates_default ();
	steps_spinner.show ();
	toolbar.pack_start (steps_spinner, false, false);

	// Octave spinner
	octave_separator.show ();
	toolbar.pack_start (octave_separator, false, false);
	octave_label.show ();
	toolbar.pack_start (octave_label, false, false);
	octave_spinner.set_activates_default ();
	octave_spinner.show ();
	toolbar.pack_start (octave_spinner, false, false);

	// Channel spinner
	channel_separator.show ();
	toolbar.pack_start (channel_separator, false, false);
	channel_label.show ();
	toolbar.pack_start (channel_label, false, false);
	channel_spinner.set_activates_default ();
	channel_spinner.show ();
	toolbar.pack_start (channel_spinner, false, false);

	// Velocity spinner
	velocity_separator.show ();
	toolbar.pack_start (velocity_separator, false, false);
	velocity_label.show ();
	toolbar.pack_start (velocity_label, false, false);
	velocity_spinner.set_activates_default ();
	velocity_spinner.show ();
	toolbar.pack_start (velocity_spinner, false, false);

	// Delay spinner
	delay_separator.show ();
	toolbar.pack_start (delay_separator, false, false);
	delay_label.show ();
	toolbar.pack_start (delay_label, false, false);
	delay_spinner.set_activates_default ();
	delay_spinner.show ();
	toolbar.pack_start (delay_spinner, false, false);

	// Position spinner
	position_separator.show ();
	toolbar.pack_start (position_separator, false, false);
	position_label.show ();
	toolbar.pack_start (position_label, false, false);
	position_spinner.set_activates_default ();
	position_spinner.show ();
	toolbar.pack_start (position_spinner, false, false);

	toolbar.show ();
}

void
MidiTrackerEditor::setup_scroller ()
{
	scroller.add (view);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	scroller.show ();
}

void
MidiTrackerEditor::build_beats_per_row_menu ()
{
	using namespace Menu_Helpers;

	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv128 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv128)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv64 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv64)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv32 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv32)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv28 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv28)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv24 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv24)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv20 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv20)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv16 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv16)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv14 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv14)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv12 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv12)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv10 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv10)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv8 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv8)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv7 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv7)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv6 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv6)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv5 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv5)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv4 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv4)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv3 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv3)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv2 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv2)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeat - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeat)));
	// TODO SnapToBar is not yet supported
	// beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBar - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBar)));

	set_size_request_to_display_given_text (beats_per_row_selector, beats_per_row_strings, COMBO_TRIANGLE_WIDTH, 2);
}

void
MidiTrackerEditor::setup_tooltips ()
{
	set_tooltip (beats_per_row_selector, _("Beats per row"));
	set_tooltip (visible_note_button, _("Toggle note visibility"));
	set_tooltip (visible_channel_button, _("Toggle channel visibility"));
	set_tooltip (visible_velocity_button, _("Toggle velocity visibility"));
	set_tooltip (visible_delay_button, _("Toggle delay visibility"));
	set_tooltip (remove_note_column_button, _("Remove note column"));
	set_tooltip (add_note_column_button, _("Add note column"));
	set_tooltip (automation_button, _("MIDI Controllers and Automation"));
	octave_spinner.set_tooltip_text (_("Default octave"));
	channel_spinner.set_tooltip_text (_("Default channel"));
	velocity_spinner.set_tooltip_text (_("Default velocity"));
	delay_spinner.set_tooltip_text (_("Default delay"));
	position_spinner.set_tooltip_text (_("Position from the numerical separator changed when step editing automation. Place value = base^(position-1)"));
	steps_spinner.set_tooltip_text (_("Step size"));
}

void
MidiTrackerEditor::set_beats_per_row_to (SnapType st)
{
	unsigned int snap_ind = (int)st - (int)SnapToBeatDiv128;

	string str = beats_per_row_strings[snap_ind];

	if (str != beats_per_row_selector.get_text()) {
		beats_per_row_selector.set_text (str);
	}

	switch (st) {
	case SnapToBeatDiv128: rows_per_beat = 128; break;
	case SnapToBeatDiv64: rows_per_beat = 64; break;
	case SnapToBeatDiv32: rows_per_beat = 32; break;
	case SnapToBeatDiv28: rows_per_beat = 28; break;
	case SnapToBeatDiv24: rows_per_beat = 24; break;
	case SnapToBeatDiv20: rows_per_beat = 20; break;
	case SnapToBeatDiv16: rows_per_beat = 16; break;
	case SnapToBeatDiv14: rows_per_beat = 14; break;
	case SnapToBeatDiv12: rows_per_beat = 12; break;
	case SnapToBeatDiv10: rows_per_beat = 10; break;
	case SnapToBeatDiv8: rows_per_beat = 8; break;
	case SnapToBeatDiv7: rows_per_beat = 7; break;
	case SnapToBeatDiv6: rows_per_beat = 6; break;
	case SnapToBeatDiv5: rows_per_beat = 5; break;
	case SnapToBeatDiv4: rows_per_beat = 4; break;
	case SnapToBeatDiv3: rows_per_beat = 3; break;
	case SnapToBeatDiv2: rows_per_beat = 2; break;
	case SnapToBeat: rows_per_beat = 1; break;
	case SnapToBar: rows_per_beat = 0; break;
	default:
		/* relax */
		break;
	}

	redisplay_model ();
}

void
MidiTrackerEditor::beats_per_row_selection_done (SnapType snaptype)
{
	RefPtr<RadioAction> ract = beats_per_row_action (snaptype);
	if (ract) {
		ract->set_active ();
	}
}

RefPtr<RadioAction>
MidiTrackerEditor::beats_per_row_action (SnapType type)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (type) {
	case Editing::SnapToBeatDiv128:
		action = "beats-per-row-onetwentyeighths";
		break;
	case Editing::SnapToBeatDiv64:
		action = "beats-per-row-sixtyfourths";
		break;
	case Editing::SnapToBeatDiv32:
		action = "beats-per-row-thirtyseconds";
		break;
	case Editing::SnapToBeatDiv28:
		action = "beats-per-row-twentyeighths";
		break;
	case Editing::SnapToBeatDiv24:
		action = "beats-per-row-twentyfourths";
		break;
	case Editing::SnapToBeatDiv20:
		action = "beats-per-row-twentieths";
		break;
	case Editing::SnapToBeatDiv16:
		action = "beats-per-row-asixteenthbeat";
		break;
	case Editing::SnapToBeatDiv14:
		action = "beats-per-row-fourteenths";
		break;
	case Editing::SnapToBeatDiv12:
		action = "beats-per-row-twelfths";
		break;
	case Editing::SnapToBeatDiv10:
		action = "beats-per-row-tenths";
		break;
	case Editing::SnapToBeatDiv8:
		action = "beats-per-row-eighths";
		break;
	case Editing::SnapToBeatDiv7:
		action = "beats-per-row-sevenths";
		break;
	case Editing::SnapToBeatDiv6:
		action = "beats-per-row-sixths";
		break;
	case Editing::SnapToBeatDiv5:
		action = "beats-per-row-fifths";
		break;
	case Editing::SnapToBeatDiv4:
		action = "beats-per-row-quarters";
		break;
	case Editing::SnapToBeatDiv3:
		action = "beats-per-row-thirds";
		break;
	case Editing::SnapToBeatDiv2:
		action = "beats-per-row-halves";
		break;
	case Editing::SnapToBeat:
		action = "beats-per-row-beat";
		break;
	case Editing::SnapToBar:
		action = "beats-per-row-bar";
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible beats-per-row", (int) type) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("BeatsPerRow"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "MidiTrackerEditor::beats_per_row_chosen could not find action to match type.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

void
MidiTrackerEditor::beats_per_row_chosen (SnapType type)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = beats_per_row_action (type);

	if (ract && ract->get_active()) {
		set_beats_per_row_to (type);
	}
}

char MidiTrackerEditor::digit_to_char(int digit, int base)
{
	return num_to_string(digit, base)[0];
}

int MidiTrackerEditor::char_to_digit(char c, int base)
{
	std::string s;
	s.push_back(c);
	return string_to_num<int>(std::string(0, c));
}

std::pair<int, int>
MidiTrackerEditor::position_range (const std::string& str)
{
	size_t sepos = str.find('.'); // TODO support other locals
	int l = 0, u = 0;
	if (sepos == std::string::npos) {
		u = (int)str.size() - 1;
	} else {
		l = (int)sepos - (int)str.size() + 1;
		u = (int)sepos - 1;
	}
	return std::pair<int, int>(l, u);
}

std::string
MidiTrackerEditor::pad (const std::string& str, int position)
{
	std::pair<int, int> pr = position_range (str);
	if (position < pr.first) {
		int diff = pr.first - position;
		return str + (pr.first == 0 ? "." : "") + std::string(diff, '0');
	}
	if (pr.second < position) {
		int diff = position - pr.second;
		return std::string(diff, '0') + str;
	}
	return str;
}

size_t MidiTrackerEditor::locate (const std::string& str, int position)
{
	std::pair<int, int> pr = position_range (str);
	if (pr.first <= position and position <= pr.second) {
		if (0 <= position) {
			return pr.second - position;
		} else {
			return str.size() + pr.first - position - 1;
		}
	}
	return std::string::npos;
}
