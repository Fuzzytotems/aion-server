#include "aion/gameserver/model/gameobjects/Kisk.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::model::gameobjects {

Kisk::Kisk(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
	player::Player& owner)
	: SummonedObject(key, std::move(controller), spawnTemplate, 0, nullptr), legionId(0), ownerRace(), kiskStatsTemplate(nullptr),
	  remainingResurrections(0) {
	// Java: super(controller, spawnTemplate, DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId()).getLevel(), null); the kisk stats
	// template (or a default one), the owner's legion id, race, id and name, setKnownlist(new PlayerAwareKnownList(this)),
	// setEffectController(new EffectController(this))
	static_cast<void>(owner);
	AION_UNPORTED();
}

Kisk::~Kisk() = default;

bool Kisk::isEnemy(Creature& creature) {
	AION_UNPORTED();
}

bool Kisk::isEnemyFrom(player::Player& player) {
	AION_UNPORTED();
}

CreatureType Kisk::getType(Creature& creature) {
	AION_UNPORTED();
}

NpcObjectType Kisk::getNpcObjectType() {
	AION_UNPORTED();
}

int32_t Kisk::getUseMask() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<player::Player>> Kisk::getCurrentMemberList() {
	AION_UNPORTED();
}

int32_t Kisk::getCurrentMemberCount() {
	AION_UNPORTED();
}

int32_t Kisk::getMaxMembers() {
	AION_UNPORTED();
}

int32_t Kisk::getMaxRessurects() {
	AION_UNPORTED();
}

int32_t Kisk::getRemainingLifetime() {
	AION_UNPORTED();
}

bool Kisk::canBind(player::Player& player) {
	AION_UNPORTED();
}

bool Kisk::isUseAllowed(player::Player& player) {
	AION_UNPORTED();
}

void Kisk::addPlayer(player::Player& player) {
	AION_UNPORTED();
}

void Kisk::removePlayer(player::Player& player) {
	AION_UNPORTED();
}

void Kisk::broadcastKiskUpdate() {
	AION_UNPORTED();
}

void Kisk::broadcastPacket(network::aion::serverpackets::SM_SYSTEM_MESSAGE& message) {
	AION_UNPORTED();
}

void Kisk::resurrectionUsed() {
	AION_UNPORTED();
}

bool Kisk::isActive() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects
