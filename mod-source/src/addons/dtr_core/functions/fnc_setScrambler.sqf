params [["_unit", objNull, [objNull]], ["_enabled", true, [true]]];
if (isNull _unit || {!local _unit} || {!(missionNamespace getVariable ["DTR_enabled", true])}
    || {!(missionNamespace getVariable ["DTR_allowToggle", true])}
    || {!([_unit] call DTR_fnc_isHelmet)}) exitWith {false};
// Public object state reaches other listeners and JIP clients; never send executable code.
_unit setVariable ["DTR_scramblerEnabled", _enabled, true];
systemChat (if (_enabled) then {"Helmet voice scrambler enabled."} else {"Helmet voice scrambler disabled."});
true
