/*
 * Copyright (C) 2018-2019 Nil Geisweiller <ngeiswei@gmail.com>
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

#ifndef __ardour_tracker_track_pattern_h_
#define __ardour_tracker_track_pattern_h_

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/region.h"
#include "ardour/track.h"

#include "base_pattern.h"
#include "automation_pattern.h"
#include "main_automation_pattern.h"
#include "processor_automation_pattern.h"
#include "track_pattern_phenomenal_diff.h"
#include "tracker_utils.h"

namespace Tracker {

class MidiTrackPattern;
class AudioTrackPattern;

// TODO: maybe implement some MultiRegionPattern class that deals with mri and
// have TrackPattern inherits that one as well.

/**
 * Abstract class to represent track patterns (midi, audio, etc).
 */
// NEXT.2: do we really want to inherit from AutomationPattern?
// Probably not, would be better to have main (fader, etc) and
// processor automation patterns.  BTW, we might want to have
// RegionAutomationPattern and TrackAutomationPattern, wait a minute!
// Is that what TrackAutomationPattern for?  Answer: Yes!
// TrackAutomationPattern is track automation, as opposed to region
// automation.  Except that now TrackAutomationPattern should actually
// be a vector of ProcessorAutomationPattern.  Need to rethink that
// carefully.
class TrackPattern : public BasePattern /* NEXT: used to inherit from AutomationPattern, but I believe it should NOT, instead BasePattern seems reasonable */ {
public:
	TrackPattern (TrackerEditor& te,
	              TrackPtr track,
	              const RegionSeq& regions,
	              bool connect);
	TrackPattern (TrackerEditor& te,
	              TrackPtr track,
	              Temporal::samplepos_t pos,
	              Temporal::samplecnt_t len,
	              Temporal::samplepos_t fst,
	              Temporal::samplepos_t lst,
	              bool connect);
	virtual ~TrackPattern ();

	virtual void setup (const RegionSeq&);

	TrackPattern& operator= (const TrackPattern& other);

	// TODO: for now do not worry about memory leaking, create a new
	// PhenomenalDiff object at every call
	TrackPatternPhenomenalDiff* phenomenal_diff_ptr (const TrackPattern* prev) const;

	MidiTrackPtr midi_track ();
	AudioTrackPtr audio_track ();

	// Self cast classes
	bool is_midi_track_pattern () const;
	bool is_audio_track_pattern () const;
	const MidiTrackPattern* midi_track_pattern () const;
	const AudioTrackPattern* audio_track_pattern () const;
	MidiTrackPattern* midi_track_pattern ();
	AudioTrackPattern* audio_track_pattern ();

