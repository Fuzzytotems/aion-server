package com.aionemu.gameserver.model.gameobjects;

/**
 * Base of all game objects.
 *
 * @author -Nemesiss-, SoulKeeper
 */
public abstract class AionObject {

	private final int objectId;

	public AionObject(int objectId) {
		this.objectId = objectId;
	}

	public final int getObjectId() {
		return objectId;
	}

	public abstract String getName();

	@Override
	public boolean equals(Object obj) {
		return obj instanceof AionObject o && o.objectId == objectId;
	}

	@Override
	public int hashCode() {
		return objectId;
	}
}
