#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/group/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Lyahim, ATracer
 */
class SM_GROUP_MEMBER_INFO : public AionServerPacket {
private:
	int32_t groupId{};
	runtime::Ref<model::gameobjects::player::Player> player{};
	model::team::common::legacy::GroupEvent event{};
	int32_t slot{};
	std::vector<runtime::Ref<skillengine::model::Effect>> abnormalEffects{};

public:
	SM_GROUP_MEMBER_INFO(model::team::group::PlayerGroup& group, model::gameobjects::player::Player& player,
		model::team::common::legacy::GroupEvent event, int32_t slot);
	SM_GROUP_MEMBER_INFO(model::team::group::PlayerGroup& group, model::gameobjects::player::Player& player,
		model::team::common::legacy::GroupEvent event);
	~SM_GROUP_MEMBER_INFO() override;

	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
