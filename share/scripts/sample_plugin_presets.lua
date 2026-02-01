ardour { ["type"] = "EditorAction", name = "Sample Plugin Presets",
	license     = "MIT",
	author      = "Nil Geisweiller",
	description = [[Sample all presets of a plugin at given notes and durations.]]
}

function factory () return function ()
      -- List all Plugins
	for p in ARDOUR.LuaAPI.list_plugins():iter() do
		print (p.name, p.unique_id, p.category, p.type, p:is_instrument ())
		local psets = p:get_presets()
		print ("Presets of ", p.name)
		if not psets:empty() then
			for pset in psets:iter() do
				print (" - ", pset.label)
			end
		end
	end
end end

function icon (params) return function (ctx, width, height, fg)
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil (height / 3) .. "px")
	txt:set_text ("Smp")
	local tw, th = txt:get_pixel_size ()
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
