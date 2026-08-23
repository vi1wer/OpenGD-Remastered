/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License    
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*************************************************************************/

#pragma once
#include <array>

#include "2d/Scene.h"

#include "GameToolbox/enums.h"
class SimplePlayer;

namespace ax 
{ 
	class Menu;
	class Sprite;
	class Label;
	class Node;

	namespace ui 
	{ 
		class TextField; 
	}
}


class GarageLayer : public ax::Scene {
public:
	static ax::Scene* scene(bool popSceneWithTransition = false);
	static GarageLayer* create();
	bool init();
	void setupIconSelect();
	const char* getSpriteName(int id, bool actived);
	void setupPage(IconType mode, int page);
	void setupColorPage();
	void createStat(const char* sprite, const char* statKey);
	int selectedGameModeInt();
	int pageForSelectedIcon(IconType mode);
	void updateModeTabs();
	void selectMode(IconType mode, bool jumpToSelectedPage = true);
	void applyPreviewColors();
	void centerPreviewIcon();
	void refreshCurrentPage();
	void onEnter() override;

private:
	bool _popSceneWithTransition = false;
	SimplePlayer* _iconPrev = nullptr;
	ax::Vec2 _previewCenter{0.f, 0.f};
	ax::ui::TextField* _userNameField = nullptr;
	ax::Menu* _menuIcons = nullptr;
	ax::Sprite* _selectSprite = nullptr;
	ax::Sprite* _unlockLabel = nullptr;
	ax::Menu* _modeTabMenu = nullptr;
	ax::Menu* _pageArrowMenu = nullptr;
	ax::Node* _colorUiLayer = nullptr;
	int _numPerRow = 12;
	int _numPerColumn = 3;
	int _stats = 0;
	IconType _selectedMode;
	bool _colorMode = false;
	int _activeColorSlot = 0;
	int _colorPage = 0;
	std::array<int, 11> _modePages{};
	ax::Label* _orbStatLabel = nullptr;
	ax::Label* _diamondStatLabel = nullptr;
};
