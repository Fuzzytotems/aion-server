#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sarynth
 */
class SM_KISK_UPDATE : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Kisk> kisk{};

public:
	explicit SM_KISK_UPDATE(model::gameobjects::Kisk& kisk);
	~SM_KISK_UPDATE() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
