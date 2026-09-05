/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "MyLevelsLayer.h"

#include "ButtonSprite.h"
#include "ComingSoonLayer.h"
#include "EditLevelLayer.h"
#include "GJGameLevel.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/nodes.h"
#include "LevelTools.h"
#include "ListLayer.h"
#include "LocalLevelManager.h"
#include "MenuItemSpriteExtra.h"

#include "2d/Label.h"
#include "2d/Layer.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "2d/Transition.h"
#include "EventDispatcher.h"
#include "EventListenerKeyboard.h"
#include "base/Director.h"
#include "ui/UIListView.h"
#include "ui/UIWidget.h"

#include <algorithm>
#include <fmt/format.h>
#include <functional>

USING_NS_AX;

namespace
{
	Sprite* tryFrame(const char* name)
	{
		if (!SpriteFrameCache::getInstance()->getSpriteFrameByName(name))
			return nullptr;
		auto* spr = Sprite::createWithSpriteFrameName(name);
		if (spr)
			spr->setStretchEnabled(false);
		return spr;
	}

	const char* checkFrame(bool on)
	{
		return on ? "GJ_checkOn_001.png" : "GJ_checkOff_001.png";
	}

	Menu* menuAt(Node* parent, const Vec2& pos, int z = 10)
	{
		auto* menu = Menu::create();
		menu->setPosition(pos);
		parent->addChild(menu, z);
		return menu;
	}

	class MyLevelCell : public ui::Widget
	{
	public:
		static MyLevelCell* create(
			GJGameLevel* level, bool altBg, bool checked, const std::function<void()>& onView,
			const std::function<void()>& onToggle)
		{
			auto* ret = new (std::nothrow) MyLevelCell();
			if (ret && ret->init(level, altBg, checked, onView, onToggle))
			{
				ret->autorelease();
				return ret;
			}
			AX_SAFE_DELETE(ret);
			return nullptr;
		}

	private:
		bool init(
			GJGameLevel* level, bool altBg, bool checked, const std::function<void()>& onView,
			const std::function<void()>& onToggle)
		{
			if (!Widget::init())
				return false;

			const Vec2 size = {356.f, 80.f};
			setContentSize(size);

			auto* bg = LayerColor::create(
				altBg ? Color4B(161, 88, 44, 255) : Color4B(191, 114, 62, 255), size.x, size.y);
			addChild(bg, 0);

			auto* divider = LayerColor::create({0, 0, 0, 80}, size.x, 1.5f);
			divider->setPosition({0.f, 0.f});
			addChild(divider, 1);

			auto* name = GameToolbox::createBMFont(level->_levelName, "bigFont.fnt");
			name->setAnchorPoint({0.f, 0.5f});
			name->setPosition({10.f, 58.f});
			GameToolbox::limitLabelWidth(name, 200.f, 0.7f);
			addChild(name, 1);

			float x = 10.f;
			const float metaY = 24.f;
			auto addMeta = [&](const char* iconName, const std::string& text, float maxW) {
				if (auto* icon = tryFrame(iconName))
				{
					icon->setScale(0.55f);
					icon->setAnchorPoint({0.f, 0.5f});
					icon->setPosition({x, metaY});
					addChild(icon, 1);
					x += icon->getContentSize().width * icon->getScale() + 3.f;
				}
				auto* label = GameToolbox::createBMFont(text, "bigFont.fnt");
				label->setScale(0.32f);
				label->setAnchorPoint({0.f, 0.5f});
				label->setPosition({x, metaY});
				GameToolbox::limitLabelWidth(label, maxW, 0.32f);
				addChild(label, 1);
				x += std::min(label->getContentSize().width * label->getScale(), maxW) + 10.f;
			};

			addMeta("GJ_timeIcon_001.png", GameToolbox::levelLengthString(level->_length), 42.f);

			std::string song = level->_songName;
			if (song.empty())
			{
				const int sid = level->_songID > 0
									? level->_songID
									: (level->_officialSongID != 0 ? level->_officialSongID : level->_musicID);
				song = LevelTools::getAudioTitle(sid);
			}
			addMeta("GJ_musicIcon_001.png", song, 95.f);
			addMeta("GJ_infoIcon_001.png", level->_setCompletes > 0 ? "VERIFIED" : "UNVERIFIED", 70.f);

			if (auto* checkSpr = tryFrame(checkFrame(checked)))
			{
				auto* checkBtn = MenuItemSpriteExtra::create(checkSpr, [onToggle](Node*) {
					if (onToggle)
						onToggle();
				});
				checkBtn->setScale(0.5f);
				menuAt(this, {262.f, size.y * 0.5f}, 2)->addChild(checkBtn);
			}

			auto* viewSpr = ButtonSprite::create(
				"View", 0x32, 0, 0.6f, false, GameToolbox::getTextureString("bigFont.fnt"),
				GameToolbox::getTextureString("GJ_button_01.png"), 30);
			auto* viewBtn = MenuItemSpriteExtra::create(viewSpr, [onView](Node*) {
				if (onView)
					onView();
			});
			menuAt(this, {318.f, size.y * 0.5f}, 2)->addChild(viewBtn);

			return true;
		}
	};
}

