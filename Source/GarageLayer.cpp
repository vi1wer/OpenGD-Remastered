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

#include "GarageLayer.h"
#include "GJShopLayer.h"
#include "MenuItemSpriteExtra.h"
#include "MenuLayer.h"
#include "SimplePlayer.h"
#include "GameManager.h"
#include "ui/UITextField.h"
#include "2d/Transition.h"
#include "2d/Menu.h"
#include "2d/Label.h"
#include "EventDispatcher.h"
#include "UTF8.h"
#include "EventListenerKeyboard.h"
#include "ui/UIScale9Sprite.h"
#include "base/Director.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/log.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/nodes.h"
#include <algorithm>
#include <string>
#include <string_view>

USING_NS_AX;

Scene* GarageLayer::scene(bool popSceneWithTransition)
{
	auto garage = GarageLayer::create();
	if (!garage)
		return Scene::create();
	garage->_popSceneWithTransition = popSceneWithTransition;
	return garage;
}

GarageLayer* GarageLayer::create()
{
	auto r = new GarageLayer();
	if (r && r->init())
		r->autorelease();
	else
	{
		delete r;
		r = nullptr;
	}
	return r;
}

bool GarageLayer::init()
{
	if (!Scene::init())
		return false;

	auto gm = GameManager::getInstance();
	_selectedMode = gm->_mainSelectedMode;
	
	auto director = Director::getInstance();
	auto size  = director->getWinSize();

	GameToolbox::createBG(this, { 150, 150, 150 });
	GameToolbox::createCorners(this, true, false, true, true);

	_userNameField = ui::TextField::create("Username", GameToolbox::getTextureString("bigFont.fnt"), 20);
	_userNameField->setPlaceHolderColor({120, 170, 240});
	_userNameField->setMaxLength(10);
	_userNameField->setMaxLengthEnabled(true);
	_userNameField->setCursorEnabled(true);
	_userNameField->setString("Player");
	_userNameField->setPosition({ size.width / 2, size.height - 34 });
	this->addChild(_userNameField);

	auto line = Sprite::createWithSpriteFrameName("floorLine_001.png");
	line->setBlendFunc(GameToolbox::getBlending());
	line->setPosition({ size.width / 2, size.height / 2 + 50 });
	this->addChild(line);

	_iconPrev = SimplePlayer::create(0); // 132
	_iconPrev->setPlayIdleAnimation(true);
	_iconPrev->updateGamemode(gm->getSelectedIcon(gm->_mainSelectedMode), gm->_mainSelectedMode);
	_previewCenter = {line->getPositionX(), line->getPositionY() + 25.f};
	centerPreviewIcon();
	applyPreviewColors();
	this->addChild(_iconPrev);

	this->setupIconSelect();

	auto menu = Menu::create();
	menu->setPosition({0, 0});

	auto backBtn = MenuItemSpriteExtra::create("GJ_arrow_03_001.png", [&](Node*) {
		if (_popSceneWithTransition) 
			GameToolbox::popSceneWithTransition(0.5f, popTransition::kTransitionShop);
		else {
			auto director = Director::getInstance();
			director->replaceScene(TransitionFade::create(0.5f, MenuLayer::scene()));
		}
	});
	backBtn->setPosition({24, size.height - 23});
	//backBtn->setSizeMult(1.6f);
	menu->addChild(backBtn);

	auto shop = MenuItemSpriteExtra::create("shopRope_001.png", [](Node*) {
		Director::getInstance()->pushScene(TransitionMoveInT::create(0.5f, GJShopLayer::scene()));
	});

	shop->setPosition({135, size.height - 25});
	shop->setDestination({0, -15});
	menu->addChild(shop);

	auto paint = MenuItemSpriteExtra::create("GJ_paintBtn_001.png", [this](Node*) {
		_colorMode = !_colorMode;
		refreshCurrentPage();
	});
	paint->setPosition({30, size.height - 80});
	menu->addChild(paint);

	
	// stats
	this->createStat("GJ_starsIcon_001.png", "6");
	this->createStat("GJ_coinsIcon_001.png", "8");
	this->createStat("GJ_coinsIcon2_001.png", "12");
	this->createStat("currencyOrbIcon_001.png", "14");
	this->createStat("GJ_diamondsIcon_001.png", "13");
	//this->createStat("GJ_demonIcon_001.png", "5");

	this->addChild(menu);

	auto listener = ax::EventListenerKeyboard::create();
	listener->onKeyPressed = [&](ax::EventKeyboard::KeyCode key, ax::Event*) {
		if (key == ax::EventKeyboard::KeyCode::KEY_ESCAPE) {
			if (_popSceneWithTransition) GameToolbox::popSceneWithTransition(0.5f, popTransition::kTransitionShop);
			else Director::getInstance()->replaceScene(TransitionFade::create(0.5f, MenuLayer::scene()));
		}
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

	return true;
}

int GarageLayer::selectedGameModeInt()
{
	if (_selectedMode == IconType::kIconTypeSpecial)
		return 9;
	if (_selectedMode == IconType::kIconTypeDeathEffect)
		return 10;
	return static_cast<int>(_selectedMode);
}

int GarageLayer::pageForSelectedIcon(IconType mode)
{
	int selected = GameManager::getInstance()->getSelectedIcon(mode);
	int maxIcon = GameToolbox::getValueForGamemode(mode);
	if (maxIcon < 1)
		return 0;
	if (selected < 1)
		selected = 1;
	if (selected > maxIcon)
		selected = maxIcon;
	return (selected - 1) / 36;
}

void GarageLayer::updateModeTabs()
{
	if (!_modeTabMenu)
		return;

	const int active = selectedGameModeInt();
	for (auto* child : _modeTabMenu->getChildren())
	{
		auto* item = dynamic_cast<MenuItemSpriteExtra*>(child);
		if (!item)
			continue;
		item->setSpriteFrame(getSpriteName(item->getTag(), item->getTag() == active));
	}
}

void GarageLayer::selectMode(IconType mode, bool jumpToSelectedPage)
{
	_selectedMode = mode;
	auto* gm = GameManager::getInstance();
	gm->_mainSelectedMode = mode;

	int iconId = gm->getSelectedIcon(mode);
	if (iconId < 1)
		iconId = 1;

	_iconPrev->updateGamemode(iconId, mode);
	const bool animatedMode = mode == IconType::kIconTypeRobot || mode == IconType::kIconTypeSpider;
	_iconPrev->setPlayIdleAnimation(!animatedMode);
	applyPreviewColors();
	centerPreviewIcon();
	updateModeTabs();

	const int tab = selectedGameModeInt();
	if (jumpToSelectedPage)
		_modePages[tab] = pageForSelectedIcon(mode);

	if (_colorMode)
		setupColorPage();
	else
		setupPage(mode, _modePages[tab]);
}

void GarageLayer::createStat(const char* sprite, const char* statKey)
{
	const auto& size = Director::getInstance()->getWinSize();

	auto stat = Sprite::createWithSpriteFrameName(sprite);
	stat->setScale(.65);
	stat->setPosition({size.width - 20, size.height - 14 - 20 * _stats});
	this->addChild(stat);

	auto gm = GameManager::getInstance();
	std::string value = "0";
	if (statKey && std::string_view(statKey) == "14")
		value = std::to_string(gm->getOrbs());
	else if (statKey && std::string_view(statKey) == "13")
		value = std::to_string(gm->getDiamonds());

	auto statL = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), value);
	statL->setScale(.45);
	statL->setAnchorPoint({1, 0});
	statL->setPosition({stat->getPositionX() - 16, stat->getPositionY() - 7});
	this->addChild(statL);

	if (statKey && std::string_view(statKey) == "14")
		_orbStatLabel = statL;
	else if (statKey && std::string_view(statKey) == "13")
		_diamondStatLabel = statL;

	_stats++;
}

