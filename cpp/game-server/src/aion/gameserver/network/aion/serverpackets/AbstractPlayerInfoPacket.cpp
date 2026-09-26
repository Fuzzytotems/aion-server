#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Rc.h"

namespace aion::gameserver::network::aion::serverpackets {

AbstractPlayerInfoPacket::AbstractPlayerInfoPacket(int32_t opCode) : AionServerPacket(opCode) {
}

void AbstractPlayerInfoPacket::writePlayerInfo(model::account::PlayerAccountData& accPlData, AionConnection* con) {
	runtime::Ptr<model::gameobjects::player::PlayerCommonData> pcd = accPlData.getPlayerCommonData();
	int32_t playerId = pcd->getPlayerObjId();
	runtime::Ptr<model::team::legion::LegionMember> legionMember = detail::getLegionMember(*pcd);
	runtime::Ptr<model::gameobjects::player::PlayerAppearance> playerAppearance = accPlData.getAppearance();
	runtime::Ref<model::account::CharacterBanInfo> cbi = getCharBanInfo(accPlData, con);

	writeD(playerId);
	writeS(pcd->getName(), CHARNAME_MAX_LENGTH);
	writeD(model::getGenderId(pcd->getGender()));
	writeD(model::getRaceId(pcd->getRace()));
	writeD(model::getClassId(pcd->getPlayerClass()));
	writeD(playerAppearance->getVoice());
	writeD(playerAppearance->getSkinRGB());
	writeD(playerAppearance->getHairRGB());
	writeD(playerAppearance->getEyeRGB());
	writeD(playerAppearance->getLipRGB());
	writeC(playerAppearance->getFace());
	writeC(playerAppearance->getHair());
	writeC(playerAppearance->getDeco());
	writeC(playerAppearance->getTattoo());
	writeC(playerAppearance->getFaceContour());
	writeC(playerAppearance->getExpression());
	writeC(5); // always 5 o0
	writeC(playerAppearance->getJawLine());
	writeC(playerAppearance->getForehead());
	writeC(playerAppearance->getEyeHeight());
	writeC(playerAppearance->getEyeSpace());
	writeC(playerAppearance->getEyeWidth());
	writeC(playerAppearance->getEyeSize());
	writeC(playerAppearance->getEyeShape());
	writeC(playerAppearance->getEyeAngle());
	writeC(playerAppearance->getBrowHeight());
	writeC(playerAppearance->getBrowAngle());
	writeC(playerAppearance->getBrowShape());
	writeC(playerAppearance->getNose());
	writeC(playerAppearance->getNoseBridge());
	writeC(playerAppearance->getNoseWidth());
	writeC(playerAppearance->getNoseTip());
	writeC(playerAppearance->getCheek());
	writeC(playerAppearance->getLipHeight());
	writeC(playerAppearance->getMouthSize());
	writeC(playerAppearance->getLipSize());
	writeC(playerAppearance->getSmile());
	writeC(playerAppearance->getLipShape());
	writeC(playerAppearance->getJawHeigh());
	writeC(playerAppearance->getChinJut());
	writeC(playerAppearance->getEarShape());
	writeC(playerAppearance->getHeadSize());
	// 1.5.x 0x00, shoulderSize, armLength, legLength (BYTE) after HeadSize
	writeC(playerAppearance->getNeck());
	writeC(playerAppearance->getNeckLength());
	writeC(playerAppearance->getShoulderSize());
	writeC(playerAppearance->getTorso());
	writeC(playerAppearance->getChest());
	writeC(playerAppearance->getWaist());
	writeC(playerAppearance->getHips());
	writeC(playerAppearance->getArmThickness());
	writeC(playerAppearance->getHandSize());
	writeC(playerAppearance->getLegThickness());
	writeC(playerAppearance->getFootSize());
	writeC(playerAppearance->getFacialRate());
	writeC(0x00); // 0x00
	writeC(playerAppearance->getArmLength());
	writeC(playerAppearance->getLegLength());
	writeC(playerAppearance->getShoulders());
	writeC(playerAppearance->getFaceShape());
	writeC(0x00); // always 0 may be acessLevel
	writeC(0x00); // sometimes 0xC7 (199) for all chars, else 0
	writeC(0x00); // sometimes 0x04 (4) for all chars, else 0
	writeF(playerAppearance->getHeight());
	writeD(pcd->getTemplateId());
	writeD(pcd->getMapId()); // mapid for preloading map
	writeF(pcd->getX());
	writeF(pcd->getY());
	writeF(pcd->getZ());
	writeD(pcd->getHeading());
	writeH(pcd->getLevel());
	writeH(0); // unk 2.5
	writeD(pcd->getTitleId());
	writeD(legionMember != nullptr ? legionMember->getLegion()->getLegionId() : 0);
	writeS(legionMember != nullptr ? legionMember->getLegion()->getName() : std::string(), 40); // Java null: zero-padded like ""
	writeH(legionMember != nullptr ? 1 : 0);
	writeD(pcd->getLastOnlineEpochSeconds());
	using VisibleItem = model::account::PlayerAccountData::VisibleItem;
	const std::vector<runtime::Ptr<VisibleItem>> visibleItems = accPlData.getVisibleItems()->snapshot();
	for (size_t i = 0; i < 16; i++) { // 16 items is always expected by the client...
		runtime::Ptr<VisibleItem> item = i < visibleItems.size() ? visibleItems[i] : nullptr;
		writeC(item == nullptr ? 0 : item->slotType()); // 0 = not visible, 1 = default (right-hand) slot, 2 = secondary (left-hand) slot
		writeD(item == nullptr ? 0 : item->itemId());
		writeD(item == nullptr ? 0 : item->godStoneId());
		writeDyeInfo(item == nullptr ? std::nullopt : item->color());
	}
	writeD(0);
	writeD(0);
	writeD(0); // 4.5
	writeD(0); // 4.5
	writeD(0); // 4.5
	writeD(0); // 4.5
	writeB(std::vector<uint8_t>(68)); // 4.7
	writeD(accPlData.getDeletionTimeInSeconds());
	writeH(detail::loadDisplaySettings(playerId)); // display helmet 0 show, 5 dont show , possible bit operation
	writeH(0);
	writeD(0); // total mail count
	writeD(detail::haveUnreadMail(playerId) ? 1 : 0); // unread mail count
	writeD(0); // express mail count
	writeD(0); // blackcloud mail count
	writeQ(detail::getEarnedKinahFromSoldItems(*pcd)); // collected money from broker
	writeD(0);
	writeD(0);
	writeD(0);
	writeD(0);
	writeD(0);
	writeD(cbi == nullptr ? 0 : static_cast<int32_t>(cbi->getStart())); // startPunishDate
	writeD(cbi == nullptr ? 0 : static_cast<int32_t>(cbi->getEnd())); // endPunishDate
	writeS(cbi == nullptr ? std::string() : cbi->getReason());
}

void AbstractPlayerInfoPacket::writeEquippedItems(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	int32_t mask = 0;
	for (const runtime::Ptr<model::gameobjects::Item>& item : items) {
		// Java `mask |= item.getEquipmentSlot()` and `mask &= ~...` narrow the long result to int (lossy-conversions)
		mask = static_cast<int32_t>(mask | item->getEquipmentSlot());
		// remove sub hand mask bits (sub hand is present on TwoHandeds by default and would produce display bugs)
		if (model::items::isTwoHandedWeapon(item->getEquipmentSlot()))
			mask = static_cast<int32_t>(mask & ~model::items::getSlotIdMask(model::items::ItemSlot::SUB_HAND));
	}

	writeD(mask);
	for (const runtime::Ptr<model::gameobjects::Item>& item : items) {
		writeD(item->getItemSkinTemplate()->getTemplateId());
		writeD(item->getGodStoneId());
		writeDyeInfo(item->getItemColor());
		writeH(item->getItemEnchantParam());
		writeH(0); // 4.7
	}
}

runtime::Ref<model::account::CharacterBanInfo> AbstractPlayerInfoPacket::getCharBanInfo(model::account::PlayerAccountData& playerAccountData,
	AionConnection* con) {
	runtime::Ref<model::account::CharacterBanInfo> cbi(playerAccountData.getCharBanInfo());
	int64_t nowSeconds = commons::utils::currentTimeMillis() / 1000;
	if (cbi != nullptr && nowSeconds >= cbi->getEnd())
		cbi.reset();
	if (cbi == nullptr
		&& configs::main::SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE.load() == configs::main::SecurityConfig::MultiClientingRestrictionMode::SAME_FACTION) {
		int32_t cdMinutes = configs::main::SecurityConfig::MULTI_CLIENTING_FACTION_SWITCH_COOLDOWN_MINUTES.load();
		if (cdMinutes > 0 && detail::checkForFactionSwitchCooldownTime(playerAccountData.getPlayerCommonData()->getRace(), con).has_value()) {
			int32_t durationSeconds = 61; // client will send CM_CHARACTER_LIST after this duration to update the ban info (<61s corrupts the ban info)
			cbi = model::account::CharacterBanInfo::create(nowSeconds, durationSeconds,
				"\n\n\n\xEE\x80\xA6 " + std::to_string(cdMinutes) + " minute cooldown between switching factions\n\n\n\n\n\n\n");
		}
	}
	return cbi;
}

} // namespace aion::gameserver::network::aion::serverpackets
