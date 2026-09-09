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

#include "LoadingLayer.h"

#include "MenuLayer.h"
#include "CocosExplorer.h"
#include "GameManager.h"

#include "external/constants.h"
#include <array>
#include "2d/SpriteFrameCache.h"
#include "Director.h"
#include "renderer/TextureCache.h"
#include "2d/Label.h"
#include "SimpleProgressBar.h"
#include "2d/ActionInterval.h"
#include "2d/ActionInstant.h"
#include "GameToolbox/log.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/rand.h"
#include "GameToolbox/conv.h"
#include "platform/FileUtils.h"

#include "fmt/format.h"
#include <algorithm>

USING_NS_AX;


LoadingLayer* LoadingLayer::create() {
	LoadingLayer* pRet = new LoadingLayer();
	if (pRet->init()) {
		pRet->autorelease();
		return pRet;
	} else {
		delete pRet;
		pRet = nullptr;
		return nullptr;
	}
}


constexpr static auto splashes = std::to_array <const char*>({
	"Use practice mode to learn the layout of a level",
	"Back for more are ya?",
	"Customize your character's icon and color!",
	"If at first you don't succeed, try, try again...",
	"Listen to the music to help time your jumps",
	"Loading resources",
	"Here be dragons...",
	"Can you beat them all?",
	"Hold down to keep jumping",
	"Pro tip: Don't crash",
	"Spikes are not your friends, don't forget to jump",
	"You can download all songs from the level select page!",
	"Go online to play other players levels!",
	"Build your own levels using the level editor",
	"Does anyone even read this?",
	"Pro tip: Jump",
	"Waiting for planets to align",
	"Collecting scrap metal",
	"Looking for pixels",
	"The spikes whisper to me...",
	"What if the spikes are the good guys?",
	"Loading awesome soundtracks...",
	"Hiding secrets",
	"Calculating chance of success",
	"Programmer is sleeping, please wait",
	"Drawing pretty pictures",
	"Wandering around aimlessly",
	"Starting the flux capacitor",
	"Loading the progressbar",
	"Where did I put that coin...",
	"Loading Rage Cannon",
	"Fus Ro DASH!",
	"It's all in the timing",
	"Counting to 1337",
	"Play, Crash, Rage, Quit, Repeat",
	"RobTop is Love, RobTop is Life",
	"Such wow, very amaze.",
	"Only one button required to crash",
	"The Vault Keeper's name is 'Spooky'...",
	"This seems like a good place to hide a secret...",
	"Shhhh! You're gonna wake the big one!",
	"Hope the big guy doesn't wake up...",
	"Spikes... OF DOOM!",
	"Fake spikes are fake",
	"Loading will be finished... soon",
	"Why don't you go outside?",
	"Programming amazing AI",
	"It's Over 9000!",
	"Spooky doesn't get out much",
	"Hiding secret vault",
	"A wild RubRub appeared!",
	"I have been expecting you.",
	"Hiding rocket launcher",
	"So many secrets...",
	"Why u have to be mad?",
	"I don't know how this works...",
	"Unlock new icons and colors by completing achievements!",
	"It is only game...",
	"Warp Speed",
	"RubRub was here",
	"Hold on, reading the manual",
	"So, what's up?"
});

constexpr static auto pngs = std::to_array<const char*>({
	"GJ_GameSheetGlow.png", "GJ_GameSheet04.png", "GJ_GameSheet03.png", "GJ_GameSheet02.png", "GJ_GameSheet.png",
	"GJ_GameSheetEditor.png", "GJ_gradientBG.png", "edit_barBG_001.png", "GJ_button_01.png",
	"GJ_button_02.png", "GJ_button_04.png", "gravityOverlay.png", "goldFont.png", "bigFont.png", "chatFont.png", "CCControlColourPickerSpriteSheet.png", "GJ_ShopSheet.png", "GJ_ShopSheet01.png", "SecretSheet.png", "TreasureRoomSheet.png", "PixelSheet_01.png"
});

constexpr static auto fonts = std::to_array<const char*>({
	"bigFont.fnt", "chatFont.fnt", "goldFont.fnt"
});

