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
#include "ardour/midi_track.h"
#include "ardour/audio_track.h"
#include "ardour/region.h"

#include "automation_pattern.h"
#include "base_phenomenal_diff.h"

namespace Tracker {

class MidiTrackPattern;
class AudioTrackPattern;

// TODO: maybe implement some MultiRegionPattern class that deals with mri and
// have TrackPattern inherits that one as well.

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

	TrackPattern& operator=(const TrackPattern& other);

	struct PhenomenalDiff : public BasePhenomenalDiff
	{
	};
	// VT: for now do not worry about memory leaking, create a new
	// PhenomenalDiff object at every call
	PhenomenalDiff* phenomenal_diff_ptr(const TrackPattern* prev) const;
	
	boost::shared_ptr<ARDOUR::MidiTrack> midi_track ();
	boost::shared_ptr<ARDOUR::AudioTrack> audio_track ();

	// Self cast classes
	bool is_midi_track_pattern () const;
	bool is_audio_track_pattern () const;
	const MidiTrackPattern* midi_track_pattern () const;
	const AudioTrackPattern* audio_track_pattern () const;
	MidiTrackPattern* midi_track_pattern ();
	AudioTrackPattern* audio_track_pattern ();

	// Default implementation is for tracks not supporting regions
	virtual Temporal::Beats	region_relative_beats (uint32_t rowi, size_t mri, int32_t delay) const;
	virtual int64_t region_relative_delay_ticks (const Temporal::Beats& event_time, uint32_t rowi, size_t mri) const;
	virtual bool is_auto_displayable (uint32_t rowi, size_t mri, const Evoral::Parameter& param) const;
	virtual size_t get_automation_list_count (uint32_t rowi, size_t mri, const Evoral::Parameter& param) const;
	virtual Evoral::ControlEvent* get_automation_control_event (uint32_t rowi, size_t mri, const Evoral::Parameter& param) const;
	virtual bool is_region_defined (int rowi) const;
	virtual int to_rrri (uint32_t rowi, size_t mri) const;
	virtual int to_rrri (uint32_t rowi) const;
	virtual int to_mri (uint32_t rowi) const;
	virtual boost::shared_ptr<ARDOUR::AutomationList> get_alist_at_mri (int mri, const Evoral::Parameter& param);
	virtual const boost::shared_ptr<ARDOUR::AutomationList> get_alist_at_mri (int mri, const Evoral::Parameter& param) const;
	virtual std::pair<double, bool> get_automation_value (size_t rowi, size_t mri, const Evoral::Parameter& param);
	virtual void set_automation_value (double val, size_t rowi, size_t mri, const Evoral::Parameter& param, int delay);
	virtual void delete_automation_value (size_t rowi, size_t mri, const Evoral::Parameter& param);
	virtual std::pair<int, bool> get_automation_delay (size_t rowi, size_t mri, const Evoral::Parameter& param);
	virtual void set_automation_delay (int delay, size_t rowi, size_t mri, const Evoral::Parameter& param);

	boost::shared_ptr<ARDOUR::Track> track;
};

} // ~namespace Tracker

#endif /* __ardour_tracker_midi_track_pattern_h_ */
