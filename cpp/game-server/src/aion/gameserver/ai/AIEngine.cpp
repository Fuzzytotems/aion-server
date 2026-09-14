#include "aion/gameserver/ai/AIEngine.h"

#include <string>
#include <typeinfo>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/ai/AITemplate.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::ai {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.ai.AIEngine");

template <class T>
class AIEngine::DummyAI final : public AITemplate<T> {
public:
	explicit DummyAI(T& owner) : AITemplate<T>(owner) {}
};

namespace {

/** Java: Class.getSimpleName() of the owner's class; stands for utils::simpleClassName (P4-05, handlers-and-porting-plan.md §1.10) */
std::string simpleClassNameOf(const std::type_info& type) {
	std::string name = type.name(); // MSVC: "class aion::gameserver::model::gameobjects::Npc"
	if (size_t space = name.rfind(' '); space != std::string::npos)
		name.erase(0, space + 1);
	if (size_t colons = name.rfind("::"); colons != std::string::npos)
		name.erase(0, colons + 2);
	return name;
}

} // namespace

AIEngine::AIEngine() = default;

AIEngine::~AIEngine() = default;

void AIEngine::init() {
	// Java: ScriptManager loading of AIConfig.HANDLER_DIRECTORY; C++: the registry (registerAI per entry), validateScripts, "Loaded N AI handlers."
	AION_UNPORTED();
}

void AIEngine::reload() {
	AION_UNPORTED();
}

void AIEngine::registerAI(const handlers::AIHandlerEntry& aiClass) {
	AION_UNPORTED();
}

std::unique_ptr<AbstractAI> AIEngine::newAI(std::optional<std::string_view> name, model::gameobjects::Creature& owner) {
	std::unique_ptr<AbstractAI> aiInstance;
	if (!name) {
		aiInstance = std::make_unique<DummyAI<model::gameobjects::Creature>>(owner);
	} else {
		const handlers::AIHandlerEntry* entry = handlers::findAIHandler(handlers::aiHandlerEntries(), *name);
		if (entry == nullptr)
			throw runtime::IllegalArgumentException("No AI found for name " + std::string(*name));
		try {
			aiInstance = entry->create(owner);
		} catch (const runtime::UnportedException&) {
			throw; // an unported constructor body keeps its own trace (runtime/base/Unported.h)
		} catch (const std::exception&) {
			// Java: "Could not instantiate AI for class " + aiClass + " (owner: " + owner + ")"
			throw runtime::IllegalArgumentException(
				"Could not instantiate AI for class class " + std::string(entry->javaClass) + " (owner: " + owner.toString() + ")",
				std::current_exception());
		}
		if (aiInstance == nullptr) // Java: findConstructor(aiClass, owner.getClass(), false) == null
			throw runtime::IllegalArgumentException(handlers::aiOwnerMismatchMessage(*entry, simpleClassNameOf(typeid(owner))));
		aiInstance->setRegistryEntry(entry);
	}
	if (configs::main::AIConfig::ONCREATE_DEBUG.load())
		aiInstance->setLogging(true);
	return aiInstance;
}

void AIEngine::validateScripts() {
	AION_UNPORTED();
}

AIEngine& AIEngine::getInstance() {
	static AIEngine instance; // Java: SingletonHolder
	return instance;
}

} // namespace aion::gameserver::ai
