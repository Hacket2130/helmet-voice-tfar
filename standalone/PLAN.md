# R12 standalone addon correction

The user intended an addon alongside stock TFAR Beta. R11 incorrectly shipped a replacement tfar_core.pbo and TFAR_win64.dll. Preserve that history but stop presenting it as the required installation model.

1. Retain approved DSP and CBA/ACE behavior; separate all integration from TFAR source. Stock TFAR mod and TS plugin must remain enabled and byte-unchanged.
2. Build @TFV_TFAR_Addon with only its own tfv_core.pbo and tfv_helmet_voice_x64.dll. Use TFAR public currentUnit/serverUID APIs, a server-generated mission token and bounded local heartbeat.
3. Build a separate task_force_vanguard_win64.dll TeamSpeak companion using official SDK26. Process remote audio in onEditPlaybackVoiceDataEvent before stock TFAR's postprocess stage. Preserve capture and normal TFAR routing. Broadcast only sender-owned helmet state through SDK plugin commands.
4. Validate protocol, per-client state, timeout/reset, server/mission/identity boundaries, PCM routes, DLL loading, coexistence filenames and actual IPC. Keep DSP headers byte-identical and rerun appropriate existing DSP checks.
5. Package/sign only addon PBO, own extension and own TS package. Rework setup for schema2 addon layout, independent install root and instructions enabling both stockTFAR and companion. Provide explicit R11 rollback steps; do not silently reinstall or remove anything.
6. Inspect package exclusions and signatures, run installer safety suite against real bundle, publish R12 with correction/migration notes, verify publicfeed/download. Keep live multiplayer/CBA/ACE/audio timing and GUI acceptance open until actually exercised.

Known boundaries: current-channel TS state transport requires common-channel TFAR use; custom cross-channel whisper setups need separate acceptance. Each listener needs the companion. Missing/expired context bypasses effects. This is voice presentation, not secure encryption.

Status at package sealing: steps 1-4 and the addon/signing portion of step 5 are implemented and pass offline checks. Step 6 installer/public verification is recorded in the release validation report after the immutable addon ZIP is sealed. Live acceptance remains open regardless of release publication.

Post-seal: schema-2 installer compiled and all 137 backend assertions passed against the sealed bundle. Source and binary publication plus live update-feed/download verification are the remaining delivery steps.

Delivery status: R12 source and six release assets published; public asset sizes/digests and source tag verified. Live update-feed/download/install verification was attempted but tool dispatch stalled and was cancelled without a returned result. This and the existing live gameplay/GUI acceptance gates remain open.
