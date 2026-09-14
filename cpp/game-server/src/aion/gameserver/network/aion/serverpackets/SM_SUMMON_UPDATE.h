#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_SUMMON_UPDATE : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Summon> summon{};
public:
	explicit SM_SUMMON_UPDATE(model::gameobjects::Summon& summon);
	~SM_SUMMON_UPDATE() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
