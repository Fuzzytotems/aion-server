#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/team/group/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Lyahim, ATracer, xTz
 */
class SM_GROUP_INFO : public AionServerPacket {
private:
	runtime::Ref<model::team::common::legacy::LootGroupRules> lootRules{};
	int32_t groupId{};
	int32_t leaderId{};
	model::team::TeamType type{};

public:
	explicit SM_GROUP_INFO(model::team::group::PlayerGroup& group);
	~SM_GROUP_INFO() override;

	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
