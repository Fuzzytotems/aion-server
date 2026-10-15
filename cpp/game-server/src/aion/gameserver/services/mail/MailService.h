#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/mail/fwd.h"
#include "aion/gameserver/services/mail/fwd.h"

namespace aion::gameserver::services::mail {

/**
 * @author kosyachok
 */
class MailService {
public:
	MailService() = delete; // Java: private constructor of a static-only class
	static void sendMail(model::gameobjects::player::Player& sender, std::string_view recipientName, std::string_view title, std::string_view message,
		int32_t attachedItemObjId, int64_t attachedItemCount, int64_t attachedKinah, model::gameobjects::LetterType letterType);
private:
	static model::templates::mail::MailMessage validateRecipient(model::gameobjects::player::Player& sender,
		runtime::Ptr<model::gameobjects::player::PlayerCommonData> recipientCommonData);
	static float getQualityPriceRate(model::gameobjects::Item& senderItem);
public:
	/** Read letter with specified letter id */
	static void readMail(model::gameobjects::player::Player& player, int32_t letterId);
	static void getAttachments(model::gameobjects::player::Player& player, int32_t letterId, int8_t attachmentType);
	static void deleteMail(model::gameobjects::player::Player& player, std::span<const int32_t> mailObjId);
	static void onPlayerLogin(model::gameobjects::player::Player& player);
	static void sendMailList(model::gameobjects::player::Player& player, bool isExpress, bool sendRefreshPacket);
};

} // namespace aion::gameserver::services::mail
