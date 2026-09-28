#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author nrg, Neon
 */
class SM_INSTANCE_INFO : public AionServerPacket {
private:
	int8_t updateType{};
	std::vector<runtime::Ref<model::gameobjects::player::Player>> players{};
	std::vector<int32_t> instanceIds{};

public:
	SM_INSTANCE_INFO(int8_t updateType, model::gameobjects::player::Player& player, std::initializer_list<int32_t> instanceId = {});
	SM_INSTANCE_INFO(int8_t updateType, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players,
		std::initializer_list<int32_t> instanceId = {});
	~SM_INSTANCE_INFO() override;

	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
