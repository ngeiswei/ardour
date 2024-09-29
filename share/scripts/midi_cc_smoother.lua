ardour {
    ["type"]    = "dsp",
    name        = "MIDI CC Smoother",
    category    = "Utility",
    license     = "MIT",
    author      = "Nil Geisweiller",
    description = [[Smooth MIDI Control Changes.]]
}

function dsp_ioconfig ()
	return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0}, }
end

function dsp_params ()
	return
	{
		{ ["type"] = "input",
			name = "Control change",
			doc = "Control change number affected by the smoothing",
			min = 0, max = 127, default = 1, integer = true },
		{ ["type"] = "input",
			name = "Channel",
			doc = "Control change channel affected by the smoothing",
			min = 1, max = 16, default = 1, integer = true },
		{ ["type"] = "input",
			name = "Smoothing",
			doc = "Speed of the smoothing, from 0 (no smoothing) to 1 (maximum smoothing)",
			min = 0, max = 1, default = 0},
	}
end

function dsp_run (_, _, n_samples)
	assert (type(midiin) == "table")
	assert (type(midiout) == "table")
	local cnt = 1;

	function tx_midi (time, data)
		midiout[cnt] = {}
		midiout[cnt]["time"] = time;
		midiout[cnt]["data"] = data;
		cnt = cnt + 1;
	end

	-- for each incoming midi event
	for _,b in pairs (midiin) do
		local t = b["time"] -- t = [ 1 .. n_samples ]
		local d = b["data"] -- get midi-event
		local event_type
		tx_midi (t, d)
	end
end
