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
                                                        RegionPtr region,
                                                        bool connect,
                                                        ProcessorPtr processor)
	: AutomationPattern (te, region, connect)
	, _processor (processor)
{
}

ProcessorAutomationPattern::ProcessorAutomationPattern (TrackerEditor& te,
                                                        Temporal::timepos_t pos,
                                                        Temporal::timepos_t sta, // NEXT: really need that?
                                                        Temporal::timecnt_t len,
                                                        Temporal::timepos_t ed,
                                                        Temporal::timepos_t ntl,
                                                        bool connect,
                                                        ProcessorPtr processor)
	: AutomationPattern (te, pos, sta, len, end, ntl, connect)
	, _processor(processor)
{
}

ProcessorAutomationPattern&
ProcessorAutomationPattern::operator= (const ProcessorAutomationPattern& other)
{
	AutomationPattern::operator= (other);
	return *this;
}

ProcessorAutomationPatternPhenomenalDiff
ProcessorAutomationPattern::phenomenal_diff (const ProcessorAutomationPattern& prev) const
{
	// NEXT: fix compile error:
	//
	// could not convert ‘Tracker::AutomationPattern::phenomenal_diff(const Tracker::AutomationPattern&) const(prev.Tracker::ProcessorAutomationPattern::<anonymous>)’ from ‘Tracker::AutomationPatternPhenomenalDiff’ to ‘Tracker::ProcessorAutomationPatternPhenomenalDiff’
	//
	// return AutomationPattern::phenomenal_diff (prev);
	return ProcessorAutomationPatternPhenomenalDiff();
}

const ParameterSet&
ProcessorAutomationPattern::automatable_parameters () const
{
	// NEXT.6
	return ParameterSet();
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
	ss << AutomationPattern::to_string (indent) << std::endl;
	std::string header = indent + self_to_string () + " ";
	ss << header << "processor = " << _processor;
	return ss.str ();
}
