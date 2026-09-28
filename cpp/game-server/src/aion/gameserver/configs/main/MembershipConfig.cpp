#include "aion/gameserver/configs/main/MembershipConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void MembershipConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.membership.types", MEMBERSHIP_TYPES, "Premium");
	AION_BIND(p, "gameserver.membership.gathering.allow_on_mount", GATHERING_ALLOW_ON_MOUNT, "10");
	AION_BIND(p, "gameserver.instances.title.requirement", INSTANCES_TITLE_REQ, "10");
	AION_BIND(p, "gameserver.instances.race.requirement", INSTANCES_RACE_REQ, "10");
	AION_BIND(p, "gameserver.instances.level.requirement", INSTANCES_LEVEL_REQ, "10");
	AION_BIND(p, "gameserver.instances.group.requirement", INSTANCES_GROUP_REQ, "10");
	AION_BIND(p, "gameserver.instances.quest.requirement", INSTANCES_QUEST_REQ, "10");
	AION_BIND(p, "gameserver.instances.cooldown", INSTANCES_COOLDOWN, "10");
	AION_BIND(p, "gameserver.emotions.all", EMOTIONS_ALL, "10");
	AION_BIND(p, "gameserver.quest.stigma.slot", STIGMA_SLOT_QUEST, "10");
	AION_BIND(p, "gameserver.soulsickness.disable", DISABLE_SOULSICKNESS, "10");
	AION_BIND(p, "gameserver.autolearn.stigma", STIGMA_AUTOLEARN, "10");
	AION_BIND(p, "gameserver.quest.limit.disable", QUEST_LIMIT_DISABLED, "10");
	AION_BIND(p, "gameserver.character.additional.enable", CHARACTER_ADDITIONAL_ENABLE, "10");
	AION_BIND(p, "gameserver.character.additional.count", CHARACTER_ADDITIONAL_COUNT, "8");
}

} // namespace aion::gameserver::configs::main
