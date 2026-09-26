#include "AsyncAllowed.h"

#include <algorithm>
#include <utility>

#include "decoders/PacketDecoders.h"

namespace aion::gameserver::scenario {

AsyncAllowed AsyncAllowed::m5aDefault() {
	AsyncAllowed allowed;
	allowed.names = {"SM_GAME_TIME", "SM_PONG", "SM_WEATHER"};
	return allowed;
}

AsyncAllowed& AsyncAllowed::selfPlayerState(int32_t objectId) {
	selfObjectId = objectId;
	selfPlayerStateAllowed = true;
	return *this;
}

AsyncAllowed& AsyncAllowed::temporarySpawnUpdates(std::function<bool(int32_t)> isTemporarySpawn) {
	temporarySpawn = std::move(isTemporarySpawn);
	return *this;
}

AsyncAllowed& AsyncAllowed::serverShutdownMessage() {
	shutdownMessageAllowed = true;
	return *this;
}

AsyncAllowed& AsyncAllowed::npcActivity(std::function<bool(int32_t)> isAnnouncedNpc) {
	announcedNpc = std::move(isAnnouncedNpc);
	return *this;
}

bool AsyncAllowed::allows(std::string_view name, std::span<const uint8_t> body) const {
	if (std::ranges::find(names, name) != names.end())
		return true;
	try {
		if (selfPlayerStateAllowed && name == "SM_PLAYER_STATE")
			return decoders::decodePlayerStateObjectId(body) == selfObjectId;
		if (temporarySpawn && name == "SM_NPC_INFO")
			return temporarySpawn(decoders::decodeNpcInfoObjectId(body));
		if (temporarySpawn && name == "SM_DELETE")
			return temporarySpawn(decoders::decodeDeleteObjectId(body));
		if (shutdownMessageAllowed && name == "SM_SYSTEM_MESSAGE")
			return decoders::decodeSystemMessageId(body) == STR_SERVER_SHUTDOWN;
		if (announcedNpc && name == "SM_MOVE")
			return announcedNpc(decoders::decodeMoveObjectId(body));
		if (announcedNpc && name == "SM_EMOTION") {
			// the header is decoded first, so an SM_EMOTION of a type this set does not allow still has to decode: a malformed body is a gate
			// failure either way, and for the four EmoteManager types decodeEmotionHeader consumes the whole body
			const decoders::EmotionHeader emotion = decoders::decodeEmotionHeader(body);
			return decoders::isNpcEmote(emotion.emotionType) && announcedNpc(emotion.objectId);
		}
		if (announcedNpc && name == "SM_LOOKATOBJECT") {
			const decoders::LookAtObject look = decoders::decodeLookAtObject(body);
			return announcedNpc(look.objectId) && (look.targetObjectId == 0 || announcedNpc(look.targetObjectId));
		}
		if (announcedNpc && name == "SM_ATTACK") {
			const decoders::AttackParties parties = decoders::decodeAttackParties(body);
			return announcedNpc(parties.attackerObjectId) && announcedNpc(parties.targetObjectId);
		}
		if (announcedNpc && name == "SM_ATTACK_STATUS")
			return announcedNpc(decoders::decodeAttackStatusObjectId(body));
	} catch (const decoders::DecodeError&) {
		// a body that does not decode is a gate failure, not an async packet: let the sequence match it (and fail) at its position
		return false;
	}
	return false;
}

} // namespace aion::gameserver::scenario
