#include "aion/gameserver/services/mail/SystemMailService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::mail {

static const auto log = commons::logging::LoggerFactory::getLogger("SYSMAIL_LOG");

bool SystemMailService::sendMail(std::string_view sender, std::string_view recipientName, std::string_view title, std::string_view message,
	int32_t attachedItemId, int64_t attachedItemCount, int64_t attachedKinahCount, model::gameobjects::LetterType letterType) {
	AION_UNPORTED();
}

void SystemMailService::updateRecipientMailbox(model::gameobjects::player::PlayerCommonData& recipientCommonData,
	model::gameobjects::Letter& newLetter) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::mail
