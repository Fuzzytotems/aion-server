#include "aion/gameserver/services/transfers/PlayerTransfer.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::transfers {

PlayerTransfer::PlayerTransfer(int32_t value, int32_t targetAccountValue, std::string_view accountValue, std::string_view nameValue)
	: taskId(value), targetAccount(targetAccountValue), name(std::string(nameValue)), account(std::string(accountValue)) {
}

runtime::Ref<PlayerTransfer> PlayerTransfer::create(int32_t value, int32_t targetAccountValue, std::string_view accountValue,
	std::string_view nameValue) {
	return runtime::makeRef<PlayerTransfer>(value, targetAccountValue, accountValue, nameValue);
}

void PlayerTransfer::setItemsData(runtime::Ptr<runtime::Array<int8_t>> value) {
	itemsData.set(value);
}

void PlayerTransfer::setCommonData(runtime::Ptr<runtime::Array<int8_t>> value) {
	commonData.set(value);
}

void PlayerTransfer::setSkillData(runtime::Ptr<runtime::Array<int8_t>> value) {
	skillData.set(value);
}

void PlayerTransfer::setRecipeData(runtime::Ptr<runtime::Array<int8_t>> value) {
	recipeData.set(value);
}

void PlayerTransfer::setQuestData(runtime::Ptr<runtime::Array<int8_t>> value) {
	questData.set(value);
}

void PlayerTransfer::setData(runtime::Ptr<runtime::Array<int8_t>> value) {
	data.set(value);
}

std::vector<uint8_t> PlayerTransfer::getDB() {
	AION_UNPORTED();
}

PlayerTransfer::~PlayerTransfer() = default;

} // namespace aion::gameserver::services::transfers
