package com.aionemu.gameserver.model.templates.item;

import javax.xml.bind.Unmarshaller;
import javax.xml.bind.annotation.XmlAccessType;
import javax.xml.bind.annotation.XmlAccessorType;
import javax.xml.bind.annotation.XmlAttribute;

@XmlAccessorType(XmlAccessType.NONE)
public class ItemTemplate {

	@XmlAttribute(name = "id")
	private int templateId;

	public int getTemplateId() {
		return templateId;
	}

	void afterUnmarshal(Unmarshaller u, Object parent) {
	}
}
