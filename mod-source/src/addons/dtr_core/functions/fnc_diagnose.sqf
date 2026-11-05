/* Local debug console: call DTR_fnc_diagnose; returns and logs integration status. */
private _unit = missionNamespace getVariable ["TFAR_currentUnit", player];
private _result = [
    ["pluginCapability", "task_force_radio_pipe" callExtension ("TS_INFO" + toString [9] + "DTR_CAPABILITY")],
    ["headgear", headgear _unit],
    ["helmetMatched", [_unit] call DTR_fnc_isHelmet],
    ["speakerHelmetMatched", [_unit] call DTR_fnc_isSpeakerHelmet],
    ["voiceState", [_unit] call DTR_fnc_getState],
    ["configuredClasses", DTR_helmetClasses],
    ["configuredSpeakerClasses", DTR_speakerHelmetClasses],
    ["corePatched", (toString TFAR_fnc_preparePositionCoordinates) find "DTR2:" >= 0]
];
diag_log format ["[DTR] Diagnostic: %1", _result];
_result
