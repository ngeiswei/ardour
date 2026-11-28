# Tracker Editor Simplification Opportunities

## Scope and summary

This report reviews the Tracker Editor model in `libs/tracker/` and its GTK
implementation in `gtk2_ardour/tracker/`, looking for code that can be
simplified by reusing existing Ardour classes, functions, or data structures.

The review was static. No production source code was changed in the course of
writing the original report. Items 1–9 have since been implemented as a series
of follow-up commits; they are recorded below as one-line summaries with their
current locations and commit hashes. Items 10–14 remain outstanding.

Unit tests exist for several of the completed items (see
`libs/tracker/test/`). They were not executed for this revision because the
build tree in use was configured with `BUILD_TESTS = False`; validating them
requires a build with tests enabled.

Scale at the time of writing: `libs/tracker/` is about 9k lines and
`gtk2_ardour/tracker/` about 13k, of which `grid.cc` alone is 7.4k.

The strongest remaining candidates are:

- Removing the dead code catalogued in item 14.
- Converting phenomenal diffs (and then other raw-owned collections) to
  `std::unique_ptr`.
- Moving `BasePattern` row arithmetic into tick space, with tests.
- Optionally caching color strings and consolidating the remaining
  controller-menu callbacks.

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
| 10 | `std::unique_ptr` ownership, starting with phenomenal diffs | Outstanding |
| 11 | Tick-space row arithmetic in `BasePattern` | Outstanding |
| 12 | Cache or reuse GDK/GTK color conversion | Outstanding |
| 13 | `ARDOUR::RegionSortByPosition` for region sorting | Outstanding (remove comparator) |
| 14 | Remove unused code | Outstanding (list below expanded) |

## Completed work (items 1–9)

**1. Shared controller menu.** `MidiTrackToolbar::build_controller_menu()`
(`gtk2_ardour/tracker/midi_track_toolbar.cc:278-296`) now calls the shared
`build_controller_menu()` from `gtk2_ardour/midi_util.h:46`, passing its own
single- and multi-channel callbacks. The obsolete `get_device_names()`
forwarding methods were removed.

**2. `interpolate_linear()`.** `TrackerUtils::interpolate()`
(`libs/tracker/tracker_utils.cc:61-69`) includes `pbd/control_math.h` and
delegates the arithmetic to `interpolate_linear()`; the `last_row <= first_row`
guard remains.

**3. Temporal range coverage.** `MidiNotesPattern::overlap()`
(`libs/tracker/midi_notes_pattern.cc:877-882`) uses the `int64_t`
specialization of `Temporal::coverage_exclusive_ends()`. Tests cover disjoint,
touching, partial, containing, identical, and zero-length ranges in both
argument orders. Zero-length notes are consistently treated as empty ranges.

**4. Descriptor-driven integer detection.** `AutomationPattern::is_integer()`
(`libs/tracker/automation_pattern.cc:865-869`) reads
`actl->desc().integer_step`. Region, main, and processor automation dispatch
through `Pattern::is_integer()`, `TrackPattern::is_integer()`,
`TrackAllAutomationsPattern::is_integer()`, and the `MidiTrackPattern`
override; `Grid::is_int_param()` (`gtk2_ardour/tracker/grid.cc:3736-3739`)
forwards to it. `libs/tracker/test/AutomationIntegerTest.cc` covers the
dispatch chain, missing controls, registry refresh, and ranges after a region
move.

**5. `MidiNoteTracker`.** `Grid` uses an `ARDOUR::MidiNoteTracker`
(`gtk2_ardour/tracker/grid.h:1034`) for step and chord entry. MIDI input and
computer-keyboard step entry add notes, note-offs remove the matching
pitch/channel voice, and the tracker is reset on step-edit and track
transitions (`grid.cc:6282, 6291, 6300`). The pitch-to-channel map remains for
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
(`libs/tracker/track_all_automations_pattern.h:119`) via
`track_toolbar.cc:228-276`; its only extra registry maps `IDParameter` to GTK
menu items. The parallel `ProcessorAutomationInfo`/`ProcessorAutomationNode`
hierarchy and lookup helpers were removed. Registry refresh preserves existing
parameter state, drops departed processors, and is wired to
`Route::processors_changed` (`gtk2_ardour/tracker/tracker_editor.cc:311-321`).

**9. Keyboard modifiers and forwarding.** `Grid` reads
`Gtkmm2ext::Keyboard::the_keyboard().modifier_state()` against
`RangeSelectModifier` (`grid.cc:7388-7392`); the manual `shift_pressed` state
machine is gone. Shift key events are still consumed so they do not fall
through to Ardour. `TrackerEditor::activate_ardour_binding()`
(`tracker_editor.cc:215-263`) tries the tracker window's bindings first, relays
modified keys to the main window with `ARDOUR_UI_UTILS::relay_key_press()`, and
offers unmodified keys only to Ardour's global bindings.