constexpr static auto plists = std::to_array<const char*>({
	"GJ_GameSheetGlow.plist", "GJ_GameSheet.plist", "CCControlColourPickerSpriteSheet.plist", "GJ_GameSheet02.plist", "GJ_GameSheet03.plist",
	"GJ_GameSheet04.plist", "GJ_GameSheetEditor.plist", "GJ_ShopSheet.plist", "GJ_ShopSheet01.plist", "SecretSheet.plist", "TreasureRoomSheet.plist", "PixelSheet_01.plist",
});
	
	
const char* LoadingLayer::getSplash() {
	return splashes[GameToolbox::randomInt(0, splashes.size() - 1)];
}

Scene* LoadingLayer::scene() {
	auto scene = Scene::create();
	scene->addChild(LoadingLayer::create());
	return scene;
}

bool LoadingLayer::init() {
	if (!Layer::init()) return false;
	
	auto dir = Director::getInstance();

	_sprFrameCache = SpriteFrameCache::getInstance();
	_textureCache = dir->getTextureCache();
	
	dir->setContentScaleFactor(GameManager::getInstance()->isHigh() ? 4.0f : 2.0f);
	
	dir->purgeCachedData();
	_sprFrameCache->removeSpriteFrames();
	_textureCache->removeAllTextures();

	size_t totalAssets = fonts.size() + plists.size() + pngs.size() + static_cast<size_t>(getTotalIconPlists());
	this->m_nTotalAssets = static_cast<int>(totalAssets);
	
	_textureCache->addImage(GameToolbox::getTextureString("GJ_LaunchSheet.png"));
	_sprFrameCache->addSpriteFramesWithFile(GameToolbox::getTextureString("GJ_LaunchSheet.plist"));

	auto winSize = dir->getWinSize();

	auto bgSpr = Sprite::create(GameToolbox::getTextureString("game_bg_01_001.png"));
	bgSpr->setName("opengd_fullscreen_bg");
	bgSpr->setStretchEnabled(false);
	bgSpr->setAnchorPoint({0.f, 0.f});
	bgSpr->setPosition({0.f, 0.f});
	bgSpr->setScaleX(winSize.width / bgSpr->getContentSize().width);
	bgSpr->setScaleY(winSize.height / bgSpr->getContentSize().height);
	bgSpr->setColor({ 0, 102, 255 });
	this->addChild(bgSpr);

	auto logoSpr = Sprite::createWithSpriteFrameName("GJ_logo_001.png");
	logoSpr->setStretchEnabled(false);
	logoSpr->setPosition(winSize / 2);
	this->addChild(logoSpr);
	
	auto robLogoSpr = Sprite::createWithSpriteFrameName("RobTopLogoBig_001.png");
	robLogoSpr->setStretchEnabled(false);
	robLogoSpr->setPosition(logoSpr->getPosition() + Vec2(0, 80));
	this->addChild(robLogoSpr);

	auto splash = this->getSplash();
	auto splashText = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), splash);
	splashText->setPosition(winSize.width / 2, (winSize.height / 2) - 100);
	splashText->setScale(0.7f);

	this->addChild(splashText);
	_pBar = SimpleProgressBar::create();
	_pBar->setPercentage(0.f);
	_pBar->setPosition({ winSize.width / 2, splashText->getPosition().height + 40 });
	this->addChild(_pBar);
	
	this->runAction(Sequence::create(DelayTime::create(0), CallFunc::create([this]() { this->loadAssets(); }), nullptr));
	
#if SHOW_IMGUI == true
	CocosExplorer::openForever();
#endif
	

	GameToolbox::log("quality medium: {}, scale factor {}", GameManager::getInstance()->isMedium(), dir->getContentScaleFactor());
	
	return true;
}



void LoadingLayer::loadAssets() {
	
	for(auto image : pngs) {
		_textureCache->addImageAsync(GameToolbox::getTextureString(image), [this](Texture2D*) {
			this->assetLoaded();
		});
	}
	
	for(auto plist : plists) {
		_sprFrameCache->addSpriteFramesWithFile(GameToolbox::getTextureString(plist));
		this->assetLoaded();
	}
	
	for(auto fnt : fonts) {
		Label::createWithBMFont(GameToolbox::getTextureString(fnt), "someText");
		this->assetLoaded();
	}

	startIconLoading();
}

