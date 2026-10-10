/*
 * Copyright (C) 2024 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_tracker_context_h_
#define __ardour_tracker_tracker_context_h_

#include <string>

#include "pbd/id.h"

#include "tracker_utils.h"

namespace ARDOUR {
	class Session;
}

namespace PBD {
	class Command;
}

namespace Tracker {

/**
 * Abstract interface through which the tracker model (Pattern and friends)
 * talks back to the application (currently the GTK TrackerEditor).
 *
 * This is what allows the model (libtracker) to stay free of any UI / GTK
 * dependency, while still being able to:
 *  - get the session,
 *  - subscribe to model signals (the implementation marshals those to the
 *    appropriate thread / main loop context),
 *  - record undo history, and
 *  - tell the UI that some structural aspect changed (e.g. track headers
 *    need to be rebuilt).
 */
class TrackerContext
{
public:
	virtual ~TrackerContext () {}

	/** Return the session the tracker operates on. */
	virtual ARDOUR::Session* get_session () const = 0;

	/** Subscribe to a track's changes. */
	virtual void connect_track (TrackPtr track) = 0;

	/** Subscribe to a MIDI region's changes. */
	virtual void connect_midi_region (MidiRegionPtr midi_region) = 0;

	/** Subscribe to an automation control's changes. */
	virtual void connect_automation (AutomationControlPtr actl) = 0;

	/** Undo history plumbing, forwarded to the edit (public) editor. */
	virtual void begin_reversible_command (const std::string& name) = 0;
	virtual void commit_reversible_command () = 0;
	virtual void add_command (PBD::Command* cmd) = 0;
	virtual void set_dirty () = 0;

	/**
	 * Called after a pattern update when tracks may have been added,
	 * removed or (re)enabled, so that the UI can refresh its track headers.
	 */
	virtual void track_headers_changed () = 0;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_tracker_context_h_ */
