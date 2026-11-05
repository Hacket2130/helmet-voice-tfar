# Task force Vanguard TFAR addon — standalone setup

R12 installs **alongside official TFAR Beta**, as `@TFV_TFAR_Addon` plus a separately named TeamSpeak companion. Keep the stock TFAR Arma mod and TeamSpeak plugin enabled. This package contains no TFAR core PBO, TFAR pipe DLL, or stock TFAR TeamSpeak DLL.

## Installation

1. Install official TFAR Beta and its official TeamSpeak plugin normally.
2. Run this setup and choose **Install bundled R12**. It installs into `%LOCALAPPDATA%\TaskForceVanguardTFAR\Releases\R12` by default.
3. Add the installed `@TFV_TFAR_Addon` folder through Arma Launcher **Mods > Local mod**, alongside TFAR Beta and CBA_A3. Long Live the Empire supplies the supported helmets; ACE adds self-interaction toggles.
4. Choose **Install Vanguard companion**, complete TeamSpeak's package installer, and keep both Vanguard and stock TFAR enabled. Restart TeamSpeak yourself when convenient.

If you installed the old R11 replacement build, disable `@TFAR_DTR` in Arma Launcher and restore the official TFAR TeamSpeak plugin before enabling R12. R12 uses a separate data directory and does not delete, edit, or automatically migrate R11/TFAR installations. Old R11 updater manifests are rejected by schema/product checks.

The update helper remains in the selected install root as `Task-force-Vanguard-TFAR-addon-Setup.exe`. Optional checks run when the app opens, and updates install only when selected. The newest version gets its own folder; previous releases remain available. After updating, select the new addon folder in Arma and install the matching Vanguard companion. TFAR stays installed independently.

## Build contract

From the project root, after reviewing/building the standalone package:

```powershell
./standalone/installer/Build-Installer.ps1 -PackagePath ./outputs/Task-force-Vanguard-TFAR-addon-R12.zip -ExpectedPackageSha256 <reviewed SHA256>
```

The build uses existing .NET Framework 4.8 C# tooling with warnings as errors. It requires the package ZIP to contain `@TFV_TFAR_Addon`, `Validation/manifest.json` and these files:

- `addons/tfv_core.pbo`
- `tfv_helmet_voice_x64.dll`
- `teamspeak/task_force_vanguard.ts3_plugin`
- At least one `keys/*.bikey`

The validation manifest is an array of `{Path,SHA256,Bytes}` records relative to `@TFV_TFAR_Addon`. The nested TeamSpeak ZIP may contain only `package.ini`, `plugins/task_force_vanguard_win64.dll`, and an optional empty `plugins/` directory. The installer rejects other PBOs/DLLs/packages and other mod roots, so it cannot quietly ship another TFAR replacement.

The build embeds the exact reviewed package, runs safety/transaction tests and a real bundled install in a unique project `work` directory, then produces an EXE, native TeamSpeak package, ZIP copy, feed and checksums in a fresh `outputs/Task-force-Vanguard-TFAR-addon-R12-Release` directory. Existing artifacts are never overwritten. There is no skip-tests release path.

Schema 2 uses product `task-force-vanguard-tfar-addon`, revision 12 and companion version `0.12.0`. The feed remains `https://github.com/Hacket2130/helmet-voice-tfar/releases/latest/download/update.json`. An unavailable or incompatible feed reports unknown update status rather than claiming current. HTTPS and repository ownership establish download trust; SHA256 checks verify integrity, not an independent publisher signature. The setup is not Authenticode signed.

## Developer status

Implementation is separate from the historical R11 installer. Backend tests preserve guarded extraction, case/traversal/link rejection, hash verification, atomic state updates, failure recovery and downgrade protection. Additional tests reject stock TFAR payload paths and a nested TeamSpeak package that would replace TFAR. Actual package/build results must be recorded after the R12 native package exists. No GUI, live Arma, TeamSpeak installation, launcher preset or server was modified during installer development.
