# Ardour Tracker Editor

## Overview

The Tracker editor is a [music tracker](https://en.wikipedia.org/wiki/Music_tracker)
integrated into Ardour. It allows to visualize and edit midi notes, controls and
plugin automations with a tracker style interface.

The important point is that it is fully integrated to Ardour in the sense that
any change on the pianoroll view will affect the tracker view and vice versa.

### Channel column

In Dec mode, the channel column begins from 1, like in Ardour.  However, in Hex
mode, the channel column begins from 0.  For instance channel 1 is displayed as
channel 0 and channel 16 is displayed as channel F.

Please beware that the spinner for the default channel will remain in Dec mode.

### Note column

### Hints

The Tracker Editor provides a number of hints when the mouse cursor hovers over
certain elements of the grid and its parameters.

#### Cell



### Glossary

- *Pattern*: in Protracker a pattern in a table of rows, usually 64, and 4
  columns representing notes, samples, volumes and effects for each of the 4
  channels of the PAULA soundchip of the Amiga.  Each row corresponds to a time
  index, usually measuring quaters of beat since the start of the pattern, on
  the time line of that pattern.  In the context of the Ardour Tracker Editor
  however a pattern is any collection of timestamped events, such as notes,
  their attributes and automation values.  From the user's perspective a
  pattern can be viewed as a tracker-like representation of a region.  From the
  developer's perspective it is more fine-grained than that.

- *Grid*: the collection of all patterns of all selected regions for the Tracker
  Editor, represented as a whole table, in the same spirit as a pattern in
  Protracker.

- *Cell*: a particular cell on the tracker grid.

- *Location*: the place of a particular cell on the tracker grid.

- *Position*: may refer to a region position, or the digit of a number, in that
  case the position 0 corresponds to the unit digit, the position 1 corresponds
  to the ten digit, etc.  If the position is negative, then -1 corresponds on
  the tenth digit, -2 corresponds to the hundredth, etc.

## Shortcut Keys

The tracker editor has a number of configurable keyboard shortcuts.  They are
active only while the tracker editor window has focus.  In the tables below,
`Ctrl` is Ardour's `Primary` modifier and `Alt` is the `Secondary` modifier;
on macOS these are `Command` and `Option` respectively.

### Mode toggles

| Shortcut | Action |
|----------|--------|
| `Alt+E` | Toggle step editing |
| `Alt+C` | Toggle chord mode |
| `Alt+D` | Toggle overwrite default |
| `Alt+X` | Toggle overwrite existing |
| `Alt+S` | Toggle overwrite * |
| `Alt+P` | Toggle sync playhead |
| `Alt+J` | Toggle jump |
| `Alt+W` | Toggle wrap |
| `Alt+H` | Toggle hexadecimal (Hex) display |

### Value adjustments

`Ctrl` increases the value, `Ctrl+Shift` decreases it.

| Parameter | Increase | Decrease |
|-----------|----------|----------|
| Step size | `Ctrl+U` | `Ctrl+Shift+U` |
| Octave | `Ctrl+O` | `Ctrl+Shift+O` |
| Channel | `Ctrl+H` | `Ctrl+Shift+H` |
| Velocity | `Ctrl+E` | `Ctrl+Shift+E` |
| Delay | `Ctrl+D` | `Ctrl+Shift+D` |
| Precision | `Ctrl+P` | `Ctrl+Shift+P` |
| Position | `Ctrl+I` | `Ctrl+Shift+I` |
| Rows per beat | `Ctrl+R` | `Ctrl+Shift+R` |

The rows-per-beat shortcuts cycle through the allowed settings (1, 2, 3, 4,
5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28 and 32 rows per beat), stopping at the
first and last values.

### Cut, copy and paste

These operate on the selected subgrid (the rectangular selection made by
Shift-clicking and dragging over the grid), not on the system clipboard.  They
are also available from a popup menu, opened by right-clicking anywhere on the
tracker editor window (except where the right-click is already used, such as in
a cell being edited).

| Shortcut | Action |
|----------|--------|
| `Ctrl+X` | Cut the selected subgrid |
| `Ctrl+C` | Copy the selected subgrid |
| `Ctrl+V` | Paste |
| `Ctrl+Shift+V` | Paste overlay (paste without clearing existing content) |

### Changing the shortcuts

The shortcuts are configurable with Ardour's keyboard shortcut system.  Open
the **Keyboard Shortcuts** window (from the **Window** menu) and select the
**Tracker** tab.  To change a shortcut, select the action in the list and then
press the desired key sequence.  To remove a shortcut, select the action and
click the **Remove shortcut** button.  The **Reset Bindings to Defaults**
button restores all shortcuts to the defaults listed above.


