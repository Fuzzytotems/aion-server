#include "aion/gameserver/configs/main/PlayerTransferConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void PlayerTransferConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "ptransfer.max.kinah", MAX_KINAH, "0");
	AION_BIND(p, "ptransfer.bindpoint.elyos", BIND_ELYOS, "210010000 1212.9423 1044.8516 140.75568 32");
	AION_BIND(p, "ptransfer.bindpoint.asmo", BIND_ASMO, "220010000 571.0388 2787.3420 299.8750 32");
	AION_BIND(p, "ptransfer.allow.emotions", ALLOW_EMOTIONS, "true");
	AION_BIND(p, "ptransfer.allow.motions", ALLOW_MOTIONS, "true");
	AION_BIND(p, "ptransfer.allow.macro", ALLOW_MACRO, "true");
	AION_BIND(p, "ptransfer.allow.npcfactions", ALLOW_NPCFACTIONS, "true");
	AION_BIND(p, "ptransfer.allow.pets", ALLOW_PETS, "true");
	AION_BIND(p, "ptransfer.allow.recipes", ALLOW_RECIPES, "true");
	AION_BIND(p, "ptransfer.allow.skills", ALLOW_SKILLS, "true");
	AION_BIND(p, "ptransfer.allow.titles", ALLOW_TITLES, "true");
	AION_BIND(p, "ptransfer.allow.quests", ALLOW_QUESTS, "true");
	AION_BIND(p, "ptransfer.allow.inventory", ALLOW_INV, "true");
	AION_BIND(p, "ptransfer.allow.warehouse", ALLOW_WAREHOUSE, "true");
	AION_BIND(p, "ptransfer.allow.stigma", ALLOW_STIGMA, "true");
	AION_BIND(p, "ptransfer.block.samename", BLOCK_SAMENAME, "false");
	AION_BIND(p, "ptransfer.retransfer.hours", REUSE_HOURS, "0");
	AION_BIND(p, "ptransfer.remove.skills.list", REMOVE_SKILL_LIST, "*");
}

} // namespace aion::gameserver::configs::main
