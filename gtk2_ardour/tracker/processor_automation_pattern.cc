/*
 * Copyright (C) 2021 Nil Geisweiller <ngeiswei@gmail.com>
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

#include "processor_automation_pattern.h"

using namespace Tracker;

////////////////////////////////
// ProcessorAutomationPattern //
////////////////////////////////

ProcessorAutomationPattern::ProcessorAutomationPattern (TrackerEditor& te,
                                                        TrackPtr trk,
                                                        Temporal::timepos_t pos,
                                                        Temporal::timecnt_t len,
                                                        Temporal::timepos_t ed,
                                                        Temporal::timepos_t ntl,
                                                        bool connect,
                                                        ProcessorPtr processor)
	: TrackAutomationPattern (te, trk, pos, len, ed, ntl, connect)
	, _processor (processor)
{
	setup_processor_automation_control ();
}

void
ProcessorAutomationPattern::insert(const Evoral::Parameter& param)
{
	insert_actl (std::dynamic_pointer_cast<ARDOUR::AutomationControl> (_processor->control (param)), _processor->describe_parameter (param));
}

void
ProcessorAutomationPattern::setup_processor_automation_control ()
{
	const ParameterSet& automatable = _processor->what_can_be_automated ();
	for (const Evoral::Parameter& param : automatable) {
		insert (param);
	}
}

const PBD::ID&
ProcessorAutomationPattern::id () const
{
	return _processor->id ();
}

std::string
ProcessorAutomationPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "ProcessorAutomationPattern[" << this << "]";
	return ss.str ();
}

std::string
ProcessorAutomationPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << TrackAutomationPattern::to_string (indent) << std::endl;
	std::string header = indent + self_to_string () + " ";
	ss << header << "processor = " << _processor;
	return ss.str ();
}
