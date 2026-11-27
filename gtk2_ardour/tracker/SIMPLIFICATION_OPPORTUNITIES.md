# Tracker Editor Simplification Opportunities

## Scope and summary

This report reviews the Tracker Editor model in `libs/tracker/` and its GTK
implementation in `gtk2_ardour/tracker/`, looking for code that can be
simplified by reusing existing Ardour classes, functions, or data structures.

The review was static. No source code was changed and no tests were run.

The strongest candidates are:

- Replacing the tracker-specific MIDI controller-menu builder with Ardour's
  shared implementation.
- Reusing Temporal, PBD, and Evoral arithmetic and parameter APIs.
- Using `ARDOUR::MidiNoteTracker` for chord/note-on state.
- Consolidating tracker mode state with `Gtk::ToggleAction`.
- Making `TrackAllAutomationsPattern` the single processor-automation registry
  used by both the model and toolbar.

## Best immediate candidates

### 1. Replace the tracker controller-menu implementation with the shared builder

**Status: Done.** `MidiTrackToolbar` now calls the shared
`build_controller_menu()` from `gtk2_ardour/midi_util.h`. The multi-channel
callback accepts the shared API's channel-mask argument, and the obsolete
`get_device_names()` forwarding methods were removed.

**Current code:** `gtk2_ardour/tracker/midi_track_toolbar.cc:276-430`

`MidiTrackToolbar::build_controller_menu()` reimplements controller/MIDNAM
menu logic already provided by:

- `gtk2_ardour/midi_util.cc:50-172` — `build_controller_menu()`
- `gtk2_ardour/midi_util.h:46-49` — declaration
- `gtk2_ardour/midi_time_axis.cc:1088-1100` — existing caller
- `Pianoroll` — another existing caller

The tracker can call the shared function with its own
`add_single_channel_controller_item` and `add_multi_channel_controller_item`
callbacks, just as the other views do.

Benefits:

- Removes roughly 150 lines of duplicated menu construction.
- Picks up the shared implementation's newer behavior:
  - Proper `InstrumentInfo::channels_for_control_list()` handling.
  - Better handling of multiple MIDNAM control-name lists.
  - Current grouping logic.
  - Correct channel masks passed to multi-channel callbacks.
- Avoids future divergence between the tracker, piano roll, and track
  automation menus.

The tracker's existing `add_multi_channel_controller_item()` signature would
need the additional `uint16_t channels` argument required by the shared API,
which is a small mechanical change.

**Confidence:** very high. This is the clearest duplication found.

### 2. Reuse `interpolate_linear()` instead of maintaining another implementation

**Status: Done.** `tracker_utils.cc` now includes `pbd/control_math.h` and uses
`interpolate_linear()` for the nondegenerate calculation.

**Current code:** `libs/tracker/tracker_utils.cc:59-77`

`TrackerUtils::interpolate()` implements the same operation as
`interpolate_linear()` in `libs/pbd/pbd/control_math.h:84-88`:

```cpp
first + (last - first) * fraction
```

The tracker helper can become a row-fraction calculation followed by:

```cpp
return interpolate_linear (first, last, fraction);
```

The special handling for `last_row <= first_row` should remain.
`TrackerUtils::interpolate_int()` can continue using the helper before rounding
and clamping.

**Confidence:** very high. Small, behavior-preserving replacement.

### 3. Replace `MidiNotesPattern::overlap()` with Temporal's range coverage helper

**Status: Done.** The implementation now uses the `int64_t` specialization of
`Temporal::coverage_exclusive_ends()` in tick space. Unit tests cover disjoint,
touching, partial, containing, identical, and zero-length ranges in both
argument orders. Zero-length notes are consistently treated as empty ranges
and do not overlap, eliminating the previous argument-order inconsistency.

**Current code:** `libs/tracker/midi_notes_pattern.cc:875-886`

The hand-written overlap test can use Temporal's range machinery:

```cpp
Temporal::coverage_exclusive_ends (
    a->time (), a->end_time (),
    b->time (), b->end_time ()) != Temporal::OverlapNone
```

See `libs/temporal/temporal/range.h:127-133`.

This expresses the intended half-open note intervals more clearly and uses the
same overlap definitions used by `MidiModel` and region/playlist code.

Tests should cover:

- Notes touching end-to-start, which must **not** overlap.
- Identical notes.
- Notes wholly containing one another.
- Partial overlaps in both directions.
- Zero-length notes, since their semantics may differ between the current
  custom expression and the general range helper.

**Confidence:** high for ordinary positive-length notes; edge-case tests are
advisable.

### 4. Use `AutomationControl::desc().integer_step` for integer automation decisions