void GarageLayer::onEnter()
{
	Scene::onEnter();
	auto gm = GameManager::getInstance();
	if (_orbStatLabel)
		_orbStatLabel->setString(std::to_string(gm->getOrbs()));
	if (_diamondStatLabel)
		_diamondStatLabel->setString(std::to_string(gm->getDiamonds()));
	refreshCurrentPage();
}

void GarageLayer::setupIconSelect()
{
	const auto& size = Director::getInstance()->getWinSize();

	auto bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("square02_001.png"));
	bg->setContentSize({385, 100});
	bg->setOpacity(75);
	bg->setPosition({size.width / 2, size.height / 2 - 65});
	this->addChild(bg);

	auto unlock = Sprite::createWithSpriteFrameName("GJ_unlockTxt_001.png");
	unlock->setPosition({size.width / 2, bg->getPositionY() + bg->getContentSize().height / 2 + 12});
	this->addChild(unlock);
	_unlockLabel = unlock;

	auto menu = Menu::create();
	_modeTabMenu = menu;

	for (int i = 0; i < 11; i++)
	{
		auto s1 = Sprite::createWithSpriteFrameName(this->getSpriteName(i, false));
		s1->setScale(.72f);
		auto s2 = Sprite::createWithSpriteFrameName(this->getSpriteName(i, true));
		s2->setScale(s1->getScale());

		auto i1 = MenuItemSpriteExtra::create(s1, [this](Node* a)
		{
			int tag = a->getTag();

			IconType mode;
			if (tag == 9)
				mode = IconType::kIconTypeSpecial;
			else if (tag == 10)
				mode = IconType::kIconTypeDeathEffect;
			else
				mode = static_cast<IconType>(tag);

			selectMode(mode, true);
		});
		i1->setTag(i);
		menu->addChild(i1);
	}

	menu->alignItemsHorizontallyWithPadding(0);
	menu->setPosition({size.width / 2, unlock->getPositionY() + 30 + 3});
	this->addChild(menu);

	auto menuArr = Menu::create();
	_pageArrowMenu = menuArr;
	menuArr->setPosition({0, 0});

	// Robtop aqui hace otra peruanada de usar "GJ_arrow_%02d_001.png" para las flechas etc...

	auto arrow1 = Sprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
	arrow1->setScale(.8f);

	auto changePage = [this](bool next) {
		if (_colorMode)
		{
			constexpr int kPerPage = 36;
			const int maxPage = (GameToolbox::colorForIdxCount() - 1) / kPerPage;
			if ((!next && _colorPage <= 0) || (next && _colorPage >= maxPage))
				return;
			_colorPage = next ? _colorPage + 1 : _colorPage - 1;
			setupColorPage();
			return;
		}

		const int gameMode = selectedGameModeInt();
		if (gameMode < 0 || gameMode >= static_cast<int>(_modePages.size()))
			return;

		int page = _modePages[gameMode];
		const int maxIcon = GameToolbox::getValueForGamemode(_selectedMode);
		if (maxIcon < 1)
			return;

		const int maxPage = (maxIcon - 1) / 36;
		if ((!next && page <= 0) || (next && page >= maxPage))
			return;

		page = next ? page + 1 : page - 1;
		_modePages[gameMode] = page;
		setupPage(_selectedMode, page);
	};

	auto arrowLeftBtn = MenuItemSpriteExtra::create(arrow1, [changePage](Node*) {
		changePage(false);
	});
	arrowLeftBtn->setPosition({bg->getPositionX() - 220, bg->getPositionY()});
	menuArr->addChild(arrowLeftBtn);

	auto arrow2 = Sprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
	arrow2->setScale(.8f);
	arrow2->setFlippedX(true);
	auto arrowRightBtn = MenuItemSpriteExtra::create(arrow2, [changePage](Node*) {
		changePage(true);
	});
	arrowRightBtn->setPosition({bg->getPositionX() + 220, bg->getPositionY()});
	menuArr->addChild(arrowRightBtn);

	this->addChild(menuArr);

	_selectSprite = Sprite::createWithSpriteFrameName("GJ_select_001.png");
	_selectSprite->setScale(.9f);
	this->addChild(_selectSprite, 10);

	selectMode(GameManager::getInstance()->_mainSelectedMode, true);
}

