if (!hasInterface || {isNil "ace_interact_menu_fnc_createAction"}) exitWith {
    diag_log "[DTR] ACE interaction unavailable; automatic helmet voice profiles remain active.";
};
private _parent = [
    "DTR_Helmet", "Death Trooper Helmet", "", {},
    {(missionNamespace getVariable ["DTR_enabled", true]) &&
     {missionNamespace getVariable ["DTR_allowToggle", true]} &&
     {[_player] call DTR_fnc_isHelmet}}
] call ace_interact_menu_fnc_createAction;
["CAManBase", 1, ["ACE_SelfActions"], _parent, true] call ace_interact_menu_fnc_addActionToClass;

private _on = [
    "DTR_ScramblerOn", "Enable voice scrambler", "",
    {[_player, true] call DTR_fnc_setScrambler;},
    {([_player] call DTR_fnc_getState) isEqualTo "DTR2:1"}
] call ace_interact_menu_fnc_createAction;
private _off = [
    "DTR_ScramblerOff", "Disable voice scrambler", "",
    {[_player, false] call DTR_fnc_setScrambler;},
    {([_player] call DTR_fnc_getState) isEqualTo "DTR2:3"}
] call ace_interact_menu_fnc_createAction;
{
    ["CAManBase", 1, ["ACE_SelfActions", "DTR_Helmet"], _x, true] call ace_interact_menu_fnc_addActionToClass;
} forEach [_on, _off];

private _speakerParent = [
    "DTR_HelmetSpeaker", "Helmet Speaker", "", {},
    {(missionNamespace getVariable ["DTR_enabled", true]) &&
     {missionNamespace getVariable ["DTR_helmetVoiceEnabled", true]} &&
     {missionNamespace getVariable ["DTR_helmetAllowToggle", true]} &&
     {[_player] call DTR_fnc_isSpeakerHelmet}}
] call ace_interact_menu_fnc_createAction;
["CAManBase", 1, ["ACE_SelfActions"], _speakerParent, true] call ace_interact_menu_fnc_addActionToClass;
private _speakerOn = [
    "DTR_HelmetSpeakerOn", "Enable helmet speaker effect", "",
    {[_player, true] call DTR_fnc_setHelmetVoice;},
    {!(_player getVariable ["DTR_helmetVoiceOn", true])}
] call ace_interact_menu_fnc_createAction;
private _speakerOff = [
    "DTR_HelmetSpeakerOff", "Disable helmet speaker effect", "",
    {[_player, false] call DTR_fnc_setHelmetVoice;},
    {_player getVariable ["DTR_helmetVoiceOn", true]}
] call ace_interact_menu_fnc_createAction;
{
    ["CAManBase", 1, ["ACE_SelfActions", "DTR_HelmetSpeaker"], _x, true] call ace_interact_menu_fnc_addActionToClass;
} forEach [_speakerOn, _speakerOff];
