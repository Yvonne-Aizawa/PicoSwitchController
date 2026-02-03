# Command Reference

Commands are sent over USB serial. They are case-insensitive and newline-terminated. Lines starting with `#` are treated as comments.

## Buttons

The following button names are used across all button commands:

| Name | Button |
|------|--------|
| `a` `b` `x` `y` | Face buttons |
| `l` `r` | Shoulder buttons |
| `zl` `zr` | Trigger buttons |
| `plus` `minus` | +/- buttons |
| `home` `capture` | Home / Capture |
| `l_stick` `r_stick` | Stick clicks (L3/R3) |
| `dpad_up` `dpad_down` `dpad_left` `dpad_right` | D-pad |

Stick names for the `STICK` command: `l_stick`, `r_stick`.

---

## PRESS

Press and immediately release one or more buttons. All buttons are pressed in a single frame, then released in the next frame.

```
PRESS <button> [<button> ...]
```

**Examples:**
```
PRESS a
PRESS a b
PRESS dpad_up
```

## HOLD

Hold one or more buttons down. They remain pressed until explicitly released.

```
HOLD <button> [<button> ...]
```

**Examples:**
```
HOLD zr
HOLD a b x
```

## RELEASE

Release one or more currently held buttons.

```
RELEASE <button> [<button> ...]
```

**Examples:**
```
RELEASE zr
RELEASE a b x
```

## STICK

Set an analog stick to a position. Values range from `-1.0` to `1.0`, where `0.0` is center.

```
STICK <stick> <horizontal> <vertical>
```

- Horizontal: `-1.0` = left, `1.0` = right
- Vertical: `-1.0` = up, `1.0` = down

**Examples:**
```
STICK l_stick 0.5 0.0
STICK r_stick -1.0 1.0
STICK l_stick 0.0 0.0
```

## STATE

Set the entire controller state in a single command. Takes an 18-digit binary string where each digit (`0` or `1`) maps to a button, optionally followed by stick positions.

```
STATE <18 binary digits> [LH LV [RH RV]]
```

**Bit order (left to right):**

| Pos | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 |
|-----|---|---|---|---|---|---|---|---|---|---|----|----|----|----|----|----|----|-----|
| Button | A | B | X | Y | L | R | ZL | ZR | + | - | Home | Cap | L3 | R3 | D-Up | D-Down | D-Left | D-Right |

Stick values are optional floats in `[-1.0, 1.0]`. If omitted, sticks are not modified.

**Examples:**
```
STATE 000000000000000000
STATE 101100000000000000
STATE 100000000000000000 0.5 -1.0
STATE 100000000000000000 0.0 0.0 -1.0 0.0
```

## SLEEP

Pause command processing for a duration. Non-blocking (Bluetooth reports continue sending).

```
SLEEP <seconds>
```

**Examples:**
```
SLEEP 0.5
SLEEP 2
```