**Status: Done.** Integer decisions now use the actual automation control's
`ParameterDescriptor::integer_step`. Region automation, main automation, and
processor automation are routed through the existing pattern hierarchy, and
all four Grid callers use the descriptor. Session-backed unit tests cover the
direct lookup, main and processor dispatch, track and region dispatch, and
top-level Pattern forwarding, including missing controls.

**Current code:** `gtk2_ardour/tracker/grid.cc:3740-3744`

Currently:

```cpp
bool
Grid::is_int_param (const IDParameter& id_param) const
{
    return TrackerUtils::is_region_automation (id_param.second);
}
```

This treats all region MIDI automation as integral but ignores integer-valued
plugin and processor parameters.

The actual `AutomationControl` already owns the authoritative descriptor:

```cpp
actl->desc ().integer_step
```

See `libs/ardour/ardour/automation_control.h:117` and existing descriptor use
in `generic_pluginui.cc`.

A clean implementation would expose a small
`AutomationPattern::is_integer(param)` helper that retrieves the control and
returns `desc().integer_step`, then have `Grid::is_int_param()` forward to it.

Benefits:

- Less type-specific logic in the GTK layer.
- Correct rounding for integer plugin parameters during interpolation and step
  editing.
- One authoritative source for parameter semantics.

Constructing a generic `ParameterDescriptor` from only the
`Evoral::Parameter` would be less reliable for plugin-specific metadata;
using the existing control's descriptor is preferable.

**Confidence:** high, with a deliberate behavior improvement for integer plugin
parameters.

### 5. Use `ARDOUR::MidiNoteTracker` for chord/note-on state

**Status: Done.** `Grid` now uses a channel-aware `MidiNoteTracker` for step
and chord entry. Both MIDI-input and computer-keyboard paths register notes,
note-offs remove the matching pitch/channel voice, and the tracker resets on
step-edit and track transitions. The pitch-to-channel map remains responsible
for selecting the channel of generated note-off messages.

**Current code:**

- `gtk2_ardour/tracker/grid.h:410-412` — `current_on_notes`
- `gtk2_ardour/tracker/grid.cc:6378-6459` — incoming MIDI processing
- Related pitch/channel bookkeeping at `grid.h:1033-1035`

Ardour already has `ARDOUR::MidiNoteTracker` in
`libs/ardour/ardour/midi_state_tracker.h`, including:

- `track(const uint8_t*)`
- `empty()`
- `on()`
- `active(note, channel)`
- `reset()`
- Per-channel and repeated-note voice counting

It is already used by `StepEditor`.

The tracker grid can call `note_tracker.track(buf)` for each incoming event
and use `note_tracker.empty()` when deciding whether chord entry is complete.

This also fixes a limitation of the current `std::set<uint8_t>`: if the same
pitch is played twice before the first note-off, the first note-off empties the
set even though another voice remains active. `MidiNoteTracker` counts those
voices correctly.

The separate `pressed_keys_pitch_to_channel` map may still be useful for
generating a note-off on the same channel when the only information available
is the pitch. Alternatively, channel-aware tracker queries can be used where
appropriate.

The tracker should be reset when step editing starts, stops, or changes
tracks.

**Confidence:** high, though repeated-note behavior will intentionally become
more correct.

### 6. Reuse `EditingContext::get_grid_beat_divisions()` and `ArdourDropdown` actions

**Current code:**

- `gtk2_ardour/tracker/main_toolbar.cc:524-645`
- `gtk2_ardour/tracker/main_toolbar.cc:923-984`

`MainToolbar::set_beats_per_row_to()` repeats the `GridType` to rows-per-beat
mapping already implemented by:

```cpp
tracker_editor.public_editor.get_grid_beat_divisions (grid_type)
```

See `gtk2_ardour/editing_context.cc:3010-3040`.

`GridTypeBar` needs its existing special handling because Ardour returns `-1`
for that case, while the tracker currently uses `0`.

In addition:

- `ArdourDropdown::append(action)` can build the menu directly from the radio
  actions, as `EditingContext` does at `editing_context.cc:1341-1377`.
- A small table containing `{ GridType, rows_per_beat, action_name, label }`
  could replace the separate forward switch, reverse switch,
  menu-registration repetition, and choices array.

There is no existing reverse rows-per-beat-to-`GridType` helper worth adding
another dependency for; a single descriptor table would handle that cleanly.

**Confidence:** high for `get_grid_beat_divisions()` and dropdown actions. The
descriptor-table consolidation is a somewhat larger internal refactor.

## Broader consolidation opportunities

### 7. Use `Gtk::ToggleAction` for tracker modes instead of manually synchronizing state

**Current code:**

- Ordinary actions at `gtk2_ardour/tracker/main_toolbar.cc:465-474`
- Manual popup check items at `main_toolbar.cc:808-901`
- Independent boolean flags and `ArdourButton::set_active_state()` calls
  throughout the toolbar