const char* GarageLayer::getSpriteName(int id, bool actived)
{
	switch (id)
	{
		case 0: return actived ? "gj_iconBtn_on_001.png" : "gj_iconBtn_off_001.png";
		case 1:	return actived ? "gj_shipBtn_on_001.png" : "gj_shipBtn_off_001.png";
		case 2: return actived ? "gj_ballBtn_on_001.png" : "gj_ballBtn_off_001.png";
		case 3: return actived ? "gj_birdBtn_on_001.png" : "gj_birdBtn_off_001.png";
		case 4: return actived ? "gj_dartBtn_on_001.png" : "gj_dartBtn_off_001.png";
		case 5: return actived ? "gj_robotBtn_on_001.png" : "gj_robotBtn_off_001.png";
		case 6: return actived ? "gj_spiderBtn_on_001.png" : "gj_spiderBtn_off_001.png";
		case 7: return actived ? "gj_swingBtn_on_001.png" : "gj_swingBtn_off_001.png";
		case 8: return actived ? "gj_jetpackBtn_on_001.png" : "gj_jetpackBtn_off_001.png";
		case 9: return actived ? "gj_streakBtn_on_001.png" : "gj_streakBtn_off_001.png";
		case 10: return actived ? "gj_explosionBtn_on_001.png" : "gj_explosionBtn_off_001.png";
	}
	return "gj_iconBtn_off_001.png";
}

