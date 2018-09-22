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

#ifndef __ardour_tracker_track_pattern_h_
#define __ardour_tracker_track_pattern_h_

#include "ardour/track.h"
#include "ardour/region.h"

#include "automation_pattern.h"

namespace Tracker {

/**
 * Abstract class to represent track patterns (midi, audio, etc).
 */
class TrackPattern : public AutomationPattern {
public:
	TrackPattern (TrackerEditor& te,
	              boost::shared_ptr<ARDOUR::Track> track,
	              const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions);
	TrackPattern (TrackerEditor& te,
	              boost::shared_ptr<ARDOUR::Track> track,
	              Temporal::samplepos_t pos,
	              Temporal::samplecnt_t len,
	              Temporal::samplepos_t fst,
	              Temporal::samplepos_t lst);
	virtual ~TrackPattern ();

	boost::shared_ptr<ARDOUR::Track> track;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_midi_track_pattern_h_ */
