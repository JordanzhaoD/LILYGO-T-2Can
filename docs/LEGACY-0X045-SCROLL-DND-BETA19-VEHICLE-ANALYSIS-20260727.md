# Legacy 0x045 Scroll DND beta.19 Vehicle Analysis

## Result

Vehicle validation confirms that the Legacy Model X DND path is effective:

- CAN1 `0x399` hands-on warning states trigger the controller.
- CAN1 `0x045` positive left-wheel actions are valid and accepted.
- The warning is dismissed immediately in normal use.
- The beta.19 inverse action causes one music volume-down step after dismissal.

Capture analyzed:

`/Users/ziwind/Downloads/can_recording - 2026-07-27T030245.470.csv`

The recording contains 30,448 frames over 119.992 seconds. It includes 924
CAN1 `0x045` RX frames, 769 CAN2 `0x292` gateway copies, ten CAN1 `0x045` TX
frames, and no `0x045` TX error.

## Event Evidence

| Episode | Warning starts | Positive TX | Warning clears | Inverse TX | Finding |
| --- | ---: | ---: | ---: | ---: | --- |
| 1 | 16.546 s | 16.547 s | 17.500 s | 17.586 s after retry | Inverse occurs after clear |
| 2 | 35.753 s | 35.754 s | 36.683 s | 36.762 s after retry | Inverse occurs after clear |
| 3 | 84.924 s | 84.926 s | 84.952 s | 84.961 s | Inverse occurs 9 ms after clear |

All generated payloads pass CRC-8/J1850 and use the expected counter. Positive
frames observed in the CAN2 `0x292` mirror prove gateway acceptance. The third
episode is the cleanest causal sample: the positive action is followed by the
warning state changing from 3 to 1 in 26 ms, while the inverse action is sent
9 ms later. At that point there is no warning UI left to consume the inverse,
so the media system applies it as a real volume-down action.

The capture also contains later positive `0x045` RX activity around 115-118 s
without firmware TX records. Those are physical wheel actions and are not part
of the DND sequence.

## beta.20 Correction

The corrected sequence is:

1. Detect a new actionable `0x399` warning.
2. Derive one `+1` left-wheel frame from fresh native CAN1 `0x045`.
3. Follow the native counter and recalculate CRC-8/J1850.
4. Wait for the next native idle frame and for `0x399` to report the result.
5. Stop immediately when the warning clears. Do not send an inverse action.
6. If the warning remains active, retry after 800 ms, with a maximum of three
   positive attempts in the same episode.

The inverse and balanced-pair diagnostic keys remain in the JSON response for
API compatibility, but beta.20 does not generate inverse actions.

## Scope

This correction changes only the Legacy CAN1 `0x045` DND state machine and its
diagnostic wording. It does not alter Legacy activation, smart speed offset,
the `0x370` engine, recorder behavior, network configuration, NVS storage, or
mobile navigation.

