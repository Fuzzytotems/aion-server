#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/legionDominion/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Yeats
 */
class SM_LEGION_DOMINION_RANK : public AionServerPacket {
private:
	runtime::Ref<model::legionDominion::LegionDominionLocation> loc{};
	int32_t rank{};
	std::vector<runtime::Ref<model::legionDominion::LegionDominionParticipantInfo>> topParticipants{};
public:
	SM_LEGION_DOMINION_RANK(model::legionDominion::LegionDominionLocation& loc, runtime::Ptr<model::team::legion::Legion> legion);
	~SM_LEGION_DOMINION_RANK() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
