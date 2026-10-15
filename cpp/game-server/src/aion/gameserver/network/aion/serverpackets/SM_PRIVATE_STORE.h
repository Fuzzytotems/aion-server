#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Simple
 */
class SM_PRIVATE_STORE : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	runtime::Ref<model::gameobjects::player::PrivateStore> store{};
public:
	/** @param store the seller's store, null if the seller has none (Java: getOwner().getStore(), checked in writeImpl) */
	SM_PRIVATE_STORE(runtime::Ptr<model::gameobjects::player::PrivateStore> store, model::gameobjects::player::Player& player);
	~SM_PRIVATE_STORE() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
