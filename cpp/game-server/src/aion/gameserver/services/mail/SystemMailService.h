#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/mail/fwd.h"

namespace aion::gameserver::services::mail {

/**
 * @author xTz
 */
class SystemMailService {
public:
	SystemMailService() = delete; // Java: private constructor of a static-only class
	static bool sendMail(std::string_view sender, std::string_view recipientName, std::string_view title, std::string_view message,
		int32_t attachedItemId, int64_t attachedItemCount, int64_t attachedKinahCount, model::gameobjects::LetterType letterType);
	static void updateRecipientMailbox(model::gameobjects::player::PlayerCommonData& recipientCommonData, model::gameobjects::Letter& newLetter);
};

} // namespace aion::gameserver::services::mail
