#include "aion/gameserver/skillengine/condition/LeftHandCondition.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::Item;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::item::LeftHandSlot;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using utils::PacketSendUtility;

bool LeftHandCondition::validate(model::Skill& env) const {
	if (Ptr<Player> player = runtime::as<Player>(env.getEffector())) {
		if (!type.has_value()) // Java: a switch over the null `type` attribute throws
			throw runtime::NullPointerException("Cannot invoke \"LeftHandSlot.ordinal()\" because \"this.type\" is null");
		switch (*type) {
			case LeftHandSlot::DUAL: {
				Ptr<Item> offHandWeapon = player->getEquipment().getOffHandWeapon();
				Ptr<Item> mainHandWeapon;
				if ((offHandWeapon && offHandWeapon->getItemTemplate()->isWeapon())
					|| ((mainHandWeapon = player->getEquipment().getMainHandWeapon()) && mainHandWeapon->getItemTemplate()->isTwoHandWeapon()))
					return true;
				else {
					PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NEED_DUAL_WEAPON());
					return false;
				}
			}
			case LeftHandSlot::SHIELD:
				if (player->getEquipment().isShieldEquipped())
					return true;
				else {
					PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_NEED_SHIELD());
					return false;
				}
		}
	}
	return false;
}

} // namespace aion::gameserver::skillengine::condition
