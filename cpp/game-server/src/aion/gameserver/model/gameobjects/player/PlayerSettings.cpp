#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"

#include "aion/gameserver/runtime/base/Unported.h"

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
	AION_UNPORTED();
}

void PlayerSettings::setShortcuts(runtime::Ptr<runtime::Array<int8_t>> value) {
	AION_UNPORTED();
}

void PlayerSettings::setHouseBuddies(runtime::Ptr<runtime::Array<int8_t>> value) {
	AION_UNPORTED();
}

void PlayerSettings::setDisplay(int32_t value) {
	AION_UNPORTED();
}

void PlayerSettings::setDeny(int32_t value) {
	AION_UNPORTED();
}

bool PlayerSettings::isInDeniedStatus(DeniedStatus value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
