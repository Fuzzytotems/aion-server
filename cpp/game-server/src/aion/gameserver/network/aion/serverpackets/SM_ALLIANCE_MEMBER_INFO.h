#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sarynth (Thx Rhys2002 for Packets)
 */
class SM_ALLIANCE_MEMBER_INFO : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	model::team::common::legacy::PlayerAllianceEvent event{};
	int32_t allianceId{};
	int32_t objectId{};
	int32_t slot{};
	std::vector<runtime::Ref<skillengine::model::Effect>> abnormalEffects{};

public:
	SM_ALLIANCE_MEMBER_INFO(model::team::alliance::PlayerAllianceMember& member, model::team::common::legacy::PlayerAllianceEvent event,
		int32_t slot);
	SM_ALLIANCE_MEMBER_INFO(model::team::alliance::PlayerAllianceMember& member, model::team::common::legacy::PlayerAllianceEvent event);
	~SM_ALLIANCE_MEMBER_INFO() override;

	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
