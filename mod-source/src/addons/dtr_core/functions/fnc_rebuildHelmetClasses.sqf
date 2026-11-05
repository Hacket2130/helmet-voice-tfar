private _classes = [];
if (missionNamespace getVariable ["DTR_includeLLTE", true]) then {
    _classes append getArray (configFile >> "DTR_Settings" >> "helmetClasses");
};
_classes append getArray (missionConfigFile >> "DTR_Settings" >> "helmetClasses");
private _extra = missionNamespace getVariable ["DTR_extraHelmets", ""];
_classes append (_extra splitString (",; " + toString [9, 10, 13]));
DTR_helmetClasses = _classes arrayIntersect _classes;
DTR_helmetCache = createHashMap;

private _speakerClasses = [];
if (missionNamespace getVariable ["DTR_helmetIncludeLLTE", true]) then {
    _speakerClasses append getArray (configFile >> "DTR_Settings" >> "speakerHelmetClasses");
};
_speakerClasses append getArray (missionConfigFile >> "DTR_Settings" >> "speakerHelmetClasses");
private _speakerExtra = missionNamespace getVariable ["DTR_helmetExtraHelmets", ""];
_speakerClasses append (_speakerExtra splitString (",; " + toString [9, 10, 13]));
DTR_speakerHelmetClasses = _speakerClasses arrayIntersect _speakerClasses;
DTR_speakerHelmetCache = createHashMap;
diag_log format ["[DTR] Helmet mappings refreshed: DT=%1; speaker=%2", DTR_helmetClasses, DTR_speakerHelmetClasses];
