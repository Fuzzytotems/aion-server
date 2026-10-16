#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author MrPoke, Neon
 */
class SM_QUEST_COMPLETED_LIST : public AionServerPacket {
public:
	static constexpr int32_t STATIC_BODY_SIZE = 4;
	/** Java: Function<QuestState, Integer> `(questState) -> 6` (SplitList part sizes); defined in the .cpp */
	static const runtime::PinnedCallback<int32_t(questEngine::model::QuestState&)> DYNAMIC_BODY_PART_SIZE_CALCULATOR;
private:
	int32_t updateMode{};
	std::vector<runtime::Ref<questEngine::model::QuestState>> questStates{};

public:
	SM_QUEST_COMPLETED_LIST(int32_t updateMode, const std::vector<runtime::Ptr<questEngine::model::QuestState>>& questStates);
	~SM_QUEST_COMPLETED_LIST() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
