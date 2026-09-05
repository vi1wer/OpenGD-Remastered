/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "EditLevelLayer.h"

#include "ButtonSprite.h"
#include "ComingSoonLayer.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/nodes.h"
#include "LevelEditorLayer.h"
#include "LevelTools.h"
#include "LocalLevelManager.h"
#include "MenuItemSpriteExtra.h"
#include "PlayLayer.h"

#include "2d/Label.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "2d/Transition.h"
#include "AudioEngine.h"
#include "EventDispatcher.h"
#include "EventListenerKeyboard.h"
#include "base/Director.h"
#include "ui/UIScale9Sprite.h"
#include "ui/UITextField.h"

#include <fmt/format.h>

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

	Menu* menuAt(Node* parent, const Vec2& pos, int z = 10)
	{
		auto* menu = Menu::create();
		menu->setPosition(pos);
		parent->addChild(menu, z);
		return menu;
	}

	void showSoon(Node* parent, std::string_view name)
	{
		auto* banner = ComingSoonLayer::create(name);
		if (!banner)
			return;
		parent->addChild(banner, 200);
		banner->show();
	}

	ui::Scale9Sprite* makeFieldBox(const Vec2& pos, const Size& size)
	{
		auto* box = ui::Scale9Sprite::create(GameToolbox::getTextureString("square02b_001.png"));
		if (!box)
			box = ui::Scale9Sprite::create(GameToolbox::getTextureString("square02_001.png"));
		if (!box)
			box = ui::Scale9Sprite::createWithSpriteFrameName("square02b_001.png");
		if (!box)
			return nullptr;
		box->setContentSize(size);
		// Official EditLevel fields: deep blue (darker than page BG).
		box->setColor({0, 48, 120});
		box->setOpacity(255);
		box->setPosition(pos);
		return box;
	}
}

EditLevelLayer* EditLevelLayer::create(GJGameLevel* level)
{
	auto* ret = new (std::nothrow) EditLevelLayer();
	if (ret && ret->init(level))
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
	return nullptr;
}

Scene* EditLevelLayer::scene(GJGameLevel* level)
{
	auto* scene = Scene::create();
	scene->addChild(EditLevelLayer::create(level));
	return scene;
}

