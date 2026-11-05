# Delivery log

## 2026-11-05

- Approved R11 standard-helmet sample uses 623 Hz high-pass, 3100 Hz low-pass, 2 kHz/+3 dB resonance, drive 2.4 and 2500 Hz/+2 dB clarity. Death Trooper sound/routing remain unchanged.
- Mod passed 115 native DSP/protocol checks and 15 package checks, modified PBO signatures, DLL load/bypass and source checks. Live in-game acceptance remains separate.
- Added Windows x64 .NET Framework setup with embedded R11 ZIP, explicit TS installer handoff, optional GitHub checks, SHA256/size verification, safe extraction, atomic state switching and preserved earlier releases.
- Installer passed 130 backend assertions, including the actual bundle install, malformed archives, tamper rejection, state-write failure recovery and downgrade prevention. Compiler warnings are errors.
- Applied Task force Vanguard TFAR addon branding to installer metadata/UI and public filenames; installer version is 1.0.1.0. Embedded plugin remains 1.0.341-dtr.11 and uses its historical Helmet Voice R11 display name.
- Previous GUI inspection found DPI clipping; source was corrected to content-sized wrapping layouts. Final interactive appearance remains unverified. No live TeamSpeak/Arma installation was performed.
- Independent prepublication audit verified all six asset hashes, five checksum entries, 69 mod ZIP entries, 24 internal file hashes and 11 installer-source entries. No user recording/private key or unsafe archive entry found.
- User explicitly authorized publishing under Hacket2130. Published 44 source/doc files at commit 77d5c376b123ab5922ec3cf33a93543423c709a7, then published r11 at 17:58:54 UTC with six exact assets, release ID 435539017. GitHub latest-release and r11 tag endpoints point to the intended release/source commit. Archives retain their original prepublication validation snapshots.
- Remote asset sizes and all six GitHub SHA256 digests match local records. The actual shipped EXE backend read the public feed, downloaded the 54021360-byte package, verified it, installed into a unique workspace test directory and verified readback at 18:00:53 UTC. No live TeamSpeak/Arma installation changed.
- Windows PowerShell refused the script under its existing execution policy. Used the equivalent compiled C# checker without changing policy; checker source and instructions are added to the repository. GUI/in-game acceptance gates remain as recorded.
