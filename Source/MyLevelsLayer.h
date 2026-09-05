/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "2d/Scene.h"
#include "EventKeyboard.h"

class GJGameLevel;
class MenuItemSpriteExtra;

namespace ax
{
	class Event;
	class Label;
	namespace ui
	{
		class ListView;
	}
}

class MyLevelsLayer : public ax::Scene
{
public:
	static ax::Scene* scene();
	static MyLevelsLayer* create();
	bool init() override;
	void onEnter() override;
	void onKeyPressed(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);

private:
	void reloadLevels();
	void fillPage();
	void setPage(int page);
	int pageCount() const;
	void updatePageLabel();
	void toggleCheck(int index);
	void toggleAllChecks();
	void refreshAllCheckSprite();
	bool isChecked(int index) const;
	void deleteChecked();
	void openLevel(GJGameLevel* level, bool edit);

	std::vector<GJGameLevel*> _levels;
	std::vector<bool> _checked;
	ax::ui::ListView* _listView = nullptr;
	ax::Label* _pageLabel = nullptr;
	ax::Label* _pageNumLabel = nullptr;
	MenuItemSpriteExtra* _leftBtn = nullptr;
	MenuItemSpriteExtra* _rightBtn = nullptr;
	MenuItemSpriteExtra* _allCheckBtn = nullptr;
	int _page = 0;
	static constexpr int kLevelsPerPage = 10;
};