## Outstanding opportunities

### 10. Use `std::unique_ptr` for ownership, starting with phenomenal diffs

**Current code:**

- `libs/tracker/pattern_phenomenal_diff.h:31-48`
- `libs/tracker/pattern_phenomenal_diff.cc:27-41`

`mti2tp_diff` manually owns raw pointers, requiring custom `release()`, a
destructor, and move assignment. Changing it to something like:

```cpp
std::map<int, std::unique_ptr<TrackPatternPhenomenalDiff>>
```

and returning `std::unique_ptr` from `phenomenal_diff_ptr()` would provide
destruction and move behavior automatically.

Other raw-owned collections:

- `Pattern::tps` — `libs/tracker/pattern.h:208`, freed in `pattern.cc:46-50`
- `MidiTrackPattern::mrps` — `midi_track_pattern.h:182`, freed in
  `midi_track_pattern.cc:44-48`
- `TrackAllAutomationsPattern::id_to_processor_automation_pattern` —
  `track_all_automations_pattern.h:119`, freed in
  `track_all_automations_pattern.cc:53-58`
- `GridHeader::track_headers` — `gtk2_ardour/tracker/grid_header.h:73`, freed
  in `grid_header.cc:98-106`
- Grid column objects — owned by `Grid`, allocated during schema
  construction and deleted in `grid.cc:143-163`

These larger objects have snapshot/copy semantics, so they should be converted
deliberately with explicit clone/snapshot operations rather than mechanically
replacing pointer types. The toolbar automation info/nodes listed by an
earlier revision of this report no longer exist; item 8 removed them.

**Confidence:** very high for phenomenal diffs; medium elsewhere.

### 11. Keep `BasePattern` row arithmetic in tick space

**Current code:**

- `libs/tracker/base_pattern.cc:171-178` — `set_rows_per_beat()`
- `:199-202` — `find_nrows()`
- `:224-229` — `beats_at_row()`
- `:253-258` — `row_distance()`
- `:266-273` and `:282-289` — min/max-delay row selection
- `:317-325` — delay bounds

Several conversions go through `Temporal::DoubleableBeats` and `double`, even
though `Temporal::Beats` and PBD already provide exact integer operations:

- `beats_per_row * rowi` instead of `Beats::from_double(rowi / rows_per_beat)`.
- `PBD::muldiv_round()` from `pbd/integer_division.h:52` for nearest-row
  calculations.
- `PBD::muldiv_floor()` (`:90`) for row counts.
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
calculations consistent. A separate decision can then be made about the
desired duration for non-divisor resolutions.

`row_at_time_min_delay()` and `row_at_time_max_delay()`
(`base_pattern.cc:277, 292`) are unused wrappers around the beats variants and
can simply be removed (see item 14).

Tests should include 7, 14, and especially 28 rows/beat, plus nearest,
minimum, and maximum delay behavior.

**Confidence:** high simplification value, but arithmetic behavior should be
reviewed and tested rather than changed blindly.

### 12. Reuse GDK/GTK color conversion where convenient, without regressing the hot path

**Current code:**

- `libs/tracker/tracker_utils.cc:412-425` — `TrackerUtils::color_to_string()`
- String color columns in `gtk2_ardour/tracker/grid.h:85-116`
- `Grid::read_colors()` — `grid.cc:1569-1581`
- Direct cairo overlay — `grid.cc:2099`

Possible existing facilities include:

- `Gtkmm2ext::gdk_color_from_rgb()` / `gdk_color_from_rgba()`
- `Gdk::Color::to_string()` or `gdk_color_to_string()`
- `Gtk::TreeModelColumn<Gdk::Color>` and GDK cell-renderer properties

The custom `#rrggbb` converter is already deliberately optimized, so repeated
per-cell GDK object construction should be avoided. A better simplification
would be to convert/cache colors once in `read_colors()` and reuse those
values.

Changing every model column to `Gdk::Color` is possible but a much larger and
not obviously worthwhile schema change.

**Priority:** low; the current implementation is correct and
performance-conscious.

### 13. Reuse `ARDOUR::RegionSortByPosition` where region sorting is needed

`TrackerUtils::region_position_less` (`libs/tracker/tracker_utils.h:86-90`,
`tracker_utils.cc:30-32`) duplicates `ARDOUR::RegionSortByPosition` from
`libs/ardour/ardour/region_sorters.h:26-30`. The tracker comparator still has
no production users, so removal is simpler than replacement.

### 14. Remove unused code rather than abstracting it

