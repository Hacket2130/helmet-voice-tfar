# Standalone Arma addon handoff

Task force Vanguard TFAR addon R12 lives alongside the original TFAR Beta mod and its original TeamSpeak plugin. This directory supplies only `tfv_core.pbo`, with prefix `tfv\core`, and the mod metadata. The separate native extension and companion TeamSpeak plugin are built elsewhere. Never copy or patch `tfar_core.pbo`, replace TFAR's pipe DLLs, or assign to its compiled functions.

## Implementation sequence and status

1. Retained the accepted CBA settings, LLTE helmet roots, inheritance matching, state selection and ACE self-actions under the existing `DTR_` names. This preserves saved settings and choices. Complete.
2. Added a server-generated mission token, unique CfgPatches/PBO identifiers, local heartbeat and standalone extension protocol. Complete.
3. Added stock TFAR/legacy-replacement checks and local diagnostic output. Complete.
4. Run `Validate-Source.ps1` to compare retained behavior/defaults against the bundled `Preserved-Behavior-Baseline.json` and compile the config. The manifest pins SHA256 text hashes and the original 23-setting schema; it needs no legacy addon source. Hashes normalize line endings only. Results are written to `validation/source-checks.json`; the temporary compiled config is deleted and excluded from source. This is source/config evidence, not a runtime SQF test.
5. Integrate the separate native DLL/plugin and package only Vanguard-owned files. Then test with two real Arma/TeamSpeak clients, including JIP, ACE changes, server/channel switching and reconnects. These live checks are still required.

## Runtime responsibilities

- The server runs the pre-init and broadcasts `TFV_missionToken` using `publicVariable`, which persists its current value for JIP clients. All clients and the server must load this addon. Dedicated servers/headless clients never call a native extension.
- Each interactive client reads only its own `TFAR_fnc_currentUnit`, including TFAR's Zeus remote-control handling. The addon does not enumerate other players or infer TeamSpeak identities from player names.
- The existing public APIs `TFAR_fnc_isTeamSpeakPluginEnabled`, `TFAR_fnc_getTeamspeakPluginVersion` and `TFAR_fnc_getTeamSpeakServerUID` identify the original working TFAR connection. The old customized `-dtr.` plugin and `DTR2:` position-core patch are rejected to prevent two helmet processors running together. Reading the latter compiled function does not replace it.
- `DTR_fnc_sendState` sends local state every 0.5 seconds. CBA DSP changes also request an immediate send. ACE state/helmet changes are observed by the next heartbeat.
- The companion associates remote helmet state using TeamSpeak's own authenticated plugin-command sender identity. Arma does not transport another user's identity.
- Original TFAR keeps handling radio, position, range, channels and its own audio effects. Vanguard supplies only the additional helmet effect through its own extension/plugin.

## Native protocol

Extension basename: `tfv_helmet_voice`; Windows x64 filename: `tfv_helmet_voice_x64.dll`.

`"tfv_helmet_voice" callExtension "PING"` must return exactly `TFV12` only if the companion is reachable. PING alone does not establish that the active Arma mission matches the selected TeamSpeak server. A successful STATE request is the stronger check.

The state request is the following raw string with four tab separators, no trailing tab/newline, and 23 comma-separated numeric values:

```text
TFV12<TAB>stockTFARserverUID<TAB>missionToken<TAB>mode<TAB>settingsCSV
```

`DTR_fnc_sendState` calls the extension with that raw record. There is no additional literal `STATE` prefix. The server UID is a nonempty base64-character string of at most 128 characters. The mission token contains 1–64 characters from `[A-Za-z0-9_-]`. The companion returns exactly `OK` only after validating the record and matching the selected TeamSpeak server UID. The addon treats every other response as unavailable.

| Mode | Meaning |
| --- | --- |
| -1 | Inactive: bypass all Vanguard processing |
| 0 | Active ordinary listener; can hear another Death Trooper scrambled |
| 1 | Death Trooper decoder, outgoing microphone scrambler disabled |
| 3 | Death Trooper decoder and outgoing microphone scrambler enabled |
| 4 | Ordinary enclosed helmet speaker profile; no Death Trooper decoder |

Dead/spectating/nonplaying clients, mission completion, or the master setting being disabled produce mode -1. Missing token, original TFAR plugin or server UID stops successful heartbeats. The native local TTL of 3 seconds clears stale state; companion broadcasts occur on change and at most every 2 seconds, with remote TTL 6 seconds. The native side owns those expiry mechanisms. An unexpected Arma exit therefore does not leave an indefinitely enabled helmet effect.

Settings retain the exact prior order and defaults. Boolean hard-gate is encoded as 1 or 0. Nonfinite or wrong-type local values fall back to defaults; the companion also validates numeric ranges.

| Index | CBA variable | Default |
| --- | --- | --- |
| 0 | DTR_reverbMix | 0.25 |
| 1 | DTR_reverbDecay | 0.35 |
| 2 | DTR_highPass | 150 |
| 3 | DTR_pitch | -6 |
| 4 | DTR_ampDrive | 32 |
| 5 | DTR_metallic | 0.75 |
| 6 | DTR_screech | 0.25 |
| 7 | DTR_staticAmount | 0.65 |
| 8 | DTR_bitDepth | 8 |
| 9 | DTR_ringAmount | 0.12 |
| 10 | DTR_hardGate | 1 |
| 11 | DTR_gateThreshold | -48 |
| 12 | DTR_gateHold | 1 |
| 13 | DTR_lowPass | 3200 |
| 14 | DTR_tailLength | 0.22 |
| 15 | DTR_helmetHighPass | 623 |
| 16 | DTR_helmetLowPass | 3100 |
| 17 | DTR_helmetDrive | 2.4 |
| 18 | DTR_helmetMix | 1.0 |
| 19 | DTR_helmetResonanceFrequency | 2000 |
| 20 | DTR_helmetResonanceGain | 3 |
| 21 | DTR_helmetClarityFrequency | 2500 |
| 22 | DTR_helmetClarityGain | 2 |

The ordinary helmet profile retains no added clicks or static. The separate Death Trooper profile retains its accepted garble/static/tail controls.

## Diagnostics and acceptance boundaries

Run `call DTR_fnc_diagnose` locally in the Arma debug console. It returns and logs the stock plugin state/version/server UID, separate companion PING, mission token, last state acceptance, active mode and helmet matching. A companion PING with a failed/missing stock plugin is not success. `CONNECTED_ACTIVE` requires an `OK` state response; `CONNECTED_INACTIVE` means the same transport is working while the effect is bypassed.

Source validation does not establish SQF runtime execution, correct audio callback ordering, playable multiplayer, server signature acceptance or subjective audio quality. Verify those separately with the original TFAR mod and plugin enabled. Every listener needs the companion for the effect to be audible; this is roleplay voice processing, not cryptographic protection against clients that omit or alter it.

## Development log

- 2026-11-05: Copied only Arma-side helmet behavior into the new `tfv_core` namespace/prefix. Replaced custom TFAR plugin setting transport with the separate TFV12 record, added mission scoping/expiry support and rejected the earlier replacement build. Left the old `src/` and released packages unchanged for historical comparison.
- 2026-11-05: Added reproducible preservation/boundary checks and compiled `config.cpp` with the installed Arma Tools CfgConvert. Live client/TeamSpeak work was not performed by this implementation step.
- 2026-11-05: Replaced the validator's external legacy-source dependency with a pinned baseline manifest. Rechecked all 29 assertions using only a standalone copy and automatically removed its validation-only config binary.
