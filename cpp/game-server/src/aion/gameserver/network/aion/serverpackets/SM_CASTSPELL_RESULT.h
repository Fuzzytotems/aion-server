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
 * This packet show cast spell result (including hit time).
 *
 * @author alexa026, Sweetkr
 */
class SM_CASTSPELL_RESULT : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> effector{};
	runtime::Ref<model::gameobjects::Creature> target{};
	runtime::Ref<skillengine::model::Skill> skill{};
	int32_t cooldown{};
	int32_t hitTime{};
	std::vector<runtime::Ref<skillengine::model::Effect>> effects{};
	int32_t dashStatus{};
	int32_t targetType{};
	bool chainSuccess{};

public:
	SM_CASTSPELL_RESULT(skillengine::model::Skill& skill, const std::vector<runtime::Ptr<skillengine::model::Effect>>& effects, int32_t hitTime,
		bool chainSuccess, int32_t dashStatus);
	SM_CASTSPELL_RESULT(skillengine::model::Skill& skill, const std::vector<runtime::Ptr<skillengine::model::Effect>>& effects, int32_t hitTime,
		bool chainSuccess, int32_t dashStatus, int32_t targetType);
	~SM_CASTSPELL_RESULT() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
