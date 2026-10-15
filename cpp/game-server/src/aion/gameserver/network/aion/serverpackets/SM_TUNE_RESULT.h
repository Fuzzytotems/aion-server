#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Estrayl, Sykra
 */
class SM_TUNE_RESULT : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Item> targetItem{};
	int32_t tuningScrollItemId{};
	runtime::Ref<model::items::PendingTuneResult> result{};
	bool tuneCancelPossible{};
	bool showManastoneSlots{};
public:
	SM_TUNE_RESULT(model::gameobjects::Item& targetItem, int32_t tuningScrollItemId, model::items::PendingTuneResult& result);
	~SM_TUNE_RESULT() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
