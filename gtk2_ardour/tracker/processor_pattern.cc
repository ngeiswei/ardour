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

#include "processor_pattern.h"

using namespace Tracker;

//////////////////////
// ProcessorPattern //
//////////////////////

ProcessorPattern::ProcessorPattern (TrackerEditor& te,
                                    RegionPtr region,
                                    bool connect,
                                    ProcessorPtr processor)
	: AutomationPattern (te, region, connect)
	, _processor (processor)
{
}

ProcessorPattern::ProcessorPattern (TrackerEditor& te,
                                    Temporal::samplepos_t pos,
                                    Temporal::samplepos_t sta,
                                    Temporal::samplecnt_t len,
                                    Temporal::samplepos_t fir,
                                    Temporal::samplepos_t las,
                                    bool connect,
                                    ProcessorPtr processor)
	: AutomationPattern (te, pos, sta, len, fir, las, connect)
	, _processor(processor)
{
}

ProcessorPattern&
ProcessorPattern::operator= (const ProcessorPattern& other)
{
	AutomationPattern::operator= (other);
	return *this;
}

ProcessorPatternPhenomenalDiff
ProcessorPattern::phenomenal_diff (const ProcessorPattern& prev) const
{
	// NEXT: fix compile error:
	//
	// could not convert ‘Tracker::AutomationPattern::phenomenal_diff(const Tracker::AutomationPattern&) const(prev.Tracker::ProcessorPattern::<anonymous>)’ from ‘Tracker::AutomationPatternPhenomenalDiff’ to ‘Tracker::ProcessorPatternPhenomenalDiff’
	//
	// return AutomationPattern::phenomenal_diff (prev);
	return ProcessorPatternPhenomenalDiff();
}

std::string
ProcessorPattern::self_to_string () const
{
	std::stringstream ss;
	ss << "ProcessorPattern[" << this << "]";
	return ss.str ();
}

std::string
ProcessorPattern::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << AutomationPattern::to_string (indent) << std::endl;
	std::string header = indent + self_to_string () + " ";
	ss << header << "processor = " << _processor;
	return ss.str ();
}
