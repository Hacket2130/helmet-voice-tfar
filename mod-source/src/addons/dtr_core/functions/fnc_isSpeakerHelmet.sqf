/* Clear helmet-speaker eligibility only; never grants Death Trooper decoding. */
params [["_unit", objNull, [objNull]]];
if (isNull _unit || {!alive _unit} || {_unit getVariable ["TFAR_forceSpectator", false]}) exitWith {false};
// DT semantics take precedence even when an extra speaker root also matches.
if ([_unit] call DTR_fnc_isHelmet) exitWith {false};
private _helmet = headgear _unit;
if (_helmet isEqualTo "") exitWith {false};
private _cached = DTR_speakerHelmetCache getOrDefault [_helmet, -1];
if (_cached isEqualType true) exitWith {_cached};
private _config = configFile >> "CfgWeapons" >> _helmet;
private _matches = getNumber (_config >> "DTR_helmetSpeaker") == 1;
if (!_matches) then {
    _matches = (DTR_speakerHelmetClasses findIf {_helmet isKindOf [_x, configFile >> "CfgWeapons"]}) >= 0;
};
DTR_speakerHelmetCache set [_helmet, _matches];
_matches
