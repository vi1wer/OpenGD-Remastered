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
#include "2d/Scene.h"
#include "EventKeyboard.h"

namespace ax 
{ 
	class Event;
	class Sprite;
	namespace ui 
	{ 
		class TextField; 
	} 
}

class DialogLayer;
class MenuItemSpriteExtra;

class CreatorLayer : public ax::Scene {
public:
	static ax::Scene* scene();
	static CreatorLayer* create();
	bool init() override;
	void onEnter() override;
	
	void onKeyPressed(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);

private:
	void onLockPressed();
	void showKeymasterPoke();
	void showVaultWelcomeAndEnter();
	void enterVault();
	void refreshLockVisual();
	bool hasVaultDiamonds() const;

	ax::ui::TextField* _levelField = nullptr;
	DialogLayer* _lockDialog = nullptr;
	MenuItemSpriteExtra* _lockBtn = nullptr;
	ax::Sprite* _lockSpr = nullptr;
	int _lockPoke = 0;
	static constexpr int kVaultDiamondCost = 100;
	static constexpr int kKeymasterIcon = 2;   // dialogIcon_002
	static constexpr int kKeymasterBg = 2;     // GJ_square02 dark blue
};