Scene* MyLevelsLayer::scene()
{
	return MyLevelsLayer::create();
}

MyLevelsLayer* MyLevelsLayer::create()
{
	auto* ret = new (std::nothrow) MyLevelsLayer();
	if (ret && ret->init())
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
	return nullptr;
}

bool MyLevelsLayer::init()
{
	if (!Scene::init())
		return false;

	const auto& winSize = Director::getInstance()->getWinSize();

	GameToolbox::createBG(this, {0, 102, 255});
	GameToolbox::createCorners(this, false, false, true, true);

	auto* backBtn = MenuItemSpriteExtra::create("GJ_arrow_01_001.png", [](Node*) {
		GameToolbox::popSceneWithTransition(0.5f, popTransition::kTransitionFade);
	});
	menuAt(this, {24.f, winSize.height - 23.f})->addChild(backBtn);

	const char* findFrames[] = {
		"gj_findBtnOff_001.png", "GJ_findBtnOff_001.png", "gj_findBtn_001.png", "GJ_zoomInBtn_001.png"};
	for (const char* frame : findFrames)
	{
		if (!tryFrame(frame))
			continue;
		auto* searchBtn = MenuItemSpriteExtra::create(frame, [this](Node*) {
			auto* banner = ComingSoonLayer::create("Search");
			if (!banner)
				return;
			addChild(banner, 200);
			banner->show();
		});
		searchBtn->setScale(0.9f);
		menuAt(this, {24.f, winSize.height - 70.f})->addChild(searchBtn);
		break;
	}

	_listView = ui::ListView::create();
	_listView->setBounceEnabled(true);
	_listView->setAnchorPoint({0.5f, 0.5f});
	_listView->setScrollBarEnabled(false);

	auto* list = ListLayer::create(_listView, "My Levels", {191, 114, 62, 255});
	list->setPosition({(winSize.width - 356.f) / 2.f, ((winSize.height - 220.f) / 2.f) - 5.f});
	addChild(list, 1);

	_pageLabel = GameToolbox::createBMFont("0 TO 0 OF 0", "goldFont.fnt");
	_pageLabel->setScale(0.4f);
	_pageLabel->setAnchorPoint({1.f, 0.5f});
	_pageLabel->setPosition({winSize.width - 8.f, winSize.height - 12.f});
	addChild(_pageLabel, 5);

	_pageNumLabel = GameToolbox::createBMFont("1", "bigFont.fnt");
	_pageNumLabel->setScale(0.4f);
	_pageNumLabel->setPosition({winSize.width - 26.f, winSize.height / 2.f + 70.f});
	addChild(_pageNumLabel, 5);

	_leftBtn = MenuItemSpriteExtra::create("GJ_arrow_03_001.png", [this](Node*) { setPage(_page - 1); });
	_leftBtn->setVisible(false);
	_leftBtn->setEnabled(false);
	menuAt(this, {24.f, winSize.height / 2.f})->addChild(_leftBtn);

	_rightBtn = MenuItemSpriteExtra::create("GJ_arrow_03_001.png", [this](Node*) { setPage(_page + 1); });
	_rightBtn->setScaleX(-1.f);
	_rightBtn->setVisible(false);
	_rightBtn->setEnabled(false);
	menuAt(this, {winSize.width - 24.f, winSize.height / 2.f})->addChild(_rightBtn);

	Node* newSpr = tryFrame("GJ_newBtn_001.png");
	if (!newSpr)
	{
		newSpr = ButtonSprite::create(
			"NEW", 0x40, 0, 0.75f, false, GameToolbox::getTextureString("bigFont.fnt"),
			GameToolbox::getTextureString("GJ_button_01.png"), 40);
	}
	auto* newBtn = MenuItemSpriteExtra::create(newSpr, [this](Node*) {
		auto* level = LocalLevelManager::get()->createUnnamedLevel();
		if (!level)
			return;
		Director::getInstance()->pushScene(TransitionFade::create(0.5f, EditLevelLayer::scene(level)));
	});
	newBtn->setScale(0.9f);
	menuAt(this, {winSize.width - 48.f, 52.f})->addChild(newBtn);

	// Footer controls live on the table's bottom bar (list-local coords).
	Sprite* trashSpr = tryFrame("GJ_trashBtn_001.png");
	if (!trashSpr)
		trashSpr = tryFrame("GJ_deleteBtn_001.png");
	if (trashSpr)
	{
		auto* trashBtn = MenuItemSpriteExtra::create(trashSpr, [this](Node*) { deleteChecked(); });
		trashBtn->setScale(0.48f);
		menuAt(list, {46.f, -10.f}, 20)->addChild(trashBtn);
	}

	_allCheckBtn = MenuItemSpriteExtra::create(checkFrame(false), [this](Node*) { toggleAllChecks(); });
	_allCheckBtn->setScale(0.48f);
	menuAt(list, {78.f, -10.f}, 20)->addChild(_allCheckBtn);

	auto* allLabel = GameToolbox::createBMFont("ALL", "bigFont.fnt");
	allLabel->setScale(0.28f);
	allLabel->setAnchorPoint({0.f, 0.5f});
	allLabel->setPosition({92.f, -10.f});
	list->addChild(allLabel, 20);

	auto* listener = EventListenerKeyboard::create();
	listener->onKeyPressed = AX_CALLBACK_2(MyLevelsLayer::onKeyPressed, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

	reloadLevels();
	return true;
}

void MyLevelsLayer::onEnter()
{
	Scene::onEnter();
	reloadLevels();
}

void MyLevelsLayer::reloadLevels()
{
	_levels = LocalLevelManager::get()->getLevels();
	_checked.assign(_levels.size(), false);
	_page = std::clamp(_page, 0, std::max(0, pageCount() - 1));
	fillPage();
}

void MyLevelsLayer::fillPage()
{
	if (!_listView)
		return;

	_listView->removeAllItems();

	const int total = static_cast<int>(_levels.size());
	const int start = _page * kLevelsPerPage;
	const int end = std::min(total, start + kLevelsPerPage);

	for (int i = start; i < end; ++i)
	{
		auto* level = _levels[static_cast<size_t>(i)];
		if (!level)
			continue;
		const bool alt = ((i - start) % 2) == 1;
		const int index = i;
		auto* cell = MyLevelCell::create(
			level, alt, isChecked(index),
			[this, level] { openLevel(level, false); },
			[this, index] {
				toggleCheck(index);
				fillPage();
			});
		_listView->pushBackCustomItem(cell);
	}

	updatePageLabel();
	refreshAllCheckSprite();

	const bool multiPage = pageCount() > 1;
	if (_leftBtn)
	{
		_leftBtn->setVisible(multiPage && _page > 0);
		_leftBtn->setEnabled(multiPage && _page > 0);
	}
	if (_rightBtn)
	{
		_rightBtn->setVisible(multiPage && _page + 1 < pageCount());
		_rightBtn->setEnabled(multiPage && _page + 1 < pageCount());
	}
}

void MyLevelsLayer::setPage(int page)
{
	_page = std::clamp(page, 0, std::max(0, pageCount() - 1));
	fillPage();
}

int MyLevelsLayer::pageCount() const
{
	if (_levels.empty())
		return 1;
	return (static_cast<int>(_levels.size()) + kLevelsPerPage - 1) / kLevelsPerPage;
}

void MyLevelsLayer::updatePageLabel()
{
	const int total = static_cast<int>(_levels.size());
	if (total == 0)
	{
		if (_pageLabel)
			_pageLabel->setString("0 TO 0 OF 0");
		if (_pageNumLabel)
			_pageNumLabel->setString("1");
		return;
	}

	const int start = _page * kLevelsPerPage + 1;
	const int end = std::min(total, (_page + 1) * kLevelsPerPage);
	if (_pageLabel)
		_pageLabel->setString(fmt::format("{} TO {} OF {}", start, end, total));
	if (_pageNumLabel)
		_pageNumLabel->setString(std::to_string(_page + 1));
}

void MyLevelsLayer::toggleCheck(int index)
{
	if (index < 0 || index >= static_cast<int>(_checked.size()))
		return;
	_checked[static_cast<size_t>(index)] = !_checked[static_cast<size_t>(index)];
}

void MyLevelsLayer::toggleAllChecks()
{
	const bool anyUnchecked = std::any_of(_checked.begin(), _checked.end(), [](bool v) { return !v; });
	std::fill(_checked.begin(), _checked.end(), anyUnchecked);
	fillPage();
}

void MyLevelsLayer::refreshAllCheckSprite()
{
	if (!_allCheckBtn)
		return;
	const bool allOn = !_checked.empty() && std::all_of(_checked.begin(), _checked.end(), [](bool v) { return v; });
	_allCheckBtn->setSpriteFrame(checkFrame(allOn));
}

bool MyLevelsLayer::isChecked(int index) const
{
	if (index < 0 || index >= static_cast<int>(_checked.size()))
		return false;
	return _checked[static_cast<size_t>(index)];
}

void MyLevelsLayer::deleteChecked()
{
	std::vector<int> ids;
	for (size_t i = 0; i < _levels.size() && i < _checked.size(); ++i)
	{
		if (_checked[i] && _levels[i])
			ids.push_back(_levels[i]->_levelID);
	}
	if (ids.empty())
		return;

	LocalLevelManager::get()->removeLevels(ids);
	reloadLevels();
}

void MyLevelsLayer::openLevel(GJGameLevel* level, bool edit)
{
	(void)edit;
	if (!level)
		return;
	Director::getInstance()->pushScene(TransitionFade::create(0.5f, EditLevelLayer::scene(level)));
}

void MyLevelsLayer::onKeyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
	if (keyCode == EventKeyboard::KeyCode::KEY_ESCAPE || keyCode == EventKeyboard::KeyCode::KEY_BACK)
		GameToolbox::popSceneWithTransition(0.5f, popTransition::kTransitionFade);
}
