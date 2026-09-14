#include "aion/gameserver/network/aion/serverpackets/SM_MAIL_SERVICE.h"

#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

// callback key com.aionemu.gameserver.network.aion.serverpackets.SM_MAIL_SERVICE@L23:84 (captureless)
const runtime::PinnedCallback<int32_t(model::gameobjects::Letter&)> SM_MAIL_SERVICE::DYNAMIC_BODY_PART_SIZE_CALCULATOR(
	[](model::gameobjects::Letter& letter) -> int32_t {
		AION_UNPORTED(); // 22 + byteLengthForString(letter.getSenderName()) + byteLengthForString(letter.getTitle())
	});

SM_MAIL_SERVICE::SM_MAIL_SERVICE()
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), serviceId(0) {
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(model::templates::mail::MailMessage mailMessageValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), serviceId(1) {
	AION_UNPORTED();
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(model::gameobjects::player::Player& playerValue,
	const std::vector<runtime::Ptr<model::gameobjects::Letter>>& lettersValue, bool isLastPacketValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), player(playerValue), serviceId(2), letters(lettersValue.begin(), lettersValue.end()),
	  isLastPacket(isLastPacketValue) {
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(model::gameobjects::player::Player& playerValue, model::gameobjects::Letter& letterValue, int64_t timeValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), player(playerValue), serviceId(3), letter(letterValue), time(timeValue) {
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(int32_t letterIdValue, int8_t attachmentTypeValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), serviceId(5), letterId(letterIdValue), attachmentType(attachmentTypeValue) {
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(std::span<const int32_t> letterIdsValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), serviceId(6), letterIds(letterIdsValue.begin(), letterIdsValue.end()) {
}

SM_MAIL_SERVICE::~SM_MAIL_SERVICE() = default;

void SM_MAIL_SERVICE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_MAIL_SERVICE::writeLettersList(const std::vector<runtime::Ptr<model::gameobjects::Letter>>& value) {
	AION_UNPORTED();
}

void SM_MAIL_SERVICE::writeMailMessage(int32_t messageId) {
	AION_UNPORTED();
}

void SM_MAIL_SERVICE::writeMailboxState(int32_t totalCount, int32_t unreadCount, int32_t expressCount, int32_t blackCloudCount) {
	AION_UNPORTED();
}

void SM_MAIL_SERVICE::writeLetterRead(model::gameobjects::Letter& value, int64_t timeValue, int32_t totalCount, int32_t unreadCount,
	int32_t expressCount, int32_t blackCloudCount) {
	AION_UNPORTED();
}

void SM_MAIL_SERVICE::writeLetterState(int32_t value, int8_t attachmentTypeValue) {
	AION_UNPORTED();
}

void SM_MAIL_SERVICE::writeLetterDelete(int32_t totalCount, int32_t unreadCount, int32_t expressCount, int32_t blackCloudCount,
	std::initializer_list<int32_t> value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
