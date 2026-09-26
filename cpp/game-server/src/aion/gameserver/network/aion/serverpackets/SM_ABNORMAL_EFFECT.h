#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_ABNORMAL_EFFECT : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> effected{};
	int32_t effectType{};
	int32_t abnormals{};
	std::vector<runtime::Ref<skillengine::model::Effect>> filtered{};
	int32_t slots{};

public:
	explicit SM_ABNORMAL_EFFECT(model::gameobjects::Creature& effected);
	SM_ABNORMAL_EFFECT(model::gameobjects::Creature& effected, int32_t abnormals,
		const std::vector<runtime::Ptr<skillengine::model::Effect>>& effects, int32_t slots);
	~SM_ABNORMAL_EFFECT() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
