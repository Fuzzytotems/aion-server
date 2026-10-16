#include "aion/gameserver/network/aion/clientpackets/CM_CREATE_CHARACTER.h"

#include <memory>
#include <optional>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CREATE_CHARACTER.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/services/AccountService.h"
#include "aion/gameserver/services/NameRestrictionService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using serverpackets::SM_CREATE_CHARACTER;

CM_CREATE_CHARACTER::CM_CREATE_CHARACTER(int32_t opcode, const StateSet& validStates) : AbstractCharacterEditPacket(opcode, validStates) {
}

void CM_CREATE_CHARACTER::readImpl() {
	readD(); // account id
	readS(); // account name
	readBasicInfo(true);
	readAppearance();
	type = readUC();
}

void CM_CREATE_CHARACTER::runImpl() {
	using model::account::PlayerAccountData;
	using model::gameobjects::player::PlayerCommonData;
	runtime::Ptr<model::account::Account> account = getConnection()->getAccount();
	if (type == 1) { // flag to enter char creation screen
		sendPacket(SM_CREATE_CHARACTER(nullptr, SM_CREATE_CHARACTER::RESPONSE_OPEN_CREATION_WINDOW));
		return;
	}
	services::AccountService::removeDeletedCharacters(*account);
	int32_t responseCode = validateBasicInfo(*account);
	if (responseCode != SM_CREATE_CHARACTER::RESPONSE_OK) {
		sendPacket(SM_CREATE_CHARACTER(nullptr, responseCode));
		return;
	}
	runtime::Ref<PlayerCommonData> playerCommonData = PlayerCommonData::create(utils::idfactory::IDFactory::getInstance().nextId());
	playerCommonData->setName(characterName);
	playerCommonData->setGender(gender);
	playerCommonData->setRace(race);
	playerCommonData->setPlayerClass(*playerClass);
	playerCommonData->setLevel(1); // level (exp) must be set after class
	// C++: the account data is a part of the account (constructed with its owner, added below)
	auto accPlData = std::make_unique<PlayerAccountData>(*account, *playerCommonData, *playerAppearance);
	// C++ only: the transient Player holds a Ref to the account data, and the Reclaimer destroys the Player later; unless the part went into the
	// account, it is retired to the Reclaimer too (destroyed once no Ref holds it), on every exit (database error, exception)
	auto retireUnaddedAccountData = runtime::finally([&accPlData, &account]() noexcept {
		if (accPlData)
			runtime::Reclaimer::retirePart(*account, std::move(accPlData));
	});
	runtime::Ref<model::gameobjects::player::Player> player = services::player::PlayerService::newPlayer(*accPlData, *account);
	if (!services::player::PlayerService::storeNewPlayer(*player, account->getName(), account->getId())) {
		sendPacket(SM_CREATE_CHARACTER(nullptr, SM_CREATE_CHARACTER::RESPONSE_DB_ERROR));
		utils::idfactory::IDFactory::getInstance().releaseId(playerCommonData->getPlayerObjId());
	} else {
		accPlData->setVisibleItems(player->getEquipment().getEquippedForAppearance());
		accPlData->setCreationDate(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())));
		services::player::PlayerService::storeCreationTime(player->getObjectId(), accPlData->getCreationDate());
		runtime::Ptr<PlayerAccountData> accountData(*accPlData);
		account->addPlayerAccountData(std::move(accPlData));
		sendPacket(SM_CREATE_CHARACTER(accountData, SM_CREATE_CHARACTER::RESPONSE_OK));
	}
}

int32_t CM_CREATE_CHARACTER::validateBasicInfo(model::account::Account& account) {
	using configs::main::GSConfig;
	using configs::main::MembershipConfig;
	int32_t maxCharCount = account.getMembership() >= MembershipConfig::CHARACTER_ADDITIONAL_ENABLE.load() ? MembershipConfig::CHARACTER_ADDITIONAL_COUNT.load()
																										 : GSConfig::CHARACTER_LIMIT_COUNT.load();
	if (account.size() > maxCharCount) // Java off-by-one kept: size() > max
		return SM_CREATE_CHARACTER::RESPONSE_SERVER_LIMIT_EXCEEDED;
	if (!playerClass) // should never happen (only with type == 1 to enter char creation screen, where we won't reach this validation)
		return SM_CREATE_CHARACTER::FAILED_TO_CREATE_THE_CHARACTER;
	if (services::player::PlayerService::isNameUsedOrReserved(std::nullopt, characterName))
		return GSConfig::CHARACTER_CREATION_MODE.load() == 2 ? SM_CREATE_CHARACTER::RESPONSE_NAME_RESERVED : SM_CREATE_CHARACTER::RESPONSE_NAME_ALREADY_USED;
	if (!services::NameRestrictionService::isValidName(characterName))
		return SM_CREATE_CHARACTER::RESPONSE_INVALID_NAME;
	if (services::NameRestrictionService::isForbidden(characterName))
		return SM_CREATE_CHARACTER::RESPONSE_FORBIDDEN_CHAR_NAME;
	if (!model::isStartingClass(*playerClass))
		return SM_CREATE_CHARACTER::RESPONSE_FORBIDDEN_CLASS;
	if (GSConfig::CHARACTER_CREATION_MODE.load() == 0) {
		for (runtime::Ptr<model::account::PlayerAccountData> p : account.getPlayerAccDataList()) {
			if (p->getPlayerCommonData()->getRace() != race)
				return SM_CREATE_CHARACTER::RESPONSE_OTHER_RACE;
		}
	}
	return SM_CREATE_CHARACTER::RESPONSE_OK;
}

AION_CLIENT_PACKET(CM_CREATE_CHARACTER);

} // namespace aion::gameserver::network::aion::clientpackets
