# Legacy 0x045 Scroll DND beta.20 Vehicle Analysis

## Vehicle Result

The NAG warning is dismissed successfully, but beta.20 raises media volume.
Two 120-second captures were analyzed:

- `/Users/ziwind/Downloads/can_recording - 2026-07-27T222553.264.csv`
- `/Users/ziwind/Downloads/can_recording - 2026-07-27T222651.215.csv`

Both recordings contain valid CAN1 `0x045` traffic and no `0x045` TX error.
The generated positive frames pass CRC-8/J1850 and follow native counters.

## Warning Episodes

| Capture | Warning starts | Positive 1 | Positive 2 | Warning clears | Clear after positive 2 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 22:25:53 | 6.533 s | 6.534 s | 7.388 s | 7.413 s | 25 ms |
| 22:26:51 | 58.713 s | 58.714 s | 59.534 s | 59.559 s | 25 ms |
| 22:26:51 | 98.700 s | 98.701 s | 99.537 s | 99.570 s | 33 ms |

The first positive action in every episode is sent 820-854 ms before the
second action and does not clear `0x399` hands state 3. The second action is
followed by the warning clearing in 25-33 ms. Vehicle observation reports a
net one-step volume increase, so the evidence supports this accounting model:

1. Positive 1 reaches the media volume control: `+1` audible step.
2. Positive 2 is consumed by the active NAG prompt and clears it.
3. beta.20 stops with the audible `+1` left uncompensated.

The negative `0x045` RX activity later in the captures is physical driver
input used to lower the raised volume. It is not beta.20 TX traffic.

## Why beta.19 Lowered Volume

beta.19 compensated after every positive action rather than after observing
which action cleared the warning. In the common two-attempt episode its wire
sequence was:

```text
+1 (audible) -> -1 (audible) -> +1 (warning consumed) -> -1 (audible)
```

The first two actions cancel, the third clears NAG, and the final inverse is
an extra media step, producing the reported net `-1`.

## beta.21 Algorithm

The capture timing allows compensation before the retry without touching the
final action that clears the warning:

- After the first positive action, native idle arrives in 20-53 ms while the
  warning remains active for another 790+ ms. beta.21 sends one inverse action
  at that point, returning media volume immediately to its prior value.
- After the final positive action, `0x399` clears in 25-33 ms while the next
  native idle does not arrive until 97-100 ms. beta.21 observes the clear first
  and cancels the pending inverse action.

The expected common sequence is therefore:

```text
+1 -> warning still active -> -1 -> retry delay -> +1 -> warning clear -> stop
```

This makes the unsuccessful attempt volume-neutral and lets the final positive
action be consumed by the prompt. Every generated frame follows a fresh native
counter and CRC.

Driver wheel input, AP exit, stale context, OTA, mode change, counter conflict,
TWAI failure, or TX failure aborts compensation. Other firmware paths are not
changed.