Static reference checks found no production users for the symbols below. All
are still present in the tree; the list has been expanded since the first
revision.

**`libs/tracker`**

| Symbol | Location |
|---|---|
| `TrackerUtils::is_pan_type`, `get_pan_param_types` | `tracker_utils.h:123-124`, `tracker_utils.cc:36-52` |
| `AutomationPattern::clone_actl`, `clone_param_actl`, `clone_alist` | `automation_pattern.h:58-60`, `automation_pattern.cc:92-108` |
| `BasePattern::clamp` | `base_pattern.h:253`, `base_pattern.cc:346-349` |
| `TrackerUtils::region_position_less` | `tracker_utils.h:86-90`, `tracker_utils.cc:30-32` |
| `BasePattern::row_at_time_min_delay`, `row_at_time_max_delay` | `base_pattern.h:156,164`, `base_pattern.cc:277,292` |
| `Pattern::sample_at_row_at_mti` | `pattern.h:87`, `pattern.cc:294` |
| `Pattern::find_next_on_note` | `pattern.h:102`, `pattern.cc:416` |
| `MidiNotesPattern::find_prev_off` | `midi_notes_pattern.h:153`, `midi_notes_pattern.cc:679` |
| `AutomationPattern::previous_row` | `automation_pattern.h:99`, `automation_pattern.cc:365` |
| `MidiNotesPattern::erase` (declaration only, no definition) | `midi_notes_pattern.h:262` |

**`gtk2_ardour/tracker`**

| Symbol | Location |
|---|---|
| `Grid::set_current_row_undefined` | `grid.h:597`, `grid.cc:4895` |
| `Grid::set_current_cursor_undefined` | `grid.h:578`, `grid.cc:4761` |
| `Grid::left_separator_colnum` | `grid.h:181`, `grid.cc:745` |
| `Grid::region_name_colnum` | `grid.h:183`, `grid.cc:757` |
| `Grid::automation_col_offset` | `grid.h:194`, `grid.cc:940` |
| `Grid::right_separator_colnum` | `grid.h:200`, `grid.cc:1033` |
| `Grid::redisplay_undefined_region_name` | `grid.h:231`, `grid.cc:1937` |
| `Grid::mk_blank` | `grid.h:897`, `grid.cc:5210` |
| `MainToolbar::rows_per_beat_up`, `rows_per_beat_down` | `main_toolbar.h:98-99`, `main_toolbar.cc:733,739` |
| `TrackToolbar::setup_label` | `track_toolbar.h:58`, `track_toolbar.cc:80` |
| `MidiTrackToolbar::print_widths` | `midi_track_toolbar.h:102`, `midi_track_toolbar.cc:592` |

The unused automation cloning functions are particularly worth removing:
`clone_alist()` currently returns the original list despite the surrounding
“deep copy” terminology, which is misleading even though the functions are
unused. `MainToolbar::rows_per_beat_step()` remains live (it backs the
increase/decrease-rows-per-beat actions); only the two wrappers above are
dead.

## Further observations

These were noticed while verifying the completed items; none is urgent.

- **Rows-per-beat counts still have two sources.** `RowsPerBeatChoice::rows`
  (`main_toolbar.cc:39-65`) duplicates
  `EditingContext::get_grid_beat_divisions()`. The field is still needed for
  the reverse lookup in `rows_per_beat_step()` (`main_toolbar.cc:745-759`), so
  the table must be kept in sync with the core mapping by hand.
- **Controller-item callbacks remain near-duplicates.**
  `MidiTrackToolbar::add_single_channel_controller_item` and
  `add_multi_channel_controller_item` (`midi_track_toolbar.cc:388-467`) still
  mirror `MidiTimeAxisView`'s versions (`midi_time_axis.cc:991-1088`); only
  the leaf visibility mechanism differs. Item 1 removed the bulk of the
  duplication, so this is a smaller, optional follow-up.
- **The automation menu is rebuilt on every click.**
  `TrackToolbar::build_automation_menu()` tears down and rebuilds the
  processor menu each time (`track_toolbar.cc:149-176`), with an existing
  `TODO could be optimized` at line 165. The stable registry introduced by
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

1. **Low-risk cleanup:** remove the dead code in item 14 (including the
   expanded list above) and the unused `region_position_less` comparator
   (item 13).
2. **Ownership:** convert `PatternPhenomenalDiff` to `std::unique_ptr`
   (item 10), then consider the other raw-owned collections deliberately.
3. **Behavior-sensitive work with tests:** move row arithmetic into tick space
   (item 11), especially at 7, 14, and 28 rows/beat.
4. **Optional polish:** cache color strings (item 12) and address the
   follow-ups in “Further observations”.
