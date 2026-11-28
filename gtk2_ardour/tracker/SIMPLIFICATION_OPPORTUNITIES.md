# Tracker Editor Simplification Opportunities

## Scope and summary

This report reviews the Tracker Editor model in `libs/tracker/` and its GTK
implementation in `gtk2_ardour/tracker/`, looking for code that can be
simplified by reusing existing Ardour classes, functions, or data structures.

The review was static. No production source code was changed in the course of
writing the original report. All fourteen items have since been implemented;
items 1–9 landed first as a series of follow-up commits, and items 10–14
followed. The completed work is recorded below with current locations and
commit hashes where applicable.

Unit tests exist for the completed items (see `libs/tracker/test/`). The build
tree used for this revision has tests enabled (`BUILD_TESTS = True`), and the
tracker suite passes (81 tests).

Scale at the time of writing: `libs/tracker/` is about 9k lines and
`gtk2_ardour/tracker/` about 13k, of which `grid.cc` alone is 7.4k.

All numbered items are complete. The remaining optional follow-ups are listed
under “Further observations”.

### Status at a glance

| # | Opportunity | Status |
|---|-------------|--------|
| 1 | Shared MIDI controller-menu builder | Done (`7969deded3`) |
| 2 | `interpolate_linear()` | Done (`85cacd90bb`) |
| 3 | Temporal range coverage for note overlap | Done (`5c4f1b964d`, tests `86b9265b8a`) |
| 4 | `ParameterDescriptor::integer_step` | Done (`8cea15b7fd`) |
| 5 | `ARDOUR::MidiNoteTracker` for chord state | Done (`bab648e21e`) |
| 6 | `get_grid_beat_divisions()` and `ArdourDropdown` actions | Done (`66b37b7099`) |
| 7 | `Gtk::ToggleAction` for tracker modes | Done (`6c86ae7e86`) |
| 8 | Single automation registry for the toolbar | Done (`eb63bb69a8`) |
| 9 | Keyboard modifier tracking and forwarding | Done (`55273f6c0f`) |
| 10 | `std::unique_ptr` ownership for tracker-owned collections | Done |
| 11 | Tick-space row arithmetic in `BasePattern` | Done |
| 12 | Cache or reuse GDK/GTK color conversion | Done (colors cached) |
| 13 | `ARDOUR::RegionSortByPosition` for region sorting | Done (comparator removed) |
| 14 | Remove unused code | Done |

## Completed work (items 1–14)

**1. Shared controller menu.** `MidiTrackToolbar::build_controller_menu()`
(`gtk2_ardour/tracker/midi_track_toolbar.cc:278-296`) now calls the shared
`build_controller_menu()` from `gtk2_ardour/midi_util.h:46`, passing its own
single- and multi-channel callbacks. The obsolete `get_device_names()`
forwarding methods were removed.

**2. `interpolate_linear()`.** `TrackerUtils::interpolate()`
(`libs/tracker/tracker_utils.cc:37-45`) includes `pbd/control_math.h` and
delegates the arithmetic to `interpolate_linear()`; the `last_row <= first_row`
guard remains.

**3. Temporal range coverage.** `MidiNotesPattern::overlap()`
(`libs/tracker/midi_notes_pattern.cc:871-876`) uses the `int64_t`
specialization of `Temporal::coverage_exclusive_ends()`. Tests cover disjoint,
touching, partial, containing, identical, and zero-length ranges in both
argument orders. Zero-length notes are consistently treated as empty ranges.

**4. Descriptor-driven integer detection.** `AutomationPattern::is_integer()`
(`libs/tracker/automation_pattern.cc:831-835`) reads
`actl->desc().integer_step`. Region, main, and processor automation dispatch
through `Pattern::is_integer()`, `TrackPattern::is_integer()`,
`TrackAllAutomationsPattern::is_integer()`, and the `MidiTrackPattern`
override; `Grid::is_int_param()` (`gtk2_ardour/tracker/grid.cc:3649-3652`)
forwards to it. `libs/tracker/test/AutomationIntegerTest.cc` covers the
dispatch chain, missing controls, registry refresh, and ranges after a region
move.

**5. `MidiNoteTracker`.** `Grid` uses an `ARDOUR::MidiNoteTracker`
(`gtk2_ardour/tracker/grid.h:1014`) for step and chord entry. MIDI input and
computer-keyboard step entry add notes, note-offs remove the matching
pitch/channel voice, and the tracker is reset on step-edit and track
transitions (`grid.cc:6148, 6157, 6166`). The pitch-to-channel map remains for
choosing the channel of generated note-offs.

