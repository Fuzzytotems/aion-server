#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr, xTz, kecimis
 */
class SM_TRANSFORM : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> creature{};
public:
	explicit SM_TRANSFORM(model::gameobjects::Creature& creature);
	~SM_TRANSFORM() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
