# Task force Vanguard TFAR addon R11 — Windows setup and updates

The approved R11 standard-helmet sound, now packaged with a Windows installer and optional GitHub update checks. The R11 mod ZIP and TeamSpeak plugin are unchanged.

## Downloads

- **Task-force-Vanguard-TFAR-addon-Setup-R11.exe** — complete offline setup plus update checker. Start here.
- **Task-force-Vanguard-TFAR-addon-R11.ts3_plugin** — normal TeamSpeak package installer, for manual installation.
- **Task-force-Vanguard-TFAR-addon-R11.zip** — full Arma mod, TeamSpeak package, mod source and validation.
- **Task-force-Vanguard-TFAR-addon-Installer-Source-R11.zip** — installer source, tests and build/release instructions.
- **SHA256SUMS.txt** — hashes for download verification.
- **update.json** — machine-readable update feed used by the setup app.

Run setup, install the bundled release, then use its TeamSpeak installer button and add the installed `@TFAR_DTR` folder as an Arma Launcher Local mod. Disable the original TFAR Beta for this preset. CBA_A3 and Long Live the Empire are required; ACE enables the self-interaction toggles. All participating clients and servers need matching versions.

The app checks only when open, if enabled, and installs updates only when selected. It validates downloads and preserves earlier release folders. After an update, select the new Arma mod folder and install the matching TeamSpeak package. It does not restart applications or add background services.

Standard helmets use 623 Hz high-pass, 3100 Hz low-pass, 2 kHz resonance, +2 dB at 2500 Hz and mild distortion, with no generated clicks or static. Death Troopers retain their deeper garble and longer metallic tail, with decoding for other Death Troopers.

**Development test release:** the standard-helmet sample is user-approved, the mod passed 115 native and 15 package checks, and the setup passed 130 backend assertions. Live multiplayer, CBA/ACE operation, performance, Death Trooper intelligibility and final interactive GUI verification remain open. Installer-specific results are recorded in the repository's `installer/VALIDATION.md`. The setup executable is not Authenticode signed.

The source ZIP contains the validation snapshot from before publication. The repository documentation records later publication and online verification results. The mod/plugin retains its historical internal R11 identity while the setup and public filenames use Task force Vanguard TFAR addon.

Source and build instructions are included. TFAR attribution and its Arma Public License Share Alike remain in the package. No user recording or private signing key is included.
