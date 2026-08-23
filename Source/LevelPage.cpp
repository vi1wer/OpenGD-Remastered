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

#include "LevelPage.h"
#include "MenuItemSpriteExtra.h"
#include "core/ui/UIScale9Sprite.h"
#include "PlayLayer.h"
#include <AudioEngine.h>

#include "LevelDebugLayer.h"
#include "LevelEditorLayer.h"

#include "2d/Label.h"
#include "2d/Menu.h"
#include "2d/Transition.h"
#include "MenuLayer.h"
#include "GJGameLevel.h"
#include "base/Director.h"
#include "GameToolbox/log.h"
#include "GameToolbox/getTextureString.h"

#include "ButtonSprite.h"
#include "fmt/format.h"
#include <algorithm>

bool LevelPage::replacingScene = false;

bool LevelPage::init(GJGameLevel* level)
{
	if (!Layer::init()) return false;
	
	if (!level)
		return false;

	//testing
	//level->_normalPercent = static_cast<float>(GameToolbox::randomInt(0, 100));
	//level->_practicePercent = static_cast<float>(GameToolbox::randomInt(0, 100));
	
	GameToolbox::log("normal: {}, practice: {}", level->_normalPercent, level->_practicePercent);
	
	_level = level;
	std::string barTexture = GameToolbox::getTextureString("GJ_progressBar_001.png");
	std::string bigFontTexture = GameToolbox::getTextureString("bigFont.fnt");

	const auto& winSize = ax::Director::getInstance()->getWinSize();

	auto addProgressBar = [&](float y, float percent, const char* title, const ax::Color3B& fillColor) {
		percent = std::clamp(percent, 0.f, 100.f);

		auto* bar = ax::Sprite::create(barTexture);
		if (!bar)
			return;
		bar->setPosition({winSize.width / 2.f, y});
		bar->setColor({0, 0, 0});
		bar->setOpacity(125);
		addChild(bar, 3);

		auto* fill = ax::Sprite::create(barTexture);
		if (fill)
		{
			fill->setAnchorPoint({0.f, 0.5f});
			fill->setPosition({1.36f, bar->getContentSize().height * 0.5f});
			fill->setColor(fillColor);
			fill->setTextureRect({0.f, 0.f,
				bar->getContentSize().width * (percent / 100.f),
				bar->getTextureRect().size.height});
			fill->setScaleX(0.992f);
			fill->setScaleY(0.86f);
			bar->addChild(fill);
		}

		auto* titleLabel = ax::Label::createWithBMFont(bigFontTexture, title);
		titleLabel->setPosition({winSize.width / 2.f, y + 20.f});
		titleLabel->setScale(0.55f);
		addChild(titleLabel, 4);

		auto* percLabel = ax::Label::createWithBMFont(bigFontTexture, fmt::format("{}%", static_cast<int>(percent)));
		percLabel->setPosition({winSize.width / 2.f, y});
		percLabel->enableShadow(ax::Color4B::BLACK, {0.2f, -0.2f});
		percLabel->setScale(0.55f);
		addChild(percLabel, 4);
	};

	addProgressBar(winSize.height / 2.f - 30.f, level->_normalPercent, "Normal Mode", {0, 255, 0});
	addProgressBar(winSize.height / 2.f - 80.f, level->_practicePercent, "Practice Mode", {0, 255, 255});


	
	auto scale9 = ax::ui::Scale9Sprite::create("square02_001.png");
	if (!scale9)
		return false;
	scale9->setContentSize({340, 95});
	scale9->setOpacity(125);
	
	auto levelName = ax::Label::createWithBMFont(bigFontTexture, level->_levelName);
	if (levelName)
	{
		levelName->setPosition(190, 50.5);
		levelName->setScale(0.904f);
		scale9->addChild(levelName, 0);
	}

	if (auto diffIcon = ax::Sprite::createWithSpriteFrameName(GJGameLevel::getDifficultySprite(level, kMainLevels)))
	{
		diffIcon->setScale(1.1f);
		diffIcon->setPosition(35.75, 50.5);
		scale9->addChild(diffIcon, 0);
	}

	//1.0 didnt have stars apparently
	// auto starIcon = ax::Sprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
	// starIcon->setScale(0.7);
	// starIcon->setPosition({325, 82});
	// mainNode->addChild(starIcon, 0);

	// auto starAmt = ax::Label::createWithBMFont("bigFont.fnt", std::to_string(level->_Stars));
	// starAmt->setPosition({313, 82.5});
	// starAmt->setScale(0.5);
	// starAmt->setAnchorPoint({1, 0.5});
	// mainNode->addChild(starAmt, 0);
	auto mainBtn = MenuItemSpriteExtra::create(scale9, [this](Node* btn) { onPlay(btn); });
	mainBtn->setScaleMultiplier(1.1f);
	auto levelMenu = ax::Menu::create();
	levelMenu->addChild(mainBtn);
	levelMenu->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 60 });
	auto buttonSprite = ButtonSprite::create("editor", 0x32, 0, 0.6, false, GameToolbox::getTextureString("bigFont.fnt"), GameToolbox::getTextureString("GJ_button_01.png"), 30);
	MenuItemSpriteExtra* button = MenuItemSpriteExtra::create(buttonSprite, [this](Node* btn)
	{
		if (LevelPage::replacingScene)
			return;

		ax::Scene* scene = LevelEditorLayer::scene(_level);
		ax::AudioEngine::stopAll();
		ax::AudioEngine::play2d("playSound_01.ogg", false, 0.2f);
		ax::Director::getInstance()->replaceScene(ax::TransitionFade::create(0.5f, scene));
		LevelPage::replacingScene = true;
		MenuLayer::music = false;
	});
	button->setPositionY(70);
	levelMenu->addChild(button);
	addChild(levelMenu);
	
	return true;
}

void LevelPage::onPlay(Node* btn)
{
	if (LevelPage::replacingScene)
		return;

	LevelPage::replacingScene = true;
	GJGameLevel* level = _level;
	_level = nullptr;

	ax::Scene* scene = PlayLayer::scene(level);
	ax::AudioEngine::stopAll();
	ax::AudioEngine::play2d("playSound_01.ogg", false, 0.2f);
	ax::Director::getInstance()->replaceScene(ax::TransitionFade::create(0.5f, scene));
	MenuLayer::music = false;
}

LevelPage::~LevelPage()
{
	delete _level;
	_level = nullptr;
}
LevelPage* LevelPage::create(GJGameLevel* level)
{
	LevelPage* pRet = new LevelPage();
	if (pRet->init(level))
	{
		pRet->autorelease();
		return pRet;
	}
	else
	{
		delete pRet;
		pRet = nullptr;
		return nullptr;
	}
}