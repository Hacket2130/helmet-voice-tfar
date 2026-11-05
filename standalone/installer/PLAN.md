# Standalone installer plan

1. Preserve the original R11 installer and all released artifacts. Copy source into this isolated directory.
2. Change the payload contract to schema 2 / task-force-vanguard-tfar-addon and the separate @TFV_TFAR_Addon layout. Reject payload paths that would ship stock TFAR components.
3. Update setup instructions to keep stock TFAR enabled, install a companion plugin, and manually restore stock TFAR when migrating from R11. Keep setup data separate from the old replacement build.
4. Compile with C# 5 warnings treated as errors, run existing transaction/safety tests plus addon-collision checks, and exercise the actual R12 bundled payload once root finishes it.
5. Publish only the artifacts produced after the actual bundle tests pass; never overwrite previous files. Record runtime/GUI/in-game acceptance separately.
6. After publication, run PublishedReleaseCheck against the reviewed setup to check the live feed and package through the production download/install backend in a unique work-only directory. Never use this verification to modify live Arma or TeamSpeak installations.

Current status: steps 1–4 complete. All 137 installer assertions pass against the sealed actual R12 package, including install/readback/idempotency and standalone collision guards. Release assets are prepared in outputs/Task-force-Vanguard-TFAR-addon-R12-Release with checksums. Step 5 publication and step 6 live feed verification are pending root coordination. No live app or installation changes; GUI and two-client game/audio acceptance are not claimed.
