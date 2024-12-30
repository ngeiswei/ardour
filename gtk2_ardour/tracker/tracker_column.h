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

#ifndef __ardour_tracker_tracker_column_h_
#define __ardour_tracker_tracker_column_h_

#include <gtkmm/treemodelcolumn.h>
#include <gtkmm/treeviewcolumn.h>

namespace Tracker {

class TrackerColumn : public Gtk::TreeViewColumn
{
public:

	// TODO: merge in the same enum, maybe

	// Type of data the midi note column is holding. It also serve to calculate
	// the column index as well.
	enum class NoteType {
		NOTE,
		CHANNEL,
		VELOCITY,
		DELAY,
		SEPARATOR
	};

	// Type of data the automation column is hold. It also serve to calculate
	// the column index as well.
	enum class AutomationType {
		AUTOMATION,
		AUTOMATION_DELAY,
		AUTOMATION_SEPARATOR
	};

	TrackerColumn (const Glib::ustring& title,
	              const Gtk::TreeModelColumn<std::string>& column,
	              int mti, int cgi,
	              NoteType mnt, AutomationType at);

	int mti; // midi track index
	int cgi; // either note or automation column group index
	const NoteType note_type; // NOTE_SEPARATOR means inactive
	const AutomationType automation_type; // AUTOMATION_SEPARATOR means inactive

	bool is_note_type () const;
	bool is_automation_type () const;
};

class NoteColumn : public TrackerColumn
{
public:
	NoteColumn (const Gtk::TreeModelColumn<std::string>& column, int mti, int cgi);
};

class ChannelColumn : public TrackerColumn
{
public:
	ChannelColumn (const Gtk::TreeModelColumn<std::string>& column, int mti, int cgi);
};

class VelocityColumn : public TrackerColumn
{
public:
	VelocityColumn (const Gtk::TreeModelColumn<std::string>& column, int mti, int cgi);
};

class DelayColumn : public TrackerColumn
{
public:
	DelayColumn (const Gtk::TreeModelColumn<std::string>& column, int mti, int cgi);
};

class AutomationColumn : public TrackerColumn
{
public:
	AutomationColumn (const Gtk::TreeModelColumn<std::string>& column, int mti, int cgi);
};

class AutomationDelayColumn : public TrackerColumn
{
public:
	AutomationDelayColumn (const Gtk::TreeModelColumn<std::string>& column, int mti, int cgi);
};

} // ~namespace Tracker

#endif /* __ardour_tracker_tracker_column_h_ */
