# Task force Vanguard TFAR addon

**R12 is a standalone addon. Keep the official TFAR Beta Arma mod and TeamSpeak plugin enabled.** R11 was incorrectly packaged as a replacement and is retained only as a labelled legacy prerelease.

[Download Windows setup R12](https://github.com/Hacket2130/helmet-voice-tfar/releases/download/r12/Task-force-Vanguard-TFAR-addon-Setup-R12.exe) · [Release and manual downloads](https://github.com/Hacket2130/helmet-voice-tfar/releases/tag/r12)

The setup includes offline installation and optional GitHub update checks. Checks run while the setup app is open; updates install when selected. No background service, application restart or automatic R11 migration is added.

Current implementation and build instructions are in `standalone/`. Other source folders are retained as historical R11 code, not R12 runtime dependencies. The release contains no stock TFAR binaries, private signing keys or user audio recordings.

## Install

1. Keep the official TFAR Beta Arma mod and official TFAR TeamSpeak plugin installed and enabled.
2. Add `@TFV_TFAR_Addon` as an additional Arma Launcher Local mod. Enable CBA_A3 and your helmet mod (Long Live the Empire has built-in mappings). Enable ACE for self-interaction toggles.
3. Install `teamspeak/task_force_vanguard.ts3_plugin`. TeamSpeak should show **Task force Vanguard TFAR addon** alongside the official TFAR plugin. Enable both. Restart TeamSpeak yourself if its package installer requests it.
4. All participating clients need the addon and companion. The server needs the addon/CBA to distribute the mission token and the public signing key from `keys`. Server/mission CBA overrides remain supported.

## If you installed R11

R11 was incorrectly packaged as a TFAR replacement. Disable its `@TFAR_DTR` local mod. Enable the official TFAR Beta Workshop mod and reinstall its official TeamSpeak package to restore the original TFAR DLL. Then add R12 as described above. Keep old release files for rollback. The R12 installer does not remove files, change launcher presets, reinstall TFAR or restart applications automatically.

R12 uses an independent install root and schema-2 update feed. The old R11 setup cannot migrate this architecture automatically; use the R12 setup. R11 is retained as a clearly labelled legacy prerelease outside the latest update channel.

## How it works

The Arma addon uses TFAR's public current-unit and server-UID functions, sends only its local unit's helmet/toggle/settings through its own `tfv_helmet_voice_x64.dll`, and leaves TFAR's messages untouched. The separate TeamSpeak companion announces its own helmet state using TeamSpeak plugin commands. Receivers identify each sender from TeamSpeak's callback identity, require the same mission/server context and fresh heartbeats, and process that sender's decoded voice before stock TFAR applies positional/radio processing.

The standard speaker profile retains the approved R11 filters and slight distortion, without generated clicks or static. Death Trooper helmets decode each other; other supported listeners hear the retained deeper garble and metallic tail. CBA controls and ACE toggles retain their setting names so saved mission settings can be reused.

## Limits and validation

This is a development test build. Compilation, native/unit/IPC/package/installer checks do not establish live multiplayer acceptance. Actual CBA/ACE operation, audio timing with both plugins, performance and Death Trooper intelligibility need an in-game session.

State announcements use the current TeamSpeak channel, matching standard common-channel TFAR operation. Custom cross-channel whisper arrangements are not validated. Each listener needs this companion; the effect is not secure encryption. Missing, stale, disconnected or mismatched context bypasses the effect, and no captured microphone data is changed or uploaded by the addon. TFAR's own local microphone-to-radio-speaker monitor path stays under TFAR's control.

Stock TFAR logs foreign plugin commands, so Vanguard's two-second state heartbeat adds harmless unknown-command entries to TFAR's TeamSpeak log. It does not change TFAR's routing or settings.

The Windows setup and native DLLs are not Authenticode signed. Public Arma PBO signatures are included; server signature acceptance and BattlEye loading remain live-environment checks. Do not disable game or OS protections to load an unaccepted build.

## Source

`Source/standalone` contains the independent addon, extension, companion, SDK headers, installer and tests. The copied DSP headers are byte-identical to the approved R11 implementations. The original R11 fork remains historical and is not a runtime dependency. See `PLAN.md`, `DEVELOPMENT_LOG.md` and `Validation/` for exact evidence.

Based on TFAR/helmet DSP work under the Arma Public License Share Alike; upstream TFAR attribution is preserved in LICENSE.md. TeamSpeak SDK headers are from the official API26 repository and retain their notices. This project is unofficial and unaffiliated with TFAR, TeamSpeak, Bohemia Interactive, CBA, ACE or Long Live the Empire.
