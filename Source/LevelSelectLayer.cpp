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

#include "LevelSelectLayer.h"
#include "GJGameLevel.h"
#include "MenuItemSpriteExtra.h"
#include "AudioEngine.h"
// #include "PlayLayer.h"

#include "MenuLayer.h"
#include "LevelPage.h"
#include "SongsLayer.h"
//#include "Checkbox.h"
#include "GroundLayer.h"
#include "BoomScrollLayer.h"
#include "2d/Menu.h"
#include "2d/Label.h"
#include "2d/ActionInterval.h"
#include "EventListenerKeyboard.h"
#include "2d/Transition.h"
#include "EventDispatcher.h"
#include "base/Director.h"
#include "GameToolbox/log.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/nodes.h"
#include "LevelTools.h"
#include "GameToolbox/conv.h"
#include <algorithm>
#include <cstdint>

USING_NS_AX;

namespace
{
// Official LevelSelectLayer::colorForPage → GameManager::colorForIdx IDs (GD 2.2074).
// Page colors cycle every 9 levels (SM→Cycles, then xStep→ToE2, then Geo Dom→Dash).
const int kPageColorIdx[9] = {5, 7, 8, 9, 10, 11, 1, 3, 4};

Color3B scaleColor(const Color3B& c, float factor)
{
	return {
		static_cast<uint8_t>(std::clamp(static_cast<int>(c.r * factor), 0, 255)),
		static_cast<uint8_t>(std::clamp(static_cast<int>(c.g * factor), 0, 255)),
		static_cast<uint8_t>(std::clamp(static_cast<int>(c.b * factor), 0, 255)),
	};
}
}

Color3B LevelSelectLayer::colorForPage(int page)
{
	if (page < 0)
		page = 0;
	// Coming Soon slots (page % 22 == 0, page != 0)
	if (page != 0 && page % 22 == 0)
		return {0x25, 0x2c, 0x34};
	return GameToolbox::colorForIdx(kPageColorIdx[page % 9]);
}

void LevelSelectLayer::applyPageColor(int page, bool animate)
{
	const Color3B color = colorForPage(page);
	// Original scrollLayerMoved tints ground at 0.8× page color.
	const Color3B groundColor = scaleColor(color, 0.8f);
	if (_background)
	{
		_background->stopAllActions();
		if (animate)
			_background->runAction(TintTo::create(0.45f, color.r, color.g, color.b));
		else
			_background->setColor(color);
	}
	if (_ground && _ground->_sprite)
	{
		_ground->_sprite->stopAllActions();
		if (animate)
			_ground->_sprite->runAction(TintTo::create(0.45f, groundColor.r, groundColor.g, groundColor.b));
		else
			_ground->_sprite->setColor(groundColor);
	}
}

void LevelSelectLayer::updateForWinSize()
{
	const auto winSize = Director::getInstance()->getWinSize();
	if (_background)
	{
		const auto texSize = _background->getTextureRect().size;
		if (texSize.width > 0.f && texSize.height > 0.f)
		{
			_background->setScaleX((winSize.width + 10.0f) / texSize.width);
			_background->setScaleY((winSize.height + 10.0f) / texSize.height);
		}
		_background->setPosition({-5.0f, -5.0f});
	}
	if (_ground)
		_ground->updateForWinSize();
}


Scene* LevelSelectLayer::scene(int page)
{
	auto scene = Scene::create();
	scene->addChild(LevelSelectLayer::create(page));
	return scene;
}

LevelSelectLayer* LevelSelectLayer::create(int page) {
	LevelSelectLayer* pRet = new LevelSelectLayer();
	if (pRet->init(page)) {
		pRet->autorelease();
		return pRet;
	} else {
		delete pRet;
		pRet = nullptr;
		return nullptr;
	}
}

