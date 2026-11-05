if (!hasInterface) exitWith {};
if (!isMultiplayer && {!is3DENMultiplayer}) exitWith {};

call DTR_fnc_rebuildHelmetClasses;
[DTR_fnc_initAce, []] call CBA_fnc_execNextFrame;

if (isNil "TFAR_fnc_preparePositionCoordinates") exitWith {
    diag_log "[DTR] ERROR: TFAR Beta position function is missing.";
};
DTR_corePatched = (toString TFAR_fnc_preparePositionCoordinates) find "DTR2:" >= 0;
if (!DTR_corePatched) then {
    diag_log "[DTR] ERROR: Stock TFAR core loaded. Load the supplied TFAR_DTR mod instead.";
    systemChat "Helmet voice: use the supplied TFAR_DTR mod instead of stock TFAR Beta.";
};
diag_log format ["[DTR] Loaded DTR2 helmet mappings: DT=%1; speaker=%2", DTR_helmetClasses, DTR_speakerHelmetClasses];

// Retry through delayed TeamSpeak connection; announce missing plugin once.
DTR_pluginReady = false;
DTR_pluginWarningShown = false;
DTR_pluginCheckStart = diag_tickTime;
[{
    private _capability = "task_force_radio_pipe" callExtension ("TS_INFO" + toString [9] + "DTR_CAPABILITY");
    private _ready = _capability isEqualTo "DTR11";
    if (_ready && {!DTR_pluginReady}) then {
        diag_log "[DTR] Modified TeamSpeak plugin capability DTR11 verified.";
        if (DTR_pluginWarningShown) then {systemChat "Helmet voice: compatible plugin connected.";};
    };
    if (!_ready && {!DTR_pluginWarningShown} && {diag_tickTime - DTR_pluginCheckStart > 20}) then {
        DTR_pluginWarningShown = true;
        systemChat "Helmet voice requires the matching R11 DTR TFAR TeamSpeak plugin on every client.";
        diag_log format ["[DTR] ERROR: Modified plugin missing; capability response: %1", _capability];
    };
    if (!_ready && {DTR_pluginReady}) then {
        systemChat "Helmet voice: plugin connection lost.";
        diag_log "[DTR] ERROR: DTR11 capability lost.";
    };
    DTR_pluginReady = _ready;
    if (_ready) then {call DTR_fnc_syncSettings;};
}, 5] call CBA_fnc_addPerFrameHandler;
