#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author kosyachok
 */
class MailDAO {
public:
	static runtime::Ref<model::gameobjects::player::Mailbox> loadPlayerMailbox(model::gameobjects::player::Player& player);
	static bool haveUnread(int32_t playerId);
	static void storeMailbox(model::gameobjects::player::Player& player);
	static bool storeLetter(model::gameobjects::Letter& letter);
private:
	static bool saveLetter(model::gameobjects::Letter& letter);
	static bool updateLetter(model::gameobjects::Letter& letter);
public:
	static bool deleteLetter(int32_t letterId);
	static void updateOfflineMailCounter(model::gameobjects::player::PlayerCommonData& recipientCommonData);
	static std::vector<int32_t> getUsedIDs();
	static bool cleanMail(std::string_view recipient);
};

} // namespace aion::gameserver::dao
