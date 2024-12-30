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

#ifndef __ardour_tracker_track_toolbar_h_
#define __ardour_tracker_track_toolbar_h_

#include <gtkmm/box.h>
#include <gtkmm/separator.h>

#include "widgets/ardour_button.h"

#include "ardour/midi_playlist.h"
#include "ardour/pannable.h"
#include "ardour/processor.h"

#include "midi_track_pattern.h"

namespace Tracker {

class TrackerEditor;
class Grid;
class MidiTrackToolbar;
class AudioTrackToolbar;

struct ProcessorAutomationNode {
	ProcessorAutomationNode (ProcessorPtr proc, Evoral::Parameter param, Gtk::CheckMenuItem* mitem);
	~ProcessorAutomationNode ();

	// For debugging
	std::string to_string (const std::string& indent="") const;

	ProcessorPtr processor;
	Evoral::Parameter parameter;
	Gtk::CheckMenuItem* menu_item;

	// Corresponding column index. If set to 0 then undetermined yet
	int column;
};
typedef std::vector<ProcessorAutomationNode*> ProcessorAutomationNodeSeq;
typedef ProcessorAutomationNodeSeq::iterator ProcessorAutomationNodeSeqIt;
typedef ProcessorAutomationNodeSeq::const_iterator ProcessorAutomationNodeSeqConstIt;

struct ProcessorAutomationInfo {
	// explicit ProcessorAutomationInfo (ProcessorPtr i, const TrackPattern* tp);
	explicit ProcessorAutomationInfo (ProcessorPtr i);

	std::string to_string (const std::string& indent="") const;

	ProcessorPtr processor;
	Gtk::Menu* menu;
	ProcessorAutomationNodeSeq columns; // TODO: why is it called columns?
};
typedef std::list<ProcessorAutomationInfo*> ProcessorAutomationInfoSeq;
typedef ProcessorAutomationInfoSeq::iterator ProcessorAutomationInfoSeqIt;
typedef ProcessorAutomationInfoSeq::const_iterator ProcessorAutomationInfoSeqConstIt;

class TrackToolbar : public Gtk::HBox
{
public:
	TrackToolbar (TrackerEditor& te, TrackPattern* tp, int mti);
	~TrackToolbar ();

	typedef std::map<Evoral::Parameter, Gtk::CheckMenuItem*> ParameterMenuMap;

	void setup ();

	/**
	 * Build the toolbar layout
	 */
	void setup_label ();
	void setup_delay ();
	void setup_automation ();
	void setup_tooltips ();

	/**
	 * Triggered upon pressing the various buttons.
	 */
	bool visible_delay_press (GdkEventButton*);
	void automation_click ();

	/**
	 * Helpers for building automation menu.
	 */
	void build_automation_menu ();
	void build_main_automation_menu (Gtk::Menu_Helpers::MenuList& items);
	virtual void build_show_hide_automations (Gtk::Menu_Helpers::MenuList& items);
	void setup_processor_menu_and_curves ();
	void add_processor_to_subplugin_menu (std::weak_ptr<ARDOUR::Processor>);
	void processor_menu_item_toggled (ProcessorAutomationInfo*, ProcessorAutomationNode*);
	ProcessorAutomationInfoSeqIt find_processor_automation_info (ProcessorPtr processor);
	ProcessorAutomationInfoSeqConstIt find_processor_automation_info (ProcessorPtr processor) const;
	ProcessorAutomationNode* find_processor_automation_node (ProcessorPtr processor, Evoral::Parameter what);

	/**
	 * Helpers to update automations
	 */
	void show_all_automation ();
	void show_existing_automation ();
	void hide_all_automation ();
	// Show/hide gain, trim, mute and pan
	void show_all_main_automations ();
	void show_existing_main_automations ();
	void hide_main_automations ();
	// Show/hide processor automations
	void show_all_processor_automations ();
	void show_existing_processor_automations ();
	void hide_processor_automations ();

	/**
	 * Helpers to update buttons status display
	 */
	void update_visible_delay_button ();
	void update_automation_button ();

	bool is_midi_track_toolbar () const;
	bool is_audio_track_toolbar () const;
	const MidiTrackToolbar* midi_track_toolbar () const;
	MidiTrackToolbar* midi_track_toolbar ();
	const AudioTrackToolbar* audio_track_toolbar () const;
	AudioTrackToolbar* audio_track_toolbar ();

	virtual int get_min_width () const;

	void redisplay_grid ();

	TrackerEditor& tracker_editor;
	TrackPtr track;
	TrackPattern* track_pattern;
	size_t track_index;
	Grid& grid;

	int spacing;

	ArdourWidgets::ArdourButton  visible_delay_button;
	bool                         visible_delay;
	Gtk::VSeparator              automation_separator;
	ArdourWidgets::ArdourButton  automation_button;
	Gtk::Menu                    subplugin_menu;
	Gtk::Menu*                   automation_action_menu;
	Gtk::Menu*                   controller_menu;

	/**
	 * Information about all automatable processor parameters that apply to
	 * this route (midi track). The Amp processor is not included in this list.
	 */
	ProcessorAutomationInfoSeq processor_automations;

	/** parameter -> menu item map for the plugin automation menu */
	ParameterMenuMap _subplugin_menu_map;
	/** parameter -> menu item map for the controller menu */
	ParameterMenuMap _controller_menu_map;

	Gtk::CheckMenuItem* gain_automation_item;
	Gtk::CheckMenuItem* trim_automation_item;
	Gtk::CheckMenuItem* mute_automation_item;
	Gtk::CheckMenuItem* pan_automation_item;
};

} // ~namespace tracker

#endif /* __ardour_tracker_track_toolbar_h_ */
