# Standalone TeamSpeak companion (R12)

This directory builds `task_force_vanguard_win64.dll`, an additional TeamSpeak 3 API 26 plugin. It does not import, replace, patch, load or forward exports from `TFAR_win64.dll`. Stock TFAR Beta remains installed and enabled. The separate Arma extension is `tfv_helmet_voice_x64.dll`.

## Implementation plan and evidence

1. **Complete:** preserve the accepted R11 sound by copying both DSP headers unchanged into `dsp/`.
2. **Complete:** pin the official TeamSpeak API 26 headers in `sdk/`; build the separate plugin and Arma IPC bridge.
3. **Complete:** implement strict local/remote state validation, mission/server scoping, expiry, and early per-speaker playback effects.
4. **Complete:** exercise production DLL exports against a mock TeamSpeak function table and the real compiled Arma bridge. This is an offline native integration test.
5. **Pending live acceptance:** enable stock TFAR and this companion together in TeamSpeak 3.6.2; run two Arma clients with the standalone addon; verify proximity, SW/LR radio, intercom/loudspeaker, decoder changes, ACE toggle, CBA settings, death/spectator, channel moves, disconnect/reconnect and mission restart. Offline tests do not establish those gameplay outcomes.

## Audio ownership

The only exported voice callback is `ts3plugin_onEditPlaybackVoiceDataEvent`. TeamSpeak supplies decoded remote 16-bit PCM at 48 kHz at this stage, before 3D processing. Stock TFAR processes its radio/distance/mute behavior later, in `ts3plugin_onEditPostProcessVoiceDataEvent`. This ordering is determined by callback stage, independent of plugin load order.

The companion does not export capture, post-process or mixed-playback callbacks and never sets mute, PTT, 3D position or volume. A listener with a DT decoder receives the original transmitted speech. A listener without a decoder gets the approved DT garble for mode 3; mode 4 gets the approved standard helmet filter. The original capture is unchanged. Stock TFAR's own local radio-speaker loopback remains unchanged.

Both approved effects support mono/stereo decoded input. Unsupported layouts are exact bypass. Each remote client has independent effect state. Changes of route/mode/settings, identity, channel, mission or connection reset state; an audio gap longer than 250 ms resets it too. The realtime callback uses `try_lock`: it bypasses instead of waiting for control work. It performs no TS API calls, allocations or IPC.

## Protocol and failure behavior

The plugin serves `\\.\pipe\TFV_HelmetVoice_<WindowsSessionId>` as a message pipe. The DACL grants only the current user's SID, client session IDs must match, remote clients are rejected, and first-instance creation prevents a second companion from claiming the same endpoint. Connected reads/writes have 40 ms deadlines. Stop signals interrupt waits; pending overlapped operations are cancelled and drained before their buffers are released. No `FlushFileBuffers` or unbounded client-read wait is used.

`PING` replies `TFV12`. An Arma state record is UTF-8/ASCII with no terminating NUL, at most 4095 bytes:

```
TFV12<TAB>serverUID<TAB>missionToken<TAB>mode<TAB>23 comma-separated settings
```

The server UID must match the currently selected TS connection. Read-only local stock TFAR metadata must report both `Connected to Game: [B]Yes[/B]` and `Playing: [B]Yes[/B]`. Modes are `0` (bare), `1` (DT decoder), `3` (DT decoder/scrambler), `4` (ordinary helmet), `-1` (inactive). Inactive includes master-disabled, dead/spectator, and mission end. Valid accepted records reply `OK`; invalid/context-mismatched records reply `ERR:STATE`. The 23 setting positions match `standalone/arma/addons/tfv_core/functions/fnc_syncSettings.sqf` (DT 15, then helmet 8). Strict finite/range validation rejects partial, extra and invalid records.

Local state expires after 3 seconds. Cached context expires after 1.5 seconds if the control worker stalls. The companion sends only its own mission token, mode and monotonic sequence through its own TS plugin command every 2 seconds and on change:

```
TFV12<TAB>missionToken<TAB>mode<TAB>sequence
```

The receiver obtains speaker identity from the authenticated TeamSpeak callback, verifies the invoker UID against TS's current client identity and requires the same channel. Payloads cannot name another speaker. Remote state expires after 6 seconds; duplicate/out-of-order messages cannot renew that expiry. Connection/channel callbacks immediately clear the relevant state. Missing, stale, inactive, mismatched or unknown state is exact audio bypass.

These are cosmetic/gameplay effects, not cryptographic voice security. All receiving players need the companion for consistent garbling. Current-channel commands cover normal shared-channel TFAR sessions; custom cross-channel whisper arrangements need separate validation/support. Stock TFAR logs foreign plugin commands as unknown, so its logs may contain harmless entries from the 2-second heartbeat. No log interception is attempted.

## Reproducible offline checks

Build the Release/x64 configurations of `bridge.vcxproj`, `plugin.vcxproj`, `bridge_tests.vcxproj`, and `companion_tests.vcxproj` with VS 2022 Build Tools (v143/Windows SDK). Output is `work/standalone-native/`. Run from the repository root:

```powershell
& ./work/standalone-native/bridge_tests.exe ./work/standalone-native/tfv_helmet_voice_x64.dll
& ./work/standalone-native/companion_tests.exe ./work/standalone-native/task_force_vanguard_win64.dll ./work/standalone-native/tfv_helmet_voice_x64.dll
```

The harness uses its own process, a mock TS function table, real production DLL exports and the real local pipe. It never attaches to TeamSpeak or Arma. It fails safely if an actual companion already owns the session pipe. It verifies exact bypass and route changes, authenticated UID checks, strict protocol/settings, independent DSP state, TTL/replay rejection, shutdown, and actual bridge/companion IPC. A simulated later mute verifies buffer composition only; it is not a substitute for TFAR gameplay testing.

## Provenance

- `sdk/`: unchanged official TeamSpeak plugin SDK headers from `https://github.com/teamspeak/ts3client-pluginsdk`, commit `4aa90a53aa150cbf81e13bc97e68c0431b26499f`; example `src/plugin.c` in that commit declares API 26. Copyright TeamSpeak Systems GmbH; existing header notices are retained.
- `dsp/DeathTrooperEffect.hpp` and `dsp/HelmetVoiceEffect.hpp`: byte-identical copies of the approved project DSP in `work/tfar-upstream/ts/src/`. Their original filename comments retain historical DTR labels; they do not change TFAR's protocol in this standalone plugin.
- Callback-stage reference: `https://teamspeakdocs.github.io/PluginAPI/client_html/ar01s18.html`.
- Stock TFAR callback/presence behavior checked in pinned upstream commit `fce977fa388bb527a989af1e162ade648c8d9d75`.
