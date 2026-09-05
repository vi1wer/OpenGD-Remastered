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

#include "CreatorLayer.h"
#include <AudioEngine.h>
#include "LevelPage.h"
#include "GJGameLevel.h"
#include "LevelBrowserLayer.h"
#include "LevelInfoLayer.h"
#include "LevelSearchLayer.h"
#include "MyLevelsLayer.h"
#include "MenuItemSpriteExtra.h"
#include "MenuLayer.h"
#include "PlayLayer.h"
#include <network/HttpClient.h>
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "2d/Transition.h"
#include "EventListenerKeyboard.h"
#include "base/Director.h"
#include "EventDispatcher.h"
#include "GJSearchObject.h"
#include "SecretLayer2.h"
#include "ComingSoonLayer.h"

#include "GameToolbox/log.h"
#include "GameToolbox/nodes.h"
#include "GameToolbox/conv.h"
#include "platform/FileUtils.h"
#include <filesystem>
#include <string_view>


USING_NS_AX;

using namespace ax::network;

Scene* CreatorLayer::scene() {
	return CreatorLayer::create();
}

CreatorLayer* CreatorLayer::create()
{
	CreatorLayer* ret = new CreatorLayer();
	if (ret->init())
	{
		ret->autorelease();
		return ret;
	} 
	else
	{
		delete ret;
		ret = nullptr;
		return nullptr;
	}
}

