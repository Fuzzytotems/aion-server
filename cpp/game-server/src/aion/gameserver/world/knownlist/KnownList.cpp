#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/base/Unported.h"

// S0b transition (docs/design/hub-headers.md §3.3): the constructor and the destructor instantiate the knownObjects map, whose Ref<KnownObject>
// elements need the complete KnownObject (not a hub; its header comes with the world chunk P4-10). Remove the guard once it exists.
#if __has_include("aion/gameserver/world/knownlist/KnownObject.h")
#define AION_S0B_KNOWN_LIST_MEMBERS 1
#include "aion/gameserver/world/knownlist/KnownObject.h"
#else
#define AION_S0B_KNOWN_LIST_MEMBERS 0
#endif

namespace aion::gameserver::world::knownlist {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.knownlist.KnownList");

#if AION_S0B_KNOWN_LIST_MEMBERS
KnownList::KnownList(model::gameobjects::VisibleObject& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

KnownList::~KnownList() = default;
#endif

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
void KnownList::update() {
	AION_UNPORTED();
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
void KnownList::clear(model::animations::ObjectDeleteAnimation animation) {
	AION_UNPORTED();
}

bool KnownList::knows(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

bool KnownList::sees(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

bool KnownList::addPair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) {
	AION_UNPORTED();
}

bool KnownList::add(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void KnownList::updateVisibleObject(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void KnownList::updateVisibility(KnownObject& knownObject) {
	AION_UNPORTED();
}

void KnownList::updatePetVisibility(KnownObject& knownObject) {
	AION_UNPORTED();
}

void KnownList::del(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	AION_UNPORTED();
}

void KnownList::notifySee(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void KnownList::notifyNotSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	AION_UNPORTED();
}

void KnownList::notifyNotKnow(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void KnownList::forgetObjectsOrUpdateVisibility() {
	AION_UNPORTED();
}

void KnownList::findVisibleObjects() {
	AION_UNPORTED();
}

bool KnownList::isAwareOf(runtime::Ptr<model::gameobjects::VisibleObject> newObject) {
	AION_UNPORTED();
}

float KnownList::getVisibleDistance() {
	AION_UNPORTED();
}

bool KnownList::isInRange(model::gameobjects::VisibleObject& newObject) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> KnownList::findObject(const std::function<bool(KnownObject&)>& predicate) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> KnownList::getObject(int32_t targetObjectId) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> KnownList::getPlayer(int32_t targetObjectId) {
	AION_UNPORTED();
}

void KnownList::forEach(const std::function<void(KnownObject&)>& action) {
	AION_UNPORTED();
}

void KnownList::forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer) {
	AION_UNPORTED();
}

void KnownList::forEachNpc(const std::function<void(model::gameobjects::Npc&)>& consumer) {
	AION_UNPORTED();
}

void KnownList::forEachPlayer(const std::function<void(model::gameobjects::player::Player&)>& consumer) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<KnownObject>> KnownList::stream() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> KnownList::streamPlayers() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> KnownList::streamVisiblePlayers() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::knownlist