Ardour already provides:

- `ActionManager::register_toggle_action()`
- `ArdourButton::set_related_action()`
- `Gtk::ToggleAction::create_menu_item()`
- `ActionManager::set_toggle_action()` and related state helpers

These are used extensively by the editor and `StepEntry`.

Tracker modes could become toggle actions, with:

- Toolbar buttons related to those actions.
- Popup items created from the same actions.
- Mode state read from the action rather than independently maintained in
  toolbar/menu objects.
- Callbacks retained only for actual side effects, such as connecting the
  playhead clock or enabling MIDI input.

This would eliminate much of the manual active-state synchronization and
simplify the popup's check-item handling. It may also reduce the need to
rebuild the menu merely to refresh state.

**Confidence:** high architectural fit, but medium implementation effort
because custom button styling and singleton forwarding must be preserved.

### 8. Make `TrackAllAutomationsPattern` the single automation registry for the toolbar

**Current duplication:**

- `gtk2_ardour/tracker/track_toolbar.cc:264-354`
- `ProcessorAutomationInfo` and `ProcessorAutomationNode` in `track_toolbar.h`
- Existing model registry in
  `libs/tracker/track_all_automations_pattern.h:107-110`
- Parameter/control/name maps in
  `libs/tracker/automation_pattern.h:264-276`

`TrackToolbar` independently walks every processor and reconstructs
processor/parameter trees that `TrackAllAutomationsPattern` already owns.

The toolbar can instead iterate:

```cpp
track_pattern
    ->track_all_automations_pattern
    .id_to_processor_automation_pattern
```

and each `ProcessorAutomationPattern`'s existing parameter/control/name maps.

It only needs a GTK-specific structure for:

- Menu item pointer.
- Column index/visibility.
- Possibly submenu organization.

That structure can be keyed directly by `IDParameter`, eliminating:

- Duplicate processor discovery.
- `find_processor_automation_info()`
- `find_processor_automation_node()`
- Parallel `ProcessorAutomationInfo` and `ProcessorAutomationNode` ownership.
- Divergence between the model and toolbar when processor contents change.

The processor menu implementation also resembles
`RouteTimeAxisView::add_processor_to_subplugin_menu()`. The nested
`RouteTimeAxisView` automation-node classes should **not** be reused directly:
they are protected, own `AutomationTimeAxisView` lane pointers, and carry
unrelated lifetime responsibilities. A generic menu builder could be extracted
later if desired, but using the tracker's existing model registry is the
cleaner first step.

**Confidence:** high potential payoff, medium refactor size.

### 9. Consolidate keyboard modifier tracking and forwarding

#### Shift/range-selection state

**Current code:** `gtk2_ardour/tracker/grid.cc:7385-7402`

The grid maintains `shift_pressed` manually through key press/release
handlers.

It can instead use:

- The modifier mask from the initiating event, or
- `Gtkmm2ext::Keyboard::the_keyboard().key_is_down()`, or
- `the_keyboard().modifier_state()` with `Keyboard::RangeSelectModifier`.

Using `RangeSelectModifier` would also align tracker selection with Ardour's
configurable range-selection convention.

That removes the manual state machine and its focus-loss edge cases.

#### Forwarding unhandled keys

**Current code:** `gtk2_ardour/tracker/tracker_editor.cc:215-274`

`activate_ardour_binding()` duplicates parts of
`ARDOUR_UI::key_press_focus_accelerator_handler()`.

`ARDOUR_UI_UTILS::relay_key_press()` in `gtk2_ardour/utils.cc:331-334` can
handle forwarding to the main window. A simplified design would:

1. Try Tracker-local bindings.
2. For modified keys, relay to the main window so Editor/Editing/global
   bindings work.
3. For unmodified keys, only try the appropriate global binding, preserving
   tracker note entry.

It is important **not** to blindly relay unmodified keys to the main editor:
many single-key editor actions would steal tracker note/navigation input.
Directly calling the full focus accelerator handler from the grid also risks
recursive propagation back into the same grid.

This is a valid consolidation, but event-routing behavior needs focused
testing.

**Confidence:** high for modifier-state reuse; medium for forwarding
simplification.

### 10. Use `std::unique_ptr` for ownership, starting with phenomenal diffs

The easiest ownership conversion is:

- `libs/tracker/pattern_phenomenal_diff.h:35-48`
- `libs/tracker/pattern_phenomenal_diff.cc`

`mti2tp_diff` manually owns raw pointers, requiring custom `release()`, a
destructor, and move assignment.

Changing it to something like:

```cpp
std::map<int, std::unique_ptr<TrackPatternPhenomenalDiff>>
```

and returning `std::unique_ptr` from `phenomenal_diff_ptr()` would provide
destruction and move behavior automatically.

