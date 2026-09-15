#include "aion/gameserver/model/gameobjects/player/title/Title.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/templates/TitleTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player::title {

Title::Title(const templates::TitleTemplate* templateValue, int32_t idValue, int32_t expireTimeValue)
	: template_(templateValue), id(idValue), expireTime(expireTimeValue) {
}

Title::~Title() = default;

runtime::Ref<Title> Title::create(const templates::TitleTemplate* templateValue, int32_t idValue, int32_t expireTimeValue) {
	return runtime::makeRef<Title>(templateValue, idValue, expireTimeValue);
}

void Title::onExpire(Player& player) {
	player.getTitleList().removeTitle(id);
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_TITLE_BY_TIMEOUT(template_->getL10n()));
}

} // namespace aion::gameserver::model::gameobjects::player::title
