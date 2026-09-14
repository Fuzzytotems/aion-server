#include "aion/gameserver/configs/main/CraftConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void CraftConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.craft.skills.delete.excess.enable", DELETE_EXCESS_CRAFT_ENABLE, "false");
	AION_BIND(p, "gameserver.craft.max.expert.skills", MAX_EXPERT_CRAFTING_SKILLS, "2");
	AION_BIND(p, "gameserver.craft.max.master.skills", MAX_MASTER_CRAFTING_SKILLS, "1");
	AION_BIND(p, "gameserver.craft.disable.tapping.cap", DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP, "false");
	AION_BIND(p, "gameserver.craft.fail.chance", MAX_CRAFT_FAILURE_CHANCE, "33");
	AION_BIND(p, "gameserver.gather.fail.chance", MAX_GATHER_FAILURE_CHANCE, "33");
}

} // namespace aion::gameserver::configs::main
