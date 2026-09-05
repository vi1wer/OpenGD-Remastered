/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "PopupLayer.h"

class GameObject;
class LevelEditorLayer;

enum class EditorPropertyKind
{
	Object = 0,
	Group,
	Special,
	Color,
};

class EditorPropertyLayer : public PopupLayer
{
public:
	static EditorPropertyLayer* create(LevelEditorLayer* editor, EditorPropertyKind kind, GameObject* obj);

private:
	bool init(LevelEditorLayer* editor, EditorPropertyKind kind, GameObject* obj);

	LevelEditorLayer* _editor = nullptr;
	GameObject* _target = nullptr;
	EditorPropertyKind _kind = EditorPropertyKind::Object;
};
