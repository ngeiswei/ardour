/*
 * Unit tests for ParameterDescriptor::integer_step lookup and dispatch through
 * the tracker pattern hierarchy.
 */

#include <list>
#include <memory>

#include "pbd/properties.h"

#include "ardour/amp.h"
#include "ardour/audio_track.h"
#include "ardour/automation_control.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/source_factory.h"
#include "ardour/test/test_util.h"

#include "automation_pattern.h"
#include "audio_track_pattern.h"
#include "midi_track_pattern.h"
#include "pattern.h"
#include "processor_automation_pattern.h"
#include "track_all_automations_pattern.h"
#include "tracker_context.h"

#include "AutomationIntegerTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (AutomationIntegerTest);

using namespace ARDOUR;
using namespace Tracker;

namespace {

class SessionTrackerContext : public TrackerContext
{
public:
	SessionTrackerContext (ARDOUR::Session* session)
		: _session (session)
	{
	}

	ARDOUR::Session* get_session () const override { return _session; }
	void connect_track (TrackPtr) override {}
	void connect_midi_region (MidiRegionPtr) override {}
	void connect_automation (AutomationControlPtr) override {}
	void begin_reversible_command (const std::string&) override {}
	void commit_reversible_command () override {}
	void add_command (PBD::Command*) override {}
	void set_dirty () override {}
	void begin_bulk_edit (const std::string&) override {}
	void end_bulk_edit () override {}
	bool bulk_edit_in_progress () const override { return false; }
	void track_headers_changed () override {}

private:
	ARDOUR::Session* _session;
};

class TestAutomationPattern : public AutomationPattern
{
public:
	TestAutomationPattern (TrackerContext& context)
		: AutomationPattern (context,
		                     Temporal::timepos_t (Temporal::Beats ()),
		                     Temporal::timepos_t (Temporal::Beats ()),
		                     Temporal::timecnt_t (Temporal::Beats::beats (16)),
		                     Temporal::timepos_t (Temporal::Beats::beats (16)),
		                     Temporal::timepos_t (Temporal::Beats::beats (16)),
		                     false)
	{
	}

	void insert (const Evoral::Parameter&) override {}
};

} // anonymous namespace

AutomationIntegerTest::AutomationIntegerTest ()
	: _context (nullptr)
	, _integer_param (PluginAutomation, 0, 1000)
	, _float_param (PluginAutomation, 0, 1001)
	, _midi_param (MidiCCAutomation, 0, 64)
	, _missing_param (PluginAutomation, 0, 1002)
{
}

void
AutomationIntegerTest::setUp ()
{
	TestNeedingSession::setUp ();
	_context = new SessionTrackerContext (_session);
}

void
AutomationIntegerTest::tearDown ()
{
	_midi_region.reset ();
	_midi_track.reset ();
	_audio_track.reset ();
	delete _context;
	_context = nullptr;
	TestNeedingSession::tearDown ();
}

AutomationControlPtr
AutomationIntegerTest::make_control (const Evoral::Parameter& param, bool integer)
{
	ARDOUR::ParameterDescriptor desc (param);
	desc.integer_step = integer;
	return std::make_shared<ARDOUR::AutomationControl> (*_session, param, desc);
}

AudioTrackPtr
AutomationIntegerTest::audio_track ()
{
	if (!_audio_track) {
		AudioTrackList tracks = _session->new_audio_track (
			1, 2, std::shared_ptr<RouteGroup> (), 1, "audio",
			PresentationInfo::max_order, Normal, false);
		CPPUNIT_ASSERT (!tracks.empty ());
		_audio_track = tracks.front ();
	}
	return _audio_track;
}

MidiTrackPtr
AutomationIntegerTest::midi_track ()
{
	if (!_midi_track) {
		const ChanCount midi_io (DataType::MIDI, 1);
		std::list<MidiTrackPtr> tracks = _session->new_midi_track (
			midi_io, midi_io, true, std::shared_ptr<PluginInfo> (), nullptr,
			std::shared_ptr<RouteGroup> (), 1, "midi",
			PresentationInfo::max_order, Normal, false);
		CPPUNIT_ASSERT (!tracks.empty ());
		_midi_track = tracks.front ();
	}
	return _midi_track;
}

MidiRegionPtr
AutomationIntegerTest::midi_region ()
{
	if (!_midi_region) {
		const std::string path = _session->new_midi_source_path ("integer-test");
		std::shared_ptr<Source> source = SourceFactory::createWritable (
			DataType::MIDI, *_session, path, _session->sample_rate (), false, false);
		CPPUNIT_ASSERT (source);

		PBD::PropertyList properties;
		properties.add (Properties::start, Temporal::timepos_t (Temporal::Beats ()));
		properties.add (Properties::length, Temporal::timecnt_t (Temporal::Beats::beats (16)));
		properties.add (Properties::name, "integer-test");
		_midi_region = std::dynamic_pointer_cast<MidiRegion> (
			RegionFactory::create (source, properties, false));
		CPPUNIT_ASSERT (_midi_region);
	}
	return _midi_region;
}

