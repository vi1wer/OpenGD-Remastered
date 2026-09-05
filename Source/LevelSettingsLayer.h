/*************************************************************************
	OpenGD - Open source Geometry Dash.
	Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "PopupLayer.h"

class ButtonSprite;
class LevelEditorLayer;

namespace ax
{
	class Label;
	class Menu;
	class Sprite;
	class Node;
}

class LevelSettingsLayer : public PopupLayer
{
public:
	static LevelSettingsLayer* create(LevelEditorLayer* editor);

private:
	bool init(LevelEditorLayer* editor);
	void buildUI();
	void refreshUI();
	void onOk();

	void cycleSpeed(int delta);
	void openSpeedSelect();
	void openModeSelect();
	void cycleMode(int delta);
	void cycleFont(int delta);
	void cycleBg(int delta);
	void cycleGround(int delta);
	void cycleMg(int delta);
	void setPlatformer(bool on);
	void setCustomSong(bool on);
	void cycleSong(int delta);
	void openOptions();
	void openColorPicker(int channelId, const char* title);
	void openMoreColors();

	ButtonSprite* makeToggleButton(const char* text, int width, bool selected);
	ax::Sprite* makeColorSwatch(int channelId, float size);
	void ensureChannels();
	void updateSongLabel();
	void updateSpeedIcon();
	void updateModeIcon();
	void updatePreviewSprites();
	void updateGameTypeButtons();
	void updateSongModeButtons();
	void updateSwatches();
	void setToggleSelected(ButtonSprite* btn, bool selected);

	LevelEditorLayer* _editor = nullptr;
	bool _customSong = false;

	ax::Menu* _menu = nullptr;
	ax::Sprite* _speedIcon = nullptr;
	ax::Sprite* _modeIcon = nullptr;
	ax::Label* _songLabel = nullptr;
	ax::Sprite* _bgPreview = nullptr;
	ax::Sprite* _gPreview = nullptr;
	ax::Sprite* _mgPreview = nullptr;
	ax::Label* _mgNoneMark = nullptr;
	ButtonSprite* _classicBtn = nullptr;
	ButtonSprite* _platformerBtn = nullptr;
	ButtonSprite* _normalSongBtn = nullptr;
	ButtonSprite* _customSongBtn = nullptr;
	ButtonSprite* _fontBtn = nullptr;

	ax::Sprite* _swatchBG = nullptr;
	ax::Sprite* _swatchG = nullptr;
	ax::Sprite* _swatchG2 = nullptr;
	ax::Sprite* _swatchLine = nullptr;
	ax::Sprite* _swatchMG = nullptr;
	ax::Sprite* _swatchMG2 = nullptr;
};
