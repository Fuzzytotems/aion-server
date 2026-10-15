#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/assemblednpc/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz
 */
class SM_NPC_ASSEMBLER : public AionServerPacket {
private:
	runtime::Ref<model::assemblednpc::AssembledNpc> assembledNpc{};
	int32_t routeId{};
	int64_t timeOnMap{};
public:
	explicit SM_NPC_ASSEMBLER(runtime::Ptr<model::assemblednpc::AssembledNpc> assembledNpc);
	~SM_NPC_ASSEMBLER() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
