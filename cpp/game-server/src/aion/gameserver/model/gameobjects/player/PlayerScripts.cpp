#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"

#include <exception>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dao/HouseScriptsDAO.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/PlayerScript.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_SCRIPTS.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"
#include "aion/gameserver/utils/xml/CompressUtil.h"

namespace aion::gameserver::model::gameobjects::player {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.PlayerScripts");

namespace {

/** Java new String(bytes, StandardCharsets.UTF_16LE): a trailing odd byte and unpaired surrogates become U+FFFD */
std::string decodeUtf16Le(const std::vector<uint8_t>& bytes) {
	std::u16string utf16;
	utf16.reserve(bytes.size() / 2 + 1);
	for (size_t i = 0; i + 1 < bytes.size(); i += 2)
		utf16.push_back(static_cast<char16_t>(bytes[i] | (bytes[i + 1] << 8)));
	if (bytes.size() % 2 != 0)
		utf16.push_back(static_cast<char16_t>(0xFFFD));
	return commons::utils::StringUtils::toUtf8(utf16);
}

} // namespace

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
	return set(id, compressedXML, uncompressedSize, true);
}

bool PlayerScripts::set(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize, bool storeInDb) {
	if (isInvalidScriptId(id))
		return false;
	std::optional<std::string> scriptXML = decompressAndValidate(compressedXML, uncompressedSize);
	if (!scriptXML)
		return false;
	if (storeInDb) {
		runtime::Ptr<house::House> house = services::HousingService::getInstance().findHouseOrStudio(houseObjId);
		if (house->getPersistentState() == Persistable::PersistentState::NEW)
			house->save(); // new houses must be inserted first, due to foreign key constraints
		dao::HouseScriptsDAO::storeScript(houseObjId, id, *scriptXML);
	}
	(*scripts)[id] = house::PlayerScript::create(id, compressedXML, uncompressedSize);
	return true;
}

void PlayerScripts::remove(int32_t scriptId) {
	if (isInvalidScriptId(scriptId))
		return;
	dao::HouseScriptsDAO::deleteScript(houseObjId, scriptId);
	(*scripts)[scriptId] = house::PlayerScript::create(scriptId, nullptr, 0);
}

void PlayerScripts::removeAll() {
	dao::HouseScriptsDAO::deleteScriptsForHouse(houseObjId);
	fillEmptyScriptsArray();
}

runtime::Ptr<house::PlayerScript> PlayerScripts::get(int32_t scriptId) {
	if (isInvalidScriptId(scriptId))
		return nullptr;
	return (*scripts)[scriptId].get();
}

bool PlayerScripts::isInvalidScriptId(int32_t scriptId) {
	return scriptId < 0 || scriptId >= scripts->length();
}

std::optional<std::string> PlayerScripts::decompressAndValidate(runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize) {
	if (!compressedXML || compressedXML->length() == 0)
		return std::string();
	std::vector<uint8_t> compressed(static_cast<size_t>(compressedXML->length()));
	for (int32_t i = 0; i < compressedXML->length(); i++)
		compressed[static_cast<size_t>(i)] = static_cast<uint8_t>((*compressedXML)[i]);
	std::vector<uint8_t> bytes;
	try {
		bytes = utils::xml::CompressUtil::decompress(compressed);
	} catch (const std::exception& ex) {
		log.error("Housing script data for house {} could not be decompressed", houseObjId, ex);
		return std::nullopt;
	}
	std::string scriptXML = decodeUtf16Le(bytes);
	if (bytes.size() != static_cast<size_t>(uncompressedSize)) {
		log.warn("Unexpected housing script size after decompression for house {}: Expected {} bytes, got {} bytes:\n{}", houseObjId, uncompressedSize,
			bytes.size(), scriptXML);
		return std::nullopt;
	}
	return scriptXML;
}

void PlayerScripts::sendToPlayer(runtime::Ptr<Player> player, int32_t houseAddress) {
	if (!player)
		return;
	std::vector<runtime::Ref<house::PlayerScript>> scriptList;
	scriptList.reserve(static_cast<size_t>(scripts->length()));
	for (int32_t i = 0; i < scripts->length(); i++)
		scriptList.emplace_back((*scripts)[i].get());
	utils::collections::DynamicServerPacketBodySplitList<house::PlayerScript> scriptSplitList(std::move(scriptList), false,
		network::aion::serverpackets::SM_HOUSE_SCRIPTS::STATIC_BODY_SIZE, network::aion::serverpackets::SM_HOUSE_SCRIPTS::DYNAMIC_BODY_PART_SIZE_CALCULATOR);
	for (utils::collections::ListPart<house::PlayerScript>& part : scriptSplitList)
		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_HOUSE_SCRIPTS(houseAddress, part.borrowed()));
}

} // namespace aion::gameserver::model::gameobjects::player
