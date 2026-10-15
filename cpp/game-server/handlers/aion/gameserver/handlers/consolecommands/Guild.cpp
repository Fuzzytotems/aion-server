#include "aion/gameserver/handlers/consolecommands/Guild.h"

#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_LEGION_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_LEGION_MEMBERLIST.h"
#include "aion/gameserver/utils/collections/FixedElementCountSplitList.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Guild);

// Java Guild.java:19-21
Guild::Guild() : ConsoleCommand("guild", "Displays info about given player's legion.") {
}

// Java Guild.java:24-35
void Guild::execute(Player& admin, std::span<const std::string> params) {
	runtime::Ptr<Player> target = params.size() > 0 ? World::getInstance().getPlayer(params[0]) : nullptr; // parity= Player target = params.length > 0 ? World.getInstance().getPlayer(params[0]) : null;
	if (target != nullptr) {
		runtime::Ptr<Legion> legion = target->getLegion(); // parity= Legion legion = target.getLegion();
		if (target->getLegion() != nullptr) {
			PacketSendUtility::sendPacket(admin, SM_GM_SHOW_LEGION_INFO(*legion)); // parity= PacketSendUtility.sendPacket(admin, new SM_GM_SHOW_LEGION_INFO(legion));
			std::vector<runtime::Ref<LegionMember>> members; // parity= SplitList<LegionMember> legionMemberSplitList = new FixedElementCountSplitList<>(legion.getMembers(), true, 80);
			for (const runtime::Ptr<LegionMember>& member : legion->getMembers()) // parity: (the same)
				members.emplace_back(member); // parity: (the same)
			FixedElementCountSplitList<LegionMember> legionMemberSplitList(std::move(members), true, 80); // parity: (the same)
			for (utils::collections::ListPart<LegionMember>& part : legionMemberSplitList) // parity= legionMemberSplitList.forEach(part -> PacketSendUtility.sendPacket(admin,
				PacketSendUtility::sendPacket(admin, SM_GM_SHOW_LEGION_MEMBERLIST(part.borrowed(), part.isFirst(), part.isLast())); // parity= new SM_GM_SHOW_LEGION_MEMBERLIST(part, part.isFirst(), part.isLast())));
		}
	}
}

} // namespace aion::gameserver::handlers::consolecommands