void
AutomationIntegerTest::testAutomationPatternDescriptor ()
{
	TestAutomationPattern pattern (*_context);
	pattern.insert_actl (make_control (_integer_param, true), "integer");
	pattern.insert_actl (make_control (_float_param, false), "float");

	CPPUNIT_ASSERT (pattern.is_integer (_integer_param));
	CPPUNIT_ASSERT (!pattern.is_integer (_float_param));
	CPPUNIT_ASSERT (!pattern.is_integer (_missing_param));
}

void
AutomationIntegerTest::testTrackAllAutomationsDescriptor ()
{
	TrackAllAutomationsPattern pattern (
		*_context, audio_track (), Temporal::timepos_t (Temporal::Beats ()),
		Temporal::timecnt_t (Temporal::Beats::beats (16)),
		Temporal::timepos_t (Temporal::Beats::beats (16)),
		Temporal::timepos_t (Temporal::Beats::beats (16)), false);

	pattern.main_automation_pattern.insert_actl (
		make_control (_integer_param, true), "integer");
	pattern.main_automation_pattern.insert_actl (
		make_control (_float_param, false), "float");

	const IDParameter main_integer (PBD::ID (0), _integer_param);
	const IDParameter main_float (PBD::ID (0), _float_param);
	CPPUNIT_ASSERT (pattern.is_integer (main_integer));
	CPPUNIT_ASSERT (!pattern.is_integer (main_float));

	ProcessorPtr processor = audio_track ()->amp ();
	CPPUNIT_ASSERT (processor);
	auto processor_it = pattern.id_to_processor_automation_pattern.find (processor->id ());
	CPPUNIT_ASSERT (processor_it != pattern.id_to_processor_automation_pattern.end ());
	processor_it->second->insert_actl (make_control (_float_param, false), "float-processor");
	processor_it->second->insert_actl (make_control (_integer_param, true), "integer-processor");

	const IDParameter processor_integer (processor->id (), _integer_param);
	const IDParameter processor_float (processor->id (), _float_param);
	CPPUNIT_ASSERT (pattern.is_integer (processor_integer));
	CPPUNIT_ASSERT (!pattern.is_integer (processor_float));
	CPPUNIT_ASSERT (!pattern.is_integer (IDParameter (processor->id (), _missing_param)));
}

void
AutomationIntegerTest::testTrackPatternDescriptor ()
{
	const RegionSeq regions;
	AudioTrackPattern pattern (
		*_context, audio_track (), regions, Temporal::timepos_t (Temporal::Beats ()),
		Temporal::timecnt_t (Temporal::Beats::beats (16)),
		Temporal::timepos_t (Temporal::Beats::beats (16)),
		Temporal::timepos_t (Temporal::Beats::beats (16)), false);

	pattern.track_all_automations_pattern.main_automation_pattern.insert_actl (
		make_control (_integer_param, true), "integer");
	pattern.track_all_automations_pattern.main_automation_pattern.insert_actl (
		make_control (_float_param, false), "float");

	CPPUNIT_ASSERT (pattern.is_integer (-1, IDParameter (PBD::ID (0), _integer_param)));
	CPPUNIT_ASSERT (!pattern.is_integer (-1, IDParameter (PBD::ID (0), _float_param)));
	CPPUNIT_ASSERT (!pattern.is_integer (-1, IDParameter (PBD::ID (0), _missing_param)));
}

void
AutomationIntegerTest::testMidiTrackPatternDescriptor ()
{
	RegionSeq regions;
	regions.push_back (midi_region ());
	MidiTrackPattern pattern (
		*_context, midi_track (), regions, Temporal::timepos_t (Temporal::Beats ()),
		Temporal::timecnt_t (Temporal::Beats::beats (16)),
		Temporal::timepos_t (Temporal::Beats::beats (16)),
		Temporal::timepos_t (Temporal::Beats::beats (16)), false);

	const IDParameter midi_id (PBD::ID (0), _midi_param);
	pattern.insert (midi_id);
	CPPUNIT_ASSERT (pattern.is_integer (0, midi_id));
	CPPUNIT_ASSERT (!pattern.is_integer (0, IDParameter (PBD::ID (0), _missing_param)));

	pattern.track_all_automations_pattern.main_automation_pattern.insert_actl (
		make_control (_integer_param, true), "integer");
	pattern.track_all_automations_pattern.main_automation_pattern.insert_actl (
		make_control (_float_param, false), "float");
	CPPUNIT_ASSERT (pattern.is_integer (-1, IDParameter (PBD::ID (0), _integer_param)));
	CPPUNIT_ASSERT (!pattern.is_integer (-1, IDParameter (PBD::ID (0), _float_param)));
}

