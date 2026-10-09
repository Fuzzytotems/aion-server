#include "aion/gameserver/network/aion/clientpackets/CM_CAPTCHA.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CAPTCHA.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CAPTCHA::CM_CAPTCHA(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CAPTCHA.java:37-50
void CM_CAPTCHA::readImpl() {
	type = readUC();
	switch (type) {
		case 2:
			count = readUC();
			word = readS();
			break;
		case 4: // /ExtractStatus
			break;
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_CAPTCHA")
				.warn("Unknown CAPTCHA packet type " + std::to_string(type));
			break;
	}
}

// Java CM_CAPTCHA.java:52-82. `player.getCaptchaWord().equalsIgnoreCase(word)` of a player without a word is Java's NullPointerException
void CM_CAPTCHA::runImpl() {
	using configs::main::SecurityConfig;
	using serverpackets::SM_SYSTEM_MESSAGE;
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	switch (type) {
		case 2: {
			std::optional<std::string> captchaWord = player->getCaptchaWord();
			if (!captchaWord) // Java: getCaptchaWord().equalsIgnoreCase on null
				throw runtime::NullPointerException("Player.getCaptchaWord()");
			if (commons::utils::StringUtils::equalsIgnoreCase(*captchaWord, word)) {
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_UNRESTRICT());
				utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_CAPTCHA(true, 0));
				services::PunishmentService::setIsNotGatherable(*player, 0, false, 0);

				// fp bonus (like retail)
				player->getLifeStats()->increaseFp(serverpackets::SM_ATTACK_STATUS_TYPE::FP, SecurityConfig::CAPTCHA_BONUS_FP_TIME.load(), 0,
					serverpackets::SM_ATTACK_STATUS_LOG::REGULAR);
			} else {
				int32_t banTime = SecurityConfig::CAPTCHA_EXTRACTION_BAN_TIME.load() + (SecurityConfig::CAPTCHA_EXTRACTION_BAN_ADD_TIME.load() * count);

				if (count < 3) {
					utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_UNRESTRICT_FAILED_RETRY(3 - count));
					utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_CAPTCHA(false, banTime));
					services::PunishmentService::setIsNotGatherable(*player, count, true, banTime * 1000LL);
				} else {
					utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_UNRESTRICT_FAILED());
					services::PunishmentService::setIsNotGatherable(*player, count, true, banTime * 1000LL);
				}
			}
			break;
		}
		case 4:
			if (player->isGatherRestricted())
				sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_RESTRICTED(player->getGatherRestrictionDurationSeconds()));
			else
				sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_NOT_RESTRICTED());
	}
}

AION_CLIENT_PACKET(CM_CAPTCHA);

} // namespace aion::gameserver::network::aion::clientpackets
