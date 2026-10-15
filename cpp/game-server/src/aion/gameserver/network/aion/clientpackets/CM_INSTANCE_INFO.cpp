#include "aion/gameserver/network/aion/clientpackets/CM_INSTANCE_INFO.h"

#include <utility>
#include <vector>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_INFO.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/utils/collections/FixedElementCountSplitList.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/utils/collections/Predicates.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_INSTANCE_INFO::CM_INSTANCE_INFO(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_INSTANCE_INFO::readImpl() {
	readD(); // unk (always 0)
	updateType = readC();
}

void CM_INSTANCE_INFO::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	// always the team leader. GeneralTeam::getLeaderObject and filterMembers are P5-10 bodies and still AION_UNPORTED (GeneralTeam.cpp:56,88);
	// no code in the tree puts a player into a group or an alliance at M5a, so isInTeam() is false and neither is evaluated, as at
	// PlayerLifeStats.cpp:67-70. Written as Java has it, so the missing bodies name themselves once teams land.
	runtime::Ptr<Player> firstObject = player->isInTeam() ? player->getCurrentTeam()->getLeaderObject() : player;
	sendPacket(serverpackets::SM_INSTANCE_INFO(updateType, *firstObject));
	if (updateType == 1 && player->isInTeam()) {
		// Java: filterMembers(Predicate<Player>) on TemporaryPlayerTeam. The erasure spells GeneralTeam's M as AionObject
		// (hub-headers.md §8.1), so the Predicates callback is called through a narrowing adapter and the result narrowed back, exactly as
		// TemporaryPlayerTeam::getLeaderObject does (TemporaryPlayerTeam.cpp:44-46).
		runtime::PinnedCallback<bool(Player&)> allExcept = utils::collections::Predicates::Players::allExcept(*firstObject);
		std::vector<runtime::Ref<Player>> filteredTeamMembers;
		for (const runtime::Ptr<model::gameobjects::AionObject>& member : player->getCurrentTeam()->filterMembers(
				 [&allExcept](model::gameobjects::AionObject& teamMember) { return allExcept(*runtime::cast<Player>(teamMember)); }))
			filteredTeamMembers.emplace_back(runtime::cast<Player>(member));
		utils::collections::FixedElementCountSplitList<Player> playersSplitList(std::move(filteredTeamMembers), false, 3);
		for (utils::collections::ListPart<Player>& part : playersSplitList)
			sendPacket(serverpackets::SM_INSTANCE_INFO(int8_t{2}, part.borrowed()));
	}
}

AION_CLIENT_PACKET(CM_INSTANCE_INFO);

} // namespace aion::gameserver::network::aion::clientpackets