void GarageLayer::setupPage(IconType type, int page)
{
	_colorMode = false;
	if (_unlockLabel)
		_unlockLabel->setVisible(true);
	if (_pageArrowMenu)
		_pageArrowMenu->setVisible(true);
	if (_colorUiLayer)
	{
		_colorUiLayer->removeFromParent();
		_colorUiLayer = nullptr;
	}

	if (_selectSprite != nullptr)
		GameToolbox::log("posx: {}, posy {}", _selectSprite->getPositionX(), _selectSprite->getPositionY());
	GameToolbox::log("page: {}", page);
	_selectedMode = type;
	// aqui robtop hace cosas con funciones del gamemanager, ni idea
	auto size = Director::getInstance()->getWinSize();

	if (_menuIcons)
	{
		_menuIcons->removeFromParent();
		_menuIcons = nullptr;
	}

	_menuIcons = Menu::create();
	_menuIcons->setPosition(0, 0);

	if (page < 0)
		page = 0;

	int maxIcon = GameToolbox::getValueForGamemode(type);
	if (maxIcon < 1)
	{
		this->addChild(_menuIcons);
		return;
	}

	const int maxPage = (maxIcon - 1) / 36;
	if (page > maxPage)
		page = maxPage;

	int i = page * 36 + 1;
	int max = std::min((page + 1) * 36, maxIcon);
	int selectedForGameMode = GameManager::getInstance()->getSelectedIcon(type);
	if (selectedForGameMode < 1)
		selectedForGameMode = 1;
	auto gm = GameManager::getInstance();

	if (_selectSprite)
		_selectSprite->setVisible(false);

	for (; i <= max; i++)
	{
		auto browserItem = Sprite::createWithSpriteFrameName("playerSquare_001.png");
		if (!browserItem)
			continue;
		browserItem->setStretchEnabled(false);
		browserItem->setOpacity(0);

		if (type == IconType::kIconTypeSpecial)
		{
			auto icono = Sprite::createWithSpriteFrameName(StringUtils::format("player_special_%02d_001.png", i));
			if (!icono)
				continue;
			icono->setColor({170, 170, 170});
			icono->setAnchorPoint({0.5f, 0.5f});
			icono->setPosition(browserItem->getContentSize() / 2);
			const float w = std::max(icono->getContentSize().width, 1.f);
			const float h = std::max(icono->getContentSize().height, 1.f);
			icono->setScale(26.f / std::max(w, h));
			browserItem->addChild(icono);
		}
		else if (type == IconType::kIconTypeDeathEffect)
		{
			auto icono = Sprite::createWithSpriteFrameName(StringUtils::format("explosionIcon_%02d_001.png", i));
			if (!icono)
				continue;
			icono->setColor({170, 170, 170});
			icono->setAnchorPoint({0.5f, 0.5f});
			icono->setPosition(browserItem->getContentSize() / 2);
			const float w = std::max(icono->getContentSize().width, 1.f);
			const float h = std::max(icono->getContentSize().height, 1.f);
			icono->setScale(26.f / std::max(w, h));
			browserItem->addChild(icono);
		}
		else
		{
			auto icono = SimplePlayer::create(0);
			if (!icono)
				continue;
			icono->updateGamemode(i, type);
			if (type == IconType::kIconTypeUfo)
			{
				if (icono->m_pDomeSprite)
					icono->m_pDomeSprite->setVisible(false);
			}
			icono->applyGarageStyle(26.f);
			icono->placeCenteredAt(browserItem->getContentSize() / 2);
			browserItem->addChild(icono);
		}

		const bool unlocked = gm->isIconUnlocked(type, i);
		if (!unlocked)
			browserItem->setColor({70, 70, 70});

		auto btn = MenuItemSpriteExtra::create(browserItem, [this](Node* a)
		{
			if (!_iconPrev || !a)
				return;
			auto gm = GameManager::getInstance();
			if (!gm->isIconUnlocked(_selectedMode, a->getTag()))
				return;
			_iconPrev->updateGamemode(a->getTag(), _selectedMode);
			gm->setSelectedIcon(_selectedMode, a->getTag());
			gm->_mainSelectedMode = _selectedMode;
			applyPreviewColors();
			centerPreviewIcon();
			gm->save();
			if (_selectSprite)
			{
				_selectSprite->setPosition(a->getPosition());
				_selectSprite->setVisible(true);
			}
			updateModeTabs();
		});
		btn->setTag(i);
		// Strict 12x3 cell grid (30px pitch) so icons never drift between modes.
		const int slot = i - page * 36 - 1;
		const int col = slot % _numPerRow;
		const int row = slot / _numPerRow;
		btn->setPosition({size.width / 2 - 165 + col * 30.f, size.height / 2 - 35 - row * 30.f});
		_menuIcons->addChild(btn);
		if (i == selectedForGameMode && _selectSprite)
		{
			_selectSprite->setPosition(btn->getPosition());
			_selectSprite->setVisible(true);
		}
	}

	this->addChild(_menuIcons);
}