void LoadingLayer::assetLoaded()
{
	if (_finished)
		return;

	this->m_nAssetsLoaded++;
	if (_pBar && m_nTotalAssets > 0.f)
		_pBar->setPercentage((m_nAssetsLoaded / m_nTotalAssets) * 100.f);

	if (m_nAssetsLoaded >= m_nTotalAssets)
	{
		_finished = true;
		this->unschedule("load_icons");
		Director::getInstance()->replaceScene(MenuLayer::scene());
	}
}

int LoadingLayer::getIconPlistCount(int from, int to)
{
	return std::max(0, to - from + 1);
}

int LoadingLayer::getTotalIconPlists()
{
	return getIconPlistCount(0, GameToolbox::getValueForGamemode(IconType::kIconTypeCube))
		 + getIconPlistCount(1, GameToolbox::getValueForGamemode(IconType::kIconTypeShip))
		 + getIconPlistCount(0, GameToolbox::getValueForGamemode(IconType::kIconTypeBall))
		 + getIconPlistCount(1, GameToolbox::getValueForGamemode(IconType::kIconTypeUfo))
		 + getIconPlistCount(1, GameToolbox::getValueForGamemode(IconType::kIconTypeWave))
		 + getIconPlistCount(1, GameToolbox::getValueForGamemode(IconType::kIconTypeRobot))
		 + getIconPlistCount(1, GameToolbox::getValueForGamemode(IconType::kIconTypeSpider))
		 + getIconPlistCount(1, GameToolbox::getValueForGamemode(IconType::kIconTypeSwing))
		 + getIconPlistCount(1, GameToolbox::getValueForGamemode(IconType::kIconTypeJetpack));
}

void LoadingLayer::buildIconQueue()
{
	_iconPlists.clear();
	_iconPlists.reserve(static_cast<std::size_t>(getTotalIconPlists()));

	auto pushRange = [this](const char* prefix, int from, int to) {
		for (int i = from; i <= to; ++i)
			_iconPlists.push_back(StringUtils::format("%s_%02d.plist", prefix, i));
	};

	pushRange("player", 0, GameToolbox::getValueForGamemode(IconType::kIconTypeCube));
	pushRange("ship", 1, GameToolbox::getValueForGamemode(IconType::kIconTypeShip));
	pushRange("player_ball", 0, GameToolbox::getValueForGamemode(IconType::kIconTypeBall));
	pushRange("bird", 1, GameToolbox::getValueForGamemode(IconType::kIconTypeUfo));
	pushRange("dart", 1, GameToolbox::getValueForGamemode(IconType::kIconTypeWave));
	pushRange("robot", 1, GameToolbox::getValueForGamemode(IconType::kIconTypeRobot));
	pushRange("spider", 1, GameToolbox::getValueForGamemode(IconType::kIconTypeSpider));
	pushRange("swing", 1, GameToolbox::getValueForGamemode(IconType::kIconTypeSwing));
	pushRange("jetpack", 1, GameToolbox::getValueForGamemode(IconType::kIconTypeJetpack));
}

void LoadingLayer::startIconLoading()
{
	buildIconQueue();
	_iconLoadIndex = 0;
	// Load a chunk every frame so the progress bar can redraw.
	this->schedule([this](float dt) { this->loadIconBatch(dt); }, "load_icons");
}

void LoadingLayer::loadIconBatch(float /*dt*/)
{
	if (_finished || _iconLoadIndex >= _iconPlists.size())
	{
		this->unschedule("load_icons");
		return;
	}

	auto* fu = FileUtils::getInstance();
	constexpr int kIconsPerFrame = 12;
	int loadedThisFrame = 0;

	while (_iconLoadIndex < _iconPlists.size() && loadedThisFrame < kIconsPerFrame)
	{
		const std::string& plist = _iconPlists[_iconLoadIndex++];
		std::string path = GameToolbox::getTextureString(plist);
		if (fu->isFileExist(path) || fu->isFileExist(plist))
			_sprFrameCache->addSpriteFramesWithFile(path);

		this->assetLoaded();
		++loadedThisFrame;
	}
}