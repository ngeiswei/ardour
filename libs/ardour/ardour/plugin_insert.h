/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Johannes Mueller <github@johannes-mueller.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "pbd/stack_allocator.h"
#include "pbd/timing.h"

#include "ardour/ardour.h"
#include "ardour/libardour_visibility.h"
#include "ardour/fixed_delay.h"
#include "ardour/io.h"
#include "ardour/types.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/plugin.h"
#include "ardour/plug_insert_base.h"
#include "ardour/processor.h"
#include "ardour/readonly_control.h"
#include "ardour/sidechain.h"

class XMLNode;

namespace ARDOUR {

class Session;
class Route;
class Plugin;

/** Plugin inserts: send data through a plugin
 */
class LIBARDOUR_API PluginInsert : public Processor, public PlugInsertBase, public std::enable_shared_from_this <PluginInsert>
{
public:
	PluginInsert (Session&, Temporal::TimeDomainProvider const & tdp, std::shared_ptr<Plugin> = std::shared_ptr<Plugin>());
	~PluginInsert ();

	void drop_references ();

	std::weak_ptr<PluginInsert> weak_ptr () {
		return shared_from_this();
	}

	static const std::string port_automation_node_name;

	int set_state(const XMLNode&, int version);
	void update_id (PBD::ID);
	void set_owner (SessionObject*);
	void set_state_dir (const std::string& d = "");

	void run (BufferSet& in, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);
	void silence (samplecnt_t nframes, samplepos_t start_sample);

	void activate ();
	void deactivate ();
	void flush ();

	void enable (bool yn);
	bool enabled () const;
	bool bypassable () const;

	bool reset_parameters_to_default ();
	bool can_reset_all_parameters ();

	bool write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf);

	void automation_run (samplepos_t, pframes_t, bool only_active = false);
	bool find_next_event (Temporal::timepos_t const &, Temporal::timepos_t const &, Evoral::ControlEvent&, bool only_active = true) const;

	int set_block_size (pframes_t nframes);

	ChanMapping input_map (uint32_t num) const {
		if (num < _in_map.size()) {
			return _in_map.find (num)->second;
		} else {
			return ChanMapping ();
		}
	}

	ChanMapping output_map (uint32_t num) const {
		if (num < _out_map.size()) {
			return _out_map.find (num)->second;
		} else {
			return ChanMapping ();
		}
	}

	ChanMapping thru_map () const {
		return _thru_map;
	}

	bool pre_seed (const ChanCount&, const ChanCount&, const ChanMapping&, const ChanMapping&, const ChanMapping&);

	ChanMapping input_map () const; ///< combined (all instances) input map
	ChanMapping no_sc_input_map () const; ///< combined (all instances) input map w/o sidechain sinks
	ChanMapping output_map () const; ///< combined (all instances) output map
	bool has_midi_bypass () const;
	bool has_midi_thru () const;
	bool inplace () const { return ! _no_inplace; }

	bool is_channelstrip () const;

	UIElements ui_elements () const;

	void set_input_map (uint32_t, ChanMapping);
	void set_output_map (uint32_t, ChanMapping);
	void set_thru_map (ChanMapping);
	bool reset_map (bool emit = true);
	bool reset_sidechain_map ();
	bool configured () const { return _configured; }

	// these are ports visible on the outside
	ChanCount output_streams() const;
	ChanCount input_streams() const;
	ChanCount internal_streams() const; // with side-chain

	// actual ports of all plugins.
	// n * natural_i/o or result of reconfigurable i/o
	ChanCount internal_output_streams() const;
	ChanCount internal_input_streams() const;

	// a single plugin's internal i/o
	ChanCount natural_output_streams() const;
	ChanCount natural_input_streams() const;

	/** plugin ports marked as sidechain */
	ChanCount sidechain_input_pins() const;

	/** Plugin-Insert IO sidechain ports */
	ChanCount sidechain_input_ports() const {
		if (_sidechain) {
			return _sidechain->input ()->n_ports ();
		} else {
			return ChanCount ();
		}
	}

