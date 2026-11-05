# Task force Vanguard TFAR addon

> **R11 packaging correction:** the published R11 build is a TFAR replacement fork, not an addon that runs alongside stock TFAR. It does not meet the intended addon installation model. Do not load R11 together with stock TFAR. A separate Arma addon and TeamSpeak companion are being built so both original TFAR components stay enabled. R11 is retained as a historical build; do not use its installer for the standalone-addon setup.

An unofficial Windows x64 TFAR Beta fork for Arma 3 and TeamSpeak 3, with Long Live the Empire helmet recognition, CBA settings and optional ACE self-interaction controls.

**[Download the installer](https://github.com/Hacket2130/helmet-voice-tfar/releases/download/r11/Task-force-Vanguard-TFAR-addon-Setup-R11.exe)** · **[Releases and manual downloads](https://github.com/Hacket2130/helmet-voice-tfar/releases)**

The installer includes the R11 Arma mod and TeamSpeak package. It can check GitHub Releases when opened, download a newer version after you choose it, verify its SHA256 and keep previous releases. First installation works offline.

## Setup

1. Run `Task-force-Vanguard-TFAR-addon-Setup-R11.exe` and choose **Install bundled R11**.
2. Choose **Install TeamSpeak plugin**. This opens TeamSpeak's normal installer. Back up the existing TFAR plugin, complete installation and restart TeamSpeak when convenient. Enable only **Task Force Arrowhead Radio - Helmet Voice R11**, the unchanged plugin's display name.
3. Choose **Copy mod path**, then add it with Arma Launcher **Mods > Local mod**. Disable the original TFAR Beta for that preset. Enable CBA_A3 and Long Live the Empire; ACE adds the wearer toggles.
4. Keep clients and servers on matching mod/plugin versions. After updating, install the new TeamSpeak package and change Arma's Local mod entry to the new version folder.

Reopen `Task-force-Vanguard-TFAR-addon-Setup.exe` in the install folder to check updates. Checks on opening can be disabled. The app never restarts Arma or TeamSpeak and has no background service or scheduled task. Standalone `.ts3_plugin` installation is also available from Releases.

## Sound and controls

- Standard sealed helmets use a 623–3100 Hz band, a 2 kHz resonance, mild amplifier distortion and a +2 dB clarity boost at 2500 Hz. No generated clicks or static.
- Death Trooper speech is scrambled, deeper and metallic for other listeners, while Death Trooper helmet wearers decode it. Ordinary sealed helmets do not gain decoding.
- CBA offers 32 policy/audio controls; ACE provides independent helmet-speaker and scrambler toggles. Server and mission overrides are supported.

R11's standard-helmet sample was approved on 2026-11-05. The mod passed 115 native DSP/protocol checks and 15 package checks. **Live multiplayer, CBA/ACE operation, performance and Death Trooper intelligibility still need in-game acceptance.** This is a development test release.

## Developers

- [Installer architecture, build and release instructions](installer/README.md)
- [Installer validation](installer/VALIDATION.md)
- [Mod source build instructions](mod-source/BUILD.md)
- [Development plan](PLAN.md) and [delivery log](DEVELOPMENT_LOG.md)

The immutable mod ZIP includes its upstream patch, source, tests, licenses and historical validation. The setup executable is not Authenticode signed. Update trust is HTTPS plus this GitHub repository; SHA256 checks verify asset integrity, not an independent publisher identity. The inherited stock TFAR notification is separate from Task force Vanguard TFAR addon's updater.

Based on [Task Force Arrowhead Radio](https://github.com/michail-nikolaev/task-force-arma-3-radio), copyright Michail Nikolaev and contributors, under the [Arma Public License Share Alike](https://www.bistudio.com/community/licenses/arma-public-license-share-alike). Upstream commit: `fce977fa388bb527a989af1e162ade648c8d9d75`. This project is not affiliated with Arma 3, TeamSpeak, TFAR, CBA, ACE or Long Live the Empire.
