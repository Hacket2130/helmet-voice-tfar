params [["_unit", objNull, [objNull]]];
if (!(missionNamespace getVariable ["DTR_enabled", true])) exitWith {"DTR2:0"};
if ([_unit] call DTR_fnc_isHelmet) exitWith {
    private _default = missionNamespace getVariable ["DTR_defaultEnabled", true];
    private _on = if (missionNamespace getVariable ["DTR_allowToggle", true]) then {
        _unit getVariable ["DTR_scramblerEnabled", _default]
    } else {
        _default
    };
    // Disabling the microphone scrambler does not disable the helmet's decoder.
    if (_on) then {"DTR2:3"} else {"DTR2:1"}
};
if (!(missionNamespace getVariable ["DTR_helmetVoiceEnabled", true])
    || {!([_unit] call DTR_fnc_isSpeakerHelmet)}) exitWith {"DTR2:0"};
private _speakerOn = !(missionNamespace getVariable ["DTR_helmetAllowToggle", true])
    || {_unit getVariable ["DTR_helmetVoiceOn", true]};
if (_speakerOn) then {"DTR2:4"} else {"DTR2:0"}
