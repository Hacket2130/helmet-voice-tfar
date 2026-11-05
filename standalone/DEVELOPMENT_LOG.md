# R12 standalone addon development

## 2026-11-05
- User identified the architectural mismatch: R11 replaces TFAR despite the intended addon scope. Acknowledged the mistake and added a prominent public README correction.
- Confirmed stock TFAR processes remote voice in onEditPostProcessVoiceDataEvent; a companion can use the earlier per-client onEditPlaybackVoiceDataEvent without replacing TFAR or depending on plugin ordering within the same callback stage.
- Confirmed public TFAR currentUnit/serverUID APIs and SDK authenticated invoker identity allow each client to report its own helmet state through an independent local bridge, avoiding POS patches and nickname matching.
- Implementation split: standalone Arma/CBA/ACE addon; native TeamSpeak companion with retained DSP; own native extension and packaging; independent schema2 installer. All R11 artifacts and live installations remain untouched.
- Public R11 release was retitled as a legacy TFAR replacement, marked prerelease and removed from the latest channel. Existing downloads and tag were preserved.
- Both native DLLs compile with VS2022 v143, C++17, static runtime and warnings as errors. Actual exported bridge/DLL tests passed: 29 bridge checks and 96 companion protocol/state/audio-callback/IPC checks. The companion harness mocks TeamSpeak SDK calls; it does not launch TeamSpeak or prove live compatibility.
- Approved DSP headers retain their R11 SHA256 values. Existing suites passed 82 Death Trooper/protocol/channel checks and 33 standard-helmet checks; no sound redesign occurred in R12.
- Arma source preservation/boundary checks and CfgConvert compilation passed. Only tfv_core.pbo was built and signed with the new TFV_20261105_R12 key. Initial signing inside the sandbox could not open the Windows crypto provider; the same staged PBO was signed and verified successfully with authorized crypto access. No protection settings changed.
- Independent review confirmed the 23 setting order, heartbeat/expiry alignment, callback stages and bounded IPC. It identified stock TFAR's foreign-command log noise, now documented. No confirmed integration blocker remained.
- Package/installer/backend and public-release results are recorded separately after sealing; live multiplayer, CBA/ACE interaction, BattlEye/server acceptance, GUI review and audio/performance acceptance remain open.
- Sealed standalone addon ZIP: 563404 bytes, SHA256 FD0B3A1BAF10E56A115874B3B21737F7D0615CF32018BD6DDE514DBAA51A12C5. Its 13 PBO entries match source; its source export contains 50 text files and no build executables, private signing keys or user recordings.
- The actual sealed bundle passed 137 installer backend assertions, including verified install/readback, idempotency, schema/layout limits and nested TeamSpeak file collision rejection. Setup embeds this exact ZIP and uses an independent install root. Source/archive records distinguish pre-seal evidence from later installer/publication checks.