void GarageLayer::centerPreviewIcon()
{
	if (!_iconPrev)
		return;

	_iconPrev->setAnchorPoint({0.f, 0.f});
	_iconPrev->setScale(1.f);
	_iconPrev->fitToSize(42.f);
	_iconPrev->placeCenteredAt(_previewCenter);
}

void GarageLayer::applyPreviewColors()
{
	auto gm = GameManager::getInstance();
	_iconPrev->setMainColor(gm->getPlayerMainColor());
	_iconPrev->setSecondaryColor(gm->getPlayerSecondaryColor());
	_iconPrev->setGlow(gm->isPlayerGlowEnabled());
	_iconPrev->setGlowColor(gm->getPlayerGlowColor());
}

void GarageLayer::refreshCurrentPage()
{
	if (_colorMode)
		setupColorPage();
	else
		setupPage(_selectedMode, _modePages[selectedGameModeInt()]);
}

void GarageLayer::setupColorPage()
{
	_colorMode = true;
	if (_unlockLabel)
		_unlockLabel->setVisible(false);
	if (_pageArrowMenu)
		_pageArrowMenu->setVisible(true);
	if (_colorUiLayer)
	{
		_colorUiLayer->removeFromParent();
		_colorUiLayer = nullptr;
	}
	if (_selectSprite)
		_selectSprite->setVisible(false);

	_colorUiLayer = Node::create();
	this->addChild(_colorUiLayer);

	auto size = Director::getInstance()->getWinSize();
	auto gm = GameManager::getInstance();

	if (_menuIcons)
	{
		_menuIcons->removeFromParent();
		_menuIcons = nullptr;
	}

	_menuIcons = Menu::create();
	_menuIcons->setPosition(0, 0);

	constexpr int kPerPage = 36;
	const int totalColors = GameToolbox::colorForIdxCount();
	const int maxPage = std::max(0, (totalColors - 1) / kPerPage);
	_colorPage = std::clamp(_colorPage, 0, maxPage);
	const int startIdx = _colorPage * kPerPage;
	const int endIdx = std::min(startIdx + kPerPage, totalColors);

	const char* slotLabels[] = {"1", "2", "G"};
	for (int slot = 0; slot < 3; ++slot)
	{
		Color3B slotColor = slot == 0 ? gm->getPlayerMainColor()
			: slot == 1 ? gm->getPlayerSecondaryColor()
			: gm->getPlayerGlowColor();

		auto slotBtn = Sprite::createWithSpriteFrameName("GJ_colorBtn_001.png");
		slotBtn->setColor(slotColor);
		slotBtn->setScale(0.55f);

		auto slotItem = MenuItemSpriteExtra::create(slotBtn, [this, slot](Node*) {
			_activeColorSlot = slot;
		});
		slotItem->setPosition({size.width / 2 - 165 + slot * 35.f, size.height / 2 - 65 + 58.f});
		_menuIcons->addChild(slotItem);

		auto slotLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), slotLabels[slot]);
		slotLabel->setScale(0.35f);
		slotLabel->setPosition({size.width / 2 - 165 + slot * 35.f, size.height / 2 - 65 + 76.f});
		_colorUiLayer->addChild(slotLabel);
	}

	for (int colorIndex = startIdx; colorIndex < endIdx; ++colorIndex)
	{
		const Color3B color = GameToolbox::colorForIdx(colorIndex);
		auto swatch = Sprite::createWithSpriteFrameName("GJ_colorBtn_001.png");
		swatch->setColor(color);
		swatch->setScale(0.75f);

		auto btn = MenuItemSpriteExtra::create(swatch, [this, color](Node*) {
			auto gm = GameManager::getInstance();
			switch (_activeColorSlot)
			{
			case 0: gm->setPlayerMainColor(color); break;
			case 1: gm->setPlayerSecondaryColor(color); break;
			default:
				gm->setPlayerGlowColor(color);
				gm->setPlayerGlowEnabled(true);
				break;
			}
			applyPreviewColors();
			gm->save();
			setupColorPage();
		});

		const int slot = colorIndex - startIdx;
		const int col = slot % _numPerRow;
		const int row = slot / _numPerRow;
		btn->setPosition({size.width / 2 - 165 + col * 30.f, size.height / 2 - 65 + 30 - row * 30.f});
		_menuIcons->addChild(btn);
	}

	this->addChild(_menuIcons);
}