bool LevelSelectLayer::init(int page)
{

	if(!Layer::init()) return false;

	auto director = Director::getInstance();
	const auto& winSize = director->getWinSize();

	_background = Sprite::create("GJ_gradientBG.png");
	_background->setAnchorPoint({0.0f, 0.0f});
	addChild(_background, -2);
	updateForWinSize();
	_ground = GroundLayer::create(1);
	if (!_ground)
		return false;
	_ground->_followPlayLayerColors = false;
	_ground->setPositionY(-25.f);
	addChild(_ground, -1);

	GameToolbox::createCorners(this, false, false, true, true);
	
	constexpr auto levelData = std::to_array<std::tuple<const char*, const char*, int>>({
		{ "Stereo Madness", "RobTop", 1 },
		{ "Back on Track", "RobTop", 2 },
		{ "Polargeist", "RobTop", 3 },
		{ "Dry Out", "RobTop", 4 },
		{ "Base After Base", "RobTop", 5 },
		{ "Cant Let Go", "RobTop", 6 },
		{ "Jumper", "RobTop", 7 },
		{ "Time Machine", "RobTop", 8 },
		{ "Cycles", "RobTop", 9 },
		{ "xStep", "RobTop", 10 },
		{ "Clutterfunk", "RobTop", 11 },
		{ "Theory Of Everything", "RobTop", 12 },
		{ "Electroman Adventures", "RobTop", 13 },
		{ "Clubstep", "RobTop", 14 },
		{ "Electrodynamix", "RobTop", 15 },
		{ "Hexagon Force", "RobTop", 16 },
		{ "Blast Processing", "RobTop", 17 },
		{ "Theory Of Everything 2", "RobTop", 18 },
		{ "Geometrical Dominator", "RobTop", 19 },
		{ "Deadlocked", "RobTop", 20 },
		{ "Fingerdash", "RobTop", 21 }
		// Dash (id 22) hidden from the official level select.
	});
	
	//TODO: add getters on level page because they are actually owning the stuff

	std::vector<Layer*> layers;
	layers.reserve(levelData.size());
	
	for (const auto& [name, creator, id] : levelData)
	{
		auto level = GJGameLevel::createWithMinimumData(name, creator, id);
		LevelTools::applyMainLevelRating(level);
		if (level && level->_levelString.empty())
			level->_levelString = GJGameLevel::getLevelStrFromID(id);
		auto* page = LevelPage::create(level);
		if (page)
			layers.push_back(page);
		else
			GameToolbox::log("LevelPage::create failed for official level {}", id);
	}
	_levelPages = layers;
	_bsl = BoomScrollLayer::create(layers, page);
	if (_bsl)
	{
		_bsl->_onPageChanged = [this](int currentPage) {
			applyPageColor(currentPage, true);
		};
		addChild(_bsl);
		applyPageColor(_bsl->_currentPage, false);
	}
	else
		GameToolbox::log("BoomScrollLayer::create failed ({} pages)", layers.size());

	auto btnMenu = Menu::create();
	addChild(btnMenu, 5);

	//bool controller = PlatformToolbox::isControllerConnected();
	bool controller = false;

	auto left =
		Sprite::createWithSpriteFrameName(controller ? "controllerBtn_DPad_Left_001.png" : "navArrowBtn_001.png");
	if (!controller) left->setFlippedX(true);

	MenuItemSpriteExtra* leftBtn = MenuItemSpriteExtra::create(left, [this](Node*) {
		if (_bsl)
			_bsl->changePageLeft();
	});
	btnMenu->addChild(leftBtn);

	leftBtn->setPosition(btnMenu->convertToNodeSpace({ 25.0f, winSize.height / 2 }));

	auto right = Sprite::createWithSpriteFrameName(controller ? "controllerBtn_DPad_Right_001.png" : "navArrowBtn_001.png");

	MenuItemSpriteExtra* rightBtn = MenuItemSpriteExtra::create(right, [this](Node*) {
		if (_bsl)
			_bsl->changePageRight();
	});
	btnMenu->addChild(rightBtn);

	rightBtn->setPosition(btnMenu->convertToNodeSpace({ winSize.width - 25.0f, winSize.height / 2 }));

	auto back = Sprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
		MenuItemSpriteExtra* backBtn =
			MenuItemSpriteExtra::create(back, []( Node const* btn) { 
			Director::getInstance()->replaceScene(TransitionFade::create(0.5f, MenuLayer::scene()));
	});
	
	//auto bglCheckbox = Checkbox::create("BaseGameLayer", [this](Node* btn, bool on)
	//{
	//	GameToolbox::log("on: {}", on);
	//	if (auto currentLevelPage = dynamic_cast<LevelPage*>(_bsl->_layers.at(_bsl->_currentPage)))
	//	{
	//		currentLevelPage->_openBGL = on;
	//	}
	//});
	Menu* backMenu = Menu::create();

	addChild(backMenu, 1);
	backMenu->addChild(backBtn);
	//backMenu->addChild(bglCheckbox);
	backMenu->setPosition({25.0f, winSize.height - 22.0f });
	//bglCheckbox->setPosition({backMenu->convertToNodeSpace({winSize.width / 2 + 15.0f, winSize.height - 40.0f})});

	auto infoMenu = Menu::create();
	addChild(infoMenu);

	Sprite* info = Sprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
	MenuItemSpriteExtra* infoBtn = MenuItemSpriteExtra::create(info, [](Node* btn) {

	});
	infoMenu->addChild(infoBtn, 1);

	infoMenu->setPosition({ winSize.width - 20.0f, winSize.height - 20.0f });


	auto dlLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Download the soundtracks");
	dlLabel->setScale(0.5);

	auto dlMenuItem = MenuItemSpriteExtra::create(dlLabel, [](Node*) {
		SongsLayer::create()->showLayer(true, false);
	});

	auto dlLabelMenu = Menu::create();
	dlLabelMenu->addChild(dlMenuItem);
	addChild(dlLabelMenu);
	dlLabelMenu->setPosition({ winSize.width / 2, 35 });


	auto listener = EventListenerKeyboard::create();

	// int currentlevel = 0;

	listener->onKeyPressed = [this](EventKeyboard::KeyCode code, Event const*)
	{
		using enum ax::EventKeyboard::KeyCode;
		if (code == KEY_ESCAPE)
		{
			auto scene = MenuLayer::scene();
			Director::getInstance()->replaceScene(TransitionFade::create(0.5f, scene));
			// GameToolbox::popSceneWithTransition(0.5f);
		}
		else if (code == KEY_LEFT_ARROW) {
			if (_bsl)
				_bsl->changePageLeft();
		} else if (code == KEY_RIGHT_ARROW) {
			if (_bsl)
				_bsl->changePageRight();
		} 		else if (code == KEY_SPACE)
		{
			if (_bsl && _bsl->_currentPage >= 0 && _bsl->_currentPage < static_cast<int>(_bsl->_layers.size()))
			{
				if (auto currentLevelPage = dynamic_cast<LevelPage*>(_bsl->_layers.at(_bsl->_currentPage)))
					currentLevelPage->onPlay(nullptr);
			}
		}
	};

	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

	//if (controller) GameToolbox::addBackButton(this, backBtn);
	return true;
}

void LevelSelectLayer::onExit()
{
	if (_ground)
		_ground->unscheduleUpdate();
	Layer::onExit();
}