#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/detail/SystemMessageL10n.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: Race.getL10n() (L10n default method: ChatUtil.l10n(getL10nId())) */
std::string raceL10n(model::Race race) {
	return network::detail::l10n(network::detail::raceL10nIdOf(race));
}

/** Java: AbyssRankEnum.getRankL10n(player) = player.getAbyssRank().getRank().getRankL10n(player.getRace()) */
std::string rankL10n(model::gameobjects::player::Player& player) {
	return network::detail::l10n(network::detail::rankL10nIdOf(player.getRace(), player.getAbyssRank()->getRank()));
}

/** Java: "%SubZone:" + position.getMapId() + " " + position.getX() + " " + position.getY() + " " + position.getZ() */
std::string subZoneOf(model::gameobjects::player::Player& player) {
	runtime::Ptr<world::WorldPosition> position = player.getPosition();
	return network::detail::subZoneOf(position->getMapId(), position->getX(), position->getY(), position->getZ());
}

} // namespace

std::string SM_SYSTEM_MESSAGE::toJavaString(int32_t value) {
	return std::to_string(value);
}

std::string SM_SYSTEM_MESSAGE::toJavaString(int64_t value) {
	return std::to_string(value);
}

std::string SM_SYSTEM_MESSAGE::toJavaString(int8_t value) {
	return std::to_string(static_cast<int32_t>(value));
}

std::string SM_SYSTEM_MESSAGE::toJavaString(float value) {
	return geoEngine::math::JavaFloat::toString(value);
}

std::string SM_SYSTEM_MESSAGE::toJavaString(bool value) {
	return value ? "true" : "false";
}

std::string SM_SYSTEM_MESSAGE::toJavaString(int16_t value) {
	return std::to_string(static_cast<int32_t>(value));
}

std::string SM_SYSTEM_MESSAGE::toJavaString(char16_t value) {
	return commons::utils::StringUtils::toUtf8(std::u16string_view(&value, 1));
}

SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_SKILL_ABYSS_SKILL_IS_FIRED(model::gameobjects::player::Player& player, std::string_view skill) {
	return SM_SYSTEM_MESSAGE(1390155, std::vector<std::string>{raceL10n(player.getRace()), player.getName(), subZoneOf(player), std::string(skill)});
}

SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(model::gameobjects::player::Player& victim) {
	return SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(victim, subZoneOf(victim));
}

SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(model::gameobjects::player::Player& victim, std::string_view zoneName) {
	return SM_SYSTEM_MESSAGE(1400023, std::vector<std::string>{raceL10n(victim.getRace()), rankL10n(victim), victim.getName(), std::string(zoneName)});
}

SM_SYSTEM_MESSAGE::SM_SYSTEM_MESSAGE(int32_t msgIdValue, std::vector<std::string> paramsValue)
	: SM_SYSTEM_MESSAGE(model::ChatType::GOLDEN_YELLOW, nullptr, msgIdValue, std::move(paramsValue), {}) {
}

SM_SYSTEM_MESSAGE::SM_SYSTEM_MESSAGE(model::ChatType chatTypeValue, runtime::Ptr<model::gameobjects::VisibleObject> sender, int32_t msgIdValue,
	std::vector<std::string> paramsValue)
	: SM_SYSTEM_MESSAGE(chatTypeValue, sender, msgIdValue, std::move(paramsValue), {}) {
}

SM_SYSTEM_MESSAGE::SM_SYSTEM_MESSAGE(model::ChatType chatTypeValue, runtime::Ptr<model::gameobjects::VisibleObject> sender, int32_t msgIdValue,
	std::vector<std::string> paramsValue, std::initializer_list<std::string_view> specialParamsValue)
	: AionServerPacket(opcodeOf<SM_SYSTEM_MESSAGE>), msgId(msgIdValue), chatType(network::detail::chatTypeIdOf(chatTypeValue)),
	  senderObjId(sender ? sender->getObjectId() : 0), params(std::move(paramsValue)),
	  specialParams(specialParamsValue.begin(), specialParamsValue.end()) {
}

void SM_SYSTEM_MESSAGE::writeImpl(AionConnection* con) {
	writeC(chatType);
	writeC(0x00); // to do for shoots text encoding (unk dialect)
	writeD(senderObjId);
	writeD(msgId);
	writeC(static_cast<int32_t>(params.size()));
	for (const std::string& param : params)
		writeS(param); // Java: param == null ? null : param.toString() (formatted at construction, null as "")

	writeC(static_cast<int32_t>(specialParams.size()));
	for (const std::string& param : specialParams) {
		writeS(param);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
