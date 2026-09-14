#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author -Nemesiss-, Sweetkr
 */
class SM_ATTACK : public AionServerPacket {
private:
	int32_t attackno{};
	int32_t time{};
	model::animations::AttackHandAnimation attackHandAnimation{};
	model::animations::AttackTypeAnimation attackTypeAnimation{};
	std::vector<runtime::Ref<controllers::attack::AttackResult>> attackList{};
	runtime::Ref<model::gameobjects::Creature> attacker{};
	runtime::Ref<model::gameobjects::Creature> target{};
	runtime::Ref<skillengine::model::Effect> criticalProcEffect{};

public:
	SM_ATTACK(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target, int32_t attackno, int32_t time,
		model::animations::AttackTypeAnimation attackTypeAnimation, model::animations::AttackHandAnimation attackHandAnimation,
		const std::vector<runtime::Ptr<controllers::attack::AttackResult>>& attackList);
	SM_ATTACK(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target, int32_t attackno, int32_t time,
		model::animations::AttackTypeAnimation attackTypeAnimation, model::animations::AttackHandAnimation attackHandAnimation,
		const std::vector<runtime::Ptr<controllers::attack::AttackResult>>& attackList, runtime::Ptr<skillengine::model::Effect> criticalProcEffect);
	~SM_ATTACK() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