bool EditLevelLayer::init(GJGameLevel* level)
{
	if (!Layer::init())
		return false;

	_level = level;
	const auto& winSize = Director::getInstance()->getWinSize();
	const float cx = winSize.width * 0.5f;

	GameToolbox::createBG(this, {0, 102, 255});
	GameToolbox::createCorners(this, false, false, true, true);

	// Compact official EditLevel stack: name, gap, description, buttons, meta, footer.
	const Size nameSize = {380.f, 50.f};
	const Size descSize = {380.f, 68.f};
	const float fieldGap = 10.f;
	const float nameY = winSize.height - 50.f;
	const float descY = nameY - nameSize.height * 0.5f - fieldGap - descSize.height * 0.5f;
	// Footer stays near the bottom; meta sits clearly above it.
	const float footerY = 18.f;
	const float metaY = 52.f;
	float btnY = descY - descSize.height * 0.5f - 55.f;
	if (btnY < metaY + 72.f)
		btnY = metaY + 72.f;

	if (auto* nameBox = makeFieldBox({cx, nameY}, nameSize))
		addChild(nameBox, 1);
	if (auto* descBox = makeFieldBox({cx, descY}, descSize))
		addChild(descBox, 1);

	const std::string bigFont = GameToolbox::getTextureString("bigFont.fnt");

	_nameInput = TextInputNode::create(356.f, 44.f, bigFont, "LEVEL NAME", 24);
	_nameInput->setPosition({cx - 178.f, nameY - 22.f});
	_nameInput->setAnchorPoint({0.f, 0.f});
	_nameInput->setMaxDisplayLabelScale(0.9f);
	_nameInput->setPlaceholderScale(0.8f);
	_nameInput->setPlaceholderColor({130, 170, 240});
	_nameInput->setDisplayedLabelColor(Color3B::WHITE);
	_nameInput->setAllowedChars(
		" abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_-+={}[];:'\",.<>/?\\|`~");
	_nameInput->setDelegate(this);
	if (_level && !_level->_levelName.empty())
		_nameInput->setString(_level->_levelName);
	if (auto* tf = _nameInput->getTextField())
	{
		tf->setMaxLength(40);
		tf->setMaxLengthEnabled(true);
	}
	addChild(_nameInput, 5);

	_descInput = TextInputNode::create(356.f, 60.f, bigFont, "Description [Optional]", 20);
	_descInput->setPosition({cx - 178.f, descY - 30.f});
	_descInput->setAnchorPoint({0.f, 0.f});
	_descInput->setMaxDisplayLabelScale(0.7f);
	_descInput->setPlaceholderScale(0.62f);
	_descInput->setPlaceholderColor({130, 170, 240});
	_descInput->setDisplayedLabelColor(Color3B::WHITE);
	_descInput->setAllowedChars(
		" abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_-+={}[];:'\",.<>/?\\|`~\n");
	_descInput->setDelegate(this);
	if (_level && !_level->_description.empty())
		_descInput->setString(_level->_description);
	if (auto* tf = _descInput->getTextField())
	{
		tf->setMaxLength(180);
		tf->setMaxLengthEnabled(true);
	}
	addChild(_descInput, 5);

	Node* editSpr = tryFrame("GJ_editBtn_001.png");
	if (!editSpr)
		editSpr = ButtonSprite::create(
			"Edit", 0x40, 0, 0.7f, false, bigFont, GameToolbox::getTextureString("GJ_button_01.png"), 40);
	{
		auto* btn = MenuItemSpriteExtra::create(editSpr, [this](Node*) { openEditor(); });
		btn->setScale(0.92f);
		menuAt(this, {cx - 115.f, btnY}, 5)->addChild(btn);
	}

	Node* playSpr = tryFrame("GJ_playBtn2_001.png");
	if (!playSpr)
		playSpr = tryFrame("GJ_playBtn_001.png");
	if (!playSpr)
		playSpr = ButtonSprite::create(
			"Play", 0x40, 0, 0.7f, false, bigFont, GameToolbox::getTextureString("GJ_button_01.png"), 40);
	{
		auto* btn = MenuItemSpriteExtra::create(playSpr, [this](Node*) { playtest(); });
		btn->setScale(0.92f);
		menuAt(this, {cx, btnY}, 5)->addChild(btn);
	}

	Node* shareSpr = tryFrame("GJ_shareBtn_001.png");
	if (!shareSpr)
		shareSpr = tryFrame("GJ_downloadBtn_001.png");
	if (!shareSpr)
		shareSpr = ButtonSprite::create(
			"Share", 0x40, 0, 0.7f, false, bigFont, GameToolbox::getTextureString("GJ_button_01.png"), 40);
	{
		auto* btn = MenuItemSpriteExtra::create(shareSpr, [this](Node*) { onShare(); });
		btn->setScale(0.92f);
		menuAt(this, {cx + 115.f, btnY}, 5)->addChild(btn);
	}

	// Pack meta left-to-right with tight gaps, then center the whole row.
	std::string songText = "STEREO MADNESS";
	if (_level)
	{
		songText = _level->_songName;
		if (songText.empty())
		{
			const int sid = _level->_songID > 0
								? _level->_songID
								: (_level->_officialSongID != 0 ? _level->_officialSongID : _level->_musicID);
			songText = LevelTools::getAudioTitle(sid);
		}
	}

	struct MetaItem
	{
		const char* icon;
		Label** outLabel;
		std::string text;
		float maxW;
		Sprite* spr = nullptr;
		Label* label = nullptr;
	};
	MetaItem items[] = {
		{"GJ_timeIcon_001.png", &_lengthLabel, "TINY", 55.f},
		{"GJ_musicIcon_001.png", &_songLabel, songText, 130.f},
		{"GJ_infoIcon_001.png", &_statusLabel, "UNVERIFIED", 90.f},
	};

	float totalW = 0.f;
	const float iconGap = 4.f;
	const float groupGap = 22.f;
	for (auto& it : items)
	{
		it.spr = tryFrame(it.icon);
		float iconW = 0.f;
		if (it.spr)
		{
			it.spr->setScale(0.65f);
			it.spr->setAnchorPoint({0.f, 0.5f});
			iconW = it.spr->getContentSize().width * it.spr->getScale() + iconGap;
		}
		it.label = GameToolbox::createBMFont(it.text, "bigFont.fnt");
		it.label->setScale(0.35f);
		it.label->setAnchorPoint({0.f, 0.5f});
		GameToolbox::limitLabelWidth(it.label, it.maxW, 0.35f);
		totalW += iconW + it.label->getContentSize().width * it.label->getScale() + groupGap;
	}
	totalW -= groupGap;

	float x = cx - totalW * 0.5f;
	for (auto& it : items)
	{
		if (it.spr)
		{
			it.spr->setPosition({x, metaY});
			addChild(it.spr, 2);
			x += it.spr->getContentSize().width * it.spr->getScale() + iconGap;
		}
		it.label->setPosition({x, metaY});
		addChild(it.label, 2);
		if (it.outLabel)
			*it.outLabel = it.label;
		x += it.label->getContentSize().width * it.label->getScale() + groupGap;
	}

	auto* versionLabel = GameToolbox::createBMFont("VERSION: 1", "goldFont.fnt");
	versionLabel->setScale(0.4f);
	versionLabel->setPosition({cx - 75.f, footerY});
	addChild(versionLabel, 2);

	auto* idLabel = GameToolbox::createBMFont("ID: NA", "goldFont.fnt");
	idLabel->setScale(0.4f);
	idLabel->setPosition({cx + 75.f, footerY});
	addChild(idLabel, 2);

	// Left column
	const float leftX = 30.f;
	float leftY = winSize.height - 28.f;
	menuAt(this, {leftX, leftY})->addChild(
		MenuItemSpriteExtra::create("GJ_arrow_01_001.png", [this](Node*) { goBack(); }));
	leftY -= 52.f;

	auto addSideSoon = [&](float x, float& y, const char* frame, const char* soonName, float scale = 0.9f) {
		Sprite* spr = tryFrame(frame);
		if (!spr)
			return;
		auto* btn = MenuItemSpriteExtra::create(spr, [this, soonName](Node*) { showSoon(this, soonName); });
		btn->setScale(scale);
		menuAt(this, {x, y})->addChild(btn);
		y -= 50.f;
	};

	addSideSoon(leftX, leftY, "GJ_updateBtn_001.png", "Update");
	{
		const char* folderFrame = tryFrame("gj_folderBtn_001.png") ? "gj_folderBtn_001.png" : "GJ_folderBtn_001.png";
		addSideSoon(leftX, leftY, folderFrame, "Folder");
	}
	addSideSoon(leftX, leftY, "GJ_optionsBtn_001.png", "Settings");

	// Right column
	const float rightX = winSize.width - 30.f;
	float rightY = winSize.height - 28.f;
	if (auto* closeSpr = tryFrame("GJ_closeBtn_001.png"))
	{
		auto* closeBtn = MenuItemSpriteExtra::create(closeSpr, [this](Node*) { goBack(); });
		closeBtn->setScale(0.85f);
		menuAt(this, {rightX, rightY})->addChild(closeBtn);
		rightY -= 52.f;
	}

	Node* helpNode = tryFrame("GJ_helpBtn_001.png");
	if (!helpNode)
		helpNode = tryFrame("GJ_helpBtn2_001.png");
	if (!helpNode)
	{
		auto* holder = Node::create();
		auto* circle = tryFrame("GJ_button_01.png");
		if (!circle)
			circle = Sprite::create(GameToolbox::getTextureString("GJ_button_01.png"));
		if (circle)
		{
			circle->setStretchEnabled(false);
			const float d = 40.f;
			circle->setScaleX(d / circle->getContentSize().width);
			circle->setScaleY(d / circle->getContentSize().height);
			circle->setPosition({0.f, 0.f});
			holder->addChild(circle);
			holder->setContentSize({d, d});
		}
		auto* helpLabel = GameToolbox::createBMFont("HELP", "bigFont.fnt");
		helpLabel->setScale(0.28f);
		helpLabel->setPosition({0.f, 0.f});
		holder->addChild(helpLabel, 1);
		if (holder->getContentSize().equals(Size::ZERO))
			holder->setContentSize({40.f, 40.f});
		helpNode = holder;
	}
	menuAt(this, {rightX, rightY})->addChild(
		MenuItemSpriteExtra::create(helpNode, [this](Node*) { showSoon(this, "Help"); }));
	rightY -= 50.f;

	addSideSoon(rightX, rightY, "GJ_duplicateBtn_001.png", "Move");
	addSideSoon(rightX, rightY, "GJ_orderUpBtn_001.png", "Reorder");

	// Bottom corners: same info icon as UNVERIFIED meta, report opposite.
	const float bottomY = 28.f;
	{
		Sprite* infoSpr = tryFrame("GJ_infoIcon_001.png");
		if (!infoSpr)
			infoSpr = tryFrame("GJ_infoBtn_001.png");
		if (infoSpr)
		{
			auto* infoBtn = MenuItemSpriteExtra::create(infoSpr, [this](Node*) { showSoon(this, "Info"); });
			// Match the Unverified meta icon look, sized like other side buttons.
			infoBtn->setScale(infoSpr->getContentSize().width < 60.f ? 1.55f : 0.85f);
			menuAt(this, {leftX, bottomY})->addChild(infoBtn);
		}
	}
	{
		Sprite* reportSpr = tryFrame("GJ_reportBtn_001.png");
		if (reportSpr)
		{
			auto* reportBtn = MenuItemSpriteExtra::create(reportSpr, [this](Node*) { showSoon(this, "Report"); });
			reportBtn->setScale(0.85f);
			menuAt(this, {rightX, bottomY})->addChild(reportBtn);
		}
	}

	auto* listener = EventListenerKeyboard::create();
	listener->onKeyPressed = AX_CALLBACK_2(EditLevelLayer::onKeyPressed, this);
	Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);

	refreshLabels();
	return true;
}

