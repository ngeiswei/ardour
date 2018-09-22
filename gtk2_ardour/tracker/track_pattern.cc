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

#include "tracker_utils.h"
#include "track_pattern.h"

using namespace Tracker;

TrackPattern::TrackPattern (TrackerEditor& te,
                            boost::shared_ptr<ARDOUR::Track> trk,
                            const std::vector<boost::shared_ptr<ARDOUR::Region> >& regions)
	: AutomationPattern(te,
	                    TrackerUtils::get_position(regions),
	                    0,
	                    TrackerUtils::get_length(regions),
	                    TrackerUtils::get_first_sample(regions),
	                    TrackerUtils::get_last_sample(regions))
	, track(trk)
{
}

TrackPattern::TrackPattern (TrackerEditor& te,
                            boost::shared_ptr<ARDOUR::Track> trk,
                            Temporal::samplepos_t pos,
                            Temporal::samplecnt_t len,
                            Temporal::samplepos_t fst,
                            Temporal::samplepos_t lst)
	: AutomationPattern(te, pos, 0, len, fst, lst)
	, track(trk)
{
}

TrackPattern::~TrackPattern ()
{
}
