#include "aion/gameserver/network/aion/clientpackets/CM_PET_EMOTE.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetEmoteInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET_EMOTE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_PET_EMOTE::CM_PET_EMOTE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_PET_EMOTE.java:36-58
void CM_PET_EMOTE::readImpl() {
	emoteId = readUC();
	emote = model::gameobjects::getEmoteById(emoteId);
	switch (emote) {
		case model::gameobjects::PetEmote::MOVE_STOP:
		case model::gameobjects::PetEmote::MOVE_POSITION_UPDATE:
			x1 = readF();
			y1 = readF();
			z1 = readF();
			h = readC();
			break;
		case model::gameobjects::PetEmote::MOVETO:
			x1 = readF();
			y1 = readF();
			z1 = readF();
			h = readC();
			x2 = readF();
			y2 = readF();
			z2 = readF();
			break;
		default:
			emotionId = readUC();
			unk2 = readUC();
	}
}

// Java CM_PET_EMOTE.java:60-95
void CM_PET_EMOTE::runImpl() {
	using geoEngine::math::JavaFloat;
	static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_PET_EMOTE");
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	runtime::Ptr<model::gameobjects::Pet> pet = player->getPet();

	if (pet == nullptr || !pet->isSpawned()) // client sometimes just doesn't care...
		return;
	if (emote == model::gameobjects::PetEmote::UNKNOWN) {
		log.warn(player->toString() + " / " + pet->toString() + " sent pet emote " + std::to_string(emoteId) + " (emotionId: " +
		         std::to_string(emotionId) + ", unk2: " + std::to_string(unk2) + ")");
		return;
	}

	// sometimes client is crazy enough to send -2.4457384E7 as z coordinate
	// TODO (check retail) either its client bug or packet problem somewhere
	// reproducible by flying randomly and falling from long height with fly resume
	const auto position = [&] {
		return pet->toString() + " of " + player->toString() + " sent " + std::string(xml::enumName(emote)) + " at x:" + JavaFloat::toString(x1) +
		       ", y:" + JavaFloat::toString(y1) + ", z:" + JavaFloat::toString(z1) + ", h:" + std::to_string(h);
	};
	if (x1 < 0 || y1 < 0 || z1 < 0) {
		log.warn(position());
		return;
	}

	switch (emote) {
		case model::gameobjects::PetEmote::MOVE_STOP:
		case model::gameobjects::PetEmote::MOVE_POSITION_UPDATE: {
			if (emote == model::gameobjects::PetEmote::MOVE_POSITION_UPDATE) { // TODO remove once we're sure "MOVE_POSITION_UPDATE" is correct and h is actually h
				log.warn(position());
			}
			world::World::getInstance().updatePosition(*pet, x1, y1, z1, h);
			serverpackets::SM_PET_EMOTE packet(*pet, emote);
			broadcastToSightedPlayers(*pet, packet, false);
			break;
		}
		case model::gameobjects::PetEmote::MOVETO: {
			world::World::getInstance().updatePosition(*pet, x1, y1, z1, h);
			pet->getMoveController().setNewDirection(x2, y2, z2, h);
			serverpackets::SM_PET_EMOTE packet(*pet, emote);
			broadcastToSightedPlayers(*pet, packet, false);
			break;
		}
		default: {
			serverpackets::SM_PET_EMOTE packet(*pet, emote, emotionId, unk2);
			broadcastToSightedPlayers(*pet, packet, emote == model::gameobjects::PetEmote::EMOTION);
		}
	}
}

// Java CM_PET_EMOTE.java:97-100
void CM_PET_EMOTE::broadcastToSightedPlayers(model::gameobjects::Pet& pet, AionServerPacket& packet, bool withMaster) {
	utils::PacketSendUtility::broadcastPacket(pet, packet, false, [&pet, withMaster](model::gameobjects::player::Player& other) {
		return (withMaster || !other.equals(*pet.getMaster())) && other.getKnownList().sees(pet);
	});
}

AION_CLIENT_PACKET(CM_PET_EMOTE);

} // namespace aion::gameserver::network::aion::clientpackets
