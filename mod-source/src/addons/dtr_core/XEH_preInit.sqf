DTR_helmetCache = createHashMap;
DTR_speakerHelmetCache = createHashMap;
call DTR_fnc_rebuildHelmetClasses;

[
    "DTR_enabled", "CHECKBOX",
    ["Enable helmet voice effects", "Master switch for Death Trooper scrambling/decoding and clear helmet speaker effects for the mission."],
    "Death Trooper Voice", true, 1
] call CBA_settings_fnc_init;
[
    "DTR_defaultEnabled", "CHECKBOX",
    ["Scrambler on by default", "Initial state on a new unit/respawn. Existing ACE choices persist for the current unit."],
    "Death Trooper Voice", true, 1
] call CBA_settings_fnc_init;
[
    "DTR_allowToggle", "CHECKBOX",
    ["Allow ACE scrambler toggle", "Players may switch outgoing scrambling on/off. If disabled, all helmets use the default state."],
    "Death Trooper Voice", true, 1
] call CBA_settings_fnc_init;
[
    "DTR_includeLLTE", "CHECKBOX",
    ["Use Long Live the Empire helmets", "Include the built-in LLTE Death Trooper helmet classes and their subclasses."],
    "Death Trooper Voice", true, 1, {call DTR_fnc_rebuildHelmetClasses;}
] call CBA_settings_fnc_init;
[
    "DTR_extraHelmets", "EDITBOX",
    ["Additional helmet classes", "Exact classnames separated by commas. Subclasses are included. No quotes or brackets needed."],
    "Death Trooper Voice", "", 1, {call DTR_fnc_rebuildHelmetClasses;}
] call CBA_settings_fnc_init;

["CBA_settingsInitialized", {call DTR_fnc_rebuildHelmetClasses;}] call CBA_fnc_addEventHandler;

