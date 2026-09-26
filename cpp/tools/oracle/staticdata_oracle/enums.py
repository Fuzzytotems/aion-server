"""Java enum constants (declaration order = ordinal) needed by the count rules.

Copied by hand from the Java sources; tests/test_real_data.py verifies them against the enum declarations when the Java tree exists.
Enums whose ordinals do not matter for any count (TribeClass, SpawnType, ...) are not listed; their raw literals are compared instead.
"""

ENUM_SOURCES = {
	"PlayerClass": "model/PlayerClass.java",
	"Race": "model/Race.java",
	"SiegeType": "model/siege/SiegeType.java",
	"FoodType": "model/templates/pet/FoodType.java",
	"StatBonusType": "model/templates/item/bonuses/StatBonusType.java",
	"SignetEnum": "skillengine/model/SignetEnum.java",
	"AreaType": "model/templates/zone/AreaType.java",
}

PLAYER_CLASS = ("WARRIOR", "GLADIATOR", "TEMPLAR", "SCOUT", "ASSASSIN", "RANGER", "MAGE", "SORCERER", "SPIRIT_MASTER", "PRIEST", "CLERIC",
                "CHANTER", "ENGINEER", "RIDER", "GUNNER", "ARTIST", "BARD")

RACE = ("ELYOS", "ASMODIANS", "LYCAN", "CONSTRUCT", "CARRIER", "DRAKAN", "LIZARDMAN", "TELEPORTER", "NAGA", "BROWNIE", "KRALL", "SHULACK",
        "BARRIER", "PC_LIGHT_CASTLE_DOOR", "PC_DARK_CASTLE_DOOR", "DRAGON_CASTLE_DOOR", "GCHIEF_LIGHT", "GCHIEF_DARK", "DRAGON", "OUTSIDER",
        "RATMAN", "DEMIHUMANOID", "UNDEAD", "BEAST", "MAGICALMONSTER", "ELEMENTAL", "LIVINGWATER", "NONE", "PC_ALL", "DEFORM", "NEUT",
        "GHENCHMAN_LIGHT", "GHENCHMAN_DARK", "EVENT_TOWER_DARK", "EVENT_TOWER_LIGHT", "GOBLIN", "TRICODARK", "NPC", "LIGHT", "DARK",
        "WORLD_EVENT_DEFTOWER", "ORC", "DRAGONET", "SIEGEDRAKAN", "GCHIEF_DRAGON", "WORLD_EVENT_BONFIRE", "DOOR_KILLER", "LF5_Q_ITEM")

SIEGE_TYPE = ("FORTRESS", "ARTIFACT", "OUTPOST", "AGENT_FIGHT", "INDUN", "UNDERPASS")

FOOD_TYPE = ("AETHER_CHERRY", "AETHER_CRYSTAL_BISCUIT", "AETHER_GEM_BISCUIT", "AETHER_POWDER_BISCUIT", "ARMOR", "BALAUR_SCALES", "BONES",
             "EXCLUDES", "FLUIDS", "HEALTHY_FOOD_ALL", "HEALTHY_FOOD_SPICY", "MISCELLANEOUS", "POPPY_SNACK", "POPPY_SNACK_TASTY",
             "POPPY_SNACK_NUTRITIOUS", "SOULS", "SHUGO_EVENT_COIN", "STINKY", "THORNS")

STAT_BONUS_TYPE = ("INVENTORY", "POLISH")

SIGNET_ENUM = ("SIGNET1", "SIGNET2", "SIGNET3", "SIGNET4")

AREA_TYPE = ("POLYGON", "CYLINDER", "SPHERE", "SEMISPHERE")

BY_NAME = {
	"PlayerClass": PLAYER_CLASS,
	"Race": RACE,
	"SiegeType": SIEGE_TYPE,
	"FoodType": FOOD_TYPE,
	"StatBonusType": STAT_BONUS_TYPE,
	"SignetEnum": SIGNET_ENUM,
	"AreaType": AREA_TYPE,
}
