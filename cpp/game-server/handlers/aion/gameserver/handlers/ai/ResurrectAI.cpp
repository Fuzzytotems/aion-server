#include "aion/gameserver/handlers/ai/ResurrectAI.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dao/PlayerBindPointDAO.h"
#include "aion/gameserver/dataholders/BindPointData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/BindPointTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ResurrectAI, "resurrect");

const commons::logging::Logger ResurrectAI::log = commons::logging::LoggerFactory::getLogger("ai.ResurrectAI");

/**
 * Java: the anonymous AIRequest of bindHere (ResurrectAI.java:76-106, fieldmap ai.ResurrectAI$1, K3), stored through AIActions.addRequest in
 * the player's ResponseRequester and ObserveController until he answers STR_ASK_REGISTER_RESURRECT_POINT or walks away. It captures the
 * bind point template (static data); the requester is the obelisk AIActions hands over.
 */
struct ResurrectAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	const BindPointTemplate* const bindPointTemplate; // captured param BindPointTemplate bindPointTemplate (line 83)

	static runtime::Ref<ResurrectAI_AIRequest> create(const BindPointTemplate* bindPointTemplate) {
		return runtime::makeRef<ResurrectAI_AIRequest>(bindPointTemplate);
	}

	// Java ResurrectAI.java:78-105
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requestId);
		if (!requester)
			throw runtime::NullPointerException("requester"); // Java: requester.getWorldId()
		// check if this both creatures are in same world
		if (responder.getWorldId() == requester->getWorldId()) {
			// check enough kinah
			if (responder.getInventory().getKinah() < bindPointTemplate->getPrice()) {
				PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_RESURRECT_POINT_NOT_ENOUGH_FEE());
				return;
			} else if (PositionUtil::getDistance(*requester, responder) > 5) {
				PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_RESURRECT_POINT_FAR_FROM_NPC());
				return;
			}

			runtime::Ptr<BindPointPosition> old = responder.getBindPoint();
			runtime::Ref<BindPointPosition> bpp =
				BindPointPosition::create(requester->getWorldId(), responder.getX(), responder.getY(), responder.getZ(), responder.getHeading());
			bpp->setPersistentState(!old ? model::gameobjects::Persistable::PersistentState::NEW
										 : model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
			responder.setBindPoint(bpp);
			if (PlayerBindPointDAO::store(responder)) {
				responder.getInventory().decreaseKinah(bindPointTemplate->getPrice());
				TeleportService::sendObeliskBindPoint(responder);
				PacketSendUtility::broadcastPacket(responder, SM_ACTION_ANIMATION(responder.getObjectId(), ActionAnimation::BIND_KISK), true);
				PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_DEATH_REGISTER_RESURRECT_POINT());
			} else
				// if any errors happen, left that player with old bind point
				responder.setBindPoint(old);
		}
	}

protected:
	explicit ResurrectAI_AIRequest(const BindPointTemplate* bindPointTemplateValue) : bindPointTemplate(bindPointTemplateValue) {}
	~ResurrectAI_AIRequest() override = default;
};

// Java ResurrectAI.java:43-73
void ResurrectAI::handleDialogStart(Player& player) {
	const BindPointTemplate* bindPointTemplate = DataManager::BIND_POINT_DATA->getBindPointTemplate(getNpcId());
	Race race = player.getRace();
	if (bindPointTemplate == nullptr) {
		log.info("There is no bind point template for npc: " + std::to_string(getNpcId()));
		return;
	}

	if (player.getBindPoint() && player.getBindPoint()->getMapId() == getPosition()->getMapId()
		&& PositionUtil::getDistance(player.getBindPoint()->getX(), player.getBindPoint()->getY(), player.getBindPoint()->getZ(), getPosition()->getX(),
			   getPosition()->getY(), getPosition()->getZ())
			   < 20) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ALREADY_REGISTER_THIS_RESURRECT_POINT());
		return;
	}

	WorldType worldType = player.getWorldType();
	if (!CustomConfig::ENABLE_CROSS_FACTION_BINDING.load()) {
		// Java `!getTribe().equals(TribeClass.FIELD_OBJECT_ALL)`: an npc without a tribe is a NullPointerException
		std::optional<TribeClass> tribe = getTribe();
		if (!tribe)
			throw runtime::NullPointerException("getTribe() of npc " + std::to_string(getNpcId()));
		if (*tribe != TribeClass::FIELD_OBJECT_ALL) {
			if ((getRace() != Race::NONE && getRace() != race) || (race == Race::ASMODIANS && *tribe == TribeClass::FIELD_OBJECT_LIGHT)
				|| (race == Race::ELYOS && *tribe == TribeClass::FIELD_OBJECT_DARK)) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_BINDSTONE_CANNOT_FOR_INVALID_RIGHT(xml::enumName(player.getOppositeRace())));
				return;
			}
		}
	}
	if (worldType == WorldType::PRISON) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_RESURRECT_POINT_FAR_FROM_NPC());
		return;
	}
	bindHere(player, bindPointTemplate);
}

// Java ResurrectAI.java:75-107
void ResurrectAI::bindHere(Player& player, const BindPointTemplate* bindPointTemplate) {
	runtime::Ref<ResurrectAI_AIRequest> request = ResurrectAI_AIRequest::create(bindPointTemplate);
	// Java passes the Integer price as the request's one parameter. The variadic addRequest of AIActions.h cannot be instantiated (it calls
	// SM_QUESTION_WINDOW::toParam, which is private), so the parameter is formatted here with the packet's public String.valueOf(int) and passed
	// through the std::vector<std::string> overload - the same string.
	AIActions::addRequest(*this, player, SM_QUESTION_WINDOW::STR_ASK_REGISTER_RESURRECT_POINT, *request,
		std::vector<std::string>{SM_QUESTION_WINDOW::toJavaString(bindPointTemplate->getPrice())});
}

} // namespace aion::gameserver::handlers::ai
