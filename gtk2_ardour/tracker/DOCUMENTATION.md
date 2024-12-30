# Ardour Tracker Editor

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

