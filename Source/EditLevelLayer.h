/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "2d/Layer.h"
#include "EventKeyboard.h"
#include "GJGameLevel.h"
#include "TextInputNode.h"

namespace ax
{
	class Event;
	class Scene;
	class Label;
}

class EditLevelLayer : public ax::Layer, public TextInputDelegate
{
public:
	static EditLevelLayer* create(GJGameLevel* level);
	static ax::Scene* scene(GJGameLevel* level);
	bool init(GJGameLevel* level);
	void onEnter() override;
	void onKeyPressed(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);

	void textChanged(TextInputNode* input) override;
	void textInputClosed(TextInputNode* input) override;

private:
	void goBack();
	void openEditor();
	void playtest();
	void onShare();
	void refreshLabels();
	void syncFieldsToLevel();

	GJGameLevel* _level = nullptr;
	TextInputNode* _nameInput = nullptr;
	TextInputNode* _descInput = nullptr;
	ax::Label* _statusLabel = nullptr;
	ax::Label* _songLabel = nullptr;
	ax::Label* _lengthLabel = nullptr;
};