**6. Rows-per-beat menu.** `MainToolbar` derives row counts from
`PublicEditor::get_grid_beat_divisions()` (`main_toolbar.cc:489-505`) and
builds the dropdown with `ArdourDropdown::append(action)`. A single descriptor
table (`main_toolbar.cc:39-65`) supplies grid types, action names, labels, and
stepping order; one-row-per-bar remains excluded from the dropdown. Action and
menu labels deliberately share the concise `N Row/Beat` text. See the residual
note under “Further observations”.

**7. Toggle-action modes.** All ten tracker modes are `Gtk::ToggleAction`
instances (`main_toolbar.cc:408-417`). Toolbar and `TimeHeader` buttons use
`set_related_action()`, popup check items are created from the same actions
(`main_toolbar.cc:670-699`), and `Grid` reads the action state. Callbacks
remain only for side effects (step editing, playhead synchronization,
following, redisplay); original defaults and keyboard bindings are preserved.

**8. Single automation registry.** `TrackToolbar` iterates
`TrackAllAutomationsPattern::id_to_processor_automation_pattern`
(`libs/tracker/track_all_automations_pattern.h:121`) via
`track_toolbar.cc:217-264`; its only extra registry maps `IDParameter` to GTK
menu items. The parallel `ProcessorAutomationInfo`/`ProcessorAutomationNode`
hierarchy and lookup helpers were removed. Registry refresh preserves existing
parameter state, drops departed processors, and is wired to
`Route::processors_changed` (`gtk2_ardour/tracker/tracker_editor.cc:311-321`).

**9. Keyboard modifiers and forwarding.** `Grid` reads
`Gtkmm2ext::Keyboard::the_keyboard().modifier_state()` against
`RangeSelectModifier` (`grid.cc:7256-7259`); the manual `shift_pressed` state
machine is gone. Shift key events are still consumed so they do not fall
through to Ardour. `TrackerEditor::activate_ardour_binding()`
(`tracker_editor.cc:215-263`) tries the tracker window's bindings first, relays
modified keys to the main window with `ARDOUR_UI_UTILS::relay_key_press()`, and
offers unmodified keys only to Ardour's global bindings.

**10. `std::unique_ptr` ownership.** Every tracker-owned collection now uses
`std::unique_ptr`: `PatternPhenomenalDiff::mti2tp_diff`, `Pattern::tps`,
`MidiTrackPattern::mrps`,
`TrackAllAutomationsPattern::id_to_processor_automation_pattern`,
`GridHeader::track_headers`, `Grid::time_column`, and all Grid column
containers. Factories return `std::unique_ptr`, creation uses
`std::make_unique`, and the custom `release()`, destructors, delete loops, and
one-shot raw-pointer temporaries were removed. Copy behavior is unchanged:
`TrackAllAutomationsPattern::operator=` still deep-copies its processor
patterns, while `Pattern`/`MidiTrackPattern` continue to copy into existing
objects in place. The tracker registry and phenomenal-diff tests cover the
resulting ownership behaviour.

**11. Tick-space row arithmetic.** `BasePattern` now uses Ardour's integer
grid step: `_ticks_per_row = Temporal::ticks_per_beat / rows_per_beat` and
`beats_per_row = Beats::ticks(_ticks_per_row)` (`base_pattern.cc:174-193`).
`beats_at_row()` multiplies that tick duration directly, `find_nrows()` divides
the row range by it, and `row_distance()`/`row_at_beats()` use `int_div_round`
with the same divisor (`:262-265`). Min/max-delay row selection uses
ceiling/floor division by `_ticks_per_row` (`:274-292`), and `row_distance()`
now returns `int`. This matches `EditingContext::get_grid_beat_divisions()` and
`Beats::round_to_subdivision()`. At 28 rows/beat the row is 68 ticks, so 28
rows span 1904 ticks rather than a full 1920-tick beat. `BasePatternTest` now
covers every supported division, delay row selection, and the 28-rows/beat
boundaries and drift.

**12. Cached color strings.** `Grid::read_colors()` now converts each
configured color to its `#rrggbb` string once and stores it alongside the
`Gtkmm2ext::Color` (`gtk2_ardour/tracker/grid.h:955-977`,
`grid.cc:1488-1505`). The redisplay paths use those cached strings directly,
so `TrackerUtils::color_to_string()` is no longer called per cell; it remains
the one-time converter and is still covered by `TrackerUtilsTest`. The model
columns remain string-based: switching them to `Gdk::Color` would be a much
larger schema change without a clear benefit.

