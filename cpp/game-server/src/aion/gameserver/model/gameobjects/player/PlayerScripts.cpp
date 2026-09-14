#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/house/PlayerScript.h"

namespace aion::gameserver::model::gameobjects::player {

[[maybe_unused]] static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.PlayerScripts");

PlayerScripts::PlayerScripts(int32_t houseId)
	: houseObjId(houseId), scripts(runtime::Array<runtime::Ref<house::PlayerScript>>::make(SCRIPT_LIMIT)) {
	fillEmptyScriptsArray();
}

PlayerScripts::~PlayerScripts() = default;

runtime::Ref<PlayerScripts> PlayerScripts::create(int32_t houseId) {
	return runtime::makeRef<PlayerScripts>(houseId);
}

void PlayerScripts::fillEmptyScriptsArray() {
	for (int32_t i = 0; i < SCRIPT_LIMIT; i++)
		(*scripts)[i] = house::PlayerScript::create(i, nullptr, 0);
}

bool PlayerScripts::set(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize) {
	AION_UNPORTED();
}

bool PlayerScripts::set(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize, bool storeInDb) {
	AION_UNPORTED();
}

void PlayerScripts::remove(int32_t scriptId) {
	AION_UNPORTED();
}

void PlayerScripts::removeAll() {
	AION_UNPORTED();
}

runtime::Ptr<house::PlayerScript> PlayerScripts::get(int32_t scriptId) {
	AION_UNPORTED();
}

bool PlayerScripts::isInvalidScriptId(int32_t scriptId) {
	AION_UNPORTED();
}

std::optional<std::string> PlayerScripts::decompressAndValidate(runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize) {
	AION_UNPORTED();
}

void PlayerScripts::sendToPlayer(runtime::Ptr<Player> player, int32_t houseAddress) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
