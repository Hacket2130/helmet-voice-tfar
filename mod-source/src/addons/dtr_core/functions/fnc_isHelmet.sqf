/* Exact configured headgear and subclasses only. No display-name guesses. */
params [["_unit", objNull, [objNull]]];
if (isNull _unit || {!alive _unit} || {_unit getVariable ["TFAR_forceSpectator", false]}) exitWith {false};
private _helmet = headgear _unit;
if (_helmet isEqualTo "") exitWith {false};
private _cached = DTR_helmetCache getOrDefault [_helmet, -1];
if (_cached isEqualType true) exitWith {_cached};
private _config = configFile >> "CfgWeapons" >> _helmet;
private _matches = getNumber (_config >> "DTR_decoder") == 1;
if (!_matches) then {
    _matches = (DTR_helmetClasses findIf {_helmet isKindOf [_x, configFile >> "CfgWeapons"]}) >= 0;
};
DTR_helmetCache set [_helmet, _matches];
_matches
