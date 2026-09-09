/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "2d/Scene.h"
#include "EventKeyboard.h"
#include "GameToolbox/enums.h"

namespace ax
{
	class Event;
	class Label;
	class Menu;
	class Sprite;
}

class DialogLayer;

class GJShopLayer : public ax::Scene
{
public:
	static ax::Scene* scene();
	static GJShopLayer* create();
	bool init() override;
	void onEnterTransitionDidFinish() override;
	void onKeyPressed(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);

private:
	void goBack();
	void rebuildItemGrid();
	void onItemPressed(int index);
	void refreshOrbLabel();
	void setPage(int page);
	void showWelcomeDialog();
	void showReactMessage();
	void showCantAffordDialog();
	int pageCount() const;
	ax::Node* buildShopkeeper() const;
	float iconScaleForType(IconType type) const;

	ax::Label* _orbLabel = nullptr;
	ax::Node* _shopkeeper = nullptr;
	ax::Menu* _itemMenu = nullptr;
	ax::MenuItem* _leftArrow = nullptr;
	ax::MenuItem* _rightArrow = nullptr;
	DialogLayer* _activeDialog = nullptr;
	int _page = 0;
	int _reactIndex = 0;
	static constexpr int kItemsPerPage = 8; // ListButtonBar 4×2
};
