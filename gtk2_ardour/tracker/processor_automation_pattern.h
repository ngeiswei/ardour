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

#ifndef __ardour_tracker_processor_automation_pattern_h_
#define __ardour_tracker_processor_automation_pattern_h_

#include "automation_pattern.h"
#include "processor_automation_pattern_phenomenal_diff.h" // NEXT.2: is that really useful?  Can't we simply use automation_pattern_phenomenal_diff?

namespace Tracker {

/**
 * Data structure holding the automation list pattern for a processor
 * (plugin and such).
 */
// NEXT.11: inherit from TrackAutomationPattern.  Really?  Why?  Answer:
// because TrackAutomationPattern::event2row is using event->when while
// MidiRegionAutomationPattern::event2row is using event->when.beats ().
class ProcessorAutomationPattern : public AutomationPattern {
public:
	ProcessorAutomationPattern (TrackerEditor& te,
	                            RegionPtr region,
	                            bool connect,
	                            ProcessorPtr processor);
	ProcessorAutomationPattern (TrackerEditor& te,
	                            Temporal::timepos_t position,
	                            Temporal::timepos_t start, // NEXT: really need that?
	                            Temporal::timecnt_t length,
	                            Temporal::timepos_t end,
	                            Temporal::timepos_t nt_last,
	                            bool connect,
	                            ProcessorPtr processor);

	// Make a deep copy of the automation controls
	ProcessorAutomationPattern& operator= (const ProcessorAutomationPattern& other);

	ProcessorAutomationPatternPhenomenalDiff phenomenal_diff (const ProcessorAutomationPattern& prev) const;

	// NEXT.6: overload AutomationPattern::insert

	virtual const ParameterSet& automatable_parameters () const;

	// For displaying pattern data. Mostly for debugging
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

private:
	ProcessorPtr _processor;
};

} // ~namespace tracker

#endif /* __ardour_tracker_processor_automation_pattern_h_ */
