# Ardour Tracker Editor

### Channel Column

In Dec mode, the channel column begins from 1, like in Ardour.  However, in Hex
mode, the channel column begins from 0.  For instance channel 1 is displayed as
channel 0 and channel 16 is displayed as channel F.

Please beware that the spinner for the default channel will remain in Dec mode.

### Glossary

- Location: a particular place on the tracker grid.

- Position: may refer to a region position, or the digit of a number, in that
            case the position 0 corresponds to the unit digit, the position 1
            corresponds to the ten digit, etc.  If the position is negative,
            then -1 corresponds on the tenth digit, -2 corresponds to the
            hundredth, etc.