[
    "DTR_reverbMix", "SLIDER",
    ["Amplifier reverb amount", "Amount of small-enclosure reverb on scrambled speech. Zero disables reverb."],
    "Death Trooper Voice", [0, 0.6, 0.25, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_reverbDecay", "SLIDER",
    ["Amplifier reverb decay", "Approximate reverb decay in seconds. Longer values elongate the amp sound."],
    "Death Trooper Voice", [0.05, 1.5, 0.35, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_highPass", "SLIDER",
    ["Amplifier high-pass filter", "Sharp 24 dB/octave low-frequency cutoff in Hz. Higher values give a thinner helmet speaker sound."],
    "Death Trooper Voice", [80, 1200, 150, 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_pitch", "SLIDER",
    ["Amplifier pitch", "Pitch shift in semitones. Default -6 gives a lower voice; -12 is one octave down. Scrambling remains active."],
    "Death Trooper Voice", [-12, 0, -6, 1], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_ampDrive", "SLIDER",
    ["Amplifier drive", "Input gain into the distorted helmet amplifier. Higher values saturate quieter speech more strongly."],
    "Death Trooper Voice", [8, 48, 32, 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_metallic", "SLIDER",
    ["Metallic speaker character", "Metal enclosure resonance and upper-mid amplifier bite. Zero disables this coloration; the deep garble remains."],
    "Death Trooper Voice", [0, 1, 0.75, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_screech", "SLIDER",
    ["High-frequency screech", "Sharp upper-frequency rasp driven by the scrambled voice. Zero removes this extra rasp."],
    "Death Trooper Voice", [0, 1, 0.25, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_staticAmount", "SLIDER",
    ["Speech static amount", "Radio-style static underneath garbled speech, gated by voice level. Zero disables the static layer."],
    "Death Trooper Voice", [0, 1, 0.65, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_bitDepth", "LIST",
    ["Digital bit depth", "8 or 12 bits add digital crunch with a 12 kHz sample-and-hold. 16 bypasses this stage; output files remain PCM16."],
    "Death Trooper Voice", [[8, 12, 16], ["8-bit", "12-bit", "16-bit (bypass)"], 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_ringAmount", "SLIDER",
    ["Ring modulation amount", "Fast 90 Hz metallic modulation. Zero disables the modulation; it generates no sound without speech."],
    "Death Trooper Voice", [0, 1, 0.12, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_hardGate", "CHECKBOX",
    ["Hard squelch", "Mute dry speech and static below the threshold. Voice-only resonance may continue for the metallic tail length; set that length to zero for an immediate full cutoff."],
    "Death Trooper Voice", true, 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_gateThreshold", "SLIDER",
    ["Squelch threshold (dBFS)", "Voice level required to open the gate. Raise it to reject more background noise; lower it for quiet speech."],
    "Death Trooper Voice", [-70, -20, -48, 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_gateHold", "SLIDER",
    ["Squelch hold (ms)", "Close after this many consecutive milliseconds below threshold. A longer hold bridges more waveform zero crossings."],
    "Death Trooper Voice", [0, 10, 1, 1], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_lowPass", "SLIDER",
    ["Amplifier low-pass filter", "Upper-frequency cutoff in Hz. Lower values give a darker, less sharp helmet speaker sound."],
    "Death Trooper Voice", [1800, 12000, 3200, 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_tailLength", "SLIDER",
    ["Metallic tail length", "Maximum voice-only resonance after the squelch closes, in seconds. Static stops immediately. Zero cuts the whole effect immediately. Abrupt TeamSpeak stream termination can shorten the tail."],
    "Death Trooper Voice", [0, 0.6, 0.22, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;

[
    "DTR_helmetVoiceEnabled", "CHECKBOX",
    ["Enable helmet speaker voice", "Apply a subtle speaker filter to supported enclosed helmets. Death Trooper helmets keep their separate scrambler and decoder profile."],
    "Helmet Speaker Voice", true, 1
] call CBA_settings_fnc_init;
[
    "DTR_helmetIncludeLLTE", "CHECKBOX",
    ["Use Long Live the Empire helmets", "Include verified Stormtrooper, Scout, Shore, Snow, Tank, Phase 3 clone and TIE pilot helmet families and subclasses."],
    "Helmet Speaker Voice", true, 1, {call DTR_fnc_rebuildHelmetClasses;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetExtraHelmets", "EDITBOX",
    ["Additional speaker helmet classes", "Exact enclosed helmet classnames separated by commas; subclasses are included. These helmets do not gain Death Trooper decoding."],
    "Helmet Speaker Voice", "", 1, {call DTR_fnc_rebuildHelmetClasses;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetAllowToggle", "CHECKBOX",
    ["Allow ACE helmet speaker toggle", "Players may enable or disable their outgoing helmet speaker effect. If disabled, the effect stays on for supported helmets."],
    "Helmet Speaker Voice", true, 1
] call CBA_settings_fnc_init;
[
    "DTR_helmetHighPass", "SLIDER",
    ["Speaker high-pass filter", "Low-frequency cutoff in Hz. Higher values make the helmet speaker thinner."],
    "Helmet Speaker Voice", [80, 1200, 623, 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetLowPass", "SLIDER",
    ["Speaker low-pass filter", "Upper-frequency cutoff in Hz. Lower values soften the top end of the helmet speaker."],
    "Helmet Speaker Voice", [1200, 10000, 3100, 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetDrive", "SLIDER",
    ["Speaker drive", "Mild amplifier saturation. The default preserves clear speech; higher values add more distortion."],
    "Helmet Speaker Voice", [1, 6, 2.4, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetMix", "SLIDER",
    ["Speaker filter amount", "Blend between the original voice and the helmet speaker filter. One applies the full filter; zero removes filter coloration."],
    "Helmet Speaker Voice", [0, 1, 1.0, 2], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetResonanceFrequency", "SLIDER",
    ["Speaker resonance frequency", "Centre frequency in Hz of the speaker's resonant emphasis. The default adds presence around 2 kHz."],
    "Helmet Speaker Voice", [500, 4000, 2000, 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetResonanceGain", "SLIDER",
    ["Speaker resonance gain (dB)", "Strength of the resonant emphasis. The default is a gentle 3 dB boost; zero removes the boost."],
    "Helmet Speaker Voice", [0, 9, 3, 1], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetClarityFrequency", "SLIDER",
    ["Speaker clarity frequency", "Centre frequency in Hz of the clarity boost after amplifier distortion. The default emphasizes speech detail around 2.5 kHz."],
    "Helmet Speaker Voice", [1000, 4000, 2500, 0], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
[
    "DTR_helmetClarityGain", "SLIDER",
    ["Speaker clarity gain (dB)", "Strength of the separate clarity boost. The default adds 2 dB; zero removes this boost while preserving the speaker resonance."],
    "Helmet Speaker Voice", [0, 6, 2, 1], 1, {call DTR_fnc_syncSettings;}
] call CBA_settings_fnc_init;
