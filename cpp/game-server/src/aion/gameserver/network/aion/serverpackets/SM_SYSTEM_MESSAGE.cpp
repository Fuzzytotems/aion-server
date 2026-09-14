#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include <array>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: ChatType.getId() (constructor data of the enum; stands in for the ChatType companion of P4-05) */
int8_t chatTypeId(model::ChatType chatType) {
	static constexpr std::array<int8_t, 29> IDS{
		0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 27, 31, 32, 33, 34, 35, 36};
	return IDS.at(static_cast<size_t>(chatType));
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

SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_SKILL_ABYSS_SKILL_IS_FIRED(model::gameobjects::player::Player& player, std::string_view skill) {
	AION_UNPORTED();
}

SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(model::gameobjects::player::Player& victim, std::string_view zoneName) {
	AION_UNPORTED();
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
	: AionServerPacket(opcodeOf<SM_SYSTEM_MESSAGE>), msgId(msgIdValue), chatType(chatTypeId(chatTypeValue)),
	  senderObjId(sender ? sender->getObjectId() : 0), params(std::move(paramsValue)),
	  specialParams(specialParamsValue.begin(), specialParamsValue.end()) {
}

void SM_SYSTEM_MESSAGE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
