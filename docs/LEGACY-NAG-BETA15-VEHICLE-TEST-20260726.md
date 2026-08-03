# Legacy NAG beta.15 Vehicle Test

## Build Under Test

- Version: `4.3.0-beta.15`
- Vehicle target: 2019 Model X
- Vehicle software: 2026.2.11
- Torque model: upstream beta.7 mean-reverting random walk
- Direction input: none; `0x129` is not required

## Active 0x370 Contract

- raw zero: 2050;
- random step: -40 to +40 raw units for every admitted native frame;
- mean reversion: `(2050 - currentRaw) / 16`;
- clamp: raw 1780-2500, approximately -2.7 to +4.5 Nm;
- byte 4: copied exactly from the native frame;
- counter: native low nibble +1, wrapping at 15;
- checksum: `(sum(byte0..byte6) + 0x73) & 0xFF`;
- DAS hands states 0 and 8: no output;
- native handsOnLevel 1: no output;
- no steering-angle or fixed-polarity input.

## Controlled Test

1. Flash the full image for first installation, or use the beta.15 OTA app only
   when the currently installed partition layout is already the matching 16 MB
   project layout.
2. Confirm Legacy FSD activation and smart speed offset with Legacy NAG off.
3. Start a 120-second recording with IDs:

   ```text
   118,370,399,3EE,488,5D9
   ```

4. Enable the Legacy NAG switch only in a safe, empty test area while keeping
   normal control of the steering wheel.
5. Record at least two complete warning cycles. Do not stop after the first
   state-3 transition.
6. Stop immediately after any EPAS, body-control, camera, CAN bus-off, steering,
   or unexpected disengagement warning.

## Acceptance Evidence

- `/status.legacyNag.torqueModel` is
  `beta7_mean_reverting_random_walk`;
- `randomWalkSteps` rises with `tx370`;
- TX byte 4 matches the preceding native RX byte 4;
- TX counter is native counter+1 and every TX checksum validates;
- transmitted raw torque remains within 1780-2500;
- DAS state 0/8 produces no `0x370` TX;
- no deterministic 0.8/1.2-second polarity pattern remains;
- compare state-3 recurrence interval and clearing duration against captures
  89-93, whose normal recurrence was approximately 54 seconds and median
  clearing duration approximately 2.74 seconds.

Vehicle effectiveness remains unverified until these recordings are returned
and analyzed.