Other raw-owned collections exist in:

- `Pattern::tps`
- `MidiTrackPattern::mrps`
- `TrackAllAutomationsPattern::id_to_processor_automation_pattern`
- `GridHeader::track_headers`
- Grid column objects
- Toolbar automation info/nodes

Those larger objects have snapshot/copy semantics, so they should be converted
deliberately with explicit clone/snapshot operations rather than mechanically
replacing pointer types.

**Confidence:** very high for phenomenal diffs; medium elsewhere.

## Smaller substitutions and cleanup

### 11. Keep `BasePattern` row arithmetic in tick space

**Current code:** `libs/tracker/base_pattern.cc:143-149` and `:188-242`

Several conversions go through `Temporal::DoubleableBeats` and `double`, even
though `Temporal::Beats` and PBD already provide exact integer operations:

- `beats_per_row * rowi` instead of
  `Beats::from_double(rowi / rows_per_beat)`.
- `PBD::muldiv_round()` from `pbd/integer_division.h` for nearest-row
  calculations.
- `PBD::muldiv_floor()` for row counts.
- Deriving `_ticks_per_row` directly from `beats_per_row.to_ticks()`.

For example, nearest-row distance can be expressed using:

```cpp
PBD::muldiv_round ((to - from).to_ticks (),
                   rows_per_beat, Temporal::ticks_per_beat);
```

This is not merely cosmetic. At 28 rows/beat:

- `Beats::from_double(1.0 / 28)` rounds to **69 ticks**.
- `_ticks_per_row` currently becomes `1920 / 28 == 68`.

Thus row positions and delay bounds currently use different row durations.

The lowest-risk first change is to derive `_ticks_per_row` from the existing
`beats_per_row`, preserving the current 69-tick row spacing while making delay
calculations consistent. A separate decision can then be made about the desired
duration for non-divisor resolutions.

Tests should include 7, 14, and especially 28 rows/beat, plus nearest,
minimum, and maximum delay behavior.

**Confidence:** high simplification value, but arithmetic behavior should be
reviewed and tested rather than changed blindly.

### 12. Reuse GDK/GTK color conversion where convenient, without regressing the hot path

**Current code:**

- `libs/tracker/tracker_utils.cc:410-423`
- String color columns in `gtk2_ardour/tracker/grid.h:85-116`

Possible existing facilities include:

- `Gtkmm2ext::gdk_color_from_rgb()` / `gdk_color_from_rgba()`
- `Gdk::Color::to_string()` or `gdk_color_to_string()`
- `Gtk::TreeModelColumn<Gdk::Color>` and GDK cell-renderer properties
- `CairoWidget::set_source_rgb_a()` for the overlay at `grid.cc:2114`

The custom `#rrggbb` converter is already deliberately optimized, so repeated
per-cell GDK object construction should be avoided. A better simplification
would be to convert/cache colors once in `read_colors()` and reuse those
values.

Changing every model column to `Gdk::Color` is possible but a much larger and
not obviously worthwhile schema change.

**Priority:** low; the current implementation is correct and
performance-conscious.

### 13. Reuse `ARDOUR::RegionSortByPosition` where region sorting is needed

`TrackerUtils::region_position_less` in `tracker_utils.h:86-91` duplicates
`ARDOUR::RegionSortByPosition` from
`libs/ardour/ardour/region_sorters.h:26-30`.

At present, the tracker comparator appears unused, so removal is simpler than
replacement. If retained or reused later, it should use the Ardour comparator.

### 14. Remove unused code rather than abstracting it

Static reference checks found no production users for several items:

- `TrackerUtils::is_pan_type()` and `get_pan_param_types()`
- `AutomationPattern::clone_actl()`, `clone_param_actl()`, and `clone_alist()`
- `BasePattern::clamp()`
- `TrackerUtils::region_position_less`
- `Grid::set_current_row_undefined()` — already marked unused
- `MidiNotesPattern::erase()` declaration

The unused automation cloning functions are particularly worth removing:
`clone_alist()` currently returns the original list despite the surrounding
“deep copy” terminology, which is misleading even though the functions are
unused.

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

1. **Low-risk substitutions:** shared controller-menu builder,
   `interpolate_linear()`, Temporal coverage, `get_grid_beat_divisions()`,
   descriptor-based integer detection, and unused-code removal.
2. **Behavior-sensitive improvements with tests:** tick-space row arithmetic
   and `MidiNoteTracker`.
3. **State/registry consolidation:** toggle actions and using
   `TrackAllAutomationsPattern` as the toolbar's registry.
4. **Larger cleanups:** keyboard forwarding, ownership conversion, and optional
   color-schema changes.

The shared controller-menu builder and the small arithmetic/helper
substitutions alone would remove meaningful duplication without requiring any
redesign.
