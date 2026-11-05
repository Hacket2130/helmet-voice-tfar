class CfgPatches {
    class dtr_core {
        name = "Helmet Voice and Death Trooper Voice - TFAR Beta R11";
        author = "DTR contributors";
        requiredVersion = 2.14;
        requiredAddons[] = {"cba_main", "cba_xeh", "cba_settings", "tfar_core"};
        units[] = {};
        weapons[] = {};
    };
};

class Extended_PreInit_EventHandlers {
    class dtr_core {
        init = "call compile preprocessFileLineNumbers '\dtr\core\XEH_preInit.sqf'";
    };
};

class Extended_PostInit_EventHandlers {
    class dtr_core {
        init = "call compile preprocessFileLineNumbers '\dtr\core\XEH_postInit.sqf'";
    };
};

class CfgFunctions {
    class DTR {
        class core {
            file = "\dtr\core\functions";
            class isHelmet {};
            class isSpeakerHelmet {};
            class getState {};
            class rebuildHelmetClasses {};
            class setScrambler {};
            class setHelmetVoice {};
            class initAce {};
            class syncSettings {};
            class diagnose {};
        };
    };
};

class DTR_Settings {
    // LLTE Workshop 3447903805: JMSLLTE_empire.pbo, helmets/config.cpp.
    helmetClasses[] = {"JMSLLTE_Deathtrooper_helmet"};
    // Separate roots: ordinary helmet speakers never gain Death Trooper decoding.
    speakerHelmetClasses[] = {
        "JMSLLTE_Stormtrooper_dirtyW_helmet",
        "JMSLLTE_Stormtrooper_S_helmet",
        "JMSLLTE_Stormtrooper_com_S_helmet",
        "JMSLLTE_Clonetrooper_P3_helmet",
        "JMSLLTE_Stormtrooper_helmet",
        "JMSLLTE_Stormtrooper_shadow_helmet",
        "JMSLLTE_Stormtrooper_mimban_helmet",
        "JMSLLTE_Stormtrooper_mimban_H_helmet",
        "JMSLLTE_Stormtrooper_dirty_H_helmet",
        "JMSLLTE_Shoretrooper_helmet",
        "JMSLLTE_Tanktrooper_helmet",
        "JMSLLTE_Snowtrooper_helmet",
        "JMSLLTE_Snowtflamer_helmet",
        "JMSLLTE_EmpTiePilot_helmet",
        "JMSLLTE_EmpTiePilotTech_helmet",
        "JMSLLTE_Scouttrooper_helmet"
    };
};