void EditLevelLayer::onEnter()
{
	Layer::onEnter();
	refreshLabels();
}

void EditLevelLayer::textChanged(TextInputNode*)
{
	syncFieldsToLevel();
}

void EditLevelLayer::textInputClosed(TextInputNode*)
{
	syncFieldsToLevel();
	if (_level)
		LocalLevelManager::get()->upsertLevel(_level);
}

void EditLevelLayer::syncFieldsToLevel()
{
	if (!_level)
		return;
	if (_nameInput)
	{
		std::string name = std::string(_nameInput->getString());
		while (!name.empty() && name.front() == ' ')
			name.erase(name.begin());
		while (!name.empty() && name.back() == ' ')
			name.pop_back();
		if (!name.empty())
			_level->_levelName = name;
	}
	if (_descInput)
		_level->_description = std::string(_descInput->getString());
}

void EditLevelLayer::refreshLabels()
{
	if (!_level)
		return;

	if (!_level->_levelString.empty())
		_level->_length = GameToolbox::calculateLevelLengthCategoryFromString(_level->_levelString);

	if (_lengthLabel)
	{
		_lengthLabel->setString(GameToolbox::levelLengthString(_level->_length));
		GameToolbox::limitLabelWidth(_lengthLabel, 55.f, 0.35f);
	}

	if (_songLabel)
	{
		std::string song = _level->_songName;
		if (song.empty())
		{
			const int sid = _level->_songID > 0
								? _level->_songID
								: (_level->_officialSongID != 0 ? _level->_officialSongID : _level->_musicID);
			song = LevelTools::getAudioTitle(sid);
		}
		_songLabel->setString(song);
		GameToolbox::limitLabelWidth(_songLabel, 120.f, 0.35f);
	}

	if (_statusLabel)
		_statusLabel->setString(_level->_setCompletes > 0 ? "VERIFIED" : "UNVERIFIED");
}

