/*
 * Copyright (C) 2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#include "pbd/i18n.h"

#include "tracker_column.h"

using namespace std;
using namespace Gtk;
using namespace Tracker;

TrackerColumn::TrackerColumn(const Glib::ustring& title,
                             const TreeModelColumn<string>& column,
                             int mti, int cgi,
                             midi_note_type mnt, automation_type at)
	: TreeViewColumn(title, column),
	  midi_track_idx(mti), col_group_idx(cgi),
	  note_type(mnt), auto_type(at) {}

NoteColumn::NoteColumn(const TreeModelColumn<string>& column, int mti, int cgi)
	: TrackerColumn(_("Note"), column, mti, cgi, NOTE, AUTOMATION_SEPARATOR) {}

ChannelColumn::ChannelColumn(const TreeModelColumn<string>& column, int mti, int cgi)
	: TrackerColumn(S_("Channel|Ch"), column, mti, cgi, CHANNEL, AUTOMATION_SEPARATOR) {}

VelocityColumn::VelocityColumn(const TreeModelColumn<string>& column, int mti, int cgi)
	: TrackerColumn(S_("Velocity|Vel"), column, mti, cgi, VELOCITY, AUTOMATION_SEPARATOR) {}

DelayColumn::DelayColumn(const TreeModelColumn<string>& column, int mti, int cgi)
	: TrackerColumn(_("Delay"), column, mti, cgi, DELAY, AUTOMATION_SEPARATOR) {}

AutomationColumn::AutomationColumn(const TreeModelColumn<string>& column, int mti, int cgi)
	: TrackerColumn("", column, mti, cgi, SEPARATOR, AUTOMATION) {}

AutomationDelayColumn::AutomationDelayColumn(const TreeModelColumn<string>& column, int mti, int cgi)
	: TrackerColumn(_("Delay"), column, mti, cgi, SEPARATOR, AUTOMATION_DELAY) {}
