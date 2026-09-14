#include "aion/gameserver/network/aion/serverpackets/SM_CHALLENGE_LIST.h"

#include "aion/gameserver/model/challenge/ChallengeTask.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHALLENGE_LIST::SM_CHALLENGE_LIST(int32_t actionValue, int32_t ownerIdValue, model::templates::challenge::ChallengeType ownerTypeValue,
	const std::vector<runtime::Ptr<model::challenge::ChallengeTask>>& tasksValue)
	: AionServerPacket(opcodeOf<SM_CHALLENGE_LIST>), action(actionValue), ownerId(ownerIdValue), ownerType(ownerTypeValue),
	  tasks(tasksValue.begin(), tasksValue.end()) {
}

SM_CHALLENGE_LIST::SM_CHALLENGE_LIST(int32_t actionValue, int32_t ownerIdValue, model::templates::challenge::ChallengeType ownerTypeValue,
	model::challenge::ChallengeTask& taskValue)
	: AionServerPacket(opcodeOf<SM_CHALLENGE_LIST>), action(actionValue), ownerId(ownerIdValue), ownerType(ownerTypeValue), task(taskValue) {
}

SM_CHALLENGE_LIST::~SM_CHALLENGE_LIST() = default;

void SM_CHALLENGE_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
