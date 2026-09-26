// =====================================================================================================================================
// PROTOTYPE MODEL CODE (design runtime-architecture.md §19 P4). NOT THE REAL GAME MODEL.
// =====================================================================================================================================
//
// Shells of VisibleObject/Npc/Player, WorldPosition, MapRegion, WorldMapInstance, KnownList/NpcKnownList/FlagKnownList, KnownObject,
// ObserveController, NpcMoveController, MoveTaskManager, World and an eager server packet path, written with the kernel types exactly as the
// mechanical mapping of design §3.2 would declare the Java members. Only the hot paths of design §19 P4 are modelled; everything else (AI,
// stats, zones, geo, controllers) is reduced to counters. The Java bodies ported line by line are named with their Java source.
//
// This file is included by ModelLockFree.cpp and ModelLockedReads.cpp with
//   P4_NAMESPACE      namespace of this instantiation (lockfree / lockedreads)
//   P4_LOCKED_READS   0: lock-free ConcurrentHashMap reads (default design), 1: stripe-locked reads (AION_CHM_LOCKED_READS fallback)
// so both variants run identical model code in one executable.
//
// Choices where the design leaves details open (prototype only):
// - Npc parts follow design §3.2.1 pattern 2 (setKnownList/setMoveController in the constructor -> PartSlot); ObserveController is a Java
//   `final` reference to a K4 class -> `const Ref<ObserveController>`; KnownObject is K4 (it has a `synchronized` block) -> RefCounted.
// - WorldMapInstance.regions (Java `final HashMap<Integer, MapRegion>` of owner-created regions) is a PartMap (design §3.2.1 pattern 1). The
//   --flat-regions option replaces the lookup by an immutable flat array to measure a fieldmap.toml override.
// - Region deactivation (MapRegion.scheduleDeactivation, 60 s) is not modelled: players never leave their clusters.
// - Controller callbacks: Npc see/notSee/notKnow count AI events; Player see/notSee send SM_NPC_INFO / SM_DELETE-like packets.
// - The SM_MOVE rule is configurable: every move step (default, conservative) or Java's "mask or destination changed".
// - FlagKnownList.update's one-sided removeIf is ported as the two-sided removal of design §5.3 (DEVIATION 12).
// - World layout: 80k npcs on 16 maps of 2048 m (the density of one 8192 m map, 1 npc / 840 m²) so the Java per-instance scans
//   (worldMapNpcs for flags) see realistic instance sizes; movers spawn near player clusters so their regions are active.
// - Walkers get randomized speed, route radius and first route step by default. With identical walkers (--synchronized-walkers) all 2,000
//   arrive in the same tick and call updateKnownlist together every ~47 ticks: a worst case that dominates p99.
// - Players relog (new Player object) to keep pair-add patterns 2 and 3 running; npc churn replaces objects (new ids), half of them movers.
// - Stress and measurement switches (--despawn-window-us, --fast-instanceof, --profile) change timing or code paths and are off by default.

#if !defined(P4_NAMESPACE) || !defined(P4_LOCKED_READS)
#error "ProtoModel.inl needs P4_NAMESPACE and P4_LOCKED_READS"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <format>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/CopyOnWriteArrayList.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

#include "p4/BenchSupport.h"
#include "p4/Scenario.h"

namespace aion::gameserver::bench::P4_NAMESPACE {

using namespace aion::gameserver::runtime;
using aion::gameserver::utils::FutureRef;
using aion::gameserver::utils::ThreadPoolManager;

template <class K, class V>
using Chm = ConcurrentHashMap<K, V, P4_LOCKED_READS != 0>;

constexpr bool LOCKED_READS = P4_LOCKED_READS != 0;
constexpr float DEFAULT_VISIBLE_DISTANCE = 95.0f; // VisibleObject.java:247
constexpr int32_t REGION_SIZE = 128;              // gameserver.world.region.size
constexpr float MOVE_OFFSET = 0.5f;
constexpr float PLAYER_CLUSTER_RADIUS = 60.0f;

enum class ObjectDeleteAnimation : uint8_t { NONE, FADE_OUT };
enum class PairPattern : uint8_t { REGION_SCAN, PLAYER_FLAGS, FLAG_UPDATE };
enum class ObserverType : uint8_t { MOVE = 1, ATTACK = 2 };

namespace MovementMask {
constexpr uint8_t POSITION = 0x40;
constexpr uint8_t MANUAL = 0x80;
constexpr uint8_t NPC_STARTMOVE = POSITION | MANUAL | 0x20;
constexpr uint8_t NPC_WALK_SLOW = POSITION | MANUAL | 0x08;
} // namespace MovementMask

class Model;
class VisibleObject;
class Npc;
class Player;
class KnownList;
class MapRegion;
class WorldMapInstance;

/** The running scenario (set before any shell exists, cleared after the Reclaimer drained every shell). */
Model* model = nullptr;

// ---------------------------------------------------------------------------------------------------------------------- static data

/** Spawn template (K1 static data, immortal for the process: `const SpawnPoint*`). */
struct SpawnPoint final : StaticTemplate {
	int32_t mapIndex = 0;
	float x = 0;
	float y = 0;
	float z = 0;
	bool mover = false;
	bool flag = false;
	float speed = 0;
	std::vector<std::array<float, 3>> route;
	int32_t firstRouteStep = 0;
};

// ---------------------------------------------------------------------------------------------------------------------- connections, packets

/** design §8.2 SerializedBody */
struct SerializedBody {
	std::shared_ptr<const std::vector<uint8_t>> bytes;
	uint64_t seq = 0;
	int32_t opCode = 0;
};

/** AionConnection stand-in: a byte-buffer sink with the design's seq-ordered queue (§8.4) behind a CONNECTION_QUEUE leaf mutex. */
class FakeConnection {
public:
	explicit FakeConnection(int32_t connectionId) : id(connectionId) {}

	/** design §8.2 enqueue: CONNECTION_QUEUE leaf; insert by seq */
	void enqueue(SerializedBody body) {
		PhaseTimer timer(Phase::ENQUEUE);
		std::scoped_lock lock(queueMutex_);
		auto position = queue_.end();
		while (position != queue_.begin() && std::prev(position)->seq > body.seq)
			--position;
		if (position != queue_.end())
			++outOfOrderInserts_;
		queue_.insert(position, std::move(body));
		++enqueued_;
		maxDepth_ = std::max(maxDepth_, queue_.size());
	}

	/** IO strand stand-in: takes everything queued and "writes" it */
	void drain() {
		std::deque<SerializedBody> taken;
		{
			std::scoped_lock lock(queueMutex_);
			taken.swap(queue_);
		}
		uint64_t bytes = 0;
		for (const SerializedBody& body : taken)
			bytes += body.bytes->size();
		bytesWritten_.fetch_add(bytes, std::memory_order_relaxed);
		packetsWritten_.fetch_add(taken.size(), std::memory_order_relaxed);
	}

	struct Stats {
		uint64_t enqueued;
		uint64_t outOfOrderInserts;
		size_t maxDepth;
		uint64_t bytesWritten;
		uint64_t packetsWritten;
	};
	Stats stats() {
		std::scoped_lock lock(queueMutex_);
		return {enqueued_, outOfOrderInserts_, maxDepth_, bytesWritten_.load(), packetsWritten_.load()};
	}

	const int32_t id;

private:
	LeafMutex queueMutex_{LockRank::CONNECTION_QUEUE};
	std::deque<SerializedBody> queue_;
	uint64_t enqueued_ = 0;
	uint64_t outOfOrderInserts_ = 0;
	size_t maxDepth_ = 0;
	std::atomic<uint64_t> bytesWritten_{0};
	std::atomic<uint64_t> packetsWritten_{0};
};

std::atomic<uint64_t> packetSequence{1};
thread_local std::vector<uint8_t> scratchBuffer; // design §8.2: thread-local scratch buffer

/** AionServerPacket stand-in (design §8.2): eager serialization on the sending thread into a thread-local buffer. K2: not RefCounted. */
class ServerPacket {
public:
	virtual ~ServerPacket() = default;
	ServerPacket() = default;
	ServerPacket(const ServerPacket&) = delete;
	ServerPacket& operator=(const ServerPacket&) = delete;

	SerializedBody serialize() const {
		SerializedBody body;
		body.seq = packetSequence.fetch_add(1, std::memory_order_acq_rel); // seq sampled at serialization start
		body.opCode = opCode();
		std::vector<uint8_t>& buffer = scratchBuffer;
		buffer.clear();
		writeH(0); // length, patched below
		writeH(static_cast<int16_t>(body.opCode));
		writeC(0x5D); // static server packet code
		writeH(static_cast<int16_t>(~body.opCode));
		writeImpl();
		uint16_t length = static_cast<uint16_t>(buffer.size());
		buffer[0] = static_cast<uint8_t>(length & 0xFF);
		buffer[1] = static_cast<uint8_t>(length >> 8);
		body.bytes = std::make_shared<const std::vector<uint8_t>>(buffer.begin(), buffer.end());
		countEvent(Event::PACKETS_SERIALIZED);
		countEvent(Event::PACKET_BYTES_SERIALIZED, buffer.size());
		return body;
	}

protected:
	virtual int32_t opCode() const noexcept = 0;
	virtual void writeImpl() const = 0;

	static void writeC(uint8_t value) { scratchBuffer.push_back(value); }
	static void writeH(int16_t value) {
		writeC(static_cast<uint8_t>(value & 0xFF));
		writeC(static_cast<uint8_t>((value >> 8) & 0xFF));
	}
	static void writeD(int32_t value) {
		for (int shift = 0; shift < 32; shift += 8)
			writeC(static_cast<uint8_t>((value >> shift) & 0xFF));
	}
	static void writeF(float value) {
		uint32_t bits = std::bit_cast<uint32_t>(value);
		writeD(static_cast<int32_t>(bits));
	}
};

/** design §8.3 "SerializedPacket once": a SHARED broadcast is serialized once, on the first eligible recipient */
class SerializedPacket {
public:
	const SerializedBody& getOrSerialize(const ServerPacket& packet) {
		if (!body_) {
			PhaseTimer timer(Phase::SERIALIZE);
			body_ = packet.serialize();
		}
		return *body_;
	}

private:
	std::optional<SerializedBody> body_;
};

// ---------------------------------------------------------------------------------------------------------------------- positions and regions

/** WorldPosition.java: position fields are non-final -> Field<T>; mapRegion non-final -> Field<Ref<MapRegion>> (a Ref to a part retains its owner). */
class WorldPosition final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<WorldPosition> create(int32_t mapId, float x, float y, float z, int8_t heading, MapRegion& region) {
		return makeRef<WorldPosition>(mapId, x, y, z, heading, region);
	}

	int32_t getMapId() const noexcept { return mapId_; }
	float getX() const noexcept { return x_; }
	float getY() const noexcept { return y_; }
	float getZ() const noexcept { return z_; }
	int8_t getHeading() const noexcept { return heading_; }
	Ptr<MapRegion> getMapRegion() const { return mapRegion_.get(); }
	WorldMapInstance& getWorldMapInstance() const;
	bool isMapRegionActive() const;
	bool isSpawned() const noexcept { return isSpawned_; }
	void setIsSpawned(bool spawned) noexcept { isSpawned_ = spawned; }
	void setXYZH(float newX, float newY, float newZ, int8_t newHeading) noexcept {
		x_ = newX;
		y_ = newY;
		z_ = newZ;
		heading_ = newHeading;
	}
	void setMapRegion(Ptr<MapRegion> region);

protected:
	WorldPosition(int32_t mapId, float x, float y, float z, int8_t heading, MapRegion& region);
	~WorldPosition() override = default;

private:
	const int32_t mapId_;
	Field<Ref<MapRegion>> mapRegion_;
	Field<float> x_;
	Field<float> y_;
	Field<float> z_;
	Field<int8_t> heading_;
	Field<bool> isSpawned_{false}; // Java volatile
};

/** MapRegion.java, a part of its WorldMapInstance. */
class MapRegion final : public OwnedPart {
public:
	MapRegion(WorldMapInstance& parent, int32_t regionId);

	int32_t getRegionId() const noexcept { return regionId_; }
	WorldMapInstance& getParent() const noexcept { return parent_; }
	const Chm<int32_t, Ref<VisibleObject>>& getObjects() const noexcept { return objects_; }
	Ptr<Array<MapRegion*>> getNeighbours() const { return neighboursIncludingSelf_.get(); }
	void setNeighbours(Ref<Array<MapRegion*>> neighbours) { neighboursIncludingSelf_ = std::move(neighbours); }

	void add(VisibleObject& object);
	void remove(VisibleObject& object);

