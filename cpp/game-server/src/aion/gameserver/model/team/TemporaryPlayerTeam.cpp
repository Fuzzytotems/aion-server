#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"

#include <unordered_map>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_BRAND.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/collections/Predicates.h"

namespace aion::gameserver::model::team {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_SHOW_BRAND;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;
using utils::collections::Predicates;

TemporaryPlayerTeam::TemporaryPlayerTeam(int32_t objId, bool autoReleaseObjectId)
	: GeneralTeam(objId, autoReleaseObjectId), lootGroupRules(common::legacy::LootGroupRules::create()) {
}

TemporaryPlayerTeam::~TemporaryPlayerTeam() = default;

void TemporaryPlayerTeam::updateBrand(int32_t brandId, int32_t targetObjectId) {
	targetIdsByBrandId.put(brandId, targetObjectId);
	SM_SHOW_BRAND packet(brandId, targetObjectId);
	sendPackets({packet});
}

void TemporaryPlayerTeam::sendBrands(Player& member) {
	// Java passes the live map; SM_SHOW_BRAND copies it into its own member (the packet's frozen constructor takes a std::unordered_map)
	std::unordered_map<int32_t, int32_t> brands;
	for (const auto& entry : targetIdsByBrandId.snapshot())
		brands.emplace(entry.key, entry.value);
	PacketSendUtility::sendPacket(member, SM_SHOW_BRAND(brands));
}

Race TemporaryPlayerTeam::getRace() {
	return runtime::cast<Player>(getLeader()->getObject())->getRace();
}

void TemporaryPlayerTeam::sendPackets(std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) {
	// Java: sendPacket(Predicates.alwaysTrue(), packets)
	sendPacket([](gameobjects::AionObject&) { return true; }, packets);
}

void TemporaryPlayerTeam::sendPacket(const std::function<bool(gameobjects::AionObject&)>& predicate,
	std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) {
	forEach([&predicate, &packets](gameobjects::AionObject& object) {
		Player& player = *runtime::cast<Player>(object);
		if (predicate(player)) {
			for (network::aion::AionServerPacket& packet : packets)
				PacketSendUtility::sendPacket(player, packet);
		}
	});
}

std::vector<runtime::Ptr<Player>> TemporaryPlayerTeam::getOnlineMembers() {
	// Java: filterMembers(Predicates.Players.ONLINE)
	std::vector<runtime::Ptr<Player>> onlineMembers;
	for (const runtime::Ptr<gameobjects::AionObject>& member :
		filterMembers([](gameobjects::AionObject& object) { return Predicates::Players::ONLINE(*runtime::cast<Player>(object)); }))
		onlineMembers.push_back(runtime::cast<Player>(member));
	return onlineMembers;
}

void TemporaryPlayerTeam::setLootGroupRules(runtime::Ptr<common::legacy::LootGroupRules> lootGroupRulesValue) {
	this->lootGroupRules.set(lootGroupRulesValue);
	if (lootGroupRulesValue && lootGroupRulesValue->getLootRule() == common::legacy::LootRuleType::FREEFORALL) {
		SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_MSG_LOOTING_PET_MESSAGE03();
		sendPacket([](gameobjects::AionObject& object) { return Predicates::Players::WITH_LOOT_PET(*runtime::cast<Player>(object)); }, {message});
	}
}

runtime::Ptr<Player> TemporaryPlayerTeam::getLeaderObject() {
	return runtime::cast<Player>(GeneralTeam::getLeaderObject());
}

} // namespace aion::gameserver::model::team
