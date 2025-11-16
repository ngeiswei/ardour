# Next

- [ ] NEXT.4: fix last issue with two consecutive on notes and nearby off note
- [ ] NEXT.3: make sure `***` is correctly supported all over the place
      (overwrite with notes, don't forget automations)
- [ ] NEXT.2: reenable tracker for audio tracks
- [ ] Carefully go over all NEXT comments

# Tracker Editor

## Overview

The Tracker editor is a [music tracker](https://en.wikipedia.org/wiki/Music_tracker)
integrated into Ardour. It allows to visualize and edit midi notes, controls and
plugin automations with a tracker style interface.

## Videos

- [Tracker Demo (a call for help)-Jan 2022](https://odysee.com/@ngeiswei:d/ardour-tracker-interface-on-2022-01-24-17-37:9)
- [Presentation at Sonoj-2019](https://media.ccc.de/v/sonoj2019-1909-tracker-pianoroll)
- [Small introduction](https://lbry.tv/@ngeiswei:d/Tracker-inside-Ardour:9)
- [Demo song](https://lbry.tv/@ngeiswei:d/Tracker-in-Ardour.-Song-demo,-blend-of-audio-and-midi-tracks:e)

## Status

WARNING!!! **Alpha** software, it may **crash** and you may **loose your work**.

## Build

Compile Ardour as you regularly would after checking out that branch, appending
the flag `--with-tracker-editor`, such as

```bash
./waf configure --with-tracker-editor
```

## Documentation

A documentation can be found [here](DOCUMENTATION.md).

## TODO

- [ ] Disable *Editing* when the window is hidden
- [ ] Final off note management fixes (look for NEXT)
- [ ] Have the playhead move only stop notes coming from the tracks, not the
      notes coming from midi input
- [ ] Avoid using `get_<something> ()` just use `<something> ()`
- [ ] Try to get to the bottom of `foo ()` vs `foo()`
- [ ] Remove `Route::get_control`
- [ ] Create PR
- [ ] All the code for modifying notes and such should be moved outside of
      Grid.  See "TODO: move the following outside of Grid" in grid.cc for
      example.
- [ ] Create shortcut key to move to automation vs note
- [ ] Add shortcut key to cycle through group index
- [ ] Set position when clicking on the digit position (if the cell is already selected)
- [ ] Add row index column (very first one)
- [ ] Fix situation when the cursor on the current track is different than the
      selected track
- [ ] Add option for velocity ramp
- [ ] Handle keypressed the right way (which includes passing on space to play/stop)
- [ ] Convert direct call to action when possible, see
      `gtk2_ardour/editor_actions.cc`, like `nudge-later`.
- [ ] What to do when playing and step editing is on?
- [ ] Fix when trying to paste notes on values (add _note flag in std::string
      extension)
- [ ] Add popup menu with copy/paste, interpolate, etc
- [ ] Maybe save octave value per each track, possibly more
- [ ] Support arbitrary number of tracks (put all properties in tracker_column)
- [ ] Fix alignment with track toolbar
- [ ] Fix grid focus. When click on a button, bring focus immediately back to
      the grid.
- [ ] Fix window focus (the tracker editor window should have focus right away)
- [ ] Support toggling visibility of regions and tracks
- [ ] Add access from top menu Windows->Tracker Editor
- [ ] Try to replace as many gtk widgets by cairo widgets
- [ ] Support shift-tab to move from right to left
- [ ] Add shortcut for parameters, steps, etc
- [ ] Add piano keyboard display (see gtk_pianokeyboard.h)
- [ ] Transfer Ardour shortcuts, spacebar, etc
- [ ] Support layered regions
- [ ] Drawn horizontal seperator to show the begin and end or a region (maybe
      insert slim row, cause it seems one cannot alter the grid line colors).
- [ ] Maybe only have Octave, Ch, Vel in main toolbar is there is a midi track
- [ ] Use signals whenever possible (instead of for instance calling
      Grid::redisplay_grid) to not block. This can make ardour crash when jack
      buffer size is too short.
- [ ] Add play a line by pressing enter or so