	const ChanCount& required_buffers () const { return _required_buffers; }
	const ChanCount& preset_out () const { return _preset_out; }

	// allow to override output_streams(), implies "Custom Mode"

	// only the owning route may call these (with process lock held)
	// route is not a friend class, it owns us
	bool set_count      (uint32_t num);
	void set_sinks      (const ChanCount&); // reconfigurable I/O ONLY
	void set_outputs    (const ChanCount&);
	void set_strict_io  (bool b);
	void set_custom_cfg (bool b);
	bool set_preset_out (const ChanCount&);
	bool add_sidechain  (uint32_t n_audio = 1, uint32_t n_midi = 0);
	bool del_sidechain ();
	void update_sidechain_name ();
	std::shared_ptr<SideChain> sidechain () const { return _sidechain; }
	// end C++ class slavery!

	uint32_t get_count  () const { return _plugins.size(); }
	bool     strict_io  () const { return _strict_io; }
	bool     custom_cfg () const { return _custom_cfg; }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	bool has_no_inputs() const;
	bool has_no_audio_inputs() const;

	bool is_instrument () const;
	bool has_automatables () const;

	bool has_output_presets (
			ChanCount in = ChanCount (DataType::MIDI, 1),
			ChanCount out = ChanCount (DataType::AUDIO, 2)
			);

	void realtime_handle_transport_stopped ();
	void realtime_locate (bool);
	void monitoring_changed ();

	bool load_preset (Plugin::PresetRecord);

	bool provides_stats () const;
	bool get_stats (PBD::microseconds_t& min, PBD::microseconds_t& max, double& avg, double& dev) const;
	void clear_stats ();

	struct PIControl : public PluginControl
	{
		PIControl (Session&                        s,
		           PlugInsertBase*                 p,
		           const Evoral::Parameter&        param,
		           const ParameterDescriptor&      desc,
		           std::shared_ptr<AutomationList> list = std::shared_ptr<AutomationList>())
		: PluginControl (s, p, param, desc, list) {}
	private:
		void actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
	};

	std::shared_ptr<Plugin> plugin(uint32_t num=0) const {
		if (num < _plugins.size()) {
			return _plugins[num];
		} else {
			return _plugins[0]; // we always have one
		}
	}

	samplecnt_t plugin_latency () const;

	bool has_sidechain () const {
		return _sidechain ? true : false;
	}

	std::shared_ptr<IO> sidechain_input () const {
		if (_sidechain) {
			return _sidechain->input ();
		}
		return std::shared_ptr<IO> ();
	}

	PluginType type () const;

	std::shared_ptr<ReadOnlyControl> control_output (uint32_t) const;

	std::string describe_parameter (Evoral::Parameter param);

	samplecnt_t signal_latency () const;

	std::shared_ptr<Plugin> get_impulse_analysis_plugin();

	void collect_signal_for_analysis (samplecnt_t nframes);

	bool strict_io_configured () const {
		return _match.strict_io;
	}

	bool splitting () const {
		return _match.method == Split;
	}

	void configured_io (ChanCount &in, ChanCount &out) const {
		in = _configured_in;
		out = _configured_out;
	}

	PBD::Signal<void(BufferSet*, BufferSet*)> AnalysisDataGathered;
	PBD::Signal<void()> PluginIoReConfigure;
	PBD::Signal<void()> PluginMapChanged;
	PBD::Signal<void()> PluginConfigChanged;
protected:
	XMLNode& state () const;

private:
	/* disallow copy construction */
	PluginInsert (const PluginInsert&);

	void parameter_changed_externally (uint32_t, float);

	void set_parameter (Evoral::Parameter param, float val, sampleoffset_t);

	float default_parameter_value (const Evoral::Parameter& param);

	typedef std::vector<std::shared_ptr<Plugin> > Plugins;
	Plugins _plugins;

	std::shared_ptr<SideChain> _sidechain;
	uint32_t _sc_playback_latency;
	uint32_t _sc_capture_latency;
	uint32_t _plugin_signal_latency;

	std::weak_ptr<Plugin> _impulseAnalysisPlugin;

