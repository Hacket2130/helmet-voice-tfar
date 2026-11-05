# Standalone addon correction

R12 runs alongside the official TFAR Beta Arma mod and TeamSpeak plugin. R11 was incorrectly packaged as a replacement; it is retained only as a labelled legacy prerelease.

## Download

- **Task-force-Vanguard-TFAR-addon-Setup-R12.exe**: Windows installer with update checks; includes the addon package for offline installation.
- **Task-force-Vanguard-TFAR-addon-R12.zip**: manual addon package, source and validation evidence.
- **Task-force-Vanguard-TFAR-addon-R12.ts3_plugin**: separate TeamSpeak companion.
- **Task-force-Vanguard-TFAR-addon-Source-R12.zip**: clean source and developer documentation.

## Install and migrate

Keep the official TFAR Beta mod and TeamSpeak plugin enabled. Add `@TFV_TFAR_Addon` in the Arma Launcher, enable CBA and your helmet mod, and install the separate Vanguard TeamSpeak companion. Enable both TeamSpeak plugins. ACE is optional for self-interaction toggles. All participants need the addon/companion; the server needs the addon/CBA and public key.

If R11 was installed, disable `@TFAR_DTR` and reinstall the official TFAR TeamSpeak package to restore its original DLL before using R12. The new setup uses a separate install folder and schema-2 update feed. The old R11 setup cannot perform this migration. R12 does not remove existing mods or restart applications.

The release contains only Vanguard's `tfv_core.pbo`, bridge extension and companion plugin. It contains no replacement TFAR PBO, pipe extension, radio assets or TFAR TeamSpeak DLL. The approved helmet and Death Trooper sound effects are preserved, including the CBA settings and ACE controls.

## Validation and limits

Development test build: native compilation with warnings as errors, 29 bridge checks, 96 companion/state/exported-callback/real-IPC checks, 115 retained DSP checks, 29 Arma source/config checks, PBO signature verification and 137 installer/backend assertions passed. The companion harness uses mocked TeamSpeak SDK functions. Full installer/backend evidence is included with the source release.

Live multiplayer audio timing, intelligibility, performance, CBA/ACE interaction, GUI acceptance and BattlEye/server acceptance remain untested. Normal common-channel TFAR use is the supported transport scope; custom cross-channel whisper configurations need separate validation. Every listener needs the companion; garbling is not secure encryption. Stock TFAR may log harmless unknown-command entries from the companion's heartbeat. Native DLLs and setup are not Authenticode signed.