	bool isActive() const {
		SYNCHRONIZED(*this) {
			return regionActive_;
		}
	}

private:
	int32_t incrementPlayerCount() {
		SYNCHRONIZED(*this) {
			return ++playerCount_;
		}
	}
	int32_t decrementPlayerCount() {
		SYNCHRONIZED(*this) {
			return playerCount_ == 0 ? 0 : --playerCount_;
		}
	}
	bool setRegionState(bool active) {
		SYNCHRONIZED(*this) {
			if (regionActive_ == active)
				return false;
			regionActive_ = active;
			return true;
		}
	}
	void activate();
	void notifyCreatures() const;

	const int32_t regionId_;
	const OwnerRef<WorldMapInstance> parent_;
	Field<Ref<Array<MapRegion*>>> neighboursIncludingSelf_;
	Chm<int32_t, Ref<VisibleObject>> objects_{AION_LOCK_CLASS(MapRegion::objects#stripe)};
	Field<int32_t> playerCount_{0};
	Field<bool> regionActive_{false};
};

/** WorldMapInstance.java (2D regions, WorldMap2DInstance.java). */
class WorldMapInstance final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<WorldMapInstance> create(int32_t mapId, int32_t worldSize) { return makeRef<WorldMapInstance>(mapId, worldSize); }

	int32_t getMapId() const noexcept { return mapId_; }
	int32_t getWorldSize() const noexcept { return worldSize_; }

	/** WorldMap2DInstance.getRegion: null outside the map */
	Ptr<MapRegion> getRegion(float x, float y) const;

	void addObject(VisibleObject& object);
	void removeObject(VisibleObject& object);

	template <class F>
	void forEachNpc(F&& consumer) const {
		for (Ptr<Npc> npc : worldMapNpcs_.values())
			consumer(*npc);
	}
	template <class F>
	void forEachPlayer(F&& consumer) const {
		for (Ptr<Player> player : worldMapPlayers_.values())
			consumer(*player);
	}
	template <class F>
	void forEachRegion(F&& consumer) const {
		for (MapRegion* region : flatRegions_)
			consumer(*region);
	}

protected:
	WorldMapInstance(int32_t mapId, int32_t worldSize);
	~WorldMapInstance() override = default;

private:
	const int32_t mapId_;
	const int32_t worldSize_;
	const int32_t regionsPerSide_;
	PartMap<int32_t, MapRegion> regions_{*this};
	// PROTOTYPE: immutable flat index of the same parts (fieldmap.toml override candidate; used with --flat-regions and for verification)
	std::vector<MapRegion*> flatRegions_;
	Chm<int32_t, Ref<VisibleObject>> worldMapObjects_{AION_LOCK_CLASS(WorldMapInstance::worldMapObjects#stripe)};
	Chm<int32_t, Ref<Npc>> worldMapNpcs_{AION_LOCK_CLASS(WorldMapInstance::worldMapNpcs#stripe)};
	Chm<int32_t, Ref<Player>> worldMapPlayers_{AION_LOCK_CLASS(WorldMapInstance::worldMapPlayers#stripe)};
};

// ---------------------------------------------------------------------------------------------------------------------- observers

/** ActionObserver.java (MOVE observers only). */
class ActionObserver : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<ActionObserver> create(ObserverType type) { return makeRef<ActionObserver>(type); }

	bool matches(ObserverType type) const noexcept { return (static_cast<uint8_t>(observerType_) & static_cast<uint8_t>(type)) != 0; }
	void makeOneTimeUse() noexcept { oneTimeUse_ = true; }
	bool isOneTimeUse() const noexcept { return oneTimeUse_; }
	virtual void moved() noexcept { ++notifications_; } // java-race: relaxed counter like the Java callbacks' plain fields
	void onRemoved() noexcept {}

protected:
	explicit ActionObserver(ObserverType type) noexcept : observerType_(type) {}
	~ActionObserver() override = default;

private:
	const ObserverType observerType_;
	Field<bool> oneTimeUse_{false};
	Field<int64_t> notifications_{0};
};

/** ObserveController.java: `final List<ActionObserver> observers = new ArrayList<>()`, attackCalcObservers CopyOnWriteArrayList. */
class ObserveController final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<ObserveController> create() { return makeRef<ObserveController>(); }

	void attach(Ref<ActionObserver> observer) {
		observer->makeOneTimeUse();
		addObserver(std::move(observer));
	}
	void addObserver(Ref<ActionObserver> observer) {
		SYNCHRONIZED(observers_) {
			observers_.add(std::move(observer));
		}
	}

	/** ObserveController.java:57-84 with the design §3.3 lock-free isEmpty fast path in front of the synchronized block */
	void notifyObservers(ObserverType type) {
		PhaseTimer timer(Phase::OBSERVERS);
		if (observers_.isEmpty()) {
			countEvent(Event::OBSERVER_FASTPATH_EMPTY);
			return;
		}
		std::vector<Ptr<ActionObserver>> notifiable;
		SYNCHRONIZED(observers_) {
			if (observers_.isEmpty())
				return;
			for (auto iterator = observers_.iterator(); iterator.hasNext();) {
				Ptr<ActionObserver> observer = iterator.next();
				if (observer->matches(type)) {
					notifiable.push_back(observer);
					if (observer->isOneTimeUse())
						iterator.remove();
				}
			}
		}
		// notify outside of lock
		for (const Ptr<ActionObserver>& observer : notifiable) {
			observer->moved();
			if (observer->isOneTimeUse())
				observer->onRemoved();
			countEvent(Event::OBSERVER_NOTIFIED);
		}
	}
	void notifyMoveObservers() { notifyObservers(ObserverType::MOVE); }

protected:
	ObserveController() = default;
	~ObserveController() override = default;

private:
	ArrayList<Ref<ActionObserver>> observers_{AION_LOCK_CLASS(ObserveController::observers)};
	CopyOnWriteArrayList<Ref<ActionObserver>> attackCalcObservers_{AION_LOCK_CLASS(ObserveController::attackCalcObservers)};
};

// ---------------------------------------------------------------------------------------------------------------------- visible objects

/** VisibleObject.java / Creature.java shell. */
class VisibleObject : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	int32_t getObjectId() const noexcept { return objectId_; }
	const SpawnPoint& getSpawn() const noexcept { return *spawn_; }
	virtual bool isFlag() const noexcept { return false; }
	virtual float getVisibleDistance() const noexcept { return DEFAULT_VISIBLE_DISTANCE; }
	/** bench --fast-instanceof: virtual kind query standing in for a generated class id */
	enum class Kind : uint8_t { NPC, PLAYER };
	virtual Kind kind() const noexcept = 0;
	bool canSee(const VisibleObject&) const noexcept { return true; }

	Ptr<WorldPosition> getPosition() const { return position_.get(); }
	void setPosition(Ref<WorldPosition> position) { position_ = std::move(position); }
	bool isSpawned() const {
		Ptr<WorldPosition> position = position_.get();
		return position && position->isSpawned();
	}
	float getX() const { return position_->getX(); }
	float getY() const { return position_->getY(); }
	float getZ() const { return position_->getZ(); }

	KnownList& getKnownList() const { return *knownList_; }
	ObserveController& getObserveController() const { return *observeController_; }
	void updateKnownlist();
	void clearKnownlist(ObjectDeleteAnimation animation);

	// VisibleObjectController callbacks
	virtual void see(VisibleObject& object) = 0;
	virtual void notSee(VisibleObject& object, ObjectDeleteAnimation animation) = 0;
	virtual void notKnow(VisibleObject& object) = 0;
	virtual void onDespawn() {}

	/** bench instrumentation: time the object was removed from the world (reclamation lag histogram) */
	void markRemoved() noexcept { removedNanos_ = nowNanos(); }

protected:
	VisibleObject(int32_t objectId, const SpawnPoint& spawn) : objectId_(objectId), spawn_(&spawn), observeController_(ObserveController::create()) {
		countEvent(Event::SHELLS_CREATED);
	}
	/** release-only (design §2.7): records the reclamation lag of removed objects */
	~VisibleObject() override;

	PartSlot<KnownList> knownList_{*this};

private:
	const int32_t objectId_;
	const SpawnPoint* const spawn_;
	Field<Ref<WorldPosition>> position_;
	const Ref<ObserveController> observeController_;
	Field<int64_t> removedNanos_{0};
};

class NpcMoveController;

/** Npc.java shell. */
class Npc final : public VisibleObject {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create(int32_t objectId, const SpawnPoint& spawn) { return makeRef<Npc>(objectId, spawn); }

	bool isFlag() const noexcept override { return getSpawn().flag; }
	Kind kind() const noexcept override { return Kind::NPC; }
	NpcMoveController& getMoveController() const { return *moveController_; }

	void see(VisibleObject&) override { ++aiEvents_; }
	void notSee(VisibleObject&, ObjectDeleteAnimation) override { ++aiEvents_; }
	void notKnow(VisibleObject&) override { ++aiEvents_; }
	void onDespawn() override;

protected:
	Npc(int32_t objectId, const SpawnPoint& spawn);
	~Npc() override; // out of line: PartSlot<NpcMoveController> needs the complete part type

private:
	PartSlot<NpcMoveController> moveController_{*this};
	Field<int32_t> aiEvents_{0}; // java-race: relaxed event counter
};

/** Player.java shell: a connection and see/notSee packets. */
class Player final : public VisibleObject {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Player> create(int32_t objectId, const SpawnPoint& spawn, std::shared_ptr<FakeConnection> connection) {
		return makeRef<Player>(objectId, spawn, std::move(connection));
	}

	std::shared_ptr<FakeConnection> getClientConnection() const noexcept { return clientConnection_.get(); }
	Kind kind() const noexcept override { return Kind::PLAYER; }

	void see(VisibleObject& object) override;
	void notSee(VisibleObject& object, ObjectDeleteAnimation animation) override;
	void notKnow(VisibleObject&) override {}

protected:
	Player(int32_t objectId, const SpawnPoint& spawn, std::shared_ptr<FakeConnection> connection);
	~Player() override = default;

private:
	Field<std::shared_ptr<FakeConnection>> clientConnection_;
};

/** Java `object instanceof Player player`: runtime::as<Player> (dynamic_cast), or the virtual kind query with --fast-instanceof */
inline Ptr<Player> asPlayer(const VisibleObject& object);
/** Java `object instanceof Npc npc` */
inline Ptr<Npc> asNpc(const VisibleObject& object);

// ---------------------------------------------------------------------------------------------------------------------- known lists

/** KnownObject.java (K4: `synchronized (this)` in updateVisible). */
class KnownObject final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<KnownObject> create(VisibleObject& object) { return makeRef<KnownObject>(object); }

	VisibleObject& get() const { return *object_; }
	bool isVisible() const noexcept { return visible_; }

	bool updateVisible(bool visible) {
		SYNCHRONIZED(*this) {
			if (visible_ != visible) {
				visible_ = visible;
				return true;
			}
		}
		return false;
	}

protected:
	explicit KnownObject(VisibleObject& object) : object_(object) { countEvent(Event::KNOWN_OBJECTS_CREATED); }
	~KnownObject() override { countEvent(Event::KNOWN_OBJECTS_DESTROYED); }

private:
	const Ref<VisibleObject> object_;
	Field<bool> visible_{false};
};

/** KnownList.java with the design §5.3 addPair handshake (a part of its owner). */
class KnownList : public OwnedPart {
public:
	explicit KnownList(VisibleObject& owner) : OwnedPart(owner), owner_(owner) {}
	~KnownList() override = default;

	/** KnownList.java:43-46 (synchronized) */
	virtual void update() {
		PhaseTimer timer(Phase::KNOWNLIST_UPDATE);
		countEvent(Event::KNOWNLIST_UPDATES);
		SYNCHRONIZED(*this) {
			forgetObjectsOrUpdateVisibility();
			findVisibleObjects();
		}
	}

	/** KnownList.java:51-57 (synchronized) */
	void clear(ObjectDeleteAnimation animation) {
		SYNCHRONIZED(*this) {
			for (Ptr<KnownObject> object : knownObjects_.values()) {
				VisibleObject& other = object->get();
				del(other, ObjectDeleteAnimation::NONE);
				other.getKnownList().del(owner_, animation);
			}
		}
	}

	bool knows(const VisibleObject& object) const { return knownObjects_.containsKey(object.getObjectId()); }
	int32_t size() const { return knownObjects_.size(); }
	bool isEmpty() const noexcept { return knownObjects_.isEmpty(); }

	/** design §5.3: the only pair add (L17). Dekker handshake with World.despawn (setIsSpawned(false) before clearKnownlist). */
	static bool addPair(VisibleObject& a, VisibleObject& b, PairPattern pattern) {
		PhaseTimer timer(Phase::ADD_PAIR);
		if (b.getKnownList().add(a) && a.getKnownList().add(b)) {
			if (!a.isSpawned() || !b.isSpawned()) {
				a.getKnownList().del(b, ObjectDeleteAnimation::NONE);
				b.getKnownList().del(a, ObjectDeleteAnimation::NONE);
				countEvent(Event::PAIR_ADD_HANDSHAKE_UNDONE);
				return false;
			}
			countEvent(pattern == PairPattern::REGION_SCAN ? Event::PAIR_ADD_REGION_SCAN
					: pattern == PairPattern::PLAYER_FLAGS ? Event::PAIR_ADD_PLAYER_FLAGS
																								 : Event::PAIR_ADD_FLAG_UPDATE);
			return true;
		}
		return false;
	}

	/** KnownList.java:239-244 (Java instanceof -> as<Player>) */
	template <class F>
	void forEachPlayer(F&& consumer) const {
		PhaseTimer timer(Phase::BROADCAST_ITERATE);
		for (Ptr<KnownObject> object : knownObjects_.values()) {
			if (Ptr<Player> player = asPlayer(object->get()))
				consumer(*player);
		}
	}
	template <class F>
	void forEachKnownObject(F&& consumer) const {
		for (Ptr<KnownObject> object : knownObjects_.values())
			consumer(*object);
	}

protected:
	friend class FlagKnownList;

	/** KnownList.java:74-82 */
	bool add(VisibleObject& object) {
		if (!isAwareOf(object))
			return false;
		Ref<KnownObject> created = KnownObject::create(object);
		KnownObject& knownObject = *created;
		if (knownObjects_.putIfAbsent(object.getObjectId(), std::move(created)))
			return false;
		updateVisibility(knownObject);
		return true;
	}

	/** KnownList.java:112-120 */
	void del(VisibleObject& object, ObjectDeleteAnimation animation) {
		Ptr<KnownObject> knownObject = knownObjects_.remove(object.getObjectId());
		if (knownObject) {
			if (knownObject->updateVisible(false))
				owner_.notSee(object, animation);
			owner_.notKnow(object);
		}
	}

	/** KnownList.java:93-104 (pet visibility omitted) */
	void updateVisibility(KnownObject& knownObject) {
		VisibleObject& object = knownObject.get();
		bool visible = owner_.canSee(object);
		if (!knownObject.updateVisible(visible))
			return;
		if (visible)
			owner_.see(object);
		else
			owner_.notSee(object, ObjectDeleteAnimation::FADE_OUT);
	}

	/** KnownList.java:149-158 */
	void forgetObjectsOrUpdateVisibility() {
		PhaseTimer timer(Phase::KNOWN_FORGET);
		for (Ptr<KnownObject> object : knownObjects_.values()) {
			VisibleObject& other = object->get();
			if (isInRange(other)) {
				updateVisibility(*object);
			} else {
				del(other, ObjectDeleteAnimation::NONE);
				other.getKnownList().del(owner_, ObjectDeleteAnimation::NONE);
				countEvent(Event::KNOWN_FORGET);
			}
		}
	}

	/** KnownList.java:163-195 with addPair (patterns 1 and 2) */
	virtual void findVisibleObjects();

	/** KnownList.java:200-202 (CreatureAwareKnownList: every shell is a Creature) */
	virtual bool isAwareOf(const VisibleObject& newObject) const { return &newObject != &owner_; }

	/** KnownList.java:207-209 */
	virtual float getVisibleDistance() const { return owner_.getVisibleDistance(); }

	/** KnownList.java:211-215 (PositionUtil.isInRange: same instance, 3D distance) */
	bool isInRange(const VisibleObject& newObject) const {
		float distance = std::max(getVisibleDistance(), newObject.getKnownList().getVisibleDistance());
		Ptr<WorldPosition> a = owner_.getPosition();
		Ptr<WorldPosition> b = newObject.getPosition();
		if (a->getMapId() != b->getMapId())
			return false;
		float dx = a->getX() - b->getX();
		float dy = a->getY() - b->getY();
		float dz = a->getZ() - b->getZ();
		if (distance == std::numeric_limits<float>::max())
			return true;
		return dx * dx + dy * dy + dz * dz <= distance * distance;
	}

	const OwnerRef<VisibleObject> owner_;
	Chm<int32_t, Ref<KnownObject>> knownObjects_{AION_LOCK_CLASS(KnownList::knownObjects#stripe)};
};

/** NpcKnownList.java */
class NpcKnownList final : public KnownList {
public:
	explicit NpcKnownList(VisibleObject& owner) : KnownList(owner) {}

	void update() override {
		if (owner_.getPosition()->isMapRegionActive())
			KnownList::update();
		else
			clear(ObjectDeleteAnimation::FADE_OUT);
	}
};

/** KnownList for players (PlayerKnownList without the player-specific filters). */
class PlayerKnownList final : public KnownList {
public:
	explicit PlayerKnownList(VisibleObject& owner) : KnownList(owner) {}
};

/** FlagKnownList.java (PlayerAwareKnownList) */
class FlagKnownList final : public KnownList {
public:
	explicit FlagKnownList(VisibleObject& owner) : KnownList(owner) {}

	/** FlagKnownList.java:15-22 (synchronized); removeIf ported as a two-sided removal (design §5.3, DEVIATION 12) */
	void update() override;

protected:
	bool isAwareOf(const VisibleObject& newObject) const override;
	float getVisibleDistance() const override { return std::numeric_limits<float>::max(); }
};

// ---------------------------------------------------------------------------------------------------------------------- movement

/** NpcMoveController.java shell (part of the Npc; walker route from the spawn template). */
class NpcMoveController final : public OwnedPart {
public:
	explicit NpcMoveController(Npc& owner);

	/** NpcMoveController.moveToDestination / moveToLocation, reduced to the POINT destination without geo */
	void moveToDestination();
	bool isDestinationReached() const {
		float dx = owner_.getX() - pointX_;
		float dy = owner_.getY() - pointY_;
		float dz = owner_.getZ() - pointZ_;
		return dx * dx + dy * dy + dz * dz <= MOVE_OFFSET * MOVE_OFFSET;
	}
	/** WalkManager.chooseNextRouteStep -> moveToPoint -> MoveTaskManager.addCreature */
	void chooseNextRouteStep();
	/** onDespawn -> abortMove -> MoveTaskManager.removeCreature */
	void abortMove();

	float getTargetX2() const noexcept { return targetDestX_; }
	float getTargetY2() const noexcept { return targetDestY_; }
	float getTargetZ2() const noexcept { return targetDestZ_; }
	int8_t getHeading() const noexcept { return heading_; }

private:
	const OwnerRef<Npc> owner_;
	Field<float> pointX_{0};
	Field<float> pointY_{0};
	Field<float> pointZ_{0};
	Field<float> targetDestX_{0};
	Field<float> targetDestY_{0};
	Field<float> targetDestZ_{0};
	Field<int8_t> heading_{0};
	Field<uint8_t> movementMask_{0};
	Field<int64_t> lastMoveUpdate_{0};
	Field<int32_t> routeStep_{-1};
};

/** MoveTaskManager.java (design §14.2h) */
class MoveTaskManager {
public:
	bool addCreature(Npc& creature) {
		if (!creature.isSpawned())
			return false;
		return !movingCreatures_.putIfAbsent(creature.getObjectId(), Ref<Npc>(creature));
	}
	bool removeCreature(Npc& creature) { return static_cast<bool>(movingCreatures_.remove(creature.getObjectId())); }
	int32_t size() const { return movingCreatures_.size(); }
	void clear() { movingCreatures_.clear(); }

	void run();

	/** per-run element timing (bench instrumentation): summed and maximum element durations of the last run */
	std::atomic<int64_t> elementNanosSum{0};
	std::atomic<int64_t> elementNanosMax{0};

private:
	Chm<int32_t, Ref<Npc>> movingCreatures_{AION_LOCK_CLASS(MoveTaskManager::movingCreatures#stripe)};
};

/** SM_MOVE.java */
class SM_MOVE final : public ServerPacket {
public:
	SM_MOVE(Npc& creature, uint8_t movementMask) : creature_(creature), movementMask_(movementMask) {}

protected:
	int32_t opCode() const noexcept override { return 0x00F2; }
	void writeImpl() const override;

private:
	const Ref<Npc> creature_;
	const uint8_t movementMask_;
};

/** SM_NPC_INFO-like spawn packet sent to a player that starts seeing an object */
class SM_NPC_INFO final : public ServerPacket {
public:
	explicit SM_NPC_INFO(VisibleObject& object) : object_(object) {}

protected:
	int32_t opCode() const noexcept override { return 0x00F4; }
	void writeImpl() const override {
		Ptr<WorldPosition> position = object_->getPosition();
		writeF(position->getX());
		writeF(position->getY());
		writeF(position->getZ());
		writeD(object_->getObjectId());
		writeD(object_->getObjectId() ^ 0x5A5A);
		writeC(static_cast<uint8_t>(position->getHeading()));
		for (int i = 0; i < 12; ++i) // appearance, stats, names...
			writeD(i);
	}

private:
	const Ref<VisibleObject> object_;
};

class SM_DELETE final : public ServerPacket {
public:
	SM_DELETE(VisibleObject& object, ObjectDeleteAnimation animation) : objectId_(object.getObjectId()), animation_(animation) {}

protected:
	int32_t opCode() const noexcept override { return 0x00F5; }
	void writeImpl() const override {
		writeD(objectId_);
		writeC(static_cast<uint8_t>(animation_));
	}

private:
	const int32_t objectId_;
	const ObjectDeleteAnimation animation_;
};

namespace PacketSendUtility {

/** design §14.2e sendTo */
inline void sendTo(Player& player, const ServerPacket& packet, SerializedPacket& once) {
	std::shared_ptr<FakeConnection> con = player.getClientConnection();
	if (!con)
		return;
	con->enqueue(once.getOrSerialize(packet));
	countEvent(Event::PACKETS_ENQUEUED);
}

inline void sendPacket(Player& player, const ServerPacket& packet) {
	std::shared_ptr<FakeConnection> con = player.getClientConnection();
	if (!con)
		return;
	con->enqueue(packet.serialize());
	countEvent(Event::PACKETS_ENQUEUED);
}

/** PacketSendUtility.broadcastPacket(VisibleObject, packet): serialized once, enqueued to every known player */
template <std::derived_from<ServerPacket> P>
void broadcastPacket(VisibleObject& object, const P& packet) {
	PhaseTimer timer(Phase::BROADCAST);
	SerializedPacket once;
	object.getKnownList().forEachPlayer([&](Player& player) { sendTo(player, packet, once); });
}

} // namespace PacketSendUtility

// ---------------------------------------------------------------------------------------------------------------------- world

/** --despawn-window-us stress knob (defined after Model; not applied during teardown) */
void despawnWindowStress();

/** World.java shell (spawn/despawn/updatePosition, allObjects). */
class World {
public:
	void storeObject(VisibleObject& object) { allObjects_.putIfAbsent(object.getObjectId(), Ref<VisibleObject>(object)); }

	/** World.removeObject: despawns spawned objects */
	void removeObject(VisibleObject& object) {
		if (allObjects_.remove(object.getObjectId())) {
			if (object.isSpawned())
				despawn(object, ObjectDeleteAnimation::FADE_OUT);
		}
	}

	/** World.java:283-298 */
	void spawn(VisibleObject& object) {
		Ptr<WorldPosition> position = object.getPosition();
		if (position->isSpawned())
			throw IllegalStateException(std::format("already spawned: {}", object.getObjectId()));
		position->setIsSpawned(true);
		Ptr<MapRegion> region = position->getMapRegion();
		region->getParent().addObject(object);
		region->add(object);
		object.updateKnownlist();
		countEvent(Event::SPAWNS);
	}

	/** World.java:312-327 */
	void despawn(VisibleObject& object, ObjectDeleteAnimation animation) {
		Ptr<WorldPosition> position = object.getPosition();
		auto always = finally([&] {
			Ptr<MapRegion> oldMapRegion = position->getMapRegion();
			position->setIsSpawned(false);
			despawnWindowStress(); // --despawn-window-us: concurrent scanners still find the object in its region
			if (oldMapRegion) {
				oldMapRegion->getParent().removeObject(object);
				oldMapRegion->remove(object);
			}
			object.clearKnownlist(animation);
			countEvent(Event::DESPAWNS);
		});
		object.onDespawn();
	}

	/** World.java:171-236 (no zones, no invalid-region handling beyond abortMove) */
	void updatePosition(VisibleObject& object, float newX, float newY, float newZ, int8_t newHeading, bool updateKnownList);

	int32_t objectCount() const { return allObjects_.size(); }
	template <class F>
	void forEachObject(F&& consumer) const {
		for (Ptr<VisibleObject> object : allObjects_.values())
			consumer(*object);
	}

private:
	Chm<int32_t, Ref<VisibleObject>> allObjects_{AION_LOCK_CLASS(World::allObjects#stripe)};
};

// ---------------------------------------------------------------------------------------------------------------------- scenario driver

enum class TickPhase : uint8_t { WARMUP, MEASURE, COUNT, TAIL };

struct TickRecord {
	TickPhase phase = TickPhase::WARMUP;
	int64_t startNanos = 0;
	int64_t durationNanos = 0;
	int32_t movers = 0;
	int64_t reclaimLagMillis = 0;
	uint64_t reclaimBacklog = 0;
	uint64_t reclaimBacklogBytes = 0;
	int64_t snapshotNanos = 0;
	int64_t elementNanosSum = 0;
	int64_t elementNanosMax = 0;
};

using EventSnapshot = std::array<uint64_t, static_cast<size_t>(Event::COUNT)>;

inline EventSnapshot snapshotEvents() {
	EventSnapshot snapshot{};
	for (size_t i = 0; i < snapshot.size(); ++i)
		snapshot[i] = eventTotal(static_cast<Event>(i));
	return snapshot;
}

/** Scenario state. The vectors of Refs are confined: players/flags are written only while no task runs; npcSlots only by the churn task. */
class Model {
public:
	explicit Model(const Options& scenarioOptions) : options(scenarioOptions), rngChurn(scenarioOptions.seed * 31 + 7), rngPlayers(scenarioOptions.seed * 17 + 3) {}

	const Options options;
	World world;
	MoveTaskManager moveTaskManager;
	std::vector<Ref<WorldMapInstance>> maps;
	std::vector<std::unique_ptr<SpawnPoint>> spawnPoints; // immortal templates, destroyed after every shell was reclaimed
	std::vector<std::shared_ptr<FakeConnection>> connections;
	std::vector<Ref<Player>> players;
	std::vector<Ref<Npc>> npcSlots; // slot i was spawned from spawnPoints[i]; slots [0, movers) are movers
	std::vector<Ref<Npc>> flags;
	struct Cluster {
		int32_t mapIndex;
		float x;
		float y;
	};
	std::vector<Cluster> clusters;

	std::atomic<int32_t> nextObjectId{1};
	std::atomic<bool> activationInline{true};
	std::atomic<bool> stopping{false};
	std::atomic<int32_t> inflight{0};
	MillisHistogram destroyLag;
	std::chrono::steady_clock::time_point epoch = std::chrono::steady_clock::now();

	int64_t nowMillis() const noexcept {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - epoch).count();
	}

	// ---- tick records (written by the tick task, read by main after stopping)
	std::mutex tickMutex;
	std::vector<TickRecord> ticks;
	std::optional<AllocationTotals> allocationsAtMeasureStart;
	std::optional<AllocationTotals> allocationsAtMeasureEnd;
	std::optional<EventSnapshot> eventsAtMeasureStart;
	std::optional<EventSnapshot> eventsAtMeasureEnd;
	int64_t measureStartNanos = 0;
	int64_t measureEndNanos = 0;
	int64_t countStartNanos = 0;
	int64_t countEndNanos = 0;
	std::atomic<int32_t> ticksDone{0};

	// ---- task-confined state (fixed-rate tasks never overlap themselves)
	std::mt19937_64 rngChurn;
	double churnBudget = 0;
	double oneTimeObserverBudget = 0;
	std::mt19937_64 rngPlayers;
	size_t playerCursor = 0;
	double relogBudget = 0;

	/** RAII in-flight counter for quiescence detection */
	struct Inflight {
		explicit Inflight(Model& m) noexcept : model(m) { model.inflight.fetch_add(1, std::memory_order_acq_rel); }
		~Inflight() { model.inflight.fetch_sub(1, std::memory_order_acq_rel); }
		Model& model;
	};

	Ref<Npc> createNpc(const SpawnPoint& spawn, std::mt19937_64& rng);
	void movementTick();
	void churnTick();
	void playerTick();
	void flagTick();
	void drainConnections();
};

// ====================================================================================================================== definitions

inline Ptr<Player> asPlayer(const VisibleObject& object) {
	if (model->options.fastInstanceof)
		return object.kind() == VisibleObject::Kind::PLAYER ? Ptr<Player>(static_cast<Player&>(const_cast<VisibleObject&>(object))) : Ptr<Player>();
	return as<Player>(const_cast<VisibleObject&>(object));
}

inline Ptr<Npc> asNpc(const VisibleObject& object) {
	if (model->options.fastInstanceof)
		return object.kind() == VisibleObject::Kind::NPC ? Ptr<Npc>(static_cast<Npc&>(const_cast<VisibleObject&>(object))) : Ptr<Npc>();
	return as<Npc>(const_cast<VisibleObject&>(object));
}

void WorldPosition::setMapRegion(Ptr<MapRegion> region) {
	mapRegion_ = region;
}

WorldPosition::WorldPosition(int32_t mapId, float x, float y, float z, int8_t heading, MapRegion& region)
	: mapId_(mapId), x_(x), y_(y), z_(z), heading_(heading) {
	mapRegion_ = Ptr<MapRegion>(region);
}

WorldMapInstance& WorldPosition::getWorldMapInstance() const {
	return getMapRegion()->getParent();
}

bool WorldPosition::isMapRegionActive() const {
	Ptr<MapRegion> region = getMapRegion();
	return region && region->isActive();
}

MapRegion::MapRegion(WorldMapInstance& parent, int32_t regionId) : OwnedPart(parent), regionId_(regionId), parent_(parent) {}

/** MapRegion.java:66-69 */
void MapRegion::add(VisibleObject& object) {
	if (!objects_.put(object.getObjectId(), Ref<VisibleObject>(object)) && asPlayer(object) && incrementPlayerCount() == 1)
		activate();
}

/** MapRegion.java:71-74 (deactivation not modelled) */
void MapRegion::remove(VisibleObject& object) {
	Ptr<VisibleObject> removed = objects_.remove(object.getObjectId());
	if (removed && asPlayer(*removed))
		(void)decrementPlayerCount();
}

/** MapRegion.java:99-108: activated regions notify their creatures on the instant pool (ActivateEventHandler -> updateKnownlist) */
void MapRegion::activate() {
	std::vector<Ref<MapRegion>> activatedRegions;
	for (MapRegion* region : *getNeighbours()) {
		if (region->setRegionState(true))
			activatedRegions.push_back(Ref<MapRegion>(region));
	}
	if (activatedRegions.empty())
		return;
	countEvent(Event::REGION_ACTIVATIONS, activatedRegions.size());
	if (model->activationInline.load(std::memory_order_acquire)) {
		for (const Ref<MapRegion>& region : activatedRegions)
			region->notifyCreatures();
		return;
	}
	model->inflight.fetch_add(1, std::memory_order_acq_rel);
	ThreadPoolManager::getInstance().execute(Pin(&parent_), [regions = std::move(activatedRegions)] {
		auto done = finally([] { model->inflight.fetch_sub(1, std::memory_order_acq_rel); });
		for (const Ref<MapRegion>& region : regions)
			region->notifyCreatures();
	});
}

void MapRegion::notifyCreatures() const {
	for (Ptr<VisibleObject> object : objects_.values()) {
		if (asNpc(*object))
			object->updateKnownlist();
	}
}

WorldMapInstance::WorldMapInstance(int32_t mapId, int32_t worldSize)
	: mapId_(mapId), worldSize_(worldSize), regionsPerSide_(worldSize / REGION_SIZE + 1) {
	// WorldMap2DInstance.initMapRegions
	flatRegions_.resize(static_cast<size_t>(regionsPerSide_) * static_cast<size_t>(regionsPerSide_));
	for (int32_t ix = 0; ix < regionsPerSide_; ++ix) {
		for (int32_t iy = 0; iy < regionsPerSide_; ++iy) {
			int32_t regionId = ix * 1000 + iy; // RegionUtil.get2DRegionId
			auto region = std::make_unique<MapRegion>(*this, regionId);
			flatRegions_[static_cast<size_t>(ix * regionsPerSide_ + iy)] = region.get();
			regions_.put(regionId, std::move(region));
		}
	}
	for (int32_t ix = 0; ix < regionsPerSide_; ++ix) {
		for (int32_t iy = 0; iy < regionsPerSide_; ++iy) {
			std::vector<MapRegion*> neighbours{flatRegions_[static_cast<size_t>(ix * regionsPerSide_ + iy)]};
			for (int32_t nx = ix - 1; nx <= ix + 1; ++nx) {
				for (int32_t ny = iy - 1; ny <= iy + 1; ++ny) {
					if ((nx == ix && ny == iy) || nx < 0 || ny < 0 || nx >= regionsPerSide_ || ny >= regionsPerSide_)
						continue;
					neighbours.push_back(flatRegions_[static_cast<size_t>(nx * regionsPerSide_ + ny)]);
				}
			}
			Ref<Array<MapRegion*>> array = Array<MapRegion*>::make(static_cast<int32_t>(neighbours.size()));
			for (size_t i = 0; i < neighbours.size(); ++i)
				(*array)[static_cast<int32_t>(i)] = neighbours[i];
			neighbours.front()->setNeighbours(std::move(array));
		}
	}
}

Ptr<MapRegion> WorldMapInstance::getRegion(float x, float y) const {
	PhaseTimer timer(Phase::REGION_LOOKUP);
	if (x < 0 || y < 0 || x > static_cast<float>(worldSize_) || y > static_cast<float>(worldSize_))
		return nullptr;
	int32_t ix = static_cast<int32_t>(x) / REGION_SIZE;
	int32_t iy = static_cast<int32_t>(y) / REGION_SIZE;
	if (model->options.flatRegions)
		return Ptr<MapRegion>(*flatRegions_[static_cast<size_t>(ix * regionsPerSide_ + iy)]);
	return regions_.get(ix * 1000 + iy);
}

void WorldMapInstance::addObject(VisibleObject& object) {
	if (worldMapObjects_.put(object.getObjectId(), Ref<VisibleObject>(object)))
		return;
	if (Ptr<Npc> npc = asNpc(object))
		worldMapNpcs_.put(object.getObjectId(), Ref<Npc>(npc));
	else if (Ptr<Player> player = asPlayer(object))
		worldMapPlayers_.put(object.getObjectId(), Ref<Player>(player));
}

void WorldMapInstance::removeObject(VisibleObject& object) {
	if (!worldMapObjects_.remove(object.getObjectId()))
		return;
	if (asNpc(object))
		worldMapNpcs_.remove(object.getObjectId());
	else if (asPlayer(object))
		worldMapPlayers_.remove(object.getObjectId());
}

VisibleObject::~VisibleObject() {
	int64_t removed = removedNanos_.get();
	if (removed != 0)
		model->destroyLag.add(nowNanos() - removed);
	countEvent(Event::SHELLS_DESTROYED);
}

void VisibleObject::updateKnownlist() {
	getKnownList().update();
}

void VisibleObject::clearKnownlist(ObjectDeleteAnimation animation) {
	getKnownList().clear(animation);
}

Npc::Npc(int32_t objectId, const SpawnPoint& spawn) : VisibleObject(objectId, spawn) {
	// Npc.java:70-71 setKnownList(new NpcKnownList(this)) / setMoveController(new NpcMoveController(this)): design §3.2.1 pattern 2
	if (spawn.flag)
		knownList_.set(std::make_unique<FlagKnownList>(*this));
	else
		knownList_.set(std::make_unique<NpcKnownList>(*this));
	moveController_.set(std::make_unique<NpcMoveController>(*this));
}

Npc::~Npc() = default;

void Npc::onDespawn() {
	getMoveController().abortMove();
}

Player::Player(int32_t objectId, const SpawnPoint& spawn, std::shared_ptr<FakeConnection> connection) : VisibleObject(objectId, spawn) {
	knownList_.set(std::make_unique<PlayerKnownList>(*this));
	clientConnection_ = std::move(connection);
}

void Player::see(VisibleObject& object) {
	PhaseTimer timer(Phase::SEE_PACKET);
	PacketSendUtility::sendPacket(*this, SM_NPC_INFO(object));
}

void Player::notSee(VisibleObject& object, ObjectDeleteAnimation animation) {
	PhaseTimer timer(Phase::SEE_PACKET);
	PacketSendUtility::sendPacket(*this, SM_DELETE(object, animation));
}

void KnownList::findVisibleObjects() {
	if (!owner_.isSpawned())
		return;
	Ptr<WorldPosition> position = owner_.getPosition();
	if (asPlayer(owner_)) {
		PhaseTimer timer(Phase::PLAYER_FLAG_SCAN);
		position->getWorldMapInstance().forEachNpc([this](Npc& npc) {
			if (npc.isFlag())
				addPair(owner_, npc, PairPattern::PLAYER_FLAGS);
		});
	}
	Ptr<MapRegion> region = position->getMapRegion();
	PhaseTimer timer(Phase::REGION_SCAN);
	for (MapRegion* neighbour : *region->getNeighbours()) {
		for (Ptr<VisibleObject> newObject : neighbour->getObjects().values()) {
			if (!isAwareOf(*newObject))
				continue;
			if (knows(*newObject))
				continue;
			if (!isInRange(*newObject))
				continue;
			addPair(owner_, *newObject, PairPattern::REGION_SCAN);
		}
	}
}

void FlagKnownList::update() {
	countEvent(Event::KNOWNLIST_UPDATES);
	SYNCHRONIZED(*this) {
		WorldMapInstance& worldMapInstance = owner_.getPosition()->getWorldMapInstance();
		for (Ptr<KnownObject> knownObject : knownObjects_.values()) {
			VisibleObject& other = knownObject->get();
			if (&other.getPosition()->getWorldMapInstance() != &worldMapInstance) {
				del(other, ObjectDeleteAnimation::NONE);
				other.getKnownList().del(owner_, ObjectDeleteAnimation::NONE);
			}
		}
		worldMapInstance.forEachPlayer([this](Player& player) { addPair(owner_, player, PairPattern::FLAG_UPDATE); });
	}
}

bool FlagKnownList::isAwareOf(const VisibleObject& newObject) const {
	return KnownList::isAwareOf(newObject) && asPlayer(newObject); // PlayerAwareKnownList: instanceof Player
}

NpcMoveController::NpcMoveController(Npc& owner) : OwnedPart(owner), owner_(owner), routeStep_(owner.getSpawn().firstRouteStep - 1) {}

void NpcMoveController::moveToDestination() {
	countEvent(Event::MOVE_STEPS);
	float targetX = pointX_;
	float targetY = pointY_;
	float targetZ = pointZ_;
	float ownerX = owner_.getX();
	float ownerY = owner_.getY();
	float ownerZ = owner_.getZ();

	bool destinationChanged = targetX != targetDestX_ || targetY != targetDestY_ || targetZ != targetDestZ_;
	if (targetX != targetDestX_ || targetY != targetDestY_) {
		double angle = std::atan2(targetY - ownerY, targetX - ownerX) * 180.0 / 3.141592653589793;
		heading_ = static_cast<int8_t>(static_cast<int32_t>((angle < 0 ? angle + 360.0 : angle) / 3.0) & 0x7F);
	}
	targetDestX_ = targetX;
	targetDestY_ = targetY;
	targetDestZ_ = targetZ;

	int64_t now = model->nowMillis();
	float currentSpeed = owner_.getSpawn().speed;
	float futureDistPassed = currentSpeed * static_cast<float>(now - lastMoveUpdate_) / 1000.0f;
	float dx = targetX - ownerX;
	float dy = targetY - ownerY;
	float dz = targetZ - ownerZ;
	float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (dist == 0) {
		lastMoveUpdate_ = now;
		return;
	}
	if (futureDistPassed > dist)
		futureDistPassed = dist;
	float distFraction = futureDistPassed / dist;
	float newX = dx * distFraction + ownerX;
	float newY = dy * distFraction + ownerY;
	float newZ = dz * distFraction + ownerZ;
	model->world.updatePosition(owner_, newX, newY, newZ, heading_, false);

	uint8_t newMask = destinationChanged ? MovementMask::NPC_STARTMOVE : MovementMask::NPC_WALK_SLOW;
	if (movementMask_ != newMask || destinationChanged || model->options.broadcastEveryTick) {
		movementMask_ = newMask;
		PacketSendUtility::broadcastPacket(owner_, SM_MOVE(owner_, newMask));
	}
	lastMoveUpdate_ = now;
}

void NpcMoveController::chooseNextRouteStep() {
	const std::vector<std::array<float, 3>>& route = owner_.getSpawn().route;
	if (route.empty())
		return;
	int32_t next = (routeStep_ + 1) % static_cast<int32_t>(route.size());
	routeStep_ = next;
	const std::array<float, 3>& point = route[static_cast<size_t>(next)];
	pointX_ = point[0];
	pointY_ = point[1];
	pointZ_ = point[2];
	lastMoveUpdate_ = model->nowMillis();
	model->moveTaskManager.addCreature(owner_);
}

void NpcMoveController::abortMove() {
	model->moveTaskManager.removeCreature(owner_);
}

void SM_MOVE::writeImpl() const {
	NpcMoveController& mc = creature_->getMoveController();
	Ptr<WorldPosition> position = creature_->getPosition();
	writeD(creature_->getObjectId());
	writeF(position->getX());
	writeF(position->getY());
	writeF(position->getZ());
	writeC(static_cast<uint8_t>(position->getHeading()));
	writeC(movementMask_);
	if ((movementMask_ & MovementMask::POSITION) == MovementMask::POSITION && (movementMask_ & MovementMask::MANUAL) == MovementMask::MANUAL) {
		writeF(mc.getTargetX2());
		writeF(mc.getTargetY2());
		writeF(mc.getTargetZ2());
	}
}

void World::updatePosition(VisibleObject& object, float newX, float newY, float newZ, int8_t newHeading, bool updateKnownList) {
	PhaseTimer timer(Phase::UPDATE_POSITION);
	if (!object.isSpawned())
		return;
	Ptr<WorldPosition> position = object.getPosition();
	Ptr<MapRegion> oldRegion = position->getMapRegion();
	if (!oldRegion)
		return;
	Ptr<MapRegion> newRegion = oldRegion->getParent().getRegion(newX, newY);
	if (!newRegion) {
		if (Ptr<Npc> npc = asNpc(object))
			npc->getMoveController().abortMove();
		return;
	}
	position->setXYZH(newX, newY, newZ, newHeading);
	if (newRegion != oldRegion) {
		oldRegion->remove(object);
		newRegion->add(object);
		position->setMapRegion(newRegion);
		countEvent(Event::REGION_CHANGES);
	}
	if (updateKnownList)
		object.updateKnownlist();
}

/** MoveTaskManager.java:43-58 / design §14.2h */
void MoveTaskManager::run() {
	elementNanosSum.store(0, std::memory_order_relaxed);
	elementNanosMax.store(0, std::memory_order_relaxed);
	std::vector<Ptr<Npc>> creatures = movingCreatures_.values();
	ForkJoinPool::commonPool().parallelForEach(creatures, [this](Ptr<Npc> creature) {
		PhaseTimer elementTimer(Phase::MOVE_ELEMENT);
		int64_t elementStart = nowNanos();
		auto timing = finally([this, elementStart] {
			int64_t elapsed = nowNanos() - elementStart;
			elementNanosSum.fetch_add(elapsed, std::memory_order_relaxed);
			int64_t currentMax = elementNanosMax.load(std::memory_order_relaxed);
			while (elapsed > currentMax && !elementNanosMax.compare_exchange_weak(currentMax, elapsed, std::memory_order_relaxed)) {
			}
		});
		try {
			if (!creature->isSpawned()) {
				if (removeCreature(*creature))
					countEvent(Event::MOVER_DESPAWNED_IN_TICK);
				return;
			}
			NpcMoveController& moveController = creature->getMoveController();
			moveController.moveToDestination();
			if (moveController.isDestinationReached()) {
				removeCreature(*creature);
				// AIEventType.MOVE_ARRIVED -> WalkManager.targetReached (WALK_PATH): updateKnownlist, chooseNextRouteStep (walker re-add)
				creature->updateKnownlist();
				moveController.chooseNextRouteStep();
				countEvent(Event::WALKER_READDS);
			} else {
				// AIEventType.MOVE_VALIDATE -> MoveEventHandler -> CreatureController.onMove -> notifyMoveObservers
				creature->getObserveController().notifyMoveObservers();
			}
		} catch (const std::exception&) {
			countEvent(Event::TASK_EXCEPTIONS);
		}
	});
}

// ---------------------------------------------------------------------------------------------------------------------- model tasks

void despawnWindowStress() {
	if (model->options.despawnWindowMicros <= 0 || model->stopping.load(std::memory_order_relaxed))
		return;
	const int64_t end = nowNanos() + int64_t{model->options.despawnWindowMicros} * 1000; // spin: OS sleeps round up to the timer resolution
	while (nowNanos() < end)
		std::this_thread::yield();
}

Ref<Npc> Model::createNpc(const SpawnPoint& spawn, std::mt19937_64& rng) {
	Ref<Npc> npc = Npc::create(nextObjectId.fetch_add(1, std::memory_order_acq_rel), spawn);
	WorldMapInstance& instance = *maps[static_cast<size_t>(spawn.mapIndex)];
	Ptr<MapRegion> region = instance.getRegion(spawn.x, spawn.y);
	npc->setPosition(WorldPosition::create(instance.getMapId(), spawn.x, spawn.y, spawn.z, 0, *region));
	if (spawn.mover && std::uniform_real_distribution<double>(0, 1)(rng) < options.observerRatio)
		npc->getObserveController().addObserver(ActionObserver::create(ObserverType::MOVE));
	return npc;
}

/** Tick sequence: warmupTicks (WARMUP), ticks (MEASURE), then countTicks with PCT counting hooks (COUNT, checked builds) or one TAIL tick. */
int32_t totalTickCount(const Options& o) {
	return o.warmupTicks + o.ticks + (SiteCounting::available() && o.countTicks > 0 ? o.countTicks : 1);
}

void Model::movementTick() {
	Inflight guard(*this);
	if (stopping.load(std::memory_order_acquire))
		return;
	const int32_t index = ticksDone.load(std::memory_order_acquire);
	const int32_t measureBegin = options.warmupTicks;
	const int32_t measureEnd = measureBegin + options.ticks;
	const bool countingAvailable = SiteCounting::available() && options.countTicks > 0;
	const int32_t total = totalTickCount(options);
	if (index >= total)
		return;
	TickPhase phase = index < measureBegin ? TickPhase::WARMUP
		: index < measureEnd              ? TickPhase::MEASURE
		: countingAvailable               ? TickPhase::COUNT
																			: TickPhase::TAIL;

	int64_t start = nowNanos();
	{
		std::scoped_lock lock(tickMutex);
		if (index == measureBegin) {
			allocationsAtMeasureStart = allocationTotals();
			eventsAtMeasureStart = snapshotEvents();
			measureStartNanos = start;
		}
		if (index == measureEnd) {
			allocationsAtMeasureEnd = allocationTotals();
			eventsAtMeasureEnd = snapshotEvents();
			measureEndNanos = start;
			if (phase == TickPhase::COUNT) {
				countStartNanos = start;
				SiteCounting::begin();
			}
		}
	}

	int32_t movers = moveTaskManager.size();
	moveTaskManager.run();
	int64_t duration = nowNanos() - start;
	int64_t elementSum = moveTaskManager.elementNanosSum.load(std::memory_order_relaxed);
	int64_t elementMax = moveTaskManager.elementNanosMax.load(std::memory_order_relaxed);

	Reclaimer::Stats reclaim = Reclaimer::getInstance().stats();
	std::scoped_lock lock(tickMutex);
	ticks.push_back({phase, start, duration, movers, reclaim.lag.count(), reclaim.backlog, reclaim.backlogBytes, 0, elementSum, elementMax});
	if (phase == TickPhase::COUNT && index + 1 == total) {
		SiteCounting::end();
		countEndNanos = nowNanos();
	}
	ticksDone.store(index + 1, std::memory_order_release);
}

void Model::churnTick() {
	Inflight guard(*this);
	if (stopping.load(std::memory_order_acquire) || npcSlots.empty())
		return;
	churnBudget += static_cast<double>(options.churnPerSecond) * static_cast<double>(options.periodMillis) / 1000.0;
	int32_t respawns = static_cast<int32_t>(churnBudget);
	churnBudget -= respawns;
	std::uniform_int_distribution<size_t> anySlot(0, npcSlots.size() - 1);
	std::uniform_int_distribution<size_t> moverSlot(0, static_cast<size_t>(std::max(options.movers, 1) - 1));
	for (int32_t i = 0; i < respawns; ++i) {
		size_t slot = (options.movers > 0 && (rngChurn() & 1) != 0) ? moverSlot(rngChurn) : anySlot(rngChurn);
		Ref<Npc> old = std::move(npcSlots[slot]);
		if (old) {
			old->markRemoved();
			world.removeObject(*old); // despawn (abortMove, region/instance removal, clearKnownlist)
			old.reset();
		}
		const SpawnPoint& spawn = *spawnPoints[slot];
		Ref<Npc> npc = createNpc(spawn, rngChurn);
		world.storeObject(*npc);
		world.spawn(*npc);
		if (spawn.mover)
			npc->getMoveController().chooseNextRouteStep();
		npcSlots[slot] = std::move(npc);
	}

	oneTimeObserverBudget += static_cast<double>(options.oneTimeObserversPerSecond) * static_cast<double>(options.periodMillis) / 1000.0;
	int32_t observers = static_cast<int32_t>(oneTimeObserverBudget);
	oneTimeObserverBudget -= observers;
	for (int32_t i = 0; i < observers && options.movers > 0; ++i) {
		if (Ref<Npc>& npc = npcSlots[moverSlot(rngChurn)])
			npc->getObserveController().attach(ActionObserver::create(ObserverType::MOVE));
	}
}

void Model::playerTick() {
	Inflight guard(*this);
	if (stopping.load(std::memory_order_acquire) || players.empty())
		return;
	std::uniform_real_distribution<float> step(-2.5f, 2.5f);
	size_t updates = static_cast<size_t>(std::ceil(static_cast<double>(players.size()) * options.playerUpdateFraction));
	for (size_t i = 0; i < players.size(); ++i) {
		Player& player = *players[i];
		const SpawnPoint& home = player.getSpawn();
		float x = std::clamp(player.getX() + step(rngPlayers), home.x - PLAYER_CLUSTER_RADIUS, home.x + PLAYER_CLUSTER_RADIUS);
		float y = std::clamp(player.getY() + step(rngPlayers), home.y - PLAYER_CLUSTER_RADIUS, home.y + PLAYER_CLUSTER_RADIUS);
		world.updatePosition(player, x, y, player.getZ(), 0, false);
	}
	for (size_t i = 0; i < updates; ++i) {
		Player& player = *players[(playerCursor + i) % players.size()];
		player.updateKnownlist();
	}
	playerCursor = (playerCursor + updates) % players.size();

	// relogs: a new Player object for the same character (leaveWorld despawn + enterWorld spawn): pair-add patterns 2 and 3 keep running
	relogBudget += static_cast<double>(options.playerRelogsPerSecond) * static_cast<double>(options.periodMillis) / 1000.0;
	int32_t relogs = static_cast<int32_t>(relogBudget);
	relogBudget -= relogs;
	std::uniform_int_distribution<size_t> anyPlayer(0, players.size() - 1);
	for (int32_t i = 0; i < relogs; ++i) {
		size_t index = anyPlayer(rngPlayers);
		Ref<Player> old = std::move(players[index]);
		const SpawnPoint& spawn = old->getSpawn();
		std::shared_ptr<FakeConnection> connection = old->getClientConnection();
		old->markRemoved();
		world.removeObject(*old);
		old.reset();
		Ref<Player> player = Player::create(nextObjectId.fetch_add(1, std::memory_order_acq_rel), spawn, std::move(connection));
		WorldMapInstance& instance = *maps[static_cast<size_t>(spawn.mapIndex)];
		player->setPosition(WorldPosition::create(instance.getMapId(), spawn.x, spawn.y, spawn.z, 0, *instance.getRegion(spawn.x, spawn.y)));
		world.storeObject(*player);
		world.spawn(*player);
		players[index] = std::move(player);
	}
}

void Model::flagTick() {
	Inflight guard(*this);
	if (stopping.load(std::memory_order_acquire))
		return;
	for (const Ref<Npc>& flag : flags)
		flag->updateKnownlist();
}

void Model::drainConnections() {
	Inflight guard(*this);
	for (const std::shared_ptr<FakeConnection>& connection : connections)
		connection->drain();
}

// ---------------------------------------------------------------------------------------------------------------------- scenario

namespace {

std::string fmt(double value, int precision = 2) {
	return std::format("{:.{}f}", value, precision);
}

std::string bytes(double value) {
	if (value >= 1024.0 * 1024.0)
		return std::format("{:.1f} MB", value / (1024.0 * 1024.0));
	if (value >= 1024.0)
		return std::format("{:.1f} KB", value / 1024.0);
	return std::format("{:.0f} B", value);
}

int64_t settledLiveBytes() {
	Reclaimer::getInstance().drain(256);
	return allocationTotals().liveBytes();
}

/** Waits until no model task runs for `stableFor`. */
bool waitQuiescent(Model& m, std::chrono::milliseconds stableFor, std::chrono::seconds timeout) {
	auto deadline = std::chrono::steady_clock::now() + timeout;
	auto quietSince = std::chrono::steady_clock::now();
	while (std::chrono::steady_clock::now() < deadline) {
		if (m.inflight.load(std::memory_order_acquire) != 0)
			quietSince = std::chrono::steady_clock::now();
		else if (std::chrono::steady_clock::now() - quietSince >= stableFor)
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return false;
}

struct GhostCounts {
	uint64_t objects = 0;
	uint64_t spawned = 0;
	uint64_t knownEntries = 0;
	uint64_t knowsDespawned = 0;
	uint64_t despawnedWithEntries = 0;
	uint64_t asymmetricPairs = 0;
	uint64_t regionGhosts = 0;
	uint64_t regionMissing = 0;
	uint64_t moversNotSpawned = 0;
};

GhostCounts verifyKnownLists(Model& m) {
	TaskScope scope(AION_TASK_INFO(TaskKind::MAIN));
	GhostCounts counts;
	m.world.forEachObject([&](VisibleObject& a) {
		++counts.objects;
		bool spawned = a.isSpawned();
		counts.spawned += spawned ? 1 : 0;
		if (!spawned) {
			if (!a.getKnownList().isEmpty())
				++counts.despawnedWithEntries;
			return;
		}
		a.getKnownList().forEachKnownObject([&](KnownObject& knownObject) {
			++counts.knownEntries;
			VisibleObject& b = knownObject.get();
			if (!b.isSpawned())
				++counts.knowsDespawned;
			else if (!b.getKnownList().knows(a))
				++counts.asymmetricPairs;
		});
		Ptr<MapRegion> region = a.getPosition()->getMapRegion();
		if (!region->getObjects().containsKey(a.getObjectId()))
			++counts.regionMissing;
	});
	for (const Ref<WorldMapInstance>& instance : m.maps) {
		instance->forEachRegion([&](MapRegion& region) {
			for (Ptr<VisibleObject> object : region.getObjects().values()) {
				if (!object->isSpawned() || object->getPosition()->getMapRegion() != Ptr<MapRegion>(region))
					++counts.regionGhosts;
			}
		});
	}
	return counts;
}

void buildLayout(Model& m) {
	const Options& o = m.options;
	std::mt19937_64 rng(o.seed);
	auto uniform = [&rng](float low, float high) { return std::uniform_real_distribution<float>(low, high)(rng); };
	const float size = static_cast<float>(o.mapSize);
	const float margin = std::min(300.0f, size / 4);

	int32_t clusterCount = std::max(1, (o.players + o.playersPerCluster - 1) / std::max(1, o.playersPerCluster));
	for (int32_t c = 0; c < clusterCount; ++c)
		m.clusters.push_back({c % o.maps, uniform(margin, size - margin), uniform(margin, size - margin)});

	auto disc = [&](float cx, float cy, float radius) {
		double angle = std::uniform_real_distribution<double>(0, 6.283185307179586)(rng);
		double distance = radius * std::sqrt(std::uniform_real_distribution<double>(0, 1)(rng));
		float x = std::clamp(cx + static_cast<float>(std::cos(angle) * distance), 1.0f, size - 1);
		float y = std::clamp(cy + static_cast<float>(std::sin(angle) * distance), 1.0f, size - 1);
		return std::pair<float, float>(x, y);
	};

	// npc slots: [0, movers) walkers near clusters, then uniform npcs
	for (int32_t i = 0; i < o.npcs; ++i) {
		auto spawn = std::make_unique<SpawnPoint>();
		if (i < o.movers) {
			const Model::Cluster& cluster = m.clusters[static_cast<size_t>(i % clusterCount)];
			auto [x, y] = disc(cluster.x, cluster.y, o.moverSpawnRadius);
			spawn->mapIndex = cluster.mapIndex;
			spawn->x = x;
			spawn->y = y;
			spawn->z = uniform(0, 40);
			spawn->mover = true;
			spawn->speed = o.synchronizedWalkers ? o.moverSpeed : o.moverSpeed * uniform(0.7f, 1.3f);
			float r = o.synchronizedWalkers ? o.routeRadius : o.routeRadius * uniform(0.5f, 1.5f);
			spawn->firstRouteStep = o.synchronizedWalkers ? 0 : static_cast<int32_t>(rng() % 4);
			for (auto [ox, oy] : {std::pair{r, 0.0f}, std::pair{0.0f, r}, std::pair{-r, 0.0f}, std::pair{0.0f, -r}})
				spawn->route.push_back({std::clamp(x + ox, 1.0f, size - 1), std::clamp(y + oy, 1.0f, size - 1), spawn->z + uniform(-1, 1)});
		} else {
			spawn->mapIndex = i % o.maps;
			spawn->x = uniform(1, size - 1);
			spawn->y = uniform(1, size - 1);
			spawn->z = uniform(0, 40);
		}
		m.spawnPoints.push_back(std::move(spawn));
	}
	// flags
	for (int32_t map = 0; map < o.maps; ++map) {
		for (int32_t f = 0; f < o.flagsPerMap; ++f) {
			auto spawn = std::make_unique<SpawnPoint>();
			spawn->mapIndex = map;
			spawn->x = uniform(1, size - 1);
			spawn->y = uniform(1, size - 1);
			spawn->flag = true;
			m.spawnPoints.push_back(std::move(spawn));
		}
	}
	// players
	for (int32_t p = 0; p < o.players; ++p) {
		const Model::Cluster& cluster = m.clusters[static_cast<size_t>(p % clusterCount)];
		auto spawn = std::make_unique<SpawnPoint>();
		auto [x, y] = disc(cluster.x, cluster.y, PLAYER_CLUSTER_RADIUS);
		spawn->mapIndex = cluster.mapIndex;
		spawn->x = x;
		spawn->y = y;
		spawn->z = uniform(0, 40);
		m.spawnPoints.push_back(std::move(spawn));
	}
}

} // namespace

Results runScenario(const Options& options) {
	Results results;
	auto model_ = std::make_unique<Model>(options);
	Model& m = *model_;
	model = &m;

	std::vector<NamedValue> setupRows;
	auto setup = [&](std::string name, std::string value) { setupRows.push_back({std::move(name), std::move(value)}); };

	utils::ThreadPoolManager::Config poolConfig;
	poolConfig.serialMovement = options.serialMovement;
	ThreadPoolManager::configure(poolConfig);
	ThreadPoolManager& pools = ThreadPoolManager::getInstance();
	ForkJoinPool::commonPool().setSerial(options.serialMovement);
	Reclaimer::Config reclaimConfig;
	reclaimConfig.period = std::chrono::milliseconds(20);
	if (options.delayedFreeMB >= 0)
		reclaimConfig.delayedFreeBytes = static_cast<size_t>(options.delayedFreeMB) * 1024 * 1024;
	Reclaimer::getInstance().start(reclaimConfig);

	for (int32_t c = 0; c < options.connections; ++c)
		m.connections.push_back(std::make_shared<FakeConnection>(c));
	buildLayout(m);
	m.npcSlots.reserve(static_cast<size_t>(options.npcs));

	// ---- phase 0: maps
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::STARTUP));
		for (int32_t map = 0; map < options.maps; ++map)
			m.maps.push_back(WorldMapInstance::create(210010000 + map, options.mapSize));
	}
	int64_t bytesAfterMaps = settledLiveBytes();

	// ---- phase A: npc shells (object graph only)
	auto phaseStart = std::chrono::steady_clock::now();
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::STARTUP));
		std::mt19937_64 rng(options.seed + 1);
		for (int32_t i = 0; i < options.npcs; ++i)
			m.npcSlots.push_back(m.createNpc(*m.spawnPoints[static_cast<size_t>(i)], rng));
		for (int32_t f = 0; f < options.maps * options.flagsPerMap; ++f)
			m.flags.push_back(m.createNpc(*m.spawnPoints[static_cast<size_t>(options.npcs + f)], rng));
	}
	int64_t bytesAfterShells = settledLiveBytes();

	// ---- phase B: store + spawn (all regions inactive: npc known lists stay empty)
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::STARTUP));
		for (const Ref<Npc>& npc : m.npcSlots) {
			m.world.storeObject(*npc);
			m.world.spawn(*npc);
		}
		for (const Ref<Npc>& flag : m.flags) {
			m.world.storeObject(*flag);
			m.world.spawn(*flag);
		}
	}
	int64_t bytesAfterSpawn = settledLiveBytes();