	samplecnt_t _signal_analysis_collect_nsamples;
	samplecnt_t _signal_analysis_collect_nsamples_max;

	BufferSet _signal_analysis_inputs;
	BufferSet _signal_analysis_outputs;

	FixedDelay _delaybuffers;

	ChanCount _configured_in;
	ChanCount _configured_internal; // with side-chain
	ChanCount _configured_out;
	ChanCount _custom_out;
	ChanCount _custom_sinks;
	ChanCount _preset_out;
	ChanCount _cached_sidechain_pins;
	ChanCount _required_buffers;

	bool _configured;
	bool _no_inplace;
	bool _strict_io;
	bool _custom_cfg;
	bool _maps_from_state;

	Match private_can_support_io_configuration (ChanCount const &, ChanCount &) const;
	Match internal_can_support_io_configuration (ChanCount const &, ChanCount &) const;
	Match automatic_can_support_io_configuration (ChanCount const &, ChanCount &) const;

	/** details of the match currently being used */
	Match _match;

	/* ordered map [plugin instance ID] => ARDOUR::ChanMapping */
#if defined(_MSC_VER) /* && (_MSC_VER < 1900)
	                   * Regarding the note (below) it was initially
	                   * thought that this got fixed in VS2015 - but
	                   * in fact it's still faulty (JE - Feb 2021) */
	/* Use the older (heap based) mapping for early versions of MSVC.
	 * In fact it might be safer to use this for all MSVC builds - as
	 * our StackAllocator class depends on 'boost::aligned_storage'
	 * which is known to be troublesome with Visual C++ :-
	 * https://www.boost.org/doc/libs/1_65_0/libs/type_traits/doc/html/boost_typetraits/reference/aligned_storage.html
	 */
	class PinMappings : public std::map <uint32_t, ARDOUR::ChanMapping>
#else
	class PinMappings : public std::map <uint32_t, ARDOUR::ChanMapping, std::less<uint32_t>, PBD::StackAllocator<std::pair<const uint32_t, ARDOUR::ChanMapping>, 4> >
#endif
	{
		public:
			/* this emulates C++11's  std::map::at()
			 * return mapping for given plugin instance */
			inline ARDOUR::ChanMapping const& p (const uint32_t i) const {
#ifndef NDEBUG
				const_iterator x = find (i);
				assert (x != end ());
				return x->second;
#else
				return find(i)->second;
#endif
			}
	};

	PinMappings _in_map;
	PinMappings _out_map;
	ChanMapping _thru_map; // out-idx <=  in-idx

	void automate_and_run (BufferSet& bufs, samplepos_t start, samplepos_t end, double speed, pframes_t nframes);
	void connect_and_run (BufferSet& bufs, samplepos_t start, samplecnt_t end, double speed, pframes_t nframes, samplecnt_t offset, bool with_auto);
	void bypass (BufferSet& bufs, pframes_t nframes);
	void inplace_silence_unconnected (BufferSet&, const PinMappings&, samplecnt_t nframes, samplecnt_t offset) const;

	void create_automatable_parameters ();
	void control_list_automation_state_changed (Evoral::Parameter, AutoState);
	void set_parameter_state_2X (const XMLNode& node, int version);

	void enable_changed ();
	void bypassable_changed ();

	bool sanitize_maps ();
	bool check_inplace ();
	void mapping_changed ();

	void add_plugin (std::shared_ptr<Plugin>);
	void plugin_removed (std::weak_ptr<Plugin>);

	void add_sidechain_from_xml (const XMLNode& node, int version);

	void start_touch (uint32_t param_id);
	void end_touch (uint32_t param_id);

	void latency_changed ();
	bool _latency_changed;
	uint32_t _bypass_port;
	bool     _inverted_bypass_enable;

	typedef std::map<uint32_t, std::shared_ptr<ReadOnlyControl> >CtrlOutMap;
	CtrlOutMap _control_outputs;

	PBD::TimingStats  _timing_stats;
	std::atomic<int> _stat_reset;
	std::atomic<int> _flush;
};

} // namespace ARDOUR

