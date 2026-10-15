#include "aion/gameserver/network/aion/serverpackets/SM_CHALLENGE_LIST.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/challenge/ChallengeQuest.h"
#include "aion/gameserver/model/challenge/ChallengeTask.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/challenge/ChallengeTypeInfo.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

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
	if (con == nullptr)
		throw runtime::NullPointerException("SM_CHALLENGE_LIST::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	writeC(action);
	writeD(ownerId); // legionId or townId
	writeC(model::templates::challenge::getId(ownerType)); // 1 for legion, 2 for town
	writeD(player->getObjectId());
	switch (action) {
		case 2: // send challenge tasks list
			writeD(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000));
			writeH(static_cast<int32_t>(tasks.size()));
			for (const runtime::Ref<model::challenge::ChallengeTask>& taskEntry : tasks) {
				writeD(32); // unk
				writeD(taskEntry->getTaskId());
				writeC(1); // unk
				writeC(21); // unk
				writeC(0); // unk
				writeD(taskEntry->getCompleteTimeEpochSeconds());
			}
			break;
		case 7: { // send individual challenge task info
			runtime::Ptr<model::challenge::ChallengeTask> challengeTask = task;
			writeD(32); // unk
			writeD(challengeTask->getTaskId());
			writeH(challengeTask->getQuestsCount());
			for (runtime::Ptr<model::challenge::ChallengeQuest> quest : challengeTask->getQuests().values()) {
				writeD(quest->getQuestId());
				writeH(quest->getMaxRepeats());
				writeD(quest->getScorePerQuest());
				writeH(quest->getCompleteCount()); // unk
			}
			break;
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
