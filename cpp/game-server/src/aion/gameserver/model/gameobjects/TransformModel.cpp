#include "aion/gameserver/model/gameobjects/TransformModel.h"

#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/CreatureTypeInfo.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/skillengine/model/TransformType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::gameobjects {

TransformModel::TransformModel(Creature& creature)
	: OwnedPart(creature), owner(creature),
	  originalType(
		  dynamic_cast<player::Player*>(&creature) != nullptr ? skillengine::model::TransformType::PC : skillengine::model::TransformType::NONE),
	  transformType(skillengine::model::TransformType::NONE) {
}

TransformModel::~TransformModel() = default;

void TransformModel::apply(int32_t modelIdValue) {
	apply(modelIdValue, originalType, 0, false, false, false, false, false, false, false);
}

void TransformModel::apply(int32_t modelIdValue, skillengine::model::TransformType type, int32_t panelIdValue, bool cantUseSkills, bool cantMove,
	bool cantRecall, bool cantJump, bool cantAttack, bool cantUseItems, bool cantFly) {
	int32_t originalModelId = owner.getObjectTemplate()->getTemplateId();
	if (modelIdValue == 0 || modelIdValue == originalModelId) { // reset
		modelId.set(originalModelId);
		transformType.set(originalType);
		panelId.set(0);
		cantUseSkills_.set(false);
		cantMove_.set(false);
		cantRecall_.set(false);
		cantJump_.set(false);
		cantAttack_.set(false);
		cantUseItems_.set(false);
		cantFly_.set(false);
	} else { // set new
		modelId.set(modelIdValue);
		transformType.set(type);
		panelId.set(panelIdValue);
		cantUseSkills_.set(cantUseSkills);
		cantMove_.set(cantMove);
		cantRecall_.set(cantRecall);
		cantJump_.set(cantJump);
		cantAttack_.set(cantAttack);
		cantUseItems_.set(cantUseItems);
		cantFly_.set(cantFly);
	}

	updateVisually();
}

void TransformModel::updateVisually() {
	utils::PacketSendUtility::broadcastPacketAndReceive(owner, network::aion::serverpackets::SM_TRANSFORM(owner));
}

void TransformModel::updateTribeVisually() {
	if (Npc* npc = dynamic_cast<Npc*>(&owner)) {
		npc->getKnownList().forEachPlayer([npc](player::Player& player) {
			utils::PacketSendUtility::sendPacket(player,
				network::aion::serverpackets::SM_CUSTOM_SETTINGS(npc->getObjectId(), 0, getId(npc->getType(player)), 0));
		});
	} else if (player::Player* ownerPlayer = dynamic_cast<player::Player*>(&owner)) {
		ownerPlayer->getKnownList().forEachNpc([ownerPlayer](Npc& npc) {
			utils::PacketSendUtility::sendPacket(*ownerPlayer,
				network::aion::serverpackets::SM_CUSTOM_SETTINGS(npc.getObjectId(), 0, getId(npc.getType(*ownerPlayer)), 0));
		});
	}
}

int32_t TransformModel::getModelId() {
	int32_t templateId = owner.getObjectTemplate()->getTemplateId();
	if (eventModelId.get() == templateId && transformType.get() == skillengine::model::TransformType::PC && isUnrestricted()) { // Player removed visual appearance via Nomorph command
		return eventModelId.get();
	}
	if (isActive())
		return modelId.get();
	if (eventModelId.get() > 0)
		return eventModelId.get();
	else
		return owner.getObjectTemplate()->getTemplateId();
}

bool TransformModel::isUnrestricted() {
	return !cantUseSkills_.get() && !cantMove_.get() && !cantRecall_.get() && !cantJump_.get() && !cantAttack_.get() && !cantUseItems_.get() &&
		!cantFly_.get();
}

bool TransformModel::isActive() {
	int32_t current = modelId.get();
	return current > 0 && current != owner.getObjectTemplate()->getTemplateId();
}

void TransformModel::setTribe(std::optional<TribeClass> value) {
	bool tribeChanged = transformTribe.get() != value;
	transformTribe.set(value);
	if (player::Player* player = dynamic_cast<player::Player*>(&owner); tribeChanged && player != nullptr)
		services::RecallService::getInstance().cancel(*player, services::RecallService::CancelReason::CANCELLED);
	updateTribeVisually();
}

} // namespace aion::gameserver::model::gameobjects
