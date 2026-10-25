# Tracker Editor

## Overview

The Tracker editor is a [music tracker](https://en.wikipedia.org/wiki/Music_tracker)
integrated into Ardour. It allows to visualize and edit midi notes, controls and
plugin automations with a tracker style interface.

![Ardour + Tracker Editor Snap Shot](pics/Tracker%20Editor%20-%20Snapshot%20-%202025-10-25.png)

## Features

- Arbitrary temporal precision, number of rows per beat is not fixed.
- Positive and negative delay.
- Key bindings for all actions.
- Unlimited number of tracks, notes and automations.
- MIDI and audio tracks.
- Copy/paste.

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