	// Default implementation is for tracks not supporting regions
	// VERY VERY NEXT:
	// 1. Do not change that API, just make it work buggy like before
	// 2. Then change the API to make it work
	virtual Temporal::Beats region_relative_beats (int rowi, int mri, int delay) const;
	virtual int64_t region_relative_delay_ticks (const Temporal::Beats& event_time, int rowi, int mri) const;
	// VERY VERY NEXT:
	virtual bool is_auto_displayable (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual size_t control_events_count (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual RowToControlEventsRange control_events_range (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual Evoral::ControlEvent* get_automation_control_event (int rowi, int mri, const Evoral::Parameter& param);
	virtual const Evoral::ControlEvent* get_automation_control_event (int rowi, int mri, const Evoral::Parameter& param) const;
	virtual bool is_region_defined (int rowi) const;
	virtual int to_rrri (int rowi, int mri) const;
	virtual int to_rrri (int rowi) const;
	virtual int to_mri (int rowi) const;
	virtual AutomationListPtr get_alist_at_mri (int mri, const Evoral::Parameter& param);
	virtual const AutomationListPtr get_alist_at_mri (int mri, const Evoral::Parameter& param) const;
	virtual std::pair<double, bool> get_automation_value (int rowi, int mri, const Evoral::Parameter& param);
	virtual void set_automation_value (double val, int rowi, int mri, const Evoral::Parameter& param, int delay);
	virtual void delete_automation_value (int rowi, int mri, const Evoral::Parameter& param);
	virtual std::pair<int, bool> get_automation_delay (int rowi, int mri, const Evoral::Parameter& param);
	virtual void set_automation_delay (int delay, int rowi, int mri, const Evoral::Parameter& param);
	virtual double lower (int rowi, const Evoral::Parameter& param) const;
	virtual double upper (int rowi, const Evoral::Parameter& param) const;

	// NEXT: TEMPORARY METHODS FROM OLD API BEFORE TRANSITIONING TO NEW API
	// NEXT.9: add insert(param) method

	// For displaying pattern data. Mostly for debugging
	// NEXT.2: implement
	virtual std::string self_to_string () const;
	virtual std::string to_string (const std::string& indent = std::string ()) const;

	TrackPtr track;
	// NEXT.8
// 	../gtk2_ardour/tracker/track_pattern.h:120:31: error: cannot declare field ‘Tracker::TrackPattern::main_automation_pattern’ to be of abstract type ‘Tracker::MainAutomationPattern’
//   120 |         MainAutomationPattern main_automation_pattern;
//       |                               ^~~~~~~~~~~~~~~~~~~~~~~
// In file included from ../gtk2_ardour/tracker/track_pattern.h:29,
//                  from ../gtk2_ardour/tracker/track_automation_pattern.h:24,
//                  from ../gtk2_ardour/tracker/audio_track_pattern.h:25,
//                  from ../gtk2_ardour/tracker/audio_track_toolbar.h:22,
//                  from ../gtk2_ardour/tracker/tracker_editor.h:40,
//                  from ../gtk2_ardour/editor.h:80,
//                  from ../gtk2_ardour/editor_audiotrack.cc:25:
// ../gtk2_ardour/tracker/main_automation_pattern.h:33:7: note:   because the following virtual functions are pure within ‘Tracker::MainAutomationPattern’:
//    33 | class MainAutomationPattern : public AutomationPattern {
//       |       ^~~~~~~~~~~~~~~~~~~~~
// In file included from ../gtk2_ardour/tracker/track_pattern.h:28,
//                  from ../gtk2_ardour/tracker/track_automation_pattern.h:24,
//                  from ../gtk2_ardour/tracker/audio_track_pattern.h:25,
//                  from ../gtk2_ardour/tracker/audio_track_toolbar.h:22,
//                  from ../gtk2_ardour/tracker/tracker_editor.h:40,
//                  from ../gtk2_ardour/editor.h:80,
//                  from ../gtk2_ardour/editor_audiotrack.cc:25:
// ../gtk2_ardour/tracker/automation_pattern.h:91:22: note:     ‘virtual void Tracker::AutomationPattern::insert(const Evoral::Parameter&)’
//    91 |         virtual void insert (const Evoral::Parameter& param) = 0;
//       |                      ^~~~~~
// In file included from /usr/include/glib-2.0/glib/gthread.h:32,
//                  from /usr/include/glib-2.0/glib/gasyncqueue.h:32,
//                  from /usr/include/glib-2.0/glib.h:32,
//                  from /usr/include/glibmm-2.4/glibmm/threads.h:30,
//                  from ../libs/pbd/pbd/id.h:28,
//                  from ../libs/ardour/ardour/types.h:52,
//                  from ../libs/ardour/ardour/rc_configuration.h:30,
//                  from ../gtk2_ardour/editor_audiotrack.cc:21:
// ../libs/ardour/ardour/pannable.h: In member function ‘bool ARDOUR::Pannable::touching() const’:

	// NEXT.9: For now we use AutomationPattern for a smooth transition between
	// the old way when TrackPattern inherited from AutomationPattern and the
	// new way using MainAutomationPattern and vector of
	// ProcessorAutomationPattern as attributes.
	//
	// ARGGGGG: AutomationPattern is abstract, so I suppose we should use
	// MainAutomationPattern instead.
	// AutomationPattern automation_pattern;

	MainAutomationPattern main_automation_pattern;
	// std::vector<ProcessorAutomationPattern> processor_automation_patterns; // NEXT.2:
	// 																							  // likewise
	// 																							  // ProcessorAutomationPattern
	// 																							  // could
	// 																							  // inherit
	// 																							  // from
	// 																							  // a
	// 																							  // TrackAutomationPattern
};

} // ~namespace Tracker

#endif /* __ardour_tracker_track_pattern_h_ */