void EditLevelLayer::goBack()
{
	syncFieldsToLevel();
	if (_level)
		LocalLevelManager::get()->upsertLevel(_level);
	GameToolbox::popSceneWithTransition(0.5f);
}

void EditLevelLayer::openEditor()
{
	syncFieldsToLevel();
	if (!_level)
		return;
	if (_level->_levelString.empty())
		_level->_levelString = LocalLevelManager::kDefaultLevelString;
	LocalLevelManager::get()->upsertLevel(_level);
	Director::getInstance()->pushScene(TransitionFade::create(0.5f, LevelEditorLayer::scene(_level)));
}

void EditLevelLayer::playtest()
{
	syncFieldsToLevel();
	if (!_level)
		return;
	if (_level->_levelString.empty())
		_level->_levelString = LocalLevelManager::kDefaultLevelString;

	AudioEngine::stopAll();
	AudioEngine::play2d("playSound_01.ogg", false, 0.5f);
	Director::getInstance()->pushScene(TransitionFade::create(0.5f, PlayLayer::scene(_level)));
}

void EditLevelLayer::onShare()
{
	showSoon(this, "Upload");
}

void EditLevelLayer::onKeyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
	if (keyCode == EventKeyboard::KeyCode::KEY_ESCAPE || keyCode == EventKeyboard::KeyCode::KEY_BACK)
		goBack();
}
