#include "aion/gameserver/configs/main/RatesConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void RatesConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.rates.crafting.crit_chances", CRAFT_CRIT_CHANCES, "15.0, 30.0");
	AION_BIND(p, "gameserver.rates.crafting.combo_crit_chances", CRAFT_COMBO_CHANCES, "25.0, 50.0");
	AION_BIND(p, "gameserver.rates.manastone_chances", MANASTONE_CHANCES, "75.0, 75.0");
	AION_BIND(p, "gameserver.rates.enchantment_stone.base_chances", ENCHANTMENT_STONE_BASE_CHANCES, "65.0, 65.0");
	AION_BIND(p, "gameserver.rates.enchantment_stone.amplified_chances", ENCHANTMENT_STONE_AMPLIFIED_CHANCES, "61.0, 61.0");
	AION_BIND(p, "gameserver.rates.tampering_chances", TEMPERING_CHANCES, "65.0, 65.0");
	AION_BIND(p, "gameserver.rates.xp.solo", XP_SOLO_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.xp.group", XP_GROUP_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.xp.quest", XP_QUEST_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.xp.gathering", XP_GATHERING_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.xp.crafting", XP_CRAFTING_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.xp.pvp", XP_PVP_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.skill_xp.gathering", SKILL_XP_GATHERING_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.skill_xp.crafting", SKILL_XP_CRAFTING_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.ap.pvp.gain", AP_PVP_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.ap.pvp.loss", AP_PVP_LOSS_RATES, "1.0, 1.0");
	AION_BIND(p, "gameserver.rates.ap.pve", AP_PVE_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.ap.quest", AP_QUEST_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.ap.dredgion", AP_DREDGION_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.gp.gain", GP_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.dp.pve", DP_PVE_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.dp.pvp", DP_PVP_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.kinah.quest", QUEST_KINAH_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.drop", DROP_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.gathering.count", GATHERING_COUNT_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.pvparena.discipline", PVP_ARENA_DISCIPLINE_REWARD_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.pvparena.chaos", PVP_ARENA_CHAOS_REWARD_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.pvparena.harmony", PVP_ARENA_HARMONY_REWARD_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.pvparena.glory", PVP_ARENA_GLORY_REWARD_RATES, "1.0, 2.0");
	AION_BIND(p, "gameserver.rates.sell_limit", SELL_LIMIT_RATES, "1.0, 2.0");
}

} // namespace aion::gameserver::configs::main