	// ---- phase C: players (region activation -> known lists of active npcs fill)
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::STARTUP));
		const size_t firstPlayerSpawn = static_cast<size_t>(options.npcs + options.maps * options.flagsPerMap);
		for (int32_t p = 0; p < options.players; ++p) {
			const SpawnPoint& spawn = *m.spawnPoints[firstPlayerSpawn + static_cast<size_t>(p)];
			Ref<Player> player = Player::create(m.nextObjectId.fetch_add(1), spawn, m.connections[static_cast<size_t>(p % std::max(1, options.connections))]);
			WorldMapInstance& instance = *m.maps[static_cast<size_t>(spawn.mapIndex)];
			player->setPosition(WorldPosition::create(instance.getMapId(), spawn.x, spawn.y, spawn.z, 0, *instance.getRegion(spawn.x, spawn.y)));
			m.world.storeObject(*player);
			m.world.spawn(*player);
			m.players.push_back(std::move(player));
		}
		for (const Ref<Npc>& flag : m.flags)
			flag->updateKnownlist();
	}
	for (const std::shared_ptr<FakeConnection>& connection : m.connections)
		connection->drain();
	int64_t bytesAfterPlayers = settledLiveBytes();

	uint64_t activeNpcs = 0;
	uint64_t npcKnownEntries = 0;
	uint64_t playerKnownEntries = 0;
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::STARTUP));
		for (const Ref<Npc>& npc : m.npcSlots) {
			int32_t known = npc->getKnownList().size();
			activeNpcs += known > 0 ? 1 : 0;
			npcKnownEntries += static_cast<uint64_t>(known);
		}
		for (const Ref<Player>& player : m.players)
			playerKnownEntries += static_cast<uint64_t>(player->getKnownList().size());
		for (int32_t i = 0; i < options.movers; ++i)
			m.npcSlots[static_cast<size_t>(i)]->getMoveController().chooseNextRouteStep();
	}
	auto setupSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - phaseStart).count();

	double n = static_cast<double>(std::max(1, options.npcs + options.maps * options.flagsPerMap));
	uint64_t totalEntries = npcKnownEntries + playerKnownEntries;
	setup("threads (hardware)", std::to_string(std::thread::hardware_concurrency()));
	setup("ForkJoin parallelism", options.serialMovement ? "serial" : std::to_string(ForkJoinPool::commonPool().getParallelism()));
	setup("ConcurrentHashMap reads", LOCKED_READS ? "stripe-locked (AION_CHM_LOCKED_READS)" : "lock-free");
	setup("map regions lookup", options.flatRegions ? "flat array (override)" : "PartMap (mechanical)");
	setup("Java instanceof", options.fastInstanceof ? "virtual kind query" : "runtime::as<> (dynamic_cast)");
	setup("setup time (shells, spawn, players)", fmt(setupSeconds, 1) + " s");
	setup("maps x size, regions", std::format("{} x {} m, {} m regions", options.maps, options.mapSize, REGION_SIZE));
	setup("map instances + regions", bytes(static_cast<double>(bytesAfterMaps)));
	if (allocationCountingEnabled()) {
		setup("bytes per Npc: object graph (shell, position, parts, observers)", fmt(static_cast<double>(bytesAfterShells - bytesAfterMaps) / n, 0));
		setup("bytes per Npc: world registration (4 map entries + region tables)", fmt(static_cast<double>(bytesAfterSpawn - bytesAfterShells) / n, 0));
		setup("bytes per known-list entry (KnownObject + CHM node + tables)",
			totalEntries == 0 ? "n/a" : fmt(static_cast<double>(bytesAfterPlayers - bytesAfterSpawn) / static_cast<double>(totalEntries), 0));
		setup("bytes per active Npc (known list filled)",
			activeNpcs == 0 ? "n/a"
											: fmt(static_cast<double>(bytesAfterShells - bytesAfterMaps) / n +
														static_cast<double>(bytesAfterSpawn - bytesAfterShells) / n +
														static_cast<double>(bytesAfterPlayers - bytesAfterSpawn) * static_cast<double>(npcKnownEntries) /
															static_cast<double>(std::max<uint64_t>(1, totalEntries)) / static_cast<double>(activeNpcs),
													0));
		setup("live heap after setup", bytes(static_cast<double>(bytesAfterPlayers)));
	} else {
		setup("allocation counting", "disabled (ASan)");
	}
	setup("active npcs (non-empty known list)", std::format("{} of {}", activeNpcs, options.npcs));
	setup("addPair at setup: pattern 1 / 2 / 3 / undone by handshake",
		std::format("{} / {} / {} / {}", eventTotal(Event::PAIR_ADD_REGION_SCAN), eventTotal(Event::PAIR_ADD_PLAYER_FLAGS),
			eventTotal(Event::PAIR_ADD_FLAG_UPDATE), eventTotal(Event::PAIR_ADD_HANDSHAKE_UNDONE)));
	setup("avg known entries: active npc / player",
		std::format("{} / {}", activeNpcs == 0 ? std::string("0") : fmt(static_cast<double>(npcKnownEntries) / static_cast<double>(activeNpcs), 1),
			options.players == 0 ? std::string("0") : fmt(static_cast<double>(playerKnownEntries) / options.players, 1)));
	setup("sizeof Npc / WorldPosition / KnownObject / RefCounted / Monitor",
		std::format("{} / {} / {} / {} / {}", sizeof(Npc), sizeof(WorldPosition), sizeof(KnownObject), sizeof(RefCounted), sizeof(Monitor)));
	setup("sizeof NpcKnownList / NpcMoveController / ObserveController / MapRegion",
		std::format("{} / {} / {} / {}", sizeof(NpcKnownList), sizeof(NpcMoveController), sizeof(ObserveController), sizeof(MapRegion)));
	setup("sizeof ConcurrentHashMap / its node (int32 -> Ref)",
		std::format("{} / {}", sizeof(Chm<int32_t, Ref<KnownObject>>), sizeof(runtime::detail::ChmNode<int32_t, Ref<KnownObject>>)));
	results.sections.push_back({"Setup and memory", std::move(setupRows)});

	// ---- run
	m.activationInline.store(false, std::memory_order_release);
	const int64_t period = options.periodMillis;
	std::vector<FutureRef> futures;
	futures.push_back(pools.scheduleAtFixedRate([] { model->movementTick(); }, period, period));
	futures.push_back(pools.scheduleAtFixedRate([] { model->churnTick(); }, period + period / 2, period));
	futures.push_back(pools.scheduleAtFixedRate([] { model->playerTick(); }, period + period / 4, period));
	futures.push_back(pools.scheduleAtFixedRate([] { model->flagTick(); }, 1000, 1000));
	futures.push_back(pools.scheduleAtFixedRate([] { model->drainConnections(); }, 50, 50));

	const int32_t totalTicks = totalTickCount(options);
	auto runDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(period * totalTicks * 4 + 60'000);
	while (m.ticksDone.load(std::memory_order_acquire) < totalTicks && std::chrono::steady_clock::now() < runDeadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	bool ranAllTicks = m.ticksDone.load(std::memory_order_acquire) >= totalTicks;

	m.stopping.store(true, std::memory_order_release);
	for (const FutureRef& future : futures)
		future->cancel(false);
	SiteCounting::end();
	bool quiescent = waitQuiescent(m, std::chrono::milliseconds(3 * period), std::chrono::seconds(120));
	for (const std::shared_ptr<FakeConnection>& connection : m.connections)
		connection->drain();

	// ---- verification (known-list ghosts)
	GhostCounts ghosts = verifyKnownLists(m);

	// ---- teardown and leak census
	int32_t moversAtEnd = m.moveTaskManager.size();
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::SHUTDOWN));
		m.moveTaskManager.clear();
		for (Ref<Npc>& npc : m.npcSlots) {
			if (npc)
				m.world.removeObject(*npc); // not marked: the reclamation-lag histogram covers removals while tasks run
		}
		for (Ref<Npc>& flag : m.flags)
			m.world.removeObject(*flag);
		for (Ref<Player>& player : m.players)
			m.world.removeObject(*player);
		m.npcSlots.clear();
		m.flags.clear();
		m.players.clear();
		m.maps.clear();
	}
	Reclaimer::getInstance().drain(1024);
	for (const std::shared_ptr<FakeConnection>& connection : m.connections)
		connection->drain();
	uint64_t shellsAlive = eventTotal(Event::SHELLS_CREATED) - eventTotal(Event::SHELLS_DESTROYED);
	uint64_t knownObjectsAlive = eventTotal(Event::KNOWN_OBJECTS_CREATED) - eventTotal(Event::KNOWN_OBJECTS_DESTROYED);
	Reclaimer::Stats finalReclaim = Reclaimer::getInstance().stats();

	// ---- tick statistics
	std::vector<NamedValue> elementRows;
	std::vector<TickRecord> tickRecords;
	{
		std::scoped_lock lock(m.tickMutex);
		tickRecords = m.ticks;
	}
	LatencyStats measured;
	LatencyStats countedTicks;
	LatencyStats warmup;
	int64_t maxLag = 0;
	uint64_t maxBacklog = 0;
	uint64_t maxBacklogBytes = 0;
	int64_t maxStartJitter = 0;
	int64_t sumMovers = 0;
	size_t overPeriod = 0;
	for (size_t i = 0; i < tickRecords.size(); ++i) {
		const TickRecord& t = tickRecords[i];
		switch (t.phase) {
			case TickPhase::WARMUP:
				warmup.add(t.durationNanos);
				break;
			case TickPhase::COUNT:
				countedTicks.add(t.durationNanos);
				break;
			case TickPhase::TAIL:
				break;
			case TickPhase::MEASURE:
				measured.add(t.durationNanos);
				sumMovers += t.movers;
				maxLag = std::max(maxLag, t.reclaimLagMillis);
				maxBacklog = std::max(maxBacklog, t.reclaimBacklog);
				maxBacklogBytes = std::max(maxBacklogBytes, t.reclaimBacklogBytes);
				overPeriod += t.durationNanos > period * 1'000'000 ? 1 : 0;
				if (i > 0 && tickRecords[i - 1].phase == TickPhase::MEASURE)
					maxStartJitter = std::max(maxStartJitter, std::abs((t.startNanos - tickRecords[i - 1].startNanos) - period * 1'000'000));
				break;
		}
	}
	{
		LatencyStats elementSums;
		LatencyStats elementMaxima;
		std::vector<size_t> slowest;
		for (size_t i = 0; i < tickRecords.size(); ++i) {
			if (tickRecords[i].phase != TickPhase::MEASURE)
				continue;
			elementSums.add(tickRecords[i].elementNanosSum);
			elementMaxima.add(tickRecords[i].elementNanosMax);
			slowest.push_back(i);
		}
		std::ranges::sort(slowest, [&](size_t a, size_t b) { return tickRecords[a].durationNanos > tickRecords[b].durationNanos; });
		slowest.resize(std::min<size_t>(slowest.size(), 8));
		std::string list;
		for (size_t i : slowest) {
			const TickRecord& t = tickRecords[i];
			list += std::format("{}{}:{}ms(max elem {}ms, sum {}ms)", list.empty() ? "" : " ", i, fmt(static_cast<double>(t.durationNanos) / 1e6, 1),
				fmt(static_cast<double>(t.elementNanosMax) / 1e6, 1), fmt(static_cast<double>(t.elementNanosSum) / 1e6, 1));
		}
		elementRows.push_back({"summed element time per tick p50 / p99 (CPU-ish)",
			std::format("{} / {} ms", fmt(elementSums.percentileMillis(50), 2), fmt(elementSums.percentileMillis(99), 2))});
		elementRows.push_back({"slowest element per tick p50 / p99 / max",
			std::format("{} / {} / {} ms", fmt(elementMaxima.percentileMillis(50), 3), fmt(elementMaxima.percentileMillis(99), 3), fmt(elementMaxima.maxMillis(), 3))});
		elementRows.push_back({"slowest ticks index:wall", list});
	}
	results.tickP50 = measured.percentileMillis(50);
	results.tickP95 = measured.percentileMillis(95);
	results.tickP99 = measured.percentileMillis(99);
	results.tickMax = measured.maxMillis();
	results.tickMean = measured.meanMillis();
	results.ticksMeasured = measured.count();

	std::vector<NamedValue> tickRows;
	tickRows.push_back({"measured ticks (after warmup)", std::to_string(measured.count())});
	tickRows.push_back({"tick p50 / p95 / p99 / max / mean",
		std::format("{} / {} / {} / {} / {} ms", fmt(results.tickP50, 3), fmt(results.tickP95, 3), fmt(results.tickP99, 3), fmt(results.tickMax, 3),
			fmt(results.tickMean, 3))});
	tickRows.push_back({"warmup ticks p50 / max", std::format("{} / {} ms", fmt(warmup.percentileMillis(50), 3), fmt(warmup.maxMillis(), 3))});
	tickRows.push_back({"ticks longer than the period", std::to_string(overPeriod)});
	tickRows.push_back({"max tick start jitter", fmt(static_cast<double>(maxStartJitter) / 1e6, 3) + " ms"});
	tickRows.push_back({"avg movers per tick", measured.count() == 0 ? "0" : fmt(static_cast<double>(sumMovers) / static_cast<double>(measured.count()), 0)});
	for (NamedValue& row : elementRows)
		tickRows.push_back(std::move(row));
	if (countedTicks.count() != 0)
		tickRows.push_back({"ticks with counting hooks p50 / max (excluded)",
			std::format("{} / {} ms", fmt(countedTicks.percentileMillis(50), 3), fmt(countedTicks.maxMillis(), 3))});
	results.sections.push_back({"Movement tick (MoveTaskManager.run: values snapshot + parallelForEach)", std::move(tickRows)});

	// ---- per-tick rates in the measured window (whole process: tick, churn, players, flags, IO drain)
	std::vector<NamedValue> rateRows;
	double measuredTicks = m.measureEndNanos > m.measureStartNanos
		? static_cast<double>(m.measureEndNanos - m.measureStartNanos) / static_cast<double>(period * 1'000'000)
		: 0.0;
	if (m.allocationsAtMeasureStart && m.allocationsAtMeasureEnd && measuredTicks > 0) {
		const AllocationTotals& a = *m.allocationsAtMeasureStart;
		const AllocationTotals& b = *m.allocationsAtMeasureEnd;
		rateRows.push_back({"allocations per tick (whole process)", fmt(static_cast<double>(b.allocations - a.allocations) / measuredTicks, 0)});
		rateRows.push_back({"allocated bytes per tick (whole process)", bytes(static_cast<double>(b.bytesAllocated - a.bytesAllocated) / measuredTicks)});
		rateRows.push_back({"live heap drift over the window", bytes(static_cast<double>(b.liveBytes() - a.liveBytes()))});
	}
	if (m.eventsAtMeasureStart && m.eventsAtMeasureEnd && measuredTicks > 0) {
		for (size_t i = 0; i < static_cast<size_t>(Event::COUNT); ++i) {
			auto event = static_cast<Event>(i);
			if (event == Event::SHELLS_CREATED || event == Event::KNOWN_OBJECTS_CREATED || event == Event::KNOWN_OBJECTS_DESTROYED ||
				i >= PHASE_NANOS_BASE)
				continue;
			uint64_t delta = (*m.eventsAtMeasureEnd)[i] - (*m.eventsAtMeasureStart)[i];
			rateRows.push_back({std::string(eventName(event)) + " per tick", fmt(static_cast<double>(delta) / measuredTicks, 1)});
		}
	}
	results.sections.push_back({"Per-tick rates (measured window, whole process)", std::move(rateRows)});

	if (phaseProfiling && m.eventsAtMeasureStart && m.eventsAtMeasureEnd && measuredTicks > 0) {
		std::vector<NamedValue> phaseRows;
		constexpr std::array<Phase, static_cast<size_t>(Phase::COUNT)> displayOrder{Phase::MOVE_ELEMENT, Phase::UPDATE_POSITION, Phase::REGION_LOOKUP,
			Phase::BROADCAST, Phase::BROADCAST_ITERATE, Phase::SERIALIZE, Phase::ENQUEUE, Phase::OBSERVERS, Phase::KNOWNLIST_UPDATE, Phase::KNOWN_FORGET,
			Phase::REGION_SCAN, Phase::PLAYER_FLAG_SCAN, Phase::ADD_PAIR, Phase::SEE_PACKET};
		for (Phase phase : displayOrder) {
			const auto p = static_cast<uint32_t>(phase);
			uint64_t nanos = (*m.eventsAtMeasureEnd)[PHASE_NANOS_BASE + p] - (*m.eventsAtMeasureStart)[PHASE_NANOS_BASE + p];
			uint64_t calls = (*m.eventsAtMeasureEnd)[PHASE_CALLS_BASE + p] - (*m.eventsAtMeasureStart)[PHASE_CALLS_BASE + p];
			phaseRows.push_back({phaseName(phase),
				std::format("{} ms / tick, {} calls / tick, {} us / call", fmt(static_cast<double>(nanos) / 1e6 / measuredTicks, 2),
					fmt(static_cast<double>(calls) / measuredTicks, 0),
					calls == 0 ? std::string("-") : fmt(static_cast<double>(nanos) / 1e3 / static_cast<double>(calls), 2))});
		}
		results.sections.push_back({"Phase profile (--profile; inclusive, whole process, timer overhead included; use with --serial)", std::move(phaseRows)});
	}

	// ---- counting window
	if (SiteCounting::available() && m.countEndNanos > m.countStartNanos) {
		double countedTickCount = static_cast<double>(options.countTicks);
		std::vector<NamedValue> siteRows;
		uint64_t retains = 0;
		uint64_t releases = 0;
		std::vector<SiteCount> sites = SiteCounting::yieldSites();
		for (const SiteCount& site : sites) {
			if (site.site == "RefCounted::retain")
				retains = site.count;
			if (site.site == "RefCounted::release:cas")
				releases = site.count;
		}
		siteRows.push_back({"refcount traffic per tick: retain / release (CAS attempts)",
			std::format("{} / {}", fmt(static_cast<double>(retains) / countedTickCount, 0), fmt(static_cast<double>(releases) / countedTickCount, 0))});
		for (size_t i = 0; i < sites.size() && i < 24; ++i)
			siteRows.push_back({"yield site " + sites[i].site, fmt(static_cast<double>(sites[i].count) / countedTickCount, 0) + " / tick"});
		std::vector<SiteCount> waits = SiteCounting::blockingWaits();
		uint64_t totalWaits = 0;
		for (const SiteCount& wait : waits)
			totalWaits += wait.count;
		siteRows.push_back({"blocking waits per tick (lock contention and idle pool waits)", fmt(static_cast<double>(totalWaits) / countedTickCount, 1)});
		for (size_t i = 0; i < waits.size() && i < 12; ++i)
			siteRows.push_back({"  waits on " + waits[i].site, fmt(static_cast<double>(waits[i].count) / countedTickCount, 1) + " / tick"});
		if (SiteCounting::droppedSites() != 0)
			siteRows.push_back({"dropped site entries", std::to_string(SiteCounting::droppedSites())});
		results.sections.push_back({std::format("Kernel operation counts ({} ticks with PCT counting hooks, whole process)", options.countTicks), std::move(siteRows)});
	}

	// ---- reclamation
	std::vector<NamedValue> reclaimRows;
	reclaimRows.push_back({"max Reclaimer lag at tick end", std::to_string(maxLag) + " ms"});
	reclaimRows.push_back({"max backlog at tick end", std::format("{} entries, {}", maxBacklog, bytes(static_cast<double>(maxBacklogBytes)))});
	reclaimRows.push_back({"removed shell -> destroyed while running, p50 / p99 / max (1 ms buckets)",
		std::format("{} / {} / {} ms ({} samples)", m.destroyLag.percentileMillis(50), m.destroyLag.percentileMillis(99), m.destroyLag.maxMillis(),
			m.destroyLag.count())});
	reclaimRows.push_back({"Reclaimer scans / destroyed total", std::format("{} / {}", finalReclaim.scans, finalReclaim.destroyedTotal)});
	results.sections.push_back({"Reclamation", std::move(reclaimRows)});

	// ---- packets
	std::vector<NamedValue> packetRows;
	uint64_t enqueued = 0;
	uint64_t outOfOrder = 0;
	size_t maxDepth = 0;
	uint64_t written = 0;
	uint64_t writtenBytes = 0;
	for (const std::shared_ptr<FakeConnection>& connection : m.connections) {
		FakeConnection::Stats s = connection->stats();
		enqueued += s.enqueued;
		outOfOrder += s.outOfOrderInserts;
		maxDepth = std::max(maxDepth, s.maxDepth);
		written += s.packetsWritten;
		writtenBytes += s.bytesWritten;
	}
	packetRows.push_back({"connections", std::to_string(m.connections.size())});
	packetRows.push_back({"packets enqueued / written (whole run)", std::format("{} / {}", enqueued, written)});
	packetRows.push_back({"bytes written", bytes(static_cast<double>(writtenBytes))});
	packetRows.push_back({"seq-ordered inserts out of arrival order", std::to_string(outOfOrder)});
	packetRows.push_back({"max queue depth (drained every 50 ms)", std::to_string(maxDepth)});
	results.sections.push_back({"Eager packets (fake connections)", std::move(packetRows)});

	// ---- correctness
	std::vector<NamedValue> checkRows;
	results.ghosts = ghosts.knowsDespawned + ghosts.despawnedWithEntries;
	results.leakedShells = shellsAlive;
	results.lockdepCycles = CHECKED ? LockOrderValidator::getInstance().reportCount(LockOrderValidator::ReportKind::CYCLE) : 0;
	checkRows.push_back({"all ticks ran / tasks quiescent", std::format("{} / {}", ranAllTicks, quiescent)});
	checkRows.push_back({"objects in World / spawned", std::format("{} / {}", ghosts.objects, ghosts.spawned)});
	checkRows.push_back({"known entries of spawned objects", std::to_string(ghosts.knownEntries)});
	checkRows.push_back({"addPair whole run: pattern 1 / 2 / 3 / undone by the spawn handshake",
		std::format("{} / {} / {} / {}", eventTotal(Event::PAIR_ADD_REGION_SCAN), eventTotal(Event::PAIR_ADD_PLAYER_FLAGS),
			eventTotal(Event::PAIR_ADD_FLAG_UPDATE), eventTotal(Event::PAIR_ADD_HANDSHAKE_UNDONE))});
	checkRows.push_back({"despawns whole run / movers found despawned in a tick",
		std::format("{} / {}", eventTotal(Event::DESPAWNS), eventTotal(Event::MOVER_DESPAWNED_IN_TICK))});
	checkRows.push_back({"GHOSTS: spawned object knows a despawned one", std::to_string(ghosts.knowsDespawned)});
	checkRows.push_back({"GHOSTS: despawned object with known entries", std::to_string(ghosts.despawnedWithEntries)});
	checkRows.push_back({"asymmetric pairs (A knows B, B does not know A)", std::to_string(ghosts.asymmetricPairs)});
	checkRows.push_back({"region ghosts / spawned objects missing from their region", std::format("{} / {}", ghosts.regionGhosts, ghosts.regionMissing)});
	checkRows.push_back({"movers at stop", std::to_string(moversAtEnd)});
	checkRows.push_back({"LEAKS after teardown: object shells / KnownObjects alive", std::format("{} / {}", shellsAlive, knownObjectsAlive)});
	checkRows.push_back({"Reclaimer backlog after teardown", std::to_string(finalReclaim.backlog)});
	checkRows.push_back({"task exceptions", std::to_string(eventTotal(Event::TASK_EXCEPTIONS))});
	if (CHECKED) {
		LockOrderValidator& validator = LockOrderValidator::getInstance();
		checkRows.push_back({"lockdep CYCLE / SAME_CLASS_NESTING / BLOCKING_UNDER_MONITOR reports",
			std::format("{} / {} / {}", validator.reportCount(LockOrderValidator::ReportKind::CYCLE),
				validator.reportCount(LockOrderValidator::ReportKind::SAME_CLASS_NESTING),
				validator.reportCount(LockOrderValidator::ReportKind::BLOCKING_UNDER_MONITOR))});
		for (const LockOrderValidator::Report& report : validator.getReports()) {
			if (report.kind != LockOrderValidator::ReportKind::BLOCKING_UNDER_MONITOR)
				checkRows.push_back({"  lockdep report", std::format("{} -> {} (x{})", report.heldLockClass, report.acquiredLockClass, report.occurrences)});
		}
	}
	results.sections.push_back({"Correctness", std::move(checkRows)});

	results.completed = ranAllTicks && quiescent;
	if (!ranAllTicks)
		results.failure = "not all ticks ran before the deadline";
	else if (!quiescent)
		results.failure = "tasks did not become quiescent";

	// ---- shutdown
	pools.shutdown();
	ForkJoinPool::commonPool().shutdown();
	Reclaimer::getInstance().drain(1024);
	Reclaimer::getInstance().stop();
	if (shellsAlive == 0) {
		model = nullptr;
		model_.reset();
	} else {
		(void)model_.release(); // leaked shells may still reference the model (templates, histogram); keep it alive
	}
	return results;
}

} // namespace aion::gameserver::bench::P4_NAMESPACE
