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

Just compile Ardour as you regularly would after checking out that branch.

## TODO

- [ ] Handle keypressed the right way (which includes passing on space to play/stop)
- [ ] Add tooltips to each cell, showing the parameter or note name, hex and
      dec values (with arbitrary precision in adequate).
- [ ] Fix automation of different processors
- [ ] Access to ardour via PublicEditor whenever possible
- [ ] Deal with `***` in a proper manner (using the  "Overwrite *" button)
- [ ] Have the playhead move only stop notes coming from the tracks, not the
      notes coming from midi input
- [ ] Final off note management fixes
- [ ] Fix all compile warnings
- [ ] Create wscript under tracker folder
- [ ] Create PR
- [ ] Take care of space bar pressing
- [ ] What to do when playing and step editing is on?
- [ ] Add copy/move, interpolate, etc + popup menu
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
