#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Avol, ATracer
 */
class SM_ABNORMAL_STATE : public AionServerPacket {
private:
	std::vector<runtime::Ref<skillengine::model::Effect>> effects{};
	int32_t abnormals{};
	int32_t slot{};

public:
	SM_ABNORMAL_STATE(const std::vector<runtime::Ptr<skillengine::model::Effect>>& effects, int32_t abnormals, int32_t slot);
	~SM_ABNORMAL_STATE() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
