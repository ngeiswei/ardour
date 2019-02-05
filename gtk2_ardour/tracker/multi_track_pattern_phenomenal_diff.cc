/*
    Copyright (C) 2019 Nil Geisweiller

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

#include "multi_track_pattern_phenomenal_diff.h"

using namespace Tracker;

bool
MultiTrackPatternPhenomenalDiff::empty() const
{
	return !full && mti2tp_diff.empty();
}

std::string
MultiTrackPatternPhenomenalDiff::to_string(const std::string& indent) const
{
	std::stringstream ss;
	ss << BasePhenomenalDiff::to_string(indent) << std::endl;
	ss << indent << "mti2tp_diff:" << std::endl;
	for (Mti2TrackPatternDiff::const_iterator it = mti2tp_diff.begin(); it != mti2tp_diff.end(); it++) {
		size_t mti = it->first;
		TrackPatternPhenomenalDiff* tp_diff = it->second;
		MidiTrackPatternPhenomenalDiff* mtp_diff = dynamic_cast<MidiTrackPatternPhenomenalDiff*>(tp_diff);
		AudioTrackPatternPhenomenalDiff* atp_diff = dynamic_cast<AudioTrackPatternPhenomenalDiff*>(tp_diff);
		ss << indent + "  " << "track_pattern_diff[" << mti << "]:" << std::endl;
		ss << indent + "    " << "tp_diff = " << tp_diff << std::endl;
		ss << indent + "    " << "mtp_diff = " << mtp_diff << std::endl;
		ss << indent + "    " << "atp_diff = " << atp_diff << std::endl;
		// ss << indent + "  " << "track_pattern_diff[" << it->first << "]:" << std::endl;
		// if () {
		// 	ss << std::dynamic_cast<MidiTrackPatternPhenomenalDiff>(it->second)->to_string(indent + "    ");
		// } else if (tps[mti]->is_audio_track_pattern ()) {
		// 	ss << std::static_cast<MidiTrackPattern>(it->second)->to_string(indent + "    ");
		// } else {
		// 	std::cout << "Not implemented" << std::endl;
		// }
	}
	return ss.str();
}