void
AutomationIntegerTest::testPatternDescriptor ()
{
	TrackRegionsMap regions_per_track;
	regions_per_track[midi_track ()].push_back (midi_region ());
	Pattern pattern (*_context, regions_per_track, false);
	pattern.setup (regions_per_track);

	const IDParameter midi_id (PBD::ID (0), _midi_param);
	pattern.insert (0, midi_id);
	CPPUNIT_ASSERT (pattern.is_integer (0, 0, midi_id));
	CPPUNIT_ASSERT (!pattern.is_integer (0, 0, IDParameter (PBD::ID (0), _missing_param)));

	pattern.tps[0]->track_all_automations_pattern.main_automation_pattern.insert_actl (
		make_control (_integer_param, true), "integer");
	pattern.tps[0]->track_all_automations_pattern.main_automation_pattern.insert_actl (
		make_control (_float_param, false), "float");
	CPPUNIT_ASSERT (pattern.is_integer (0, -1, IDParameter (PBD::ID (0), _integer_param)));
	CPPUNIT_ASSERT (!pattern.is_integer (0, -1, IDParameter (PBD::ID (0), _float_param)));
}

void
AutomationIntegerTest::testProcessorRegistryRefresh ()
{
	TrackAllAutomationsPattern pattern (
		*_context, audio_track (), Temporal::timepos_t (Temporal::Beats ()),
		Temporal::timecnt_t (Temporal::Beats::beats (16)),
		Temporal::timepos_t (Temporal::Beats::beats (16)),
		Temporal::timepos_t (Temporal::Beats::beats (16)), false);

	ProcessorPtr processor = audio_track ()->amp ();
	CPPUNIT_ASSERT (processor);
	auto before_it = pattern.id_to_processor_automation_pattern.find (processor->id ());
	CPPUNIT_ASSERT (before_it != pattern.id_to_processor_automation_pattern.end ());
	ProcessorAutomationPattern* before = before_it->second;
	before->insert_actl (make_control (_integer_param, true), "integer");
	before->set_param_enabled (_integer_param, true);

	// Refreshing an unchanged route must preserve the existing object and its
	// enabled parameters rather than deleting and recreating them.
	pattern.setup_processors_automation_controls ();

	auto after_it = pattern.id_to_processor_automation_pattern.find (processor->id ());
	CPPUNIT_ASSERT (after_it != pattern.id_to_processor_automation_pattern.end ());
	CPPUNIT_ASSERT (before == after_it->second);
	CPPUNIT_ASSERT (after_it->second->is_param_enabled (_integer_param));
	CPPUNIT_ASSERT (after_it->second->is_integer (_integer_param));
}

void
AutomationIntegerTest::testAutomationRangeAfterRegionMove ()
{
	TrackRegionsMap regions_per_track;
	regions_per_track[midi_track ()].push_back (midi_region ());
	Pattern pattern (*_context, regions_per_track, false);
	pattern.setup (regions_per_track);
	pattern.set_rows_per_beat (4, true);
	pattern.update ();

	// Change the range represented by an existing selection, as happens when
	// the tracker is reopened on a different region or a region is moved.
	midi_region ()->set_position (Temporal::timepos_t (Temporal::Beats::beats (32)));
	midi_region ()->set_length (Temporal::timecnt_t (Temporal::Beats::beats (8)));
	pattern.update ();

	const BasePattern& track = *pattern.tps[0];
	const BasePattern& main_automation =
		pattern.tps[0]->track_all_automations_pattern.main_automation_pattern;
	CPPUNIT_ASSERT (main_automation.position == pattern.position);
	CPPUNIT_ASSERT (main_automation.length == pattern.length);
	CPPUNIT_ASSERT (main_automation.end == pattern.end);
	CPPUNIT_ASSERT_EQUAL (pattern.global_nrows, main_automation.nrows);

	ProcessorPtr processor = midi_track ()->amp ();
	CPPUNIT_ASSERT (processor);
	auto processor_it =
		pattern.tps[0]->track_all_automations_pattern.id_to_processor_automation_pattern.find (processor->id ());
	CPPUNIT_ASSERT (processor_it !=
	                pattern.tps[0]->track_all_automations_pattern.id_to_processor_automation_pattern.end ());
	CPPUNIT_ASSERT (processor_it->second->position == pattern.position);
	CPPUNIT_ASSERT (processor_it->second->length == pattern.length);
	CPPUNIT_ASSERT (processor_it->second->end == pattern.end);
	CPPUNIT_ASSERT_EQUAL (pattern.global_nrows, processor_it->second->nrows);

	// Keep the track's own range covered by the same assertions as the nested
	// automation patterns.
	CPPUNIT_ASSERT (track.position == pattern.position);
	CPPUNIT_ASSERT (track.length == pattern.length);
	CPPUNIT_ASSERT (track.end == pattern.end);
}
