#include "aion/gameserver/handlers/consolecommands/Skill.h"

#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_PLAYER_SKILLS.h"
#include "aion/gameserver/network/aion/skillinfo/SkillEntryWriter.h"
#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Skill);

// Java Skill.java:18-20
Skill::Skill() : ConsoleCommand("skill") {
}

// Java Skill.java:23-34
void Skill::execute(Player& admin, std::span<const std::string> params) {
	runtime::Ptr<Player> target; // parity= Player target = null;
	if (!params.empty())
		target = World::getInstance().getPlayer(params[0]);
	if (target == nullptr) // parity= if (target == null && admin.getTarget() instanceof Player player)
		if (runtime::Ptr<Player> player = runtime::as<Player>(admin.getTarget())) // parity: (the same)
			target = player;
	if (target != nullptr) {
		std::vector<runtime::Ref<PlayerSkillEntry>> skills; // parity= SplitList<PlayerSkillEntry> skillEntrySplitList = new DynamicServerPacketBodySplitList<>(target.getSkillList().getAllSkills(), false,
		for (runtime::Ptr<PlayerSkillEntry> skill : target->getSkillList()->getAllSkills()) // parity: (the same)
			skills.emplace_back(skill); // parity: (the same)
		DynamicServerPacketBodySplitList<PlayerSkillEntry> skillEntrySplitList(std::move(skills), false, // parity: (the same)
			SM_GM_SHOW_PLAYER_SKILLS::STATIC_BODY_SIZE, SkillEntryWriter::DYNAMIC_BODY_PART_SIZE_CALCULATOR);
		for (utils::collections::ListPart<PlayerSkillEntry>& part : skillEntrySplitList) // parity= skillEntrySplitList.forEach(part -> PacketSendUtility.sendPacket(admin, new SM_GM_SHOW_PLAYER_SKILLS(part)));
			PacketSendUtility::sendPacket(admin, SM_GM_SHOW_PLAYER_SKILLS(part.borrowed())); // parity: (the same)
	}
}

} // namespace aion::gameserver::handlers::consolecommands
