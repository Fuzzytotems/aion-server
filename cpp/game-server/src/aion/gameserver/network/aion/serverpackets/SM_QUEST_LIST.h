#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_QUEST_LIST : public AionServerPacket {
private:
	std::vector<runtime::Ref<questEngine::model::QuestState>> questStates{};
public:
	explicit SM_QUEST_LIST(const std::vector<runtime::Ptr<questEngine::model::QuestState>>& questState);
	~SM_QUEST_LIST() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
