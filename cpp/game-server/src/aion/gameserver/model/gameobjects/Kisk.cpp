#include "aion/gameserver/model/gameobjects/Kisk.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/stats/KiskStatsTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_KISK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::model::gameobjects {

/** Java: LoggerFactory.getLogger(Kisk.class) inside isUseAllowed */
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.Kisk");

namespace {

/** Java `DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId()).getLevel()` in the super(...) call (NullPointerException without a template) */
int8_t levelOf(templates::spawns::SpawnTemplate& spawnTemplate) {
	const templates::npc::NpcTemplate* npcTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(spawnTemplate.getNpcId());
	if (npcTemplate == nullptr) // Java: NullPointerException on getLevel()
		throw runtime::NullPointerException("No npc template for kisk " + std::to_string(spawnTemplate.getNpcId()));
	return npcTemplate->getLevel();
}

/**
 * Java `new KiskStatsTemplate()` for NPC templates without kisk stats. The template is immutable (three attributes with Java's defaults), so one
 * immortal default instance behaves like a new object per Kisk and keeps `const KiskStatsTemplate*` a static data pointer (the S0c open issue
 * "Kisk template lifetime").
 */
const templates::stats::KiskStatsTemplate* defaultKiskStatsTemplate() {
	static const templates::stats::KiskStatsTemplate* const instance = new templates::stats::KiskStatsTemplate();
	return instance;
}

} // namespace

Kisk::Kisk(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
	player::Player& owner)
	: SummonedObject(key, std::move(controller), spawnTemplate, levelOf(spawnTemplate), nullptr),
	  legionId(owner.getLegion() ? owner.getLegion()->getLegionId() : 0), ownerRace(owner.getRace()),
	  kiskStatsTemplate(getObjectTemplate()->getKiskStatsTemplate() == nullptr ? defaultKiskStatsTemplate()
																				 : getObjectTemplate()->getKiskStatsTemplate()),
	  remainingResurrections(kiskStatsTemplate->getMaxResurrects()) {
	// Java order: kiskStatsTemplate, kiskMemberIds, remainingResurrections, legionId, ownerRace (the initializers have no side effects)
	setCreatorId(owner.getObjectId());
	setMasterName(owner.getName());
	setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*this));
	setEffectController(std::make_unique<controllers::effect::EffectController>(*this));
}

Kisk::~Kisk() = default;

bool Kisk::isEnemy(Creature& creature) {
	return creature.isEnemyFrom(*this);
}

bool Kisk::isEnemyFrom(player::Player& player) {
	return player.getRace() != ownerRace && isInsidePvPZone() && player.isInsidePvPZone();
}

CreatureType Kisk::getType(Creature& creature) {
	if (player::Player* player = dynamic_cast<player::Player*>(&creature))
		return isEnemyFrom(*player) ? CreatureType::ATTACKABLE : CreatureType::SUPPORT;
	return SummonedObject::getType(creature);
}

NpcObjectType Kisk::getNpcObjectType() {
	return NpcObjectType::NORMAL;
}

int32_t Kisk::getUseMask() {
	return kiskStatsTemplate->getUseMask();
}

std::vector<runtime::Ptr<player::Player>> Kisk::getCurrentMemberList() {
	std::vector<runtime::Ptr<player::Player>> currentMemberList;

	for (int32_t memberId : kiskMemberIds) {
		runtime::Ptr<player::Player> member = world::World::getInstance().getPlayer(memberId);
		if (member)
			currentMemberList.push_back(member);
	}

	return currentMemberList;
}

int32_t Kisk::getCurrentMemberCount() {
	return kiskMemberIds.size();
}

int32_t Kisk::getMaxMembers() {
	return kiskStatsTemplate->getMaxMembers();
}

int32_t Kisk::getMaxRessurects() {
	return kiskStatsTemplate->getMaxResurrects();
}

int32_t Kisk::getRemainingLifetime() {
	if (isDead())
		return 0;
	int64_t timeElapsed = getMillisSinceSpawn() / 1000;
	int32_t timeRemaining = static_cast<int32_t>(KISK_LIFETIME_IN_SEC - timeElapsed);
	return std::max(timeRemaining, 0);
}

bool Kisk::canBind(player::Player& player) {
	return getCurrentMemberCount() < getMaxMembers() && isUseAllowed(player);
}

bool Kisk::isUseAllowed(player::Player& player) {
	switch (getUseMask()) {
		case 0: // Test item (no restrictions)
			return true;
		case 1: // Race
			if (ownerRace == player.getRace())
				return true;
			break;
		case 2: // Legion
			if (player.getObjectId() == getCreatorId() ||
				legionId != 0 && services::LegionService::getInstance().getLegion(legionId)->isMember(player.getObjectId()))
				return true;
			break;
		case 3: // Solo
			return player.getObjectId() == getCreatorId();
		case 4: // Group (PlayerGroup or PlayerAllianceGroup)
			if (player.getObjectId() == getCreatorId() || player.isInTeam() && player.getCurrentGroup()->hasMember(getCreatorId()))
				return true;
			break;
		case 5: // Alliance (PlayerGroup or PlayerAlliance)
			if (player.getObjectId() == getCreatorId() || player.isInTeam() && player.getCurrentTeam()->hasMember(getCreatorId()))
				return true;
			break;
		default:
			log.warn("Unhandled UseMask " + std::to_string(getUseMask()) + " for Kisk " + std::to_string(getNpcId()));
	}

	return false;
}

void Kisk::addPlayer(player::Player& player) {
	if (kiskMemberIds.add(player.getObjectId())) {
		broadcastKiskUpdate();
	} else {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_KISK_UPDATE(*this));
	}
	player.setKisk(*this);
}

void Kisk::removePlayer(player::Player& player) {
	player.setKisk(nullptr);
	if (kiskMemberIds.remove(player.getObjectId()))
		broadcastKiskUpdate();
}

void Kisk::broadcastKiskUpdate() {
	// on all members, but not the ones in knownlist, they will receive the update in the next step
	for (runtime::Ptr<player::Player> member : getCurrentMemberList()) {
		if (!getKnownList().knows(*member))
			utils::PacketSendUtility::sendPacket(*member, network::aion::serverpackets::SM_KISK_UPDATE(*this));
	}

	// all players having the same race in knownlist
	utils::PacketSendUtility::broadcastPacket(*this, network::aion::serverpackets::SM_KISK_UPDATE(*this),
		[this](player::Player& player) { return player.getRace() == ownerRace; });
}

void Kisk::broadcastPacket(network::aion::serverpackets::SM_SYSTEM_MESSAGE& message) {
	for (runtime::Ptr<player::Player> member : getCurrentMemberList()) {
		utils::PacketSendUtility::sendPacket(*member, message);
	}
}

void Kisk::resurrectionUsed() {
	remainingResurrections--;
	broadcastKiskUpdate();
	if (remainingResurrections.get() <= 0)
		getController().delete_();
}

bool Kisk::isActive() {
	return !isDead() && getRemainingResurrects() > 0;
}

} // namespace aion::gameserver::model::gameobjects
