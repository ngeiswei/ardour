/*
 * Copyright (C) 2023 Nil Geisweiller <ngeiswei@gmail.com>
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

#include <sstream>

#include "track_all_automations_pattern_phenomenal_diff.h"

using namespace Tracker;

bool
TrackAllAutomationsPatternPhenomenalDiff::empty () const
{
	if (!main_automation_pattern_phenomenal_diff.empty ()) {
		return false;
	}

	for (const auto& id_appd : id_to_processor_automation_pattern_phenomenal_diff) {
		if (!id_appd.second.empty ()) {
			return false;
		}
	}

	return true;
}

std::string
TrackAllAutomationsPatternPhenomenalDiff::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << indent << "main_automation_pattern_phenomenal_diff:" << std::endl;
	ss << main_automation_pattern_phenomenal_diff.to_string(indent + "  ");
	for (const auto& id_papd : id_to_processor_automation_pattern_phenomenal_diff) {
		ss << indent << "id_to_processor_automation_pattern_phenomenal_diff[" << id_papd.first << "]:" << std::endl;
		ss << id_papd.second.to_string(indent + "  ") << std::endl;
	}
	return ss.str ();
}
