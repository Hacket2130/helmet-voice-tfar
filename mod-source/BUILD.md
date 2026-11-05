# Rebuilding R11

Requires Windows, PowerShell 7, Git with Git LFS, Visual Studio 2022 C++ Build Tools with a Windows 10/11 SDK, Arma 3 Tools, and an installed TFAR Beta **1.-1.0.341** package. Builds stage an isolated test package; they do not install or publish it.

1. From this `Source` directory, run `./tools/Prepare-Source.ps1`. It clones pinned TFAR commit `fce977fa388bb527a989af1e162ade648c8d9d75`, fetches dependencies and x64 LFS libraries, then applies `tfar-dtr.patch`.
2. Run `./tools/Build-Package.ps1 -TfarMod 'PATH_TO_TFAR_BETA' -ArmaTools 'PATH_TO_ARMA_3_TOOLS'`.
3. Run `./tools/Verify-Package.ps1 -TfarMod 'PATH_TO_TFAR_BETA'`.
4. Check both changed PBO signatures with Arma 3 Tools `DSCheckSignatures.exe`. Record native checks, actual DLL smoke testing, package preservation and archive exclusions in `VALIDATION.md`.
5. Complete the separate multiplayer, CBA/ACE and listening acceptance matrix before deployment.

Generated files go into `Source/work` and `Source/outputs`. Signing uses Windows cryptographic services and a filename-only signing authority name; it may require execution outside a restricted sandbox. The private key stays under `work/keys` and must never ship. Build upstream as `Release`, `x64`, `WindowsTargetPlatformVersion=10.0`. Debug UI dependencies remain; Tracy is disabled through the release header. Only x64 TeamSpeak is supplied.

## R11 implementation sequence

1. Preserve R10's 623–3100 Hz full-wet band and pre-amplifier 2000 Hz / +3 dB / Q 1.2 peak.
2. Add a separate 2500 Hz / +2 dB / Q 1.2 peaking EQ after amplifier saturation and before the final low-pass. Increase default drive from 2.0 to 2.4.
3. Add CBA clarity frequency 1000–4000 Hz and gain 0–6 dB, with defaults 2500/+2. Keep generated static, clicks, random/envelope state and microphone classification absent.
4. Retain DT DSP, DTR2 states, standard matching and independent ACE choices. Expose 32 CBA controls and 23 synchronized DSP keys, guarded by DTR11.
5. Verify post-amp clarity behavior, retained original peak, distortion change, response/pitch, zero-gain bypass, silence/reset/continuity/bounds, DT preservation and packaged integration. Render isolated R11 previews and preserve R10.

R11 implementation, native tests, full build, staged package/signature validation and previews are complete. Final ZIP assembly and inspection pass: required files are present, with no private keys or user recordings (69 entries). Runtime and human acceptance remain separate.

## Arma and TeamSpeak integration

CBA compiles TFAR functions final, so copied `tfar_core.pbo` patches `functions/fnc_preparePositionCoordinates.sqf` directly. The build rejects unrecognized installed source format. All other core entries and TFAR PBOs must remain unchanged. `dtr_core.pbo` supplies matching, settings, independent ACE actions and diagnostics.

Strict POS states remain `DTR2:0` (none), `DTR2:1` (DT decoder), `DTR2:3` (DT decoder/scrambler) and `DTR2:4` (standard speaker). Standard helmets never gain DT decoding; DT eligibility takes precedence over overlapping standard roots. Missing, malformed or unknown state grants neither profile. Custom settings require capability `DTR11`. Plugin name is **Task Force Arrowhead Radio - Helmet Voice R11**, version **1.0.341-dtr.11**.

`DTR_enabled`, labeled **Enable helmet voice effects** in the Death Trooper Voice category, remains the global master. Standard enable controls only speakers beneath it. ACE policies and per-unit choices stay separate. Standard headgear opt-in `DTR_helmetSpeaker = 1` remains distinct from `DTR_decoder = 1`.

Playback processing precedes TFAR's copy for direct/radio branches. Local speech, radios, intercom and speakers share the processed buffer under normal audibility rules. Capture stays original. Preserve per-speaker state and clear stale processing when bypassing or leaving a profile.

The guarded `VoiceChannelConversion.hpp` mono-to-stereo duplication and integer downmix remain without out-of-bounds second-channel access; R11 channel regressions pass. Standard LLTE matching remains sixteen exact roots plus descendants, resolving to 56 declarations in `JMSLLTE_empire.pbo` / `helmets/config.cpp`, extraction SHA-256 `5EAB238E666BB0FE684039AC2AD61A22D45FF764C5FD764952CE4E9C9B930C99`. This is config/model-identity evidence, not visual/gameplay acceptance; uncertain headgear and separate-mask combinations remain excluded.

## Standard clarity chain

Confirmed order: **HP 623 Hz → original peaking EQ 2000 Hz / +3 dB / Q 1.2 → drive 2.4 → clarity peaking EQ 2500 Hz / +2 dB / Q 1.2 → LP 3100 Hz → configured wet/dry blend**.

The clarity stage supplements the original 2 kHz peak rather than moving or replacing it. Placing clarity after the amplifier prevents upstream saturation from compressing that stage's boost. The two EQ bands overlap, and +2 dB is the clarity band's parameter rather than an exclusive single-frequency change. Final LP and output limits still shape the result; optional high combined gains can reach the clamp. Source review confirms clarity state participates in reset and zero-gain bypass.

