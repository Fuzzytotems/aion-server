#include "aion/gameserver/services/BonusPackService.h"

#include "aion/gameserver/dao/BonusPackDAO.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/mail/SystemMailService.h"

namespace aion::gameserver::services {

BonusPackService::BonusPackService() {
	// itemId, count

	rewards.put(186000242, 15);   // Ceramium Medal
	rewards.put(186000130, 6500); // Crucible Insignia
	rewards.put(186000051, 5);    // Major Ancient Crown
	rewards.put(166020003, 15);   // [Event] Omega Enchantment Stone

	rewards.put(186000236, 250);  // Blood Mark
	rewards.put(186000237, 4500); // Ancient Coin
	rewards.put(186000409, 150);  // Daeva's Respite Coin

	rewards.put(188052562, 5); // Scroll Bundle
	rewards.put(190100051, 1); // Flying Pagati
}

BonusPackService::~BonusPackService() = default;

BonusPackService& BonusPackService::getInstance() {
	static BonusPackService instance; // Java SingletonHolder
	return instance;
}

void BonusPackService::addPlayerCustomReward(model::gameobjects::player::Player& player) {
	if (rewards.isEmpty()) // Java: rewards == null || rewards.isEmpty() (the final field is never null)
		return;

	if (player.getLevel() != 65)
		return;

	if (player.getCommonData()->getMailboxLetters() + rewards.size() > 100)
		return;

	int32_t accountId = player.getAccount()->getId();
	if (dao::BonusPackDAO::loadReceivingPlayer(accountId) > 0)
		return;

	if (!dao::BonusPackDAO::storeReceivingPlayer(accountId, player.getObjectId()))
		return;

	// Java: HashMap iteration order (unspecified; every entry gets its own mail)
	for (const auto& e : rewards.snapshot()) {
		mail::SystemMailService::sendMail("Beyond Aion", player.getName(), "Bonus Pack",
			"Greetings Daeva!\n\n"
			"You have reached level 65 with your first character and therefore we have a special something for you."
			" In gratitude for your support we have prepared a package with valuable items for you.\n\n"
			"Enjoy your stay on Beyond Aion!",
			e.key, e.value, 0, model::gameobjects::LetterType::EXPRESS);
	}
}

} // namespace aion::gameserver::services
