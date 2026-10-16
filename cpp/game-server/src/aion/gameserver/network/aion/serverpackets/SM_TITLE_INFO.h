#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/title/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author cura, xTz, -Enomine-
 */
class SM_TITLE_INFO : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::title::TitleList> titleList{};
	int32_t action{};
	int32_t titleId{};
	int32_t bonusTitleId{};
	int32_t playerObjId{};
public:
	explicit SM_TITLE_INFO(model::gameobjects::player::Player& player);
	explicit SM_TITLE_INFO(int32_t titleId);
	SM_TITLE_INFO(model::gameobjects::player::Player& player, int32_t titleId);
	explicit SM_TITLE_INFO(bool flag);
	SM_TITLE_INFO(model::gameobjects::player::Player& player, bool flag);
	SM_TITLE_INFO(int32_t action, int32_t bonusTitleId);
	~SM_TITLE_INFO() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