bool CreatorLayer::init()
{
	if (!Scene::init())
		return false;

	auto director = Director::getInstance();
	const auto& winSize = director->getWinSize();

	GameToolbox::createBG(this, {0, 102, 255});
	// Official 2.2 CreatorLayer only has left side art (lock + door sit on the right).
	GameToolbox::createCorners(this, true, false, true, false);

	auto* exitMenu = Menu::create();
	exitMenu->setPosition({0, 0});
	addChild(exitMenu);

	auto* backBtn = MenuItemSpriteExtra::create("GJ_arrow_01_001.png", [](Node*) {
		Director::getInstance()->replaceScene(TransitionFade::create(.5f, MenuLayer::scene()));
	});
	backBtn->setPosition({24.0f, winSize.height - 23.0f});
	exitMenu->addChild(backBtn);

	struct CreatorButton
	{
		const char* frame;
		const char* name;
		bool locked;
	};

	// Official 2.2 order: 5 columns x 3 rows.
	const CreatorButton buttons[] = {
		{"GJ_createBtn_001.png", "Create", false},
		{"GJ_savedBtn_001.png", "Saved", false},
		{"GJ_highscoreBtn_001.png", "Scores", false},
		{"GJ_challengeBtn_001.png", "Quests", false},
		{"GJ_versusBtn_001.png", "Versus", true},
		{"GJ_mapBtn_001.png", "The Map", true},
		{"GJ_dailyBtn_001.png", "Daily", false},
		{"GJ_weeklyBtn_001.png", "Weekly", false},
		{"GJ_eventBtn_001.png", "Event", false},
		{"GJ_gauntletsBtn_001.png", "Gauntlets", false},
		{"GJ_featuredBtn_001.png", "Featured", false},
		{"GJ_listsBtn_001.png", "Lists", false},
		{"GJ_pathsBtn_001.png", "Paths", false},
		{"GJ_mapPacksBtn_001.png", "Map Packs", false},
		{"GJ_searchBtn_001.png", "Search", false},
	};

	auto comingSoon = [this](std::string_view name) {
		auto* banner = ComingSoonLayer::create(name);
		if (!banner)
			return;
		addChild(banner, 200);
		banner->show();
	};

	auto onButtonsCallback = [this, comingSoon](Node* btn) {
		switch (btn->getTag())
		{
		case 0: {
			Director::getInstance()->pushScene(TransitionFade::create(0.5f, MyLevelsLayer::scene()));
			return;
		}
		case 1: {
#ifdef _WIN32
			auto* fu = FileUtils::getInstance();
			std::filesystem::path appdata(fu->getWritablePath());
			appdata = appdata.append("../GeometryDash/CCLocalLevels.dat");
			if (!std::filesystem::exists(appdata))
			{
				comingSoon("Saved");
				return;
			}
			std::string data = fu->getStringFromFile(appdata.string());
			if (data.empty())
			{
				comingSoon("Saved");
				return;
			}
			data = GJGameLevel::decompressLvlStr(GameToolbox::xorFunction(data, 11));
			size_t pos = data.find("opengd");
			if (pos == std::string::npos)
			{
				comingSoon("Saved");
				return;
			}
			size_t startPos = data.find("H4sIAAAAAAAA", pos);
			if (startPos == std::string::npos)
			{
				comingSoon("Saved");
				return;
			}
			size_t endPos = data.find("</s>");
			if (endPos == std::string::npos)
			{
				comingSoon("Saved");
				return;
			}
			auto* level = GJGameLevel::createWithMinimumData("OpenGD", "creator", 33);
			level->_levelString = data.substr(startPos, endPos - startPos);
			Director::getInstance()->pushScene(TransitionFade::create(0.5f, PlayLayer::scene(level)));
#else
			comingSoon("Saved");
#endif
			return;
		}
		case 10:
			Director::getInstance()->pushScene(
				TransitionFade::create(0.5f, LevelBrowserLayer::scene(GJSearchObject::create(kGJSearchTypeFeatured))));
			return;
		case 13:
			Director::getInstance()->pushScene(
				TransitionFade::create(0.5f, LevelBrowserLayer::scene(GJSearchObject::create(kGJSearchTypeMapPack))));
			return;
		case 14:
			Director::getInstance()->pushScene(TransitionFade::create(0.5f, LevelSearchLayer::scene()));
			return;
		default:
			comingSoon(btn->getName());
			return;
		}
	};

	auto* gridMenu = Menu::create();
	gridMenu->setPosition(winSize / 2);
	addChild(gridMenu);

	for (int i = 0; i < 15; i++)
	{
		auto* spr = Sprite::createWithSpriteFrameName(buttons[i].frame);
		if (!spr)
			continue;
		spr->setStretchEnabled(false);
		if (buttons[i].locked)
			spr->setColor({90, 90, 90});
		spr->setScale(0.8f);
		auto* btn = MenuItemSpriteExtra::create(spr, onButtonsCallback);
		btn->setTag(i);
		btn->setName(buttons[i].name);
		btn->setPosition({(i % 5 - 2) * 92.0f, (1 - i / 5) * 88.0f});
		gridMenu->addChild(btn);
	}

	if (auto* secretLock = Sprite::createWithSpriteFrameName("GJ_lock_open_001.png"))
	{
		secretLock->setStretchEnabled(false);
		auto* secretLockBtn = MenuItemSpriteExtra::create(secretLock, [](Node*) {
			Director::getInstance()->replaceScene(TransitionFade::create(.5f, SecretLayer2::scene()));
		});
		secretLockBtn->setPosition({winSize.width / 2 - 22.0f, winSize.height / 2 - 24.0f});
		gridMenu->addChild(secretLockBtn);
	}

	const char* doorFrame = "secretDoorBtn_open_001.png";
	if (!SpriteFrameCache::getInstance()->getSpriteFrameByName(doorFrame))
		doorFrame = "secretDoorBtn2_open_001.png";
	if (auto* doorSpr = Sprite::createWithSpriteFrameName(doorFrame))
	{
		doorSpr->setStretchEnabled(false);
		auto* doorBtn = MenuItemSpriteExtra::create(doorSpr, [](Node*) {
			Director::getInstance()->replaceScene(TransitionFade::create(.5f, SecretLayer2::scene()));
		});
		doorBtn->setPosition({winSize.width / 2 - 18.0f, 18.0f - winSize.height / 2});
		gridMenu->addChild(doorBtn);
	}
	
	auto listener = EventListenerKeyboard::create();

	listener->onKeyPressed  = AX_CALLBACK_2(CreatorLayer::onKeyPressed, this);

	director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);

	return true;
}

//TODO: add keybinds for other stuff
void CreatorLayer::onKeyPressed(ax::EventKeyboard::KeyCode keyCode, ax::Event* event) {
	switch (keyCode) 
	{
	case EventKeyboard::KeyCode::KEY_BACK:
		Director::getInstance()->replaceScene(TransitionFade::create(0.5f, MenuLayer::scene()));
		break;
	default:
		break;
	}
}
