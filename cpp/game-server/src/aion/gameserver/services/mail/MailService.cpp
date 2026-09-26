#include "aion/gameserver/services/mail/MailService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MAIL_SERVICE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::mail {

static const auto log = commons::logging::LoggerFactory::getLogger("MAIL_LOG");

void MailService::sendMail(model::gameobjects::player::Player& sender, std::string_view recipientName, std::string_view title,
	std::string_view message, int32_t attachedItemObjId, int64_t attachedItemCount, int64_t attachedKinah, model::gameobjects::LetterType letterType) {
	AION_UNPORTED();
}

model::templates::mail::MailMessage MailService::validateRecipient(model::gameobjects::player::Player& sender,
	runtime::Ptr<model::gameobjects::player::PlayerCommonData> recipientCommonData) {
	AION_UNPORTED();
}

float MailService::getQualityPriceRate(model::gameobjects::Item& senderItem) {
	AION_UNPORTED();
}

void MailService::readMail(model::gameobjects::player::Player& player, int32_t letterId) {
	AION_UNPORTED();
}

void MailService::getAttachments(model::gameobjects::player::Player& player, int32_t letterId, int8_t attachmentType) {
	AION_UNPORTED();
}

void MailService::deleteMail(model::gameobjects::player::Player& player, std::span<const int32_t> mailObjId) {
	AION_UNPORTED();
}

void MailService::onPlayerLogin(model::gameobjects::player::Player& player) {
	player.setMailbox(dao::MailDAO::loadPlayerMailbox(player));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_MAIL_SERVICE());
}

void MailService::sendMailList(model::gameobjects::player::Player& player, bool isExpress, bool sendRefreshPacket) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::mail
