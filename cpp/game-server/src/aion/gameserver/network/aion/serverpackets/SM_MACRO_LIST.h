#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Packet with macro list.
 *
 * @author -Nemesiss-
 */
class SM_MACRO_LIST : public AionServerPacket {
public:
	static constexpr int32_t STATIC_BODY_SIZE = 7;
	/** Java: Function<Macros.Macro, Integer> `(macro) -> 1 + macro.xml().length() * 2 + 2` (SplitList part sizes); defined in the .cpp */
	static const runtime::PinnedCallback<int32_t(model::gameobjects::player::Macros::Macro&)> DYNAMIC_BODY_PART_SIZE_CALCULATOR;
private:
	int32_t playerObjectId{};
	std::vector<runtime::Ref<model::gameobjects::player::Macros::Macro>> macros{};
	bool clearList{};

public:
	SM_MACRO_LIST(int32_t playerObjectId, const std::vector<runtime::Ptr<model::gameobjects::player::Macros::Macro>>& macros, bool clearList);
	~SM_MACRO_LIST() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