HP/LP retain gradual 12 dB/octave rolloffs, with ranges 80–1200 and 1200–10000 Hz. Drive remains adjustable from 1–6. Mix defaults to 1.0, range 0–1; lower mix deliberately restores some original voice. Original resonance ranges remain 500–4000 Hz and 0–9 dB. New `DTR_helmetClarityFrequency` spans 1000–4000 Hz, default 2500; `DTR_helmetClarityGain` spans 0–6 dB, default 2. Both peaks use fixed Q 1.2. A zero gain must bypass its corresponding EQ and clear its old state without affecting the other EQ.

Speech stays forward and unpitched, with no added speech buffer. No standard static, RNG, activity envelope, click generator or classifier is introduced. Source noise can pass through the filters; this is not denoising. ACE switches the entire standard chain.

Thirty-two CBA controls comprise twenty DT controls and twelve standard controls: four policies plus eight DSP controls. Twenty-three transported DSP keys comprise fifteen DT plus eight standard. Saved R10 drive values may retain 2.0; explicitly set drive 2.4 and clarity 2500/+2 for the R11 preset. Existing HP/LP/mix/resonance overrides remain effective too.

## Preserved Death Trooper DSP

Keep R7 defaults: -6 semitones, drive 32, cabinet 0.75, screech 0.25, static 0.65, 8-bit/four-sample crusher, 90 Hz ring amount 0.12, fourth-order HP150 and second-order LP3200. Four shuffled/reversed 50 ms grains form the 200 ms buffer only for undecoded speech; decoded speech stays original.

Dry/static processing remains separate from the voice-only wet return, with independent HP/LP state. Crusher/static do not enter that return. Reverb mix/decay remain 0.25/0.35 seconds. The -48 dBFS/1 ms gate closes after 48 consecutive sub-threshold frames at 48 kHz, cutting dry/static. The wet return can continue 0.22 seconds, fading over its final 20 ms to exact zero. Tail zero cuts immediately; mix zero removes wet output.

Reopening may overlay unexpired wet decay. Expiry clears downstream state while speech buffers/grain clock advance; full idle reset clears old talkspurts. TeamSpeak callbacks must continue for the tail to play. R11 DT regressions pass and the comparison rerender is byte-identical to the retained R10/R9/R8/R7 sample; standard EQ controls remain separate.

## Tests, previews and acceptance

R11 passes **115 native checks**: 82 DT/protocol/channel checks and 33 standard-helmet checks. Pitch remains 800 Hz in the 800 Hz fixture, with forward speech and no added buffer. Default response gains at 80/1000/12000 Hz are 0.0159399/1.03566/0.0422024. The retained original 2 kHz resonance measures 2.99268 dB in its comparison fixture.

At fixed amplifier settings, the clarity-on versus clarity-off comparison measures ratio 1.25899, or **+2.00045 dB**, at 2500 Hz. Ratios at 1000/2500/3800 Hz are 1.0314/1.25899/1.11407, reflecting the overlapping, finite-width peak. Raising drive from 2.0 to 2.4 increases the in-band third-harmonic ratio from 0.069452 to 0.0922454. Both EQ center controls, exact zero-gain bypasses and old-state clearing pass. Packet/stereo continuity, impulse/silence decay, non-finite/clamped settings, resets and maximum combined +9 dB resonance/+6 dB clarity stability, decay and bounds pass. Signal results do not establish perceived clarity.

Source review confirms 32 CBA controls, 23 synchronized values and all eight standard arguments/defaults align with DTR11 identity. Full x64 build, CfgConvert and fifteen package checks pass. The actual DLL passes `tests/plugin_load_smoke.cpp` with expected Helmet Voice R11 name/version `1.0.341-dtr.11` and outside-game bypass, without TeamSpeak initialization. Both changed PBO signatures return DSCheckSignatures exit 0. Source whitespace/reverse application and all four PowerShell parser checks pass. Known upstream macro-redefinition, conversion and C4744 warnings remain; the build is not warning-free. Final ZIP inspection passes.

The generic `Stormtrooper-Clarity-Preview-R11.wav` uses local Windows SAPI speech through production DSP: 439107 stereo PCM16 frames at 48 kHz, effect-only with 0.5 seconds of ending padding. The separate `Aftermath-and-Accusation-Stormtrooper-R11.wav` has 670473 stereo PCM16 frames at 48 kHz, 13.968187 seconds including 0.5 seconds of padding, mean -30.5 dBFS and peak -7.8 dBFS. Its record is `work/audio-previews/aftermath-stormtrooper-r11-render.json` in the project workspace. Keep the user source/render outside the mod/source package.

R10 ZIP SHA-256 remains `832178E62B45F7E86E3AEE9D733B979A33CF5FBA85AA0D4A264DB588F8546190`, and the original audio hash is unchanged. Logs and retained LLTE inventory are handed off in `../Validation/`. Earlier official references remain context only; no film audio is copied, auditioned or measured. No auditory inspection is claimed. Live CBA/ACE, multiplayer, performance and human sound/intelligibility acceptance remain open.

