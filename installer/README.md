# Task force Vanguard TFAR addon

Windows x64 setup for the R11 TFAR Beta fork. The app contains the complete approved R11 package, so first installation works offline. No administrator rights or additional .NET SDK are required on Windows with .NET Framework 4.8.

## Use

1. Run `Task-force-Vanguard-TFAR-addon-Setup-R11.exe` and select **Install bundled R11**. The default folder is `%LOCALAPPDATA%\HelmetVoiceTFAR`.
2. Select **Install TeamSpeak plugin** to open TeamSpeak's normal package installer. Back up an existing TFAR plugin before replacing it. Restart TeamSpeak yourself when convenient and enable only **Task Force Arrowhead Radio - Helmet Voice R11**, the embedded plugin's retained display name.
3. Select **Copy mod path**. In Arma 3 Launcher, use **Mods > Local mod** and select that folder. Disable the original TFAR Beta for this preset. Enable CBA_A3 and Long Live the Empire; ACE provides self-interaction toggles.
4. Reopen `Task-force-Vanguard-TFAR-addon-Setup.exe` in your install folder to check for updates. **Check for updates when I open this app** is optional and on by default. An update is installed only when you select **Install update**.
5. After an update, run its TeamSpeak plugin installer and select its new version folder in Arma Launcher. Keep all participating clients and the server on matching releases.

The standalone `Task-force-Vanguard-TFAR-addon-R11.ts3_plugin` is the same TeamSpeak package inside the approved mod. Double-click it for TFAR-style TeamSpeak installation; it does not install the Arma mod or check Task force Vanguard TFAR addon releases by itself.

The name applies to the setup app, Windows executable metadata, public downloads and current documentation. The unchanged R11 mod/plugin retain their legacy internal identities, including `@TFAR_DTR`, the TeamSpeak display name `Task Force Arrowhead Radio - Helmet Voice R11`, and the `%LOCALAPPDATA%\HelmetVoiceTFAR` data folder. The existing `helmet-voice-tfar` GitHub repository and update product ID also remain stable. The renamed mod ZIP and `.ts3_plugin` are byte-identical to the approved originals.

## Update behavior

R11 is published at [GitHub Releases](https://github.com/Hacket2130/helmet-voice-tfar/releases/tag/r11). On 2026-11-05, the shipped installer's backend successfully read the public latest-release feed, downloaded the exact package, installed it into an isolated test folder and verified readback. All six GitHub asset sizes and SHA256 digests matched local files. Final interactive GUI and in-game verification remain separate.

The app reads `https://github.com/Hacket2130/helmet-voice-tfar/releases/latest/download/update.json`. It compares integer `releaseNumber` values, checks package size and SHA256, validates archive paths and packaged file hashes, then promotes a staged release into `Releases\R<number>`. Existing releases are preserved; an older installer never downgrades the selected release. Failed checks report an unknown update status instead of claiming the installation is current.

The manifest and ZIP are trusted through HTTPS and the Hacket2130 GitHub repository. Hashes detect corruption and mismatched assets; they are not a separate publisher signature. The setup executable is not Authenticode signed. There is no background service, scheduled task, startup entry, silent plugin installation or automatic process restart. The app does not upload recordings or inspect microphones.

The unchanged R11 plugin inherits upstream TFAR's old stock-version notification. That is separate from this Task force Vanguard TFAR addon updater and must not be interpreted as a custom-fork update source.

## Build and test

Place this directory at the project root as `installer`, with a copy of the approved `Task-force-Vanguard-TFAR-addon-R11.zip` at the legacy input path `outputs/HelmetVoice-TFAR-R11.zip` and its extracted tree at `outputs/HelmetVoice-TFAR-R11/`. Run from PowerShell:

```powershell
./installer/Build-Installer.ps1
```

The script uses the Windows .NET Framework C# compiler, embeds the approved ZIP and manifest, compiles the GUI and tests, runs tests only in a unique project `work` directory, copies the native TeamSpeak installer, and writes `SHA256SUMS.txt`. Outputs go to `outputs/Task-force-Vanguard-TFAR-addon-R11/`, preserving earlier downloads. The approved ZIP's hash and size are pinned; future releases must deliberately update the build metadata after their own acceptance and validation. No NuGet restore is used.

For a read-only GUI inspection:

```powershell
./outputs/Task-force-Vanguard-TFAR-addon-R11/Task-force-Vanguard-TFAR-addon-Setup-R11.exe --ui-preview C:\path\to\workspace\work\installer-preview
```

Preview mode disables installation, networking, external launching and preference writes. Tests never call TeamSpeak's package installer or edit launcher presets. Arma/TeamSpeak multiplayer behavior is outside installer test coverage.

For postpublication verification, `Check-PublishedRelease.ps1` uses Windows PowerShell 5.1 and the shipped EXE. If local execution policy disables script files, use the compiled checker without changing that policy:

```powershell
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /langversion:5 /warnaserror+ /r:System.Web.Extensions.dll /out:work\PublishedReleaseCheck.exe installer\PublishedReleaseCheck.cs
& ./work/PublishedReleaseCheck.exe (Get-Location).Path
```

The checker compares the public feed with the bundled manifest, calls the actual EXE's download/install/readback methods, and writes its report under `work/vanguard-installer-build`. Run from a prepared project root with the branded setup in its standard output folder. It does not launch the GUI or install into live TeamSpeak/Arma directories.

## Future releases

1. Build and verify the next mod/plugin together. Assign a strictly increasing integer revision and unique release tag/filenames. Preserve old assets.
2. Update the build's pinned package metadata and generated manifest: product `helmet-voice-tfar`, schema 1, numeric revision, display/plugin versions, exact repository release asset URL, SHA256, byte size, release URL and notes.
3. Run the installer tests and inspect the GUI. Rebuild the setup with the new approved package; include source, upstream patch, licenses, checksums and validation notes. Exclude private signing keys and user recordings.
4. Create the GitHub release as a draft. Upload the ZIP, setup EXE, native `.ts3_plugin`, `update.json`, checksums and source. Do not publish a manifest before its referenced package is ready.
5. Publish and designate the intended update-channel release as **Latest**. GitHub's `/releases/latest/download/update.json` excludes drafts and prereleases; use a normal release with clearly stated testing limitations when appropriate, or implement a separate prerelease channel before using prereleases.
6. Verify the public feed and downloaded ZIP hash, and run the real app's update check. Never publish a fake higher revision to test detection; use local test fixtures.

## Acceptance and attribution

The user approved the R11 standard-helmet sample on 2026-11-05. R11's 115 native DSP/protocol checks and 15 package checks passed before this installer work. Live multiplayer, CBA/ACE menu operation, performance and Death Trooper intelligibility remain open. The immutable mod ZIP contains the historical pre-approval validation record; this document records the later sample approval without rewriting that archive.

Based on [Task Force Arrowhead Radio](https://github.com/michail-nikolaev/task-force-arma-3-radio), upstream commit `fce977fa388bb527a989af1e162ade648c8d9d75`, copyright Michail Nikolaev and contributors, under the [Arma Public License Share Alike](https://www.bistudio.com/community/licenses/arma-public-license-share-alike). The mod ZIP includes its license, source patch and build instructions. Arma 3, TeamSpeak, TFAR, CBA, ACE and Long Live the Empire are separate projects; this is an unofficial fork.
