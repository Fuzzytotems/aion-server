#pragma once

// Stand-ins for the core classes that HandlerRegistry.h forward-declares, for the regscan fixture handlers (the real classes come with the
// spine). Same namespaces and names as the real ones; only the fixture executables link them.

#include <cstdint>
#include <string>
#include <utility>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::model::gameobjects {
class Creature {
public:
	explicit Creature(std::string name) : name(std::move(name)) {}
	virtual ~Creature() = default;
	const std::string name;
};
class Npc : public Creature {
public:
	using Creature::Creature;
};
class Summon : public Creature {
public:
	using Creature::Creature;
};
} // namespace aion::gameserver::model::gameobjects

namespace aion::gameserver::ai {
class AbstractAI {
public:
	virtual ~AbstractAI() = default;
	virtual std::string describe() const = 0;
};
/** Java: AbstractAI<T extends Creature>; exposes OwnerType like the real AITemplate */
template <class T>
class AITemplate : public AbstractAI {
public:
	using OwnerType = T;
	explicit AITemplate(T& owner) : owner(owner) {}
	T& getOwner() const noexcept { return owner; }

private:
	T& owner;
};
} // namespace aion::gameserver::ai

namespace aion::gameserver::world {
class WorldMapInstance {
public:
	explicit WorldMapInstance(int32_t mapId) : mapId(mapId) {}
	const int32_t mapId;
};
} // namespace aion::gameserver::world

namespace aion::gameserver::instance::handlers {
class InstanceHandler : public runtime::RefCounted {
public:
	virtual int32_t getMapId() const = 0;

protected:
	InstanceHandler() = default;
	~InstanceHandler() override = default;
};
} // namespace aion::gameserver::instance::handlers

namespace aion::gameserver::world::zone::handler {
class ZoneHandler : public runtime::RefCounted {
public:
	virtual int32_t getQuestId() const { return 0; }

protected:
	ZoneHandler() = default;
	~ZoneHandler() override = default;
};
class QuestZoneHandler : public ZoneHandler {
public:
	int32_t getQuestId() const override { return questId; }

protected:
	explicit QuestZoneHandler(int32_t questId) : questId(questId) {}
	const int32_t questId;
};
} // namespace aion::gameserver::world::zone::handler

namespace aion::gameserver::questEngine::handlers {
class AbstractQuestHandler {
public:
	virtual ~AbstractQuestHandler() = default;
	int32_t getQuestId() const noexcept { return questId; }

protected:
	explicit AbstractQuestHandler(int32_t questId) : questId(questId) {}

private:
	const int32_t questId;
};
} // namespace aion::gameserver::questEngine::handlers

namespace aion::gameserver::utils::chathandlers {
class ChatCommand {
public:
	virtual ~ChatCommand() = default;
	const std::string& getAlias() const noexcept { return alias; }

protected:
	explicit ChatCommand(std::string alias) : alias(std::move(alias)) {}

private:
	const std::string alias;
};
class AdminCommand : public ChatCommand {
protected:
	using ChatCommand::ChatCommand;
};
class PlayerCommand : public ChatCommand {
protected:
	using ChatCommand::ChatCommand;
};
class ConsoleCommand : public ChatCommand {
protected:
	using ChatCommand::ChatCommand;
};
} // namespace aion::gameserver::utils::chathandlers

namespace aion::gameserver::network::aion {
class StateSet {
public:
	constexpr explicit StateSet(uint8_t bits) : bits(bits) {}
	const uint8_t bits;
};
class AionClientPacket {
public:
	virtual ~AionClientPacket() = default;
	int32_t getOpCode() const noexcept { return opcode; }
	uint8_t getStateBits() const noexcept { return states; }

protected:
	AionClientPacket(int32_t opcode, const StateSet& validStates) : opcode(opcode), states(validStates.bits) {}

private:
	const int32_t opcode;
	const uint8_t states;
};
} // namespace aion::gameserver::network::aion

namespace aion::gameserver::model::DialogAction {
inline constexpr int32_t QUEST_SELECT = 10;
} // namespace aion::gameserver::model::DialogAction
