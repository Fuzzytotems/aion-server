package ai;

import com.aionemu.gameserver.model.gameobjects.Creature;

public class SpawnAwareCreature extends Creature {

	public SpawnAwareCreature(int objectId) {
		super(objectId, null);
	}

	@Override
	public void onSpawn() {
	}
}