**13. Region comparator.** Removed `TrackerUtils::region_position_less`,
which had no production users. Any future tracker-side region sort should use
`ARDOUR::RegionSortByPosition` (`libs/ardour/ardour/region_sorters.h:26-30`).

**14. Dead code.** Removed the unused helpers catalogued in earlier revisions:
the pan-type helpers, the `clone_actl`/`clone_param_actl`/`clone_alist` trio
(and the now-unused `ParamAutomationControlPair` typedef), `BasePattern::clamp`
and the `row_at_time_{min,max}_delay` wrappers, `Pattern::sample_at_row_at_mti`,
`Pattern::find_next_on_note`, `MidiNotesPattern::{find_prev_off,erase}`,
`AutomationPattern::previous_row`, Grid's dead column accessors and
`mti_col_offset`/`automation_col_offsets` (including the now-unused
`NUMBER_OF_COL_PER_*` macros), the two undefined-cursor setters,
`redisplay_undefined_region_name`, `mk_blank`, `MainToolbar::rows_per_beat_{up,
down}`, `TrackToolbar::setup_label`, and `MidiTrackToolbar::print_widths`.

## Further observations

These were noticed while verifying the completed items; none is urgent.

- **Rows-per-beat counts still have two sources.** `RowsPerBeatChoice::rows`
  (`main_toolbar.cc:39-65`) duplicates
  `EditingContext::get_grid_beat_divisions()`. The field is still needed for
  the reverse lookup in `rows_per_beat_step()` (`main_toolbar.cc:733-747`), so
  the table must be kept in sync with the core mapping by hand.
- **Controller-item callbacks remain near-duplicates.**
  `MidiTrackToolbar::add_single_channel_controller_item` and
  `add_multi_channel_controller_item` (`midi_track_toolbar.cc:388-467`) still
  mirror `MidiTimeAxisView`'s versions (`midi_time_axis.cc:991-1088`); only
  the leaf visibility mechanism differs. Item 1 removed the bulk of the
  duplication, so this is a smaller, optional follow-up.
- **The automation menu is rebuilt on every click.**
  `TrackToolbar::build_automation_menu()` tears down and rebuilds the
  processor menu each time (`track_toolbar.cc:135-165`), with an existing
  `TODO could be optimized` at line 154. The stable registry introduced by
  item 8 now makes persistent menu items keyed by `IDParameter` feasible.
- **`grid.cc` dominates the UI layer.** At 7.4k lines it contains several
  200–350-line functions (`rebuild_schema`, `redisplay_grid`,
  `interpolate_selection`). Splitting `Grid` by concern is the largest
  remaining structural change, but it is a refactor rather than a reuse
  opportunity and is out of scope here.

## Cases where reuse would not be a simplification

The following substitutions are not recommended:

- **`NotePlayer` for keyboard audition:** it implements timed audition with
  automatic note-off and transport/filter behavior, not the tracker's explicit
  key-down/key-up model.
- **`MidiCutBuffer` or `Gtk::TreeSelection` for `SubgridSelector`:** neither
  represents a rectangular selection spanning multiple tracks, note
  attributes, delays, and arbitrary automation columns. The custom register is
  justified.
- **`ARDOUR::value_as_string()` for all tracker cells:** it would change gain
  units, MIDI bender representation, labels, precision, and hexadecimal
  behavior. It may be useful for tooltips, but not as a drop-in cell
  formatter.
- **`ControlList::eval()` instead of the current `rt_safe_eval()` calls:** the
  existing code documents a real lock interaction. Also, subgrid interpolation
  is by row index and intentionally independent of the automation list's own
  interpolation style.
- **`Route::automatables()` as a universal replacement for tracker automation
  discovery:** it flattens main and processor controls, whereas the tracker
  needs processor identity and grouping. Existing per-processor APIs are a
  better fit.
- **Replacing the GTK grid with `ArdourCanvas`:** that would be a rewrite, not
  a simplification.
- **Copying `RouteTimeAxisView` automation-node classes into the tracker:**
  they contain timeline-lane state and lifetime dependencies that the tracker
  does not need.

## Suggested order of work

All numbered items are complete. The remaining optional follow-ups are in
“Further observations”.
