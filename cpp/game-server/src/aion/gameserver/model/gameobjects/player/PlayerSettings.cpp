#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"

#include "aion/gameserver/model/gameobjects/player/DeniedStatusInfo.h"

namespace aion::gameserver::model::gameobjects::player {

PlayerSettings::PlayerSettings() = default;

PlayerSettings::PlayerSettings(runtime::Ptr<runtime::Array<int8_t>> uiSettingsValue, runtime::Ptr<runtime::Array<int8_t>> shortcutsValue,
	runtime::Ptr<runtime::Array<int8_t>> houseBuddiesValue, int32_t denyValue, int32_t displayValue) {
	uiSettings.set(uiSettingsValue);
	shortcuts.set(shortcutsValue);
	houseBuddies.set(houseBuddiesValue);
	deny.set(denyValue);
	display.set(displayValue);
}

PlayerSettings::~PlayerSettings() = default;

runtime::Ref<PlayerSettings> PlayerSettings::create() {
	return runtime::makeRef<PlayerSettings>();
}

runtime::Ref<PlayerSettings> PlayerSettings::create(runtime::Ptr<runtime::Array<int8_t>> uiSettingsValue,
	runtime::Ptr<runtime::Array<int8_t>> shortcutsValue, runtime::Ptr<runtime::Array<int8_t>> houseBuddiesValue, int32_t denyValue,
	int32_t displayValue) {
	return runtime::makeRef<PlayerSettings>(uiSettingsValue, shortcutsValue, houseBuddiesValue, denyValue, displayValue);
}

void PlayerSettings::setUiSettings(runtime::Ptr<runtime::Array<int8_t>> value) {
	uiSettings.set(value);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

void PlayerSettings::setShortcuts(runtime::Ptr<runtime::Array<int8_t>> value) {
	shortcuts.set(value);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

void PlayerSettings::setHouseBuddies(runtime::Ptr<runtime::Array<int8_t>> value) {
	houseBuddies.set(value);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

void PlayerSettings::setDisplay(int32_t value) {
	display.set(value);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

void PlayerSettings::setDeny(int32_t value) {
	deny.set(value);
	persistentState.set(PersistentState::UPDATE_REQUIRED);
}

bool PlayerSettings::isInDeniedStatus(DeniedStatus value) {
	const int32_t id = getId(value);
	int32_t isDeniedStatus = deny.get() & id;
	return isDeniedStatus == id;
}

} // namespace aion::gameserver::model::gameobjects::player
