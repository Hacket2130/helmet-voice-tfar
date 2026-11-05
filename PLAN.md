# Developer plan

## R11 baseline

The R11 standard-helmet audio is approved. Preserve its 623–3100 Hz filter, 2 kHz resonance, +2 dB clarity at 2500 Hz and mild amplifier drive. Standard clicks/static remain absent. Preserve Death Trooper garble/decoding, CBA controls and ACE routing.

The installer contains this unchanged R11 package. Its public name is Task force Vanguard TFAR addon; legacy internal identifiers remain stable for compatibility.

## Delivery steps

1. Build and validate the mod/plugin together: completed, 115 native and 15 package checks.
2. Build the offline installer with guarded GitHub update downloads and immutable version directories: completed, 130 backend assertions.
3. Verify branded artifacts and publish source, license and build instructions: completed at source commit 77d5c376b123ab5922ec3cf33a93543423c709a7.
4. Publish r11 with installer, native TS package, mod ZIP, source ZIP, checksums and update.json: completed. All six remote asset hashes/sizes and the latest/tag endpoints verified. Actual installer-backend feed/download/install/readback passed in an isolated test folder.
5. Complete manual GUI, multiplayer, CBA/ACE, performance and Death Trooper intelligibility acceptance separately. Do not infer these from build/backend checks.

## Future releases

Use a higher integer release number and new filenames/tags. Keep old releases and signing keys separate; never include private keys or recordings. Follow installer/README.md and update validation records with actual evidence. The updater uses this repository's latest normal GitHub release; prerelease channels require separate implementation.
