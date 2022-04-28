# autokill
Searches and kills windows by their title after a given delay.

## Usage
```cmd
.\autokill title [seconds]
```
Kills all windows with matching titles. `title` may be a ECMAScript regex. If no time is given, kills the windows immediately.