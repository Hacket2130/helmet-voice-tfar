params [["_unit", objNull, [objNull]], ["_enabled", true, [true]]];
if (isNull _unit || {!local _unit} || {!(missionNamespace getVariable ["DTR_enabled", true])}
    || {!(missionNamespace getVariable ["DTR_helmetVoiceEnabled", true])}
    || {!(missionNamespace getVariable ["DTR_helmetAllowToggle", true])}
    || {!([_unit] call DTR_fnc_isSpeakerHelmet)}) exitWith {false};
// Replicated object state follows the speaker, including late-joining listeners.
_unit setVariable ["DTR_helmetVoiceOn", _enabled, true];
systemChat (if (_enabled) then {"Helmet speaker effect enabled."} else {"Helmet speaker effect disabled."});
true
