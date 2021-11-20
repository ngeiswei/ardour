# Tracker Editor

## Overview

The Tracker editor is a [music tracker](https://en.wikipedia.org/wiki/Music_tracker)
integrated into Ardour. It allows to visualize and edit midi notes, controls and
plugin automations with a tracker style interface.

## Videos

- [Presentation at Sonoj-2019](https://media.ccc.de/v/sonoj2019-1909-tracker-pianoroll) 
- [Small introduction](https://lbry.tv/@ngeiswei:d/Tracker-inside-Ardour:9)
- [Demo song](https://lbry.tv/@ngeiswei:d/Tracker-in-Ardour.-Song-demo,-blend-of-audio-and-midi-tracks:e)

## Status

WARNING!!! **Alpha** software, it may **crash** and you may **loose your work**.

## Build

Compile Ardour as you regularly would after checking out that branch, appending
the flag `--tracker-interface`, such as

```bash
./waf configure --tracker-interface
```

## TODO

- [ ] Fix automation of different processors
- [ ] Deal with `***` in a proper manner (using the  "Overwrite *" button)
- [ ] Final off note management fixes (look for NEXT)
- [ ] Have the playhead move only stop notes coming from the tracks, not the
      notes coming from midi input
- [ ] Avoid using `get_<something> ()` just use `<something> ()`
- [ ] Create PR
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
