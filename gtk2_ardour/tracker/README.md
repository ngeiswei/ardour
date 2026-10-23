# Next

- [ ] NEXT.2b: support audio region content and region automation
- [ ] Carefully go over all NEXT comments

# Tracker Editor

## Overview

The Tracker editor is a [music tracker](https://en.wikipedia.org/wiki/Music_tracker)
integrated into Ardour. It allows to visualize and edit midi notes, controls and
plugin automations with a tracker style interface.

## Architecture

The tracker is split in two layers:

- `libs/tracker` (`libtracker`): the model and logic. This holds the pattern
  classes (`Pattern`, `MidiTrackPattern`, `AudioTrackPattern`,
  `MidiRegionPattern`, `MidiNotesPattern`, automation patterns...) and the
  whole `*_phenomenal_diff` incremental redisplay machinery. It is free of any
  GTK / gtk2_ardour dependency; it talks back to the application through the
  small `TrackerContext` interface.
- `gtk2_ardour/tracker` (this directory): the GTK UI. This holds the `Grid`
  (`Gtk::TreeView`), the toolbars, the aligned headers, the `TrackerEditor`
  window (which implements `TrackerContext`) and the `SubgridSelector`.

The editor and the model therefore remain in sync only through
`TrackerContext` (session access, signal hookup, undo, redisplay requests).

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

- [ ] Add interpolate and such in pop-up menu.
- [ ] Have the playhead move only stop notes coming from the tracks, not the
      notes coming from midi input
- [ ] What to do when playing and step editing is on?
- [ ] Fix when trying to paste notes on values (add _note flag in std::string
      extension)
- [ ] Fix window focus (the tracker editor window should have focus right away)
- [ ] Support toggling visibility of regions and tracks, as well as reordering
      tracks and automations
- [ ] Add access from top menu Windows->Tracker Editor
- [ ] Try to replace as many gtk widgets by cairo widgets
- [ ] Support shift-tab to move from right to left
- [ ] Add piano keyboard display (see gtk_pianokeyboard.h)
- [ ] Transfer Ardour shortcuts, spacebar, etc
- [ ] Support layered regions
- [ ] Maybe only have Octave, Ch, Vel in main toolbar is there is a midi track
- [ ] Add play a line by pressing enter or so
