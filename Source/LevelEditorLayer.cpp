/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "LevelEditorLayer.h"

#include "AlertLayer.h"
#include "AudioEngine.h"
#include "ButtonSprite.h"
#include "EditorTabs22.inc"
#include "GameObject.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/math.h"
#include "GameToolbox/nodes.h"
#include "GJGameLevel.h"
#include "GameManager.h"
#include "GameToolbox/enums.h"
#include "GroundLayer.h"
#include "ComingSoonLayer.h"
#include "EditorPropertyLayer.h"
#include "LevelSettingsLayer.h"
#include "LevelTools.h"
#include "LocalLevelManager.h"
#include "MenuItemSpriteExtra.h"
#include "PlayLayer.h"
#include "PlayerObject.h"

#include "2d/Camera.h"
#include "2d/DrawNode.h"
#include "2d/Label.h"
#include "2d/Layer.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "2d/Transition.h"
#include "math/Rect.h"
#include "base/Director.h"
#include "base/EventDispatcher.h"
#include "base/EventListenerKeyboard.h"
#include "base/EventListenerMouse.h"
#include "base/EventListenerTouch.h"
#include "base/EventMouse.h"
#include "renderer/Texture2D.h"
#include "ui/UIScale9Sprite.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <fmt/format.h>

#ifndef AX_RADIANS_TO_DEGREES
#define AX_RADIANS_TO_DEGREES(r) ((r) * 57.29577951f)
#endif

USING_NS_AX;

namespace
{
	constexpr float kGridSize = 30.f;
	constexpr float kGroundOffset = 90.f;
	constexpr float kPanDragThreshold = 8.f;
	constexpr int kMaxUndoSteps = 50;
	constexpr float kMinEditorZoom = 0.5f;
	constexpr float kMaxEditorZoom = 2.0f;
	constexpr int kPaletteCols = 6;
	constexpr int kPaletteRows = 2;
	constexpr int kPalettePageSize = kPaletteCols * kPaletteRows;

	struct ObjectTabDef
	{
		const char* label;
		EditorObjectTab tab;
		std::vector<int> ids;
		int iconId = 0;
	};

	// Object lists dumped 1:1 from Geometry Dash 2.2081 EditorUI::setupCreateMenu.
	const std::vector<ObjectTabDef>& objectTabs()
	{
		static const std::vector<ObjectTabDef> tabs = [] {
			std::vector<ObjectTabDef> out;
			int count = 0;
			const auto* info = EditorTabs22::all(count);
			static const EditorObjectTab kEnum[] = {
				EditorObjectTab::Blocks, EditorObjectTab::Outlines, EditorObjectTab::Slopes,
				EditorObjectTab::Spikes, EditorObjectTab::ThreeD, EditorObjectTab::Gameplay,
				EditorObjectTab::Animated, EditorObjectTab::Pixel, EditorObjectTab::Items,
				EditorObjectTab::Symbols, EditorObjectTab::Decor, EditorObjectTab::Spin,
				EditorObjectTab::Triggers, EditorObjectTab::Custom,
			};
			out.reserve(static_cast<size_t>(count));
			for (int i = 0; i < count; ++i)
			{
				ObjectTabDef def;
				def.label = info[i].label;
				def.tab = kEnum[i];
				def.ids = info[i].ids();
				def.iconId = info[i].iconId;
				out.push_back(std::move(def));
			}
			return out;
		}();
		return tabs;
	}
}

ax::Scene* LevelEditorLayer::scene(GJGameLevel* level)
{
	auto* scene = ax::Scene::create();
	if (auto* layer = LevelEditorLayer::create(level))
		scene->addChild(layer);
	return scene;
}

LevelEditorLayer* LevelEditorLayer::create(GJGameLevel* level)
{
	auto* ret = new (std::nothrow) LevelEditorLayer();
	if (ret && ret->init(level))
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
		return nullptr;
	}

bool LevelEditorLayer::init(GJGameLevel* level)
{
	if (!level)
		return false;

	if (level->_levelString.empty())
		level->_levelString = LocalLevelManager::kDefaultLevelString;

	if (!BaseGameLayer::init(level))
		return false;

	if (!_colorChannels.contains(1000))
		_colorChannels[1000] = SpriteColor(Color3B::WHITE, 255, false);
	if (!_colorChannels.contains(1001))
		_colorChannels[1001] = SpriteColor(Color3B::WHITE, 255, false);

	setupEditorWorld();

	if (_lastObjXPos < 570.f)
		_lastObjXPos = 570.f;

	// Show every loaded object (no section culling in editor).
	for (GameObject* obj : _allObjects)
	{
		if (!obj)
			continue;
		// Ensure section vector can hold this object (BaseGameLayer can undersize).
		ensureSectionCapacity(obj->_section < 0 ? 0 : obj->_section);
		if (!obj->getParent())
			attachGameObject(obj);
		obj->setActive(true);
		obj->setVisible(true);
		obj->update();
	}
	rebuildObjectCache();
	applyEditorLayerVisibility();

	auto* grid = DrawNode::create();
	_gridNode = grid;
	int maxBlockX = static_cast<int>(_lastObjXPos / kGridSize) + 10;
	if (maxBlockX < 30)
		maxBlockX = 256;
	if (maxBlockX > 512)
		maxBlockX = 512;
	const int maxBlockY = 18;
	for (int i = 0; i < maxBlockX; ++i)
	{
		for (int j = 0; j < maxBlockY; ++j)
		{
			const Vec2 pos{i * kGridSize, j * kGridSize + kGroundOffset};
			grid->drawRect(pos, {pos.x + kGridSize, pos.y + kGridSize}, Color4F::BLACK);
		}
	}
	grid->setOpacity(128);
	if (_gameLayer)
		_gameLayer->addChild(grid, 50);
	else
		addChild(grid, 50);

	_marqueeNode = DrawNode::create();
	if (_gameLayer)
		_gameLayer->addChild(_marqueeNode, 260);
	else
		addChild(_marqueeNode, 260);

	auto* dir = Director::getInstance();
	auto* touch = EventListenerTouchOneByOne::create();
	touch->setSwallowTouches(true);
	touch->onTouchBegan = AX_CALLBACK_2(LevelEditorLayer::onTouchBegan, this);
	touch->onTouchMoved = AX_CALLBACK_2(LevelEditorLayer::onTouchMoved, this);
	touch->onTouchEnded = AX_CALLBACK_2(LevelEditorLayer::onTouchEnded, this);
	dir->getEventDispatcher()->addEventListenerWithSceneGraphPriority(touch, this);

	_hudLayer = Node::create();
	addChild(_hudLayer, 1000);

	_toolbarMenu = Menu::create();
	_toolbarMenu->setPosition({0.f, 0.f});
	_hudLayer->addChild(_toolbarMenu);

	_optionsMenu = Menu::create();
	_optionsMenu->setPosition({0.f, 0.f});
	_hudLayer->addChild(_optionsMenu);

	_toolContent = Node::create();
	_hudLayer->addChild(_toolContent);

	setupToolbar();
	setupChromeHud();
	applyEditorZoom();
	rebuildOptionsButtons();
	setToolMode(EditorToolMode::Build);

	scheduleUpdate();
	return true;
}

ax::Sprite* LevelEditorLayer::makeFrameSprite(const char* frame, float scale) const
{
	if (!SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		return nullptr;
	auto* spr = Sprite::createWithSpriteFrameName(frame);
	if (!spr)
		return nullptr;
	spr->setStretchEnabled(false);
	spr->setScale(scale);
	return spr;
}

ax::Node* LevelEditorLayer::makeBackedIconButton(
	const char* frame, float btnSize, float iconMax, bool selected, const char* backingTexture) const
{
	auto* icon = makeFrameSprite(frame, 1.f);
	if (!icon)
		return nullptr;

	auto* bg = ui::Scale9Sprite::create(
		GameToolbox::getTextureString(backingTexture), Rect(0.f, 0.f, 40.f, 40.f));
	if (!bg)
		return nullptr;
	bg->setContentSize({btnSize, btnSize});

	const float iw = std::max(1.f, icon->getContentSize().width);
	const float ih = std::max(1.f, icon->getContentSize().height);
	icon->setScale(std::min(iconMax / iw, iconMax / ih));

	auto* holder = Node::create();
	holder->setContentSize({btnSize, btnSize});
	holder->setAnchorPoint({0.5f, 0.5f});
	bg->setAnchorPoint({0.5f, 0.5f});
	icon->setAnchorPoint({0.5f, 0.5f});
	bg->setPosition({btnSize * 0.5f, btnSize * 0.5f});
	icon->setPosition({btnSize * 0.5f, btnSize * 0.5f});
	holder->addChild(bg, 0);
	holder->addChild(icon, 1);
	if (selected)
		bg->setColor({180, 255, 255});
	return holder;
}

ax::Node* LevelEditorLayer::makeRightGridButton(const char* fullFrame, const char* label, float labelScale,
	float cell) const
{
	if (fullFrame && SpriteFrameCache::getInstance()->getSpriteFrameByName(fullFrame))
	{
		auto* spr = Sprite::createWithSpriteFrameName(fullFrame);
		if (!spr)
			return nullptr;
		spr->setStretchEnabled(false);
		const float maxSide = std::max(spr->getContentSize().width, spr->getContentSize().height);
		spr->setScale(cell / std::max(maxSide, 1.f) * 0.95f);
		return spr;
	}

	if (label && label[0])
	{
		if (auto* btn = ButtonSprite::create(label, 70, 0, labelScale, false,
				GameToolbox::getTextureString("bigFont.fnt"), GameToolbox::getTextureString("GJ_button_04.png"),
				cell - 2.f))
		{
			const float maxSide = std::max(btn->getContentSize().width, btn->getContentSize().height);
			btn->setScale(cell / std::max(maxSide, 1.f) * 0.98f);
			return btn;
		}
	}

	return nullptr;
}

void LevelEditorLayer::setupToolbar()
{
	const auto winSize = Director::getInstance()->getWinSize();
	const float modeScale = 1.0f;
	const float gap = 3.f;
	const float padY = 6.f;
	const float padX = 8.f;

	float modeH = 36.f;
	float modeW = 120.f;
	if (auto* probe = makeFrameSprite("edit_buildBtn_001.png", modeScale))
	{
		modeH = probe->getContentSize().height * modeScale;
		modeW = probe->getContentSize().width * modeScale;
	}

	const float stackH = 3.f * modeH + 2.f * gap;
	_toolbarHeight = stackH + padY * 2.f;
	_contentLeft = padX + modeW + 12.f;
	// Leave room on the right for pinned Swipe/Rotate/FreeMove/Snap (2x2).
	constexpr float kOptBtn = 42.f;
	constexpr float kOptGap = 46.f;
	const float optionsBlockW = kOptBtn + kOptGap + 14.f;
	_contentRight = winSize.width - padX - optionsBlockW;

	_rowYBot = padY + modeH * 0.5f;
	_rowYMid = _rowYBot + modeH + gap;
	_rowYTop = _rowYMid + modeH + gap;

	// Dark translucent editor bar under blocks / tools.
	_toolbarBg = Sprite::create(GameToolbox::getTextureString("edit_barBG_001.png"));
	if (_toolbarBg)
	{
		_toolbarBg->setAnchorPoint({0.f, 0.f});
		_toolbarBg->setPosition({0.f, 0.f});
		const float bw = std::max(1.f, _toolbarBg->getContentSize().width);
		const float bh = std::max(1.f, _toolbarBg->getContentSize().height);
		_toolbarBg->setScaleX(winSize.width / bw);
		_toolbarBg->setScaleY(_toolbarHeight / bh);
		_toolbarBg->setOpacity(210);
		_hudLayer->addChild(_toolbarBg, 0);
	}
	if (_toolbarMenu)
		_toolbarMenu->setLocalZOrder(2);
	if (_optionsMenu)
		_optionsMenu->setLocalZOrder(3);
	if (_toolContent)
	{
		_toolContent->setPosition({0.f, 0.f});
		_toolContent->setLocalZOrder(5);
	}

	const float modeX = padX + modeW * 0.5f;
	auto makeModeBtn = [&](const char* frame, float y, EditorToolMode mode) -> MenuItemSpriteExtra* {
		auto* spr = makeFrameSprite(frame, modeScale);
		if (!spr)
			return nullptr;
		auto* btn = MenuItemSpriteExtra::create(spr, [this, mode](Node*) { setToolMode(mode); });
		btn->setPosition({modeX, y});
		_toolbarMenu->addChild(btn);
		return btn;
	};

	_deleteModeBtn = makeModeBtn("edit_deleteBtn_001.png", _rowYBot, EditorToolMode::Delete);
	_editModeBtn = makeModeBtn("edit_editBtn_001.png", _rowYMid, EditorToolMode::Edit);
	_buildModeBtn = makeModeBtn("edit_buildBtn_001.png", _rowYTop, EditorToolMode::Build);

	if (_buildModeBtn)
		_buildModeSpr = dynamic_cast<Sprite*>(_buildModeBtn->getSprite());
	if (_editModeBtn)
		_editModeSpr = dynamic_cast<Sprite*>(_editModeBtn->getSprite());
	if (_deleteModeBtn)
		_deleteModeSpr = dynamic_cast<Sprite*>(_deleteModeBtn->getSprite());
}

void LevelEditorLayer::setupPlaytestTrail()
{
	_playtestTrail = DrawNode::create();
	_playtestDeathMarks = Node::create();
	// Under _gameLayer so trail/cross stay aligned with objects after editor zoom returns.
	if (_gameLayer)
	{
		_gameLayer->addChild(_playtestTrail, 250);
		_gameLayer->addChild(_playtestDeathMarks, 251);
	}
	else
	{
		addChild(_playtestTrail, 140);
		addChild(_playtestDeathMarks, 141);
	}
}

MenuItemSpriteExtra* LevelEditorLayer::makeHudIconBtn(const char* frame, std::function<void(ax::Node*)> cb, float targetSize)
{
	if (!SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		return nullptr;
	auto* spr = Sprite::createWithSpriteFrameName(frame);
	if (!spr)
		return nullptr;
	spr->setStretchEnabled(false);
	const float maxSide = std::max(spr->getContentSize().width, spr->getContentSize().height);
	if (maxSide > 0.f && targetSize > 0.f)
		spr->setScale(targetSize / maxSide);
	auto* btn = MenuItemSpriteExtra::create(spr, std::move(cb));
	return btn;
}

void LevelEditorLayer::showComingSoon(const char* feature)
{
	if (auto* layer = ComingSoonLayer::create(feature ? feature : "Feature"))
		layer->show();
}

void LevelEditorLayer::setupChromeHud()
{
	setupPlaytestTrail();
	const auto winSize = Director::getInstance()->getWinSize();

	_topHudMenu = Menu::create();
	_topHudMenu->setPosition({0.f, 0.f});
	_hudLayer->addChild(_topHudMenu, 40);

	_leftHudMenu = Menu::create();
	_leftHudMenu->setPosition({0.f, 0.f});
	_hudLayer->addChild(_leftHudMenu, 40);

	_rightHudMenu = Menu::create();
	_rightHudMenu->setPosition({0.f, 0.f});
	_hudLayer->addChild(_rightHudMenu, 40);

	_playtestMenu = _leftHudMenu;

	const float topY = winSize.height - 14.f;
	const float leftX = 20.f;
	constexpr float kLeftStep = 40.f;
	const float leftTopY = winSize.height - 52.f;

	constexpr float kTopIcon = 30.f;
	if (auto* undo = makeHudIconBtn("GJ_undoBtn_001.png", [this](Node*) { editorUndo(); }, kTopIcon))
	{
		undo->setPosition({24.f, topY});
		_topHudMenu->addChild(undo);
	}
	if (auto* redo = makeHudIconBtn("GJ_redoBtn_001.png", [this](Node*) { editorRedo(); }, kTopIcon))
	{
		redo->setPosition({56.f, topY});
		_topHudMenu->addChild(redo);
	}
	if (auto* trash = makeHudIconBtn("GJ_trashBtn_001.png", [this](Node*) { deleteSelectedObject(); }, kTopIcon))
	{
		trash->setPosition({88.f, topY});
		_topHudMenu->addChild(trash);
	}

	class EditorScrubSlider : public Node
	{
	public:
		static EditorScrubSlider* create(LevelEditorLayer* editor)
		{
			auto* ret = new (std::nothrow) EditorScrubSlider();
			if (ret && ret->init(editor))
			{
				ret->autorelease();
				return ret;
			}
			AX_SAFE_DELETE(ret);
			return nullptr;
		}

		bool init(LevelEditorLayer* editor)
		{
			if (!Node::init())
				return false;
			_editor = editor;
			const auto winSize = Director::getInstance()->getWinSize();
			_groove = Sprite::create(GameToolbox::getTextureString("slidergroove.png"));
			_thumb = Sprite::create(GameToolbox::getTextureString("sliderthumb.png"));
			if (!_groove)
				return true;
			addChild(_groove, 0);
			if (_thumb)
				addChild(_thumb, 1);
			_barWidth = std::max(_groove->getContentSize().width - 8.f, 8.f);
			const float visW = std::min(winSize.width * 0.34f, 180.f);
			setScale(visW / std::max(_groove->getContentSize().width, 1.f));
			refreshThumb(_value);
			auto* listener = EventListenerTouchOneByOne::create();
			listener->setSwallowTouches(true);
			listener->onTouchBegan = [this](Touch* touch, Event*) {
				const Vec2 world = touch->getLocation();
				const Vec2 center = convertToWorldSpace({0.f, 0.f});
				if (std::abs(world.x - center.x) > (_barWidth * getScaleX() * 0.5f + 12.f) ||
					std::abs(world.y - center.y) > 22.f)
					return false;
				setFromTouch(touch);
				return true;
			};
			listener->onTouchMoved = [this](Touch* touch, Event*) { setFromTouch(touch); };
			listener->onTouchEnded = [this](Touch* touch, Event*) { setFromTouch(touch); };
			Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);
			return true;
		}

		void setValue(float v)
		{
			_value = std::clamp(v, 0.f, 1.f);
			refreshThumb(_value);
		}

	private:
		LevelEditorLayer* _editor = nullptr;
		Sprite* _groove = nullptr;
		Sprite* _thumb = nullptr;
		float _barWidth = 80.f;
		float _value = 0.f;

		void refreshThumb(float v)
		{
			if (_thumb)
				_thumb->setPosition({(v - 0.5f) * _barWidth, 0.f});
		}

		void setFromTouch(Touch* touch)
		{
			const float localX = convertToNodeSpace(touch->getLocation()).x;
			_value = std::clamp(localX / std::max(_barWidth, 1.f) + 0.5f, 0.f, 1.f);
			refreshThumb(_value);
			if (_editor)
				_editor->setScrubCameraX(_value);
		}
	};

	if (auto* scrub = EditorScrubSlider::create(this))
	{
		scrub->setPosition({winSize.width * 0.5f + 20.f, topY});
		_scrubSlider = scrub;
		_scrubSliderSet = [scrub](float v) { scrub->setValue(v); };
		_hudLayer->addChild(scrub, 40);
	}

	if (auto* settings = makeHudIconBtn("GJ_optionsBtn02_001.png", [this](Node*) { showLevelSettings(); }, kTopIcon))
	{
		settings->setPosition({winSize.width - 56.f, topY});
		_topHudMenu->addChild(settings);
	}
	if (auto* pause = makeHudIconBtn("GJ_pauseEditorBtn_001.png", [this](Node*) { showEditorMenu(); }, kTopIcon))
	{
		pause->setPosition({winSize.width - 24.f, topY});
		_topHudMenu->addChild(pause);
	}

	float leftY = leftTopY;
	_musicBtn = makeHudIconBtn("GJ_playMusicBtn_001.png", [this](Node*) {
		if (_musicPlaying)
			stopEditorMusic();
		else
			startEditorMusic();
	});
	if (_musicBtn)
	{
		_musicBtn->setPosition({leftX, leftY});
		_leftHudMenu->addChild(_musicBtn);
		leftY -= kLeftStep;
	}

	_playtestBtn = makeHudIconBtn("GJ_playEditorBtn_001.png", [this](Node*) {
		if (_playtesting)
			stopEditorPlaytest(false);
		else
			startEditorPlaytest();
	});
	if (_playtestBtn)
	{
		_playtestBtn->setPosition({leftX, leftY});
		_leftHudMenu->addChild(_playtestBtn);
		leftY -= kLeftStep;
	}

	_zoomInBtn = makeHudIconBtn("GJ_zoomInBtn_001.png", [this](Node*) {
		_editorZoom = std::min(kMaxEditorZoom, _editorZoom + 0.1f);
		applyEditorZoom();
	});
	if (_zoomInBtn)
	{
		_zoomInBtn->setPosition({leftX, leftY});
		_leftHudMenu->addChild(_zoomInBtn);
		leftY -= kLeftStep;
	}

	_zoomOutBtn = makeHudIconBtn("GJ_zoomOutBtn_001.png", [this](Node*) {
		_editorZoom = std::max(kMinEditorZoom, _editorZoom - 0.1f);
		applyEditorZoom();
	});
	if (_zoomOutBtn)
	{
		_zoomOutBtn->setPosition({leftX, leftY});
		_leftHudMenu->addChild(_zoomOutBtn);
	}

	constexpr float kGridCell = 38.f;
	const float gridRight = winSize.width - 8.f;
	const float gridTop = winSize.height - 50.f;

	struct RightGridItem
	{
		const char* frame;
		const char* label;
		float labelScale;
	};

	// Square editor buttons (33x35) from GJ_GameSheet03 — Btn2 variants where available.
	static const RightGridItem kRightGrid[] = {
		{"GJ_copyBtn2_001.png", nullptr, 0.f},
		{"GJ_pasteBtn2_001.png", nullptr, 0.f},
		{"GJ_duplicateObjectBtn2_001.png", nullptr, 0.f},
		{"GJ_editObjBtn4_001.png", nullptr, 0.f},
		{"GJ_groupIDBtn2_001.png", nullptr, 0.f},
		{"GJ_editObjBtn3_001.png", nullptr, 0.f},
		{"GJ_copyStateBtn_001.png", nullptr, 0.f},
		{"GJ_pasteStateBtn_001.png", nullptr, 0.f},
		{"GJ_pasteColorBtn_001.png", nullptr, 0.f},
		{"GJ_editHSVBtn2_001.png", nullptr, 0.f},
		{"GJ_goToLayerBtn_001.png", nullptr, 0.f},
		{"GJ_deSelBtn2_001.png", nullptr, 0.f},
	};
	std::function<void()> rightActions[12] = {
		[this] { copySelection(); },
		[this] { pasteClipboard(); },
		[this] { duplicateSelection(); },
		[this] {
			if (_selectedObjectReal)
				showEditSpecialPopup(_selectedObjectReal);
			else
				showComingSoon("Edit Special");
		},
		[this] {
			if (_selectedObjectReal)
				showEditGroupPopup(_selectedObjectReal);
			else
				showComingSoon("Edit Group");
		},
		[this] {
			if (_selectedObjectReal)
				showEditObjectPopup(_selectedObjectReal);
			else
				showComingSoon("Edit Object");
		},
		[this] { showComingSoon("Copy Values"); },
		[this] { showComingSoon("Paste State"); },
		[this] { showComingSoon("Paste Color"); },
		[this] {
			if (_selectedObjectReal)
				showEditColorPopup(_selectedObjectReal);
			else
				showComingSoon("Edit Color");
		},
		[this] { showComingSoon("Go To Layer"); },
		[this] { deselectObject(); },
	};

	for (int i = 0; i < 12; ++i)
	{
		const int col = i % 3;
		const int row = i / 3;
		const auto& item = kRightGrid[i];
		Node* spr = makeRightGridButton(item.frame, item.label, item.labelScale, kGridCell);
		if (!spr)
			continue;
		auto action = rightActions[i];
		auto* btn = MenuItemSpriteExtra::create(spr, [action](Node*) {
			if (action)
				action();
		});
		btn->setPosition({gridRight - (col + 0.5f) * kGridCell, gridTop - (row + 0.5f) * kGridCell});
		_rightHudMenu->addChild(btn);
	}

	setupLayerHud();
}

void LevelEditorLayer::setupLayerHud()
{
	if (!_hudLayer)
	return;
	if (_layerHud)
	{
		_layerHud->removeFromParent();
		_layerHud = nullptr;
		_layerLabel = nullptr;
	}

	const auto winSize = Director::getInstance()->getWinSize();
	_layerHud = Node::create();
	_hudLayer->addChild(_layerHud, 4);

	constexpr float kOptBtn = 42.f;
	constexpr float kOptGap = 46.f;
	const float optRight = winSize.width - 14.f - kOptBtn * 0.5f;
	const float layerY = _toolbarHeight + kOptGap + 22.f;

	auto* menu = Menu::create();
	menu->setPosition({0.f, 0.f});
	_layerHud->addChild(menu);

	auto addArrow = [&](const char* frame, float x, int delta) {
		if (auto* btn = makeHudIconBtn(frame, [this, delta](Node*) { changeEditorLayer(delta); }))
		{
			btn->setScale(0.55f);
			btn->setPosition({x, layerY});
			menu->addChild(btn);
		}
	};
	addArrow("edit_leftBtn2_001.png", optRight - kOptGap * 0.85f, -1);
	addArrow("edit_rightBtn2_001.png", optRight + kOptGap * 0.85f, 1);

	_layerLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "0");
	if (_layerLabel)
	{
		_layerLabel->setScale(0.45f);
		_layerLabel->setPosition({optRight, layerY});
		_layerHud->addChild(_layerLabel, 2);
	}
}

void LevelEditorLayer::refreshPlaytestButton()
{
	if (!_playtestBtn)
		return;
	const char* frame = _playtesting ? "GJ_stopEditorBtn_001.png" : "GJ_playEditorBtn_001.png";
	if (!SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		return;
	_playtestBtn->setSpriteFrame(frame);
	if (auto* spr = dynamic_cast<Sprite*>(_playtestBtn->getSprite()))
	{
		const float maxSide = std::max(spr->getContentSize().width, spr->getContentSize().height);
		if (maxSide > 0.f)
			spr->setScale(38.f / maxSide);
	}
}

void LevelEditorLayer::setEditorHudVisible(bool visible)
{
	if (_toolbarBg)
		_toolbarBg->setVisible(visible);
	if (_toolbarMenu)
		_toolbarMenu->setVisible(visible);
	if (_optionsMenu)
		_optionsMenu->setVisible(visible);
	if (_toolContent)
		_toolContent->setVisible(visible);
	if (_topHudMenu)
		_topHudMenu->setVisible(visible);
	if (_rightHudMenu)
		_rightHudMenu->setVisible(visible);
	if (_layerHud)
		_layerHud->setVisible(visible);
	if (_scrubSlider)
		_scrubSlider->setVisible(visible);
	if (_leftHudMenu)
		_leftHudMenu->setVisible(true);
	if (_musicBtn)
		_musicBtn->setVisible(visible && !_playtesting);
	if (_zoomInBtn)
		_zoomInBtn->setVisible(visible && !_playtesting);
	if (_zoomOutBtn)
		_zoomOutBtn->setVisible(visible && !_playtesting);
	if (_playtestBtn)
		_playtestBtn->setVisible(true);
}

void LevelEditorLayer::refreshModeButtons()
{
	auto* cache = SpriteFrameCache::getInstance();
	auto setFrameSafe = [cache](MenuItemSpriteExtra* btn, const char* frame) {
		if (!btn || !frame || !cache || !cache->getSpriteFrameByName(frame))
			return;
		btn->setSpriteFrame(frame);
	};
	setFrameSafe(
		_buildModeBtn, _toolMode == EditorToolMode::Build ? "edit_buildSBtn_001.png" : "edit_buildBtn_001.png");
	setFrameSafe(
		_editModeBtn, _toolMode == EditorToolMode::Edit ? "edit_editSBtn_001.png" : "edit_editBtn_001.png");
	setFrameSafe(
		_deleteModeBtn, _toolMode == EditorToolMode::Delete ? "edit_deleteSBtn_001.png" : "edit_deleteBtn_001.png");
}

void LevelEditorLayer::setToolMode(EditorToolMode mode)
{
	_toolMode = mode;
	_inSwapMode = false;
	refreshModeButtons();
	rebuildToolContent();
}

void LevelEditorLayer::rebuildOptionsButtons()
{
	if (!_optionsMenu)
		return;
	_optionsMenu->removeAllChildren();

	constexpr float kOptBtn = 42.f;
	constexpr float kOptIcon = 32.f;
	constexpr float kOptGap = 46.f; // center-to-center (~4px between edges)
	const auto winSize = Director::getInstance()->getWinSize();
	const float optRight = winSize.width - 12.f - kOptBtn * 0.5f;
	const float optLeft = optRight - kOptGap;
	const float midY = _toolbarHeight * 0.5f;
	const float optTopY = midY + kOptGap * 0.5f;
	const float optBotY = midY - kOptGap * 0.5f;

	auto addToggle = [&](const char* frame, float x, float y, bool on, std::function<void()> action) {
		const char* tex = on ? "GJ_button_02.png" : "GJ_button_01.png";
		auto* holder = makeBackedIconButton(frame, kOptBtn, kOptIcon, false, tex);
		if (!holder)
			return;
		auto* btn = MenuItemSpriteExtra::create(holder, [action](Node*) {
			if (action)
				action();
		});
		btn->setPosition({x, y});
		_optionsMenu->addChild(btn);
	};

	addToggle("edit_swipeBtn_001.png", optLeft, optTopY, _swipeEnabled, [this] {
		_swipeEnabled = !_swipeEnabled;
		rebuildOptionsButtons();
	});
	addToggle("edit_enableRotateBtn_001.png", optRight, optTopY, _freeRotateEnabled, [this] {
		_freeRotateEnabled = !_freeRotateEnabled;
		if (_freeRotateEnabled)
			_freeMoveEnabled = false;
		rebuildOptionsButtons();
	});
	addToggle("edit_freeMoveBtn_001.png", optLeft, optBotY, _freeMoveEnabled, [this] {
		_freeMoveEnabled = !_freeMoveEnabled;
		if (_freeMoveEnabled)
			_freeRotateEnabled = false;
		rebuildOptionsButtons();
	});
	addToggle("edit_snapBtn_001.png", optRight, optBotY, _snapEnabled, [this] {
		_snapEnabled = !_snapEnabled;
		rebuildOptionsButtons();
	});
}

const std::vector<int>& LevelEditorLayer::objectsForTab(EditorObjectTab tab) const
{
	static const std::vector<int> empty;
	for (const auto& def : objectTabs())
	{
		if (def.tab == tab)
			return def.ids;
	}
	return empty;
}

void LevelEditorLayer::setObjectTab(EditorObjectTab tab)
{
	_objectTab = tab;
	_palettePage = 0;
	if (!objectsForTab(tab).empty())
		_selectedObject = objectsForTab(tab).front();
	rebuildToolContent();
}

void LevelEditorLayer::setPalettePage(int page)
{
	const auto& ids = objectsForTab(_objectTab);
	const int totalPages = std::max(1, (static_cast<int>(ids.size()) + kPalettePageSize - 1) / kPalettePageSize);
	_palettePage = std::clamp(page, 0, totalPages - 1);
	rebuildToolContent();
}

void LevelEditorLayer::rebuildToolContent()
{
	if (!_toolContent)
		return;
	_toolContent->removeAllChildren();
	_tabStripHeight = 0.f;

	auto* contentMenu = Menu::create();
	contentMenu->setPosition({0.f, 0.f});
	_toolContent->addChild(contentMenu, 1);

	const float contentStartX = _contentLeft + 18.f;
	const float contentEndX = _contentRight - 18.f;
	(void)contentStartX;
	(void)contentEndX;
	const float barTopY = _rowYTop;
	const float barMidY = _rowYMid;
	const float barBotY = _rowYBot;
	(void)barTopY;

	if (_toolMode == EditorToolMode::Build)
	{
		const auto& ids = objectsForTab(_objectTab);
		const int totalPages = std::max(1, (static_cast<int>(ids.size()) + kPalettePageSize - 1) / kPalettePageSize);
		if (_palettePage >= totalPages)
			_palettePage = totalPages - 1;
		if (_palettePage < 0)
			_palettePage = 0;

		auto makePreview = [](int objectId) -> Sprite* {
			GameObject* preview = GameObject::createFromString(fmt::format("1,{},2,0,3,0", objectId));
			if (!preview || !preview->getSpriteFrame() || !preview->getSpriteFrame()->getTexture())
			{
				AX_SAFE_RELEASE(preview);
				return nullptr;
			}
			preview->setStretchEnabled(false);
			preview->setActive(true);
			auto* sprite = Sprite::createWithTexture(
				preview->getSpriteFrame()->getTexture(), preview->getSpriteFrame()->getRect(),
				preview->getSpriteFrame()->isRotated());
			AX_SAFE_RELEASE(preview);
			return sprite;
		};

		auto makeIcon = [&](int objectId, float iconMax, const char* fallbackLetter) -> Node* {
			Node* icon = nullptr;
			if (objectId != 0)
				icon = makePreview(objectId);
			if (!icon && fallbackLetter && fallbackLetter[0])
			{
				auto* letter = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), fallbackLetter);
				if (letter)
					icon = letter;
			}
			if (!icon)
				return nullptr;
			const float maxSide = std::max(icon->getContentSize().width, icon->getContentSize().height);
			if (maxSide > 0.f)
				icon->setScale(iconMax / maxSide);
			return icon;
		};

		auto makeTile = [&](int objectId, float tile, float iconMax, bool selected, const char* fallbackLetter) -> Node* {
			auto* bg = ui::Scale9Sprite::create(
				GameToolbox::getTextureString("GJ_button_04.png"), Rect(0.f, 0.f, 40.f, 40.f));
			if (!bg)
				return nullptr;
			bg->setContentSize({tile, tile});
			bg->setColor(selected ? Color3B::WHITE : Color3B(88, 88, 88));
			if (auto* icon = makeIcon(objectId, iconMax, fallbackLetter))
			{
				icon->setPosition({tile * 0.5f, tile * 0.5f});
				if (!selected)
					icon->setColor(Color3B(175, 175, 175));
				bg->addChild(icon);
			}
			return bg;
		};

		auto makeTabTile = [&](int objectId, float targetW, bool selected, const char* fallbackLetter, float& outH) -> Node* {
			const char* frame = selected ? "GJ_tabOn_001.png" : "GJ_tabOff_001.png";
			auto* bg = Sprite::createWithSpriteFrameName(frame);
			if (!bg)
				return nullptr;
			bg->setStretchEnabled(false);
			const float nativeW = std::max(1.f, bg->getContentSize().width);
			const float nativeH = std::max(1.f, bg->getContentSize().height);
			const float scale = targetW / nativeW;
			bg->setScale(scale);
			outH = nativeH * scale;

			if (auto* icon = makeIcon(objectId, nativeH * 0.88f, fallbackLetter))
			{
				icon->setAnchorPoint({0.5f, 0.5f});
				icon->setPosition({nativeW * 0.5f, nativeH * 0.5f});
				bg->addChild(icon, 1);
			}

			auto* holder = Node::create();
			holder->setContentSize({targetW, outH});
			holder->setAnchorPoint({0.5f, 0.5f});
			bg->setAnchorPoint({0.5f, 0.5f});
			bg->setPosition({targetW * 0.5f, outH * 0.5f});
			holder->addChild(bg, 0);
			return holder;
		};

		auto addArrow = [&](const char* frame, float x, float y, float scale, std::function<void()> action) {
			auto* spr = makeFrameSprite(frame, scale);
			if (!spr)
				spr = makeFrameSprite("navArrowBtn_001.png", scale * 0.8f);
			if (!spr)
				return;
			auto* btn = MenuItemSpriteExtra::create(spr, [action](Node*) {
				if (action)
					action();
			});
			btn->setPosition({x, y});
			contentMenu->addChild(btn);
		};

		const float bandLeft = _contentLeft + 8.f;
		const float bandRight = _contentRight - 8.f;
		const float bandW = std::max(40.f, bandRight - bandLeft);
		const float bandMidX = (bandLeft + bandRight) * 0.5f;

		const auto winSize = Director::getInstance()->getWinSize();
		const auto& tabs = objectTabs();
		const int nTabs = std::max(1, static_cast<int>(tabs.size()));
		const float sidePad = 4.f;
		const float tabGapPx = 1.f;
		float tabW = (winSize.width - sidePad * 2.f - tabGapPx * static_cast<float>(nTabs - 1)) / static_cast<float>(nTabs);
		tabW = std::max(18.f, tabW);
		const float rowW = tabW * static_cast<float>(nTabs) + tabGapPx * static_cast<float>(nTabs - 1);
		float tabX = (winSize.width - rowW) * 0.5f + tabW * 0.5f;
		float tabH = tabW * (35.f / 64.f);
		const float overlap = 3.f;

		for (const auto& tab : tabs)
		{
			const int iconId = tab.iconId != 0 ? tab.iconId : (tab.ids.empty() ? 0 : tab.ids.front());
			float visH = tabH;
			auto* tile = makeTabTile(iconId, tabW, tab.tab == _objectTab,
				tab.tab == EditorObjectTab::Custom ? "C" : nullptr, visH);
			if (!tile)
				continue;
			tabH = visH;
			auto* btn = MenuItemSpriteExtra::create(tile, [this, t = tab.tab](Node*) { setObjectTab(t); });
			btn->setPosition({tabX, _toolbarHeight - overlap + visH * 0.5f});
			contentMenu->addChild(btn);
			tabX += tabW + tabGapPx;
		}
		_tabStripHeight = std::max(0.f, tabH - overlap);

		const float paletteTop = _toolbarHeight - 4.f;
		const float paletteBot = 6.f;
		const float paletteMidY = (paletteTop + paletteBot) * 0.5f;
		const float arrowW = 18.f;
		float cell = std::min(34.f, (paletteTop - paletteBot) / 2.15f);
		cell = std::min(cell, (bandW - arrowW * 2.f) / static_cast<float>(kPaletteCols));
		cell = std::max(22.f, cell);
		const float gridW = kPaletteCols * cell;
		const float gridH = kPaletteRows * cell;
		const float gridOriginX = bandMidX - gridW * 0.5f + cell * 0.5f;
		const float gridTopY = paletteMidY + gridH * 0.25f;
		const float gridBotY = paletteMidY - gridH * 0.25f;

		addArrow("edit_leftBtn2_001.png", bandMidX - gridW * 0.5f - arrowW, paletteMidY, 0.65f, [this] {
			setPalettePage(_palettePage - 1);
		});
		addArrow("edit_rightBtn2_001.png", bandMidX + gridW * 0.5f + arrowW, paletteMidY, 0.65f, [this] {
			setPalettePage(_palettePage + 1);
		});

		const int start = _palettePage * kPalettePageSize;
		int placed = 0;
		for (int i = start; i < static_cast<int>(ids.size()) && placed < kPalettePageSize; ++i)
		{
			const int objectId = ids[i];
			auto* tile = makeTile(objectId, cell - 4.f, cell * 0.55f, objectId == _selectedObject, nullptr);
			if (!tile)
				continue;
			auto* btn = MenuItemSpriteExtra::create(tile, [this, objectId](Node*) {
				_selectedObject = objectId;
				rebuildToolContent();
			});
			const int col = placed % kPaletteCols;
			const int row = placed / kPaletteCols;
			btn->setPosition({gridOriginX + col * cell, row == 0 ? gridTopY : gridBotY});
			contentMenu->addChild(btn);
			++placed;
		}
		return;
	}

	if (_toolMode == EditorToolMode::Edit)
	{
		constexpr float kBtn = 34.f;
		constexpr float kIconMax = 22.f;
		const float toolGap = 38.f;
		const float toolTopY = barTopY;
		const float toolBotY = barBotY;
		const float startX = _contentLeft + kBtn * 0.5f + 8.f;

		auto addTool = [&](const char* frame, float x, float y, std::function<void()> action) {
			auto* holder = makeBackedIconButton(frame, kBtn, kIconMax, false, "GJ_button_01.png");
			if (!holder)
				return;
			auto* btn = MenuItemSpriteExtra::create(holder, [action](Node*) {
				if (action)
					action();
			});
			btn->setPosition({x, y});
			contentMenu->addChild(btn);
		};

		addTool("edit_leftBtn_001.png", startX, toolTopY, [this] { moveSelected(-2.f, 0.f); });
		addTool("edit_rightBtn_001.png", startX + toolGap, toolTopY, [this] { moveSelected(2.f, 0.f); });
		addTool("edit_upBtn_001.png", startX + toolGap * 2.f, toolTopY, [this] { moveSelected(0.f, 2.f); });
		addTool("edit_downBtn_001.png", startX + toolGap * 3.f, toolTopY, [this] { moveSelected(0.f, -2.f); });

		addTool("edit_leftBtn2_001.png", startX, toolBotY, [this] { moveSelected(-kGridSize, 0.f); });
		addTool("edit_rightBtn2_001.png", startX + toolGap, toolBotY, [this] { moveSelected(kGridSize, 0.f); });
		addTool("edit_upBtn2_001.png", startX + toolGap * 2.f, toolBotY, [this] { moveSelected(0.f, kGridSize); });
		addTool("edit_downBtn2_001.png", startX + toolGap * 3.f, toolBotY, [this] { moveSelected(0.f, -kGridSize); });

		const float rotX = startX + toolGap * 4.5f;
		addTool("edit_ccwBtn_001.png", rotX, toolTopY, [this] { rotateSelected(-90.f); });
		addTool("edit_cwBtn_001.png", rotX + toolGap, toolTopY, [this] { rotateSelected(90.f); });
		addTool("edit_rotate45lBtn_001.png", rotX, toolBotY, [this] { rotateSelected(-45.f); });
		addTool("edit_rotate45rBtn_001.png", rotX + toolGap, toolBotY, [this] { rotateSelected(45.f); });
		return;
	}

	// Delete mode: no extra tools — tap objects to remove.
}

void LevelEditorLayer::setupEditorWorld()
{
	const auto winSize = Director::getInstance()->getWinSize();
	const int bgId = _levelSettings._bgID > 0 ? _levelSettings._bgID : 1;
	const int groundId = _levelSettings._groundID > 0 ? _levelSettings._groundID : 1;

	_cameraFollow = Node::create();
	addChild(_cameraFollow, 100);

	_bottomGround = GroundLayer::create(groundId);
	if (_bottomGround)
	{
		// Same setup as PlayLayer: parented to camera-follow, Y compensated so the
		// floor stays at world y≈12 (player stands at 105). Without this the cube
		// renders inside the ground texture during playtest.
		_cameraFollow->addChild(_bottomGround, 1);
		_bottomGround->setPositionY(-_cameraFollow->getPositionY() + 12.f);
		_bottomGround->update(0.f);
	}
	_ceiling = GroundLayer::create(groundId);
	if (_ceiling)
	{
		_cameraFollow->addChild(_ceiling, 1);
		_ceiling->setScaleY(-1);
		_ceiling->setVisible(false);
		if (_ceiling->_sprite)
			_ceiling->setPositionY(winSize.height + _ceiling->_sprite->getTextureRect().size.height);
	}
	_lastGroundCamX = m_obCamPos.x;

	m_pBG = Sprite::create(GameToolbox::getTextureString(fmt::format("game_bg_{:02}_001.png", bgId)));
	if (!m_pBG)
		m_pBG = Sprite::create(GameToolbox::getTextureString("game_bg_01_001.png"));
	if (m_pBG)
	{
		m_pBG->setStretchEnabled(false);
		if (auto* tex = m_pBG->getTexture())
		{
			const Texture2D::TexParams texParams = {
				backend::SamplerFilter::LINEAR, backend::SamplerFilter::LINEAR,
				backend::SamplerAddressMode::REPEAT, backend::SamplerAddressMode::REPEAT};
			tex->setTexParameters(texParams);
		}
		m_pBG->setTextureRect(Rect(0, 0, 1024 * 5, 1024));
		m_pBG->setPosition(winSize.width * 0.5f, winSize.height * 0.25f);
		m_pBG->setColor(colorForChannel(1000));
		addChild(m_pBG, -100);
	}

	_gameLayer = Node::create();
	_gameLayer->setName("editorGameLayer");
	addChild(_gameLayer, 0);

	std::vector<Node*> toReparent;
	for (auto* child : getChildren())
	{
		if (child && child != _gameLayer && child != _cameraFollow && child != m_pBG && child != _hudLayer &&
			child != _middleGround)
			toReparent.push_back(child);
	}
	for (auto* child : toReparent)
	{
		const int z = child->getLocalZOrder();
		child->retain();
		child->removeFromParentAndCleanup(false);
		_gameLayer->addChild(child, z);
		child->release();
	}
}

void LevelEditorLayer::onEnter()
{
	Layer::onEnter();
	_instance = this;

	auto* dir = Director::getInstance();
	auto* keys = EventListenerKeyboard::create();
	keys->onKeyPressed = AX_CALLBACK_2(LevelEditorLayer::onKeyPressed, this);
	keys->onKeyReleased = AX_CALLBACK_2(LevelEditorLayer::onKeyReleased, this);
	dir->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keys, this);

	auto* mouse = EventListenerMouse::create();
	mouse->onMouseScroll = [this](Event* event) {
		if (_playtesting)
			return;
		auto* mouseEvent = static_cast<EventMouse*>(event);
		const float scrollY = mouseEvent->getScrollY();
		const float scrollX = mouseEvent->getScrollX();

		// Ctrl+wheel = zoom (GD editor). Plain wheel pans Y; Shift+wheel pans X.
		if (_ctrlPressed)
		{
			if (scrollY != 0.f)
			{
				_editorZoom = std::clamp(_editorZoom - scrollY * 0.05f, kMinEditorZoom, kMaxEditorZoom);
				applyEditorZoom();
			}
			return;
		}

		// Axmol scrollY is inverted vs expected cam pan on Windows.
		if (_shiftPressed)
		{
			m_obCamPos.x -= scrollY * 40.f;
			m_obCamPos.y += scrollX * 40.f;
		}
		else
		{
			m_obCamPos.y -= scrollY * 40.f;
			m_obCamPos.x += scrollX * 40.f;
		}
		updateCamera(0.f);
		refreshScrubSlider();
	};
	mouse->onMouseDown = [this](Event* event) {
		if (_playtesting)
			return;
		auto* mouseEvent = static_cast<EventMouse*>(event);
		const auto button = mouseEvent->getMouseButton();
		if (button == EventMouse::MouseButton::BUTTON_RIGHT || button == EventMouse::MouseButton::BUTTON_MIDDLE)
		{
			_cameraPanning = true;
			_leftPanning = false;
			_pendingTap = false;
			_panTouchStart = mouseEvent->getLocation();
			_camAtPanStart = m_obCamPos;
		}
	};
	mouse->onMouseUp = [this](Event* event) {
		auto* mouseEvent = static_cast<EventMouse*>(event);
		const auto button = mouseEvent->getMouseButton();
		if (button == EventMouse::MouseButton::BUTTON_RIGHT || button == EventMouse::MouseButton::BUTTON_MIDDLE)
			_cameraPanning = false;
	};
	mouse->onMouseMove = [this](Event* event) {
		if (_playtesting)
			return;
		auto* mouseEvent = static_cast<EventMouse*>(event);
		if (_cameraPanning)
			applyEditorPan(mouseEvent->getLocation());
	};
	dir->getEventDispatcher()->addEventListenerWithSceneGraphPriority(mouse, this);
}

void LevelEditorLayer::onExit()
{
	if (_instance == this)
		_instance = nullptr;
	Director::getInstance()->getEventDispatcher()->removeEventListenersForTarget(this);
	Layer::onExit();
}

void LevelEditorLayer::updatePlaytestCamera(float dt)
{
	const auto winSize = Director::getInstance()->getWinSize();
	if (!_player1)
	{
		if (auto* cam = Camera::getDefaultCamera())
			cam->setPosition(m_obCamPos + winSize * 0.5f);
		if (_hudLayer)
			_hudLayer->setPosition(m_obCamPos);
		return;
	}

	const Vec2 pPos = _player1->getPosition();
	Vec2 cam = m_obCamPos;

	if (_player1->_currentGamemode != PlayerGamemodeCube || _playtestDual)
	{
		cam.y = (winSize.height * -0.5f) + _playtestCamYCenter;
		if (cam.y < 0.f)
			cam.y = 0.f;
	}
	else
	{
		float unk2 = 90.f;
		float unk3 = 120.f;
		if (_player1->isGravityFlipped())
		{
			unk2 = 120.f;
			unk3 = 90.f;
		}
		if (pPos.y <= winSize.height + cam.y - unk2)
		{
			if (pPos.y < unk3 + cam.y)
				cam.y = pPos.y - unk3;
		}
		else
			cam.y = pPos.y - winSize.height + unk2;
		if (!_player1->isGravityFlipped() && _player1->getLastGroundPos().y == 105.f &&
			pPos.y <= cam.y + winSize.height - unk2)
			cam.y = 0.f;
	}

	cam.y = clampf(cam.y, 0.f, 1140.f - winSize.height);

	if (!_playtestDead && pPos.x >= winSize.width / 2.5f && !_levelSettings.platformer)
	{
		if (m_pBG && _bottomGround)
			m_pBG->setPositionX(m_pBG->getPositionX() -
				dt * _player1->getPlayerSpeed() * _bottomGround->getSpeed() * 0.1175f);
		if (_bottomGround)
			_bottomGround->update(dt * _player1->getPlayerSpeed());
		cam.x = pPos.x - (winSize.width / 2.5f);
	}
	else if (_levelSettings.platformer)
		cam.x = pPos.x - winSize.width / 2.f;

	if (m_pBG)
	{
		if (m_pBG->getPositionX() <= cam.x - 1024.f)
			m_pBG->setPositionX(m_pBG->getPositionX() + 1024.f);
		if (m_pBG->getPositionX() >= cam.x + 1024.f)
			m_pBG->setPositionX(m_pBG->getPositionX() - 1024.f);
		m_pBG->setPositionX(m_pBG->getPositionX() + (cam.x - m_obCamPos.x));
		m_pBG->setPositionY(cam.y * 0.2f);
	}

	m_obCamPos.x = cam.x;
	m_obCamPos.y = GameToolbox::iLerp(m_obCamPos.y, cam.y, 0.1f, dt / 60.f);

	if (auto* camera = Camera::getDefaultCamera())
		camera->setPosition(m_obCamPos + winSize * 0.5f);
	if (_cameraFollow)
		_cameraFollow->setPosition(m_obCamPos);
	if (_hudLayer)
		_hudLayer->setPosition(m_obCamPos);
	if (_bottomGround)
	{
		// PlayLayer-style: only pin cube ground; scroll once via player speed above.
		_lastGroundCamX = m_obCamPos.x;
		_bottomGround->setPositionX(0.f);
		if (_player1 && _player1->_currentGamemode == PlayerGamemodeCube && !_playtestDual)
			_bottomGround->setPositionY(-_cameraFollow->getPositionY() + 12.f);
	}
	if (_ceiling)
	{
		_ceiling->setPositionX(0.f);
		const bool flying = _player1 && (_player1->isFlying() || _player1->_currentGamemode == PlayerGamemodeBall);
		_ceiling->setVisible(_playtestDual || flying);
		if (_playtestDual || flying)
			_ceiling->update(dt * (_player1 ? _player1->getPlayerSpeed() : 1.f));
	}
}

void LevelEditorLayer::updateCamera(float dt)
{
	if (_playtesting)
	{
		updatePlaytestCamera(dt);
		return;
	}

	extern float gameSpeed;
	if (gameSpeed != 0.f)
		dt /= gameSpeed;

	m_obCamPos += m_camDelta * dt;

	const auto winSize = Director::getInstance()->getWinSize();
	const float maxCamX = std::max(0.f, _lastObjXPos - winSize.width * 0.35f);
	// Allow panning below the ground (official editor does this); keep an upper bound.
	constexpr float kMinEditorCamY = -240.f;
	m_obCamPos.x = clampf(m_obCamPos.x, 0.f, maxCamX);
	m_obCamPos.y = clampf(m_obCamPos.y, kMinEditorCamY, 1140.f - winSize.height);

	if (auto* cam = Camera::getDefaultCamera())
		cam->setPosition(m_obCamPos + winSize * 0.5f);
	if (_cameraFollow)
		_cameraFollow->setPosition(m_obCamPos);
	if (_hudLayer)
		_hudLayer->setPosition(m_obCamPos);

	if (!_playtesting)
		refreshScrubSlider();

	const float camDx = m_obCamPos.x - _lastGroundCamX;
	_lastGroundCamX = m_obCamPos.x;

	if (m_pBG)
	{
		const float parallax = camDx * 0.5f;
		if (_bottomGround)
			m_pBG->setPositionX(m_pBG->getPositionX() + parallax * _bottomGround->getSpeed() * 0.1175f);

		if (m_pBG->getPositionX() <= m_obCamPos.x - 1024.f)
			m_pBG->setPositionX(m_pBG->getPositionX() + 1024.f);
		if (m_pBG->getPositionX() >= m_obCamPos.x + 1024.f)
			m_pBG->setPositionX(m_pBG->getPositionX() - 1024.f);

		m_pBG->setPositionY(m_obCamPos.y * 0.2f);
	}

	if (_bottomGround)
	{
		// Scroll texture with pan; keep world-space floor (not glued to screen).
		_bottomGround->update(camDx * 0.7f);
		_bottomGround->setPositionX(0.f);
		_bottomGround->setPositionY(-_cameraFollow->getPositionY() + 12.f);
	}
}

void LevelEditorLayer::update(float delta)
{
	_instance = this;

	if (_playtesting)
		updatePlaytest(delta);
	else
		updateCamera(delta);

	if (m_pBG && _colorChannels.contains(1000))
		m_pBG->setColor(_colorChannels.at(1000)._color);

	if (!_playtesting && _selectedObjectReal)
	{
		if (std::find(_allObjects.begin(), _allObjects.end(), _selectedObjectReal) == _allObjects.end())
			clearSelection();
		else
		_selectedObjectReal->setColor({0, 255, 0});
	}
	for (GameObject* obj : _selectedObjects)
	{
		if (obj && std::find(_allObjects.begin(), _allObjects.end(), obj) != _allObjects.end())
			obj->setColor({0, 255, 0});
	}
}

void LevelEditorLayer::ensureSectionCapacity(int section)
{
	while (static_cast<int>(_sectionObjects.size()) <= section)
		_sectionObjects.push_back({});
}

void LevelEditorLayer::rebuildObjectCache()
{
	_objectPositionCache.clear();
	for (GameObject* object : _allObjects)
	{
		if (!object)
			continue;
		const std::string key = fmt::format("{}x{}", object->getPositionX(), object->getPositionY());
		_objectPositionCache[key] = object;
	}
}

void LevelEditorLayer::addObject(GameObject* obj)
{
	if (!obj)
		return;

	int section = sectionForPos(obj->getPositionX());
	section = section - 1 < 0 ? 0 : section - 1;
	obj->_section = section;
	obj->setDontTransform(false);
	obj->_uniqueID = static_cast<int>(_allObjects.size());

	if (obj->getPositionX() > _lastObjXPos)
		_lastObjXPos = obj->getPositionX();

	_allObjects.push_back(obj);
	ensureSectionCapacity(section);
	_sectionObjects[section].push_back(obj);

	if (!obj->getParent())
		attachGameObject(obj);
	obj->setActive(true);
	obj->setVisible(true);
	if (obj->_editorLayer < 0)
		obj->_editorLayer = _currentEditorLayer;
	obj->update();
	applyEditorLayerVisibility();

	const std::string key = fmt::format("{}x{}", obj->getPositionX(), obj->getPositionY());
	_objectPositionCache[key] = obj;
}

void LevelEditorLayer::removeEditorObject(GameObject* obj, bool recordUndo)
{
	if (!obj)
		return;

	if (recordUndo)
	{
		EditorUndoAction action;
		action.type = EditorUndoType::Delete;
		action.objectData = serializeObject(obj);
		pushUndo(action);
	}

	const std::string key = fmt::format("{}x{}", obj->getPositionX(), obj->getPositionY());
	_objectPositionCache.erase(key);

	if (obj->_section >= 0 && obj->_section < static_cast<int>(_sectionObjects.size()))
	{
		auto& section = _sectionObjects[obj->_section];
		section.erase(std::remove(section.begin(), section.end(), obj), section.end());
	}

	_allObjects.erase(std::remove(_allObjects.begin(), _allObjects.end(), obj), _allObjects.end());

	detachGameObject(obj);
	obj->setActive(false);
	obj->unscheduleAllCallbacks();

	// Drop selection BEFORE release — otherwise D/A/W/S hit a dangling pointer.
	_selectedObjects.erase(
		std::remove(_selectedObjects.begin(), _selectedObjects.end(), obj), _selectedObjects.end());
	if (_selectedObjectReal == obj)
		_selectedObjectReal = _selectedObjects.empty() ? nullptr : _selectedObjects.back();

	AX_SAFE_RELEASE(obj);
}

GameObject* LevelEditorLayer::findObject(float x, float y)
{
	const std::string key = fmt::format("{}x{}", x, y);
	auto it = _objectPositionCache.find(key);
	if (it != _objectPositionCache.end())
		return it->second;
	return nullptr;
}

GameObject* LevelEditorLayer::findObjectNear(const Vec2& worldPos, float radius) const
{
	GameObject* best = nullptr;
	float bestDist = radius * radius;
	for (GameObject* obj : _allObjects)
	{
		if (!obj)
			continue;
		const float d = obj->getPosition().distanceSquared(worldPos);
		if (d <= bestDist)
		{
			bestDist = d;
			best = obj;
		}
	}
	return best;
}

void LevelEditorLayer::clearSelection()
{
	for (GameObject* obj : _selectedObjects)
	{
		if (obj)
			obj->setColor(Color3B::WHITE);
	}
	_selectedObjects.clear();
	_selectedObjectReal = nullptr;
}

void LevelEditorLayer::setSelectedObjects(const std::vector<GameObject*>& objs)
{
	clearSelection();
	for (GameObject* obj : objs)
	{
		if (!obj)
			continue;
		_selectedObjects.push_back(obj);
		obj->setColor({0, 255, 0});
	}
	_selectedObjectReal = _selectedObjects.empty() ? nullptr : _selectedObjects.back();
}

void LevelEditorLayer::setSelectedObject(GameObject* obj)
{
	if (obj)
		setSelectedObjects({obj});
	else
		clearSelection();
}

void LevelEditorLayer::selectObjectsInMarquee(const Vec2& a, const Vec2& b)
{
	const float minX = std::min(a.x, b.x);
	const float maxX = std::max(a.x, b.x);
	const float minY = std::min(a.y, b.y);
	const float maxY = std::max(a.y, b.y);

	std::vector<GameObject*> hits;
	for (GameObject* obj : _allObjects)
	{
		if (!obj)
			continue;
		if (obj->_editorLayer != _currentEditorLayer)
			continue;
		const Vec2 p = obj->getPosition();
		if (p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY)
			hits.push_back(obj);
	}
	setSelectedObjects(hits);
}

void LevelEditorLayer::refreshMarqueeVisual()
{
	if (!_marqueeNode)
		return;
	_marqueeNode->clear();
	if (!_marqueeSelecting)
		return;

	const float minX = std::min(_marqueeStartWorld.x, _marqueeEndWorld.x);
	const float maxX = std::max(_marqueeStartWorld.x, _marqueeEndWorld.x);
	const float minY = std::min(_marqueeStartWorld.y, _marqueeEndWorld.y);
	const float maxY = std::max(_marqueeStartWorld.y, _marqueeEndWorld.y);
	if (maxX - minX < 2.f && maxY - minY < 2.f)
		return;

	const Vec2 bl{minX, minY};
	const Vec2 tr{maxX, maxY};
	_marqueeNode->drawSolidRect(bl, tr, Color4F(0.f, 1.f, 0.f, 0.12f));
	_marqueeNode->drawRect(bl, tr, Color4F(0.f, 1.f, 0.f, 0.85f));
}

void LevelEditorLayer::refreshSelectionHighlight()
{
	for (GameObject* obj : _allObjects)
	{
		if (!obj)
			continue;
		obj->setColor(Color3B::WHITE);
	}
	for (GameObject* obj : _selectedObjects)
	{
		if (obj)
			obj->setColor({0, 255, 0});
	}
}

void LevelEditorLayer::placeObjectAt(const Vec2& snappedWorldPos)
{
	auto* existing = findObject(snappedWorldPos.x, snappedWorldPos.y);
	if (existing && existing->getID() == _selectedObject)
	{
		setSelectedObject(existing);
		return;
	}

	const std::string objectString =
		fmt::format("1,{},2,{},3,{}", _selectedObject, snappedWorldPos.x, snappedWorldPos.y - kGroundOffset);
	GameObject* obj = GameObject::createFromString(objectString);
	if (!obj)
		return;

	obj->setStretchEnabled(false);
	obj->setActive(true);
	obj->setID(_selectedObject);
	obj->_editorLayer = _currentEditorLayer;
	if (_selectedObjectReal && _selectedObjectReal->getID() == obj->getID())
		obj->setRotation(_selectedObjectReal->getRotation());

	addObject(obj);
	setSelectedObject(obj);
	_inSwapMode = true;

	EditorUndoAction action;
	action.type = EditorUndoType::Place;
	action.objectData = serializeObject(obj);
	pushUndo(action);
}

void LevelEditorLayer::selectObjectAt(const Vec2& worldPos)
{
	GameObject* existing = nullptr;
	if (_snapEnabled && !_freeMoveEnabled)
		existing = findObject(worldPos.x, worldPos.y);
	if (!existing)
		existing = findObjectNear(worldPos, kGridSize * 0.65f);
	if (existing)
		setSelectedObject(existing);
}

void LevelEditorLayer::deleteObjectAt(const Vec2& worldPos)
{
	GameObject* existing = nullptr;
	if (_snapEnabled)
		existing = findObject(worldPos.x, worldPos.y);
	if (!existing)
		existing = findObjectNear(worldPos, kGridSize * 0.65f);
	if (existing)
		removeEditorObject(existing);
}

void LevelEditorLayer::deleteSelectedObject()
{
	std::vector<GameObject*> toDelete = _selectedObjects;
	if (toDelete.empty() && _selectedObjectReal)
		toDelete.push_back(_selectedObjectReal);
	for (GameObject* obj : toDelete)
	{
		if (obj)
			removeEditorObject(obj);
	}
	clearSelection();
}

void LevelEditorLayer::deselectObject()
{
	setSelectedObject(nullptr);
}

void LevelEditorLayer::moveSelected(float dx, float dy)
{
	std::vector<GameObject*> targets;
	targets.reserve(_selectedObjects.size() + 1);
	for (GameObject* obj : _selectedObjects)
	{
		if (obj && std::find(_allObjects.begin(), _allObjects.end(), obj) != _allObjects.end())
			targets.push_back(obj);
	}
	if (targets.empty() && _selectedObjectReal &&
		std::find(_allObjects.begin(), _allObjects.end(), _selectedObjectReal) != _allObjects.end())
		targets.push_back(_selectedObjectReal);
	if (targets.empty())
	{
		clearSelection();
		return;
	}

	for (GameObject* obj : targets)
	{
		if (!obj)
			continue;

		EditorUndoAction action;
		action.type = EditorUndoType::Move;
		action.objectData = serializeObject(obj);
		action.oldX = obj->getPositionX();
		action.oldY = obj->getPositionY();
		pushUndo(action);

		const Vec2 oldPos = obj->getPosition();
		Vec2 newPos = oldPos;
		newPos.x += dx;
		newPos.y += dy;
		if (_snapEnabled && !_freeMoveEnabled)
		{
			newPos.x = std::floor(newPos.x / kGridSize) * kGridSize + 15.f;
			newPos.y = std::floor((newPos.y - kGroundOffset) / kGridSize) * kGridSize + 15.f + kGroundOffset;
		}
		obj->setPosition(newPos);
		obj->setStartPosition(newPos);
		_objectPositionCache.erase(fmt::format("{}x{}", oldPos.x, oldPos.y));
		_objectPositionCache[fmt::format("{}x{}", newPos.x, newPos.y)] = obj;
	}
}

void LevelEditorLayer::rotateSelected(float degrees)
{
	std::vector<GameObject*> targets;
	targets.reserve(_selectedObjects.size() + 1);
	for (GameObject* obj : _selectedObjects)
	{
		if (obj && std::find(_allObjects.begin(), _allObjects.end(), obj) != _allObjects.end())
			targets.push_back(obj);
	}
	if (targets.empty() && _selectedObjectReal &&
		std::find(_allObjects.begin(), _allObjects.end(), _selectedObjectReal) != _allObjects.end())
		targets.push_back(_selectedObjectReal);
	if (targets.empty())
	{
		clearSelection();
		return;
	}

	for (GameObject* obj : targets)
	{
		if (!obj)
			continue;
		EditorUndoAction action;
		action.type = EditorUndoType::Rotate;
		action.objectData = serializeObject(obj);
		action.oldRot = obj->getRotation();
		pushUndo(action);
		obj->setRotation(obj->getRotation() + degrees);
	}
}

ax::Vec2 LevelEditorLayer::touchToWorld(const Vec2& screenPos, bool snap) const
{
	const float invZoom = 1.f / std::max(_editorZoom, 0.01f);
	if (snap)
	{
		Vec2 pos = (screenPos + m_obCamPos) * invZoom;
		pos.y -= kGroundOffset;
		const int mappedX = static_cast<int>(std::floor(pos.x / kGridSize));
		const int mappedY = static_cast<int>(std::floor(pos.y / kGridSize));
		pos.x = mappedX * kGridSize + 15.f;
		pos.y = mappedY * kGridSize + 15.f + kGroundOffset;
		return pos;
	}
	return (screenPos + m_obCamPos) * invZoom;
}

ax::Vec2 LevelEditorLayer::snapTouchToGrid(const Vec2& screenPos) const
{
	return touchToWorld(screenPos, true);
}

void LevelEditorLayer::applyEditorPan(const Vec2& screenPos)
{
	const float invZoom = 1.f / std::max(_editorZoom, 0.01f);
	m_obCamPos = _camAtPanStart + (screenPos - _panTouchStart) * invZoom;
	updateCamera(0.f);
}

void LevelEditorLayer::commitEditorTap(const Vec2& screenPos)
{
	const bool useSnap = _snapEnabled && !_freeMoveEnabled;
	const Vec2 world = touchToWorld(screenPos, useSnap);
	switch (_toolMode)
	{
	case EditorToolMode::Build:
		placeObjectAt(useSnap ? world : snapTouchToGrid(screenPos));
		break;
	case EditorToolMode::Edit:
		selectObjectAt(world);
		break;
	case EditorToolMode::Delete:
		deleteObjectAt(world);
		break;
	}
}

std::string LevelEditorLayer::serializeObject(GameObject* obj) const
{
	if (!obj)
		return {};
	std::string s = fmt::format("1,{},2,{},3,{}", obj->getID(), obj->getPositionX(), obj->getPositionY() - kGroundOffset);
	const float sx = obj->getStartScaleX();
	const float sy = obj->getStartScaleY();
	if (sx < 0.f)
		s += fmt::format(",4,{}", -sx);
	if (sy < 0.f)
		s += fmt::format(",5,{}", -sy);
	if (obj->getRotation() != 0.f)
		s += fmt::format(",6,{}", obj->getRotation());
	const float uniform = std::max(std::abs(sx), std::abs(sy));
	if (uniform != 1.f)
		s += fmt::format(",32,{}", uniform);
	if (obj->_editorLayer >= 0)
		s += fmt::format(",20,{}", obj->_editorLayer);
	if (obj->_mainColorChannel >= 0)
		s += fmt::format(",21,{}", obj->_mainColorChannel);
	if (obj->_secColorChannel >= 0)
		s += fmt::format(",22,{}", obj->_secColorChannel);
	if (obj->_zLayer != 0)
		s += fmt::format(",24,{}", obj->_zLayer);
	if (!obj->_groups.empty())
	{
		std::string g;
		for (int gid : obj->_groups)
			g += fmt::format("{}", gid) + ".";
		if (!g.empty())
			g.pop_back();
		s += fmt::format(",57,{}", g);
	}
	return s;
}

GameObject* LevelEditorLayer::deserializeObject(const std::string& data)
{
	if (data.empty())
		return nullptr;
	GameObject* obj = GameObject::createFromString(data);
	if (!obj)
		return nullptr;
	obj->setStretchEnabled(false);
	obj->setActive(true);
	return obj;
}

GameObject* LevelEditorLayer::findObjectFromUndoData(const std::string& data)
{
	auto props = GameToolbox::splitByDelimStringView(data, ',');
	if (props.size() < 4)
		return nullptr;
	const int id = GameToolbox::stoi(props[1]);
	float x = 0.f;
	float y = kGroundOffset;
	for (size_t i = 0; i + 1 < props.size(); i += 2)
	{
		const int key = GameToolbox::stoi(props[i]);
		if (key == 2)
			x = GameToolbox::stof(props[i + 1]);
		if (key == 3)
			y = GameToolbox::stof(props[i + 1]) + kGroundOffset;
	}
	GameObject* hit = findObject(x, y);
	if (hit && hit->getID() == id)
		return hit;
	return findObjectNear({x, y}, kGridSize * 0.75f);
}

void LevelEditorLayer::pushUndo(EditorUndoAction action)
{
	while (static_cast<int>(_undoStack.size()) >= kMaxUndoSteps)
		_undoStack.erase(_undoStack.begin());
	_undoStack.push_back(std::move(action));
	_redoStack.clear();
}

void LevelEditorLayer::editorUndo()
{
	if (_undoStack.empty())
		return;
	EditorUndoAction action = _undoStack.back();
	_undoStack.pop_back();
	EditorUndoAction redo = action;

	switch (action.type)
	{
	case EditorUndoType::Place:
		if (auto* obj = findObjectFromUndoData(action.objectData))
		{
			redo.objectData = serializeObject(obj);
			removeEditorObject(obj, false);
		}
		break;
	case EditorUndoType::Delete:
		if (auto* obj = deserializeObject(action.objectData))
		{
			addObject(obj);
			redo.objectData = serializeObject(obj);
		}
		break;
	case EditorUndoType::Move:
		if (auto* obj = findObjectFromUndoData(action.objectData))
		{
			redo.oldX = obj->getPositionX();
			redo.oldY = obj->getPositionY();
			redo.objectData = serializeObject(obj);
			const Vec2 newPos{action.oldX, action.oldY};
			const Vec2 oldPos = obj->getPosition();
			obj->setPosition(newPos);
			obj->setStartPosition(newPos);
			_objectPositionCache.erase(fmt::format("{}x{}", oldPos.x, oldPos.y));
			_objectPositionCache[fmt::format("{}x{}", newPos.x, newPos.y)] = obj;
		}
		break;
	case EditorUndoType::Rotate:
		if (auto* obj = findObjectFromUndoData(action.objectData))
		{
			redo.oldRot = obj->getRotation();
			redo.objectData = serializeObject(obj);
			obj->setRotation(action.oldRot);
		}
		break;
	}
	_redoStack.push_back(redo);
}

void LevelEditorLayer::editorRedo()
{
	if (_redoStack.empty())
		return;
	EditorUndoAction action = _redoStack.back();
	_redoStack.pop_back();
	EditorUndoAction undo = action;

	switch (action.type)
	{
	case EditorUndoType::Place:
		if (auto* obj = deserializeObject(action.objectData))
		{
			addObject(obj);
			undo.objectData = serializeObject(obj);
		}
		break;
	case EditorUndoType::Delete:
		if (auto* obj = findObjectFromUndoData(action.objectData))
		{
			undo.objectData = serializeObject(obj);
			removeEditorObject(obj, false);
		}
		break;
	case EditorUndoType::Move:
		if (auto* obj = findObjectFromUndoData(action.objectData))
		{
			undo.oldX = obj->getPositionX();
			undo.oldY = obj->getPositionY();
			undo.objectData = serializeObject(obj);
			const Vec2 newPos{action.oldX, action.oldY};
			const Vec2 oldPos = obj->getPosition();
			obj->setPosition(newPos);
			obj->setStartPosition(newPos);
			_objectPositionCache.erase(fmt::format("{}x{}", oldPos.x, oldPos.y));
			_objectPositionCache[fmt::format("{}x{}", newPos.x, newPos.y)] = obj;
		}
		break;
	case EditorUndoType::Rotate:
		if (auto* obj = findObjectFromUndoData(action.objectData))
		{
			undo.oldRot = obj->getRotation();
			undo.objectData = serializeObject(obj);
			obj->setRotation(action.oldRot);
		}
		break;
	}
	_undoStack.push_back(undo);
}

void LevelEditorLayer::copySelection()
{
	_clipboard.clear();
	for (GameObject* obj : _selectedObjects)
	{
		if (obj && std::find(_allObjects.begin(), _allObjects.end(), obj) != _allObjects.end())
			_clipboard.push_back(serializeObject(obj));
	}
	if (_clipboard.empty() && _selectedObjectReal &&
		std::find(_allObjects.begin(), _allObjects.end(), _selectedObjectReal) != _allObjects.end())
		_clipboard.push_back(serializeObject(_selectedObjectReal));
	if (_clipboard.empty())
		clearSelection();
}

void LevelEditorLayer::pasteClipboard()
{
	if (_clipboard.empty())
		return;
	for (const std::string& data : _clipboard)
	{
		GameObject* obj = deserializeObject(data);
		if (!obj)
			continue;
		auto pos = obj->getPosition();
		pos.x += kGridSize;
		obj->setPosition(pos);
		obj->setStartPosition(pos);
		obj->_editorLayer = _currentEditorLayer;
		addObject(obj);
		EditorUndoAction action;
		action.type = EditorUndoType::Place;
		action.objectData = serializeObject(obj);
		pushUndo(action);
		setSelectedObject(obj);
	}
}

void LevelEditorLayer::duplicateSelection()
{
	copySelection();
	pasteClipboard();
}

void LevelEditorLayer::applyEditorZoom()
{
	if (_gameLayer)
		_gameLayer->setScale(_editorZoom);
}

void LevelEditorLayer::applyEditorLayerVisibility()
{
	for (GameObject* obj : _allObjects)
	{
		if (!obj)
			continue;
		if (obj->_editorLayer != _currentEditorLayer)
			obj->setOpacity(90);
		else
			obj->setOpacity(255);
	}
	refreshSelectionHighlight();
}

void LevelEditorLayer::setEditorLayer(int layer)
{
	_currentEditorLayer = std::clamp(layer, 0, 999);
	refreshLayerLabel();
	applyEditorLayerVisibility();
}

void LevelEditorLayer::changeEditorLayer(int delta)
{
	setEditorLayer(_currentEditorLayer + delta);
}

void LevelEditorLayer::refreshLayerLabel()
{
	if (_layerLabel)
		_layerLabel->setString(fmt::format("{}", _currentEditorLayer));
}

void LevelEditorLayer::refreshMusicButton()
{
	if (!_musicBtn)
		return;
	const char* frame = _musicPlaying ? "GJ_stopMusicBtn_001.png" : "GJ_playMusicBtn_001.png";
	if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		_musicBtn->setSpriteFrame(frame);
	if (auto* spr = dynamic_cast<Sprite*>(_musicBtn->getSprite()))
	{
		const float maxSide = std::max(spr->getContentSize().width, spr->getContentSize().height);
		if (maxSide > 0.f)
			spr->setScale(38.f / maxSide);
	}
}

void LevelEditorLayer::startEditorMusic()
{
	if (_musicPlaying)
		return;
	auto* level = getLevel();
	if (!level)
		return;
	float musicVol = 0.1f;
	if (auto* gm = GameManager::getInstance())
		musicVol *= gm->getMusicVolume();
	_musicAudioId = AudioEngine::play2d(LevelTools::resolveAudioPath(level), true, musicVol);
	_musicPlaying = true;
	refreshMusicButton();
}

void LevelEditorLayer::stopEditorMusic()
{
	if (_musicAudioId != -1)
	{
		AudioEngine::stop(_musicAudioId);
		_musicAudioId = -1;
	}
	_musicPlaying = false;
	refreshMusicButton();
}

void LevelEditorLayer::setScrubCameraX(float normalized)
{
	const auto winSize = Director::getInstance()->getWinSize();
	const float maxCamX = std::max(0.f, _lastObjXPos - winSize.width * 0.35f);
	m_obCamPos.x = normalized * maxCamX;
	updateCamera(0.f);
}

void LevelEditorLayer::refreshScrubSlider()
{
	if (!_scrubSliderSet)
		return;
	const auto winSize = Director::getInstance()->getWinSize();
	const float maxCamX = std::max(0.f, _lastObjXPos - winSize.width * 0.35f);
	const float norm = maxCamX > 0.f ? m_obCamPos.x / maxCamX : 0.f;
	_scrubSliderSet(std::clamp(norm, 0.f, 1.f));
}

void LevelEditorLayer::showEditObjectPopup(GameObject* obj)
{
	if (auto* layer = EditorPropertyLayer::create(this, EditorPropertyKind::Object, obj))
		layer->show();
}

void LevelEditorLayer::showEditGroupPopup(GameObject* obj)
{
	if (auto* layer = EditorPropertyLayer::create(this, EditorPropertyKind::Group, obj))
		layer->show();
}

void LevelEditorLayer::showEditSpecialPopup(GameObject* obj)
{
	if (auto* layer = EditorPropertyLayer::create(this, EditorPropertyKind::Special, obj))
		layer->show();
}

void LevelEditorLayer::showEditColorPopup(GameObject* obj)
{
	if (auto* layer = EditorPropertyLayer::create(this, EditorPropertyKind::Color, obj))
		layer->show();
}

void LevelEditorLayer::showLevelSettings()
{
	if (auto* layer = LevelSettingsLayer::create(this))
		layer->show();
}

void LevelEditorLayer::applyLevelSettingsVisuals()
{
	const auto winSize = Director::getInstance()->getWinSize();
	const int bgId = _levelSettings._bgID > 0 ? _levelSettings._bgID : 1;

	if (m_pBG)
	{
		auto* tmp = Sprite::create(GameToolbox::getTextureString(fmt::format("game_bg_{:02}_001.png", bgId)));
		if (!tmp)
			tmp = Sprite::create(GameToolbox::getTextureString("game_bg_01_001.png"));
		if (tmp && tmp->getTexture())
		{
			auto* tex = tmp->getTexture();
			const Texture2D::TexParams texParams = {
				backend::SamplerFilter::LINEAR, backend::SamplerFilter::LINEAR,
				backend::SamplerAddressMode::REPEAT, backend::SamplerAddressMode::REPEAT};
			tex->setTexParameters(texParams);
			m_pBG->setTexture(tex);
			m_pBG->setTextureRect(Rect(0, 0, 1024 * 5, 1024));
		}
		if (_colorChannels.contains(1000))
			m_pBG->setColor(_colorChannels.at(1000)._color);
	}

	const int groundId = _levelSettings._groundID > 0 ? _levelSettings._groundID : 1;
	if (_bottomGround)
	{
		_bottomGround->setGroundID(groundId);
		if (_bottomGround->_sprite && _colorChannels.contains(1001))
			_bottomGround->_sprite->setColor(_colorChannels.at(1001)._color);
	}
	if (_ceiling)
	{
		_ceiling->setGroundID(groundId);
		if (_ceiling->_sprite && _colorChannels.contains(1001))
			_ceiling->_sprite->setColor(_colorChannels.at(1001)._color);
	}

	if (_levelSettings._mgID <= 0)
	{
		if (_middleGround)
			_middleGround->setVisible(false);
	}
	else
	{
		const int mgId = _levelSettings._mgID;
		auto* mgTmp = Sprite::create(GameToolbox::getTextureString(fmt::format("game_bg_{:02}_001.png", mgId)));
		if (!mgTmp)
			mgTmp = Sprite::create(GameToolbox::getTextureString("game_bg_01_001.png"));
		if (!_middleGround)
		{
			_middleGround = mgTmp;
			if (_middleGround)
			{
				_middleGround->setOpacity(90);
				_middleGround->setScale(0.45f);
				_middleGround->setPosition(winSize.width * 0.5f, winSize.height * 0.42f);
				addChild(_middleGround, -90);
			}
		}
		else if (mgTmp && mgTmp->getTexture())
		{
			_middleGround->setTexture(mgTmp->getTexture());
		}
		if (_middleGround)
		{
			_middleGround->setVisible(true);
			if (_colorChannels.contains(1009))
				_middleGround->setColor(_colorChannels.at(1009)._color);
		}
	}
}

void LevelEditorLayer::saveLevelString()
{
	if (!getLevel())
		return;

	std::string levelData;
	levelData += fmt::format("kA1,{},", getLevel()->_musicID);
	levelData += fmt::format("kA2,{},", static_cast<int>(_levelSettings.gamemode));
	levelData += fmt::format("kA3,{},", static_cast<int>(_levelSettings.mini));
	levelData += fmt::format("kA4,{},", _levelSettings.speed);
	levelData += fmt::format("kA6,{},", _levelSettings._bgID);
	levelData += fmt::format("kA7,{},", _levelSettings._groundID);
	levelData += fmt::format("kA8,{},", static_cast<int>(_levelSettings.dual));
	levelData += fmt::format("kA9,{},", 0);
	levelData += fmt::format("kA10,{},", static_cast<int>(_levelSettings.twoPlayer));
	levelData += fmt::format("kA11,{},", static_cast<int>(_levelSettings.flipGravity));
	levelData += fmt::format(
		"kS29,1,{}_2,{}_3,{},", _colorChannels[1000]._color.r, _colorChannels[1000]._color.g,
		_colorChannels[1000]._color.b);
	levelData += fmt::format("kA13,{},", _levelSettings.songOffset);
	levelData += fmt::format("kA15,{},", _levelSettings._fontID);
	levelData += fmt::format("kA16,{},", _levelSettings._mgID);
	levelData += fmt::format("kA22,{},", static_cast<int>(_levelSettings.platformer));
	levelData += "kS38,";

	for (auto& [id, col] : _colorChannels)
	{
		levelData += fmt::format(
			"1,{}_2,{}_3,{}_5,{}_6,{}_7,{}|", col._color.r, col._color.g, col._color.b, static_cast<int>(col._blending),
			id, static_cast<int>(col._opacity));
	}
	if (!levelData.empty() && levelData.back() == '|')
		levelData.pop_back();

	levelData += ";";

	int objectCount = 0;
	for (GameObject* object : _allObjects)
	{
		if (!object)
			continue;
		levelData += serializeObject(object) + ";";
		++objectCount;
	}

	getLevel()->_levelString = levelData;
	getLevel()->_objects = objectCount;
	getLevel()->_length = GameToolbox::calculateLevelLengthCategory(
		_levelSettings.speed, _levelSettings.platformer, _allObjects);
	LocalLevelManager::get()->upsertLevel(getLevel());
}

void LevelEditorLayer::goBack()
{
	saveLevelString();
	AudioEngine::stopAll();
	AudioEngine::play2d("quitSound_01.ogg", false, 0.1f);
	AudioEngine::play2d("menuLoop.mp3", true, 0.2f);
	_instance = nullptr;
	GameToolbox::popSceneWithTransition(0.5f);
}

void LevelEditorLayer::exitWithoutSaving()
{
	AudioEngine::stopAll();
	AudioEngine::play2d("quitSound_01.ogg", false, 0.1f);
	AudioEngine::play2d("menuLoop.mp3", true, 0.2f);
	_instance = nullptr;
	GameToolbox::popSceneWithTransition(0.5f);
}

void LevelEditorLayer::hideEditorMenu()
{
	if (_editorMenu)
	{
		_editorMenu->removeFromParent();
		_editorMenu = nullptr;
	}
	_menuOpen = false;
}

void LevelEditorLayer::showEditorMenu()
{
	if (_menuOpen)
		return;
	_menuOpen = true;

	const auto winSize = Director::getInstance()->getWinSize();
	_editorMenu = LayerColor::create({0, 0, 0, 160});
	_editorMenu->setContentSize(winSize);
	if (_hudLayer)
		_hudLayer->addChild(_editorMenu, 6000);
	else
		addChild(_editorMenu, 5000);

	auto* panel = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
	if (!panel)
		panel = ui::Scale9Sprite::create(GameToolbox::getTextureString("square01_001.png"));
	if (panel)
	{
		panel->setContentSize({280.f, 260.f});
		panel->setPosition(winSize * 0.5f);
		_editorMenu->addChild(panel);
	}

	auto* title = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Pause");
	if (title)
	{
		title->setScale(0.7f);
		title->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 100.f});
		_editorMenu->addChild(title);
	}

	auto* menu = Menu::create();
	menu->setPosition({winSize.width * 0.5f, winSize.height * 0.5f});
	_editorMenu->addChild(menu);

	auto addBtn = [&](const char* text, float y, std::function<void()> action) {
		auto* spr = ButtonSprite::create(
			text, 0xDC, 0, 0.6f, false, GameToolbox::getTextureString("bigFont.fnt"),
			GameToolbox::getTextureString("GJ_button_01.png"), 40);
		if (!spr)
			return;
		auto* btn = MenuItemSpriteExtra::create(spr, [action](Node*) {
			if (action)
				action();
		});
		btn->setPosition({0.f, y});
		menu->addChild(btn);
	};

	addBtn("Continue", 60.f, [this] { hideEditorMenu(); });
	addBtn("Save and Exit", 20.f, [this] {
		hideEditorMenu();
		goBack();
	});
	addBtn("Save and Play", -20.f, [this] {
		hideEditorMenu();
		openPlaytest();
	});
	addBtn("Exit", -60.f, [this] {
		hideEditorMenu();
		auto* alert = AlertLayer::create(
			"Exit?", "Exit without saving changes?", "Exit", "Cancel",
			[this](Node*) { exitWithoutSaving(); }, [](Node*) {});
		if (alert)
			alert->show();
	});
}

void LevelEditorLayer::openPlaytest()
{
	saveLevelString();

	auto* level = getLevel();
	if (!level)
		return;

	auto* playLevel = GJGameLevel::createWithMinimumData(level->_levelName, level->_levelCreator, -1);
	playLevel->_levelString = level->_levelString;

	auto* playLayer = PlayLayer::create(playLevel);
	if (!playLayer)
		return;
	playLayer->_testMode = true;

	auto* scene = Scene::create();
	scene->addChild(playLayer);
	Director::getInstance()->pushScene(TransitionFade::create(0.5f, scene));
}

void LevelEditorLayer::setPlaytestDualMode(bool dual)
{
	_playtestDual = dual;
	if (!_player2)
		return;

	if (dual)
	{
		if (_playtestCamYCenter <= 0.f)
			_playtestCamYCenter = 240.f;

		_player2->setPosition(_player1 ? _player1->getPosition() : Vec2(2.f, 105.f));
		_player2->setVisible(true);
		_player2->setActive(true);
		_player2->reset();
		_player2->flipGravity(_player1 ? !_player1->isGravityFlipped() : true);
		if (_player1)
		{
			_player2->setGamemode(_player1->_currentGamemode);
			_player2->toggleMini(_player1->_mini);
			_player2->m_dXVel = _player1->m_dXVel;
			_player2->setPlayerSpeed(_player1->getPlayerSpeed());
			_player2->m_bIsPlatformer = _player1->m_bIsPlatformer;
			_player2->direction = _player1->direction;
		}

		if (_bottomGround)
			_bottomGround->setPositionY(_player1 && _player1->_currentGamemode == PlayerGamemodeBall ? -38.f : -68.f);
		if (_ceiling)
		{
			_ceiling->setPositionY(_player1 && _player1->_currentGamemode == PlayerGamemodeBall ? 358.f : 388.f);
			_ceiling->setVisible(true);
		}

		_player2->setIsOnGround(false);
		_player2->setYVel(0.f);
	}
	else
	{
		_player2->setVisible(false);
		_player2->setActive(false);
		_player2->flipGravity(false);
		const bool keepFlyBox = _player1 && (_player1->isFlying() || _player1->_currentGamemode == PlayerGamemodeBall);
		if (_ceiling && !keepFlyBox)
			_ceiling->setVisible(false);
		if (!keepFlyBox && _bottomGround && _cameraFollow)
			_bottomGround->setPositionY(-_cameraFollow->getPositionY() + 12.f);
	}
}

void LevelEditorLayer::applyPlaytestSpeed(int speed)
{
	auto applyTo = [speed](PlayerObject* player) {
		if (!player)
			return;
		switch (speed)
		{
		case 1:
			player->m_dXVel = 5.98;
			player->setPlayerSpeed(0.7f);
			break;
		case 2:
			player->m_dXVel = 5.87;
			player->setPlayerSpeed(1.1f);
			break;
		case 3:
			player->m_dXVel = 6;
			player->setPlayerSpeed(1.3f);
			break;
		case 4:
			player->m_dXVel = 6;
			player->setPlayerSpeed(1.6f);
			break;
		default:
			player->m_dXVel = 5.77;
			player->setPlayerSpeed(0.9f);
			break;
		}
	};
	applyTo(_player1);
	applyTo(_player2);
}

void LevelEditorLayer::resetPlaytestObjectState()
{
	for (GameObject* obj : _allObjects)
	{
		if (!obj)
			continue;
		obj->_hasBeenActivatedP1 = false;
		obj->_hasBeenActivatedP2 = false;
		obj->_effectOpacityMultipler = 1.f;
		obj->_startPosOffset = {0.f, 0.f};
		obj->_toggledOn = true;
		obj->setPosition(obj->getStartPosition());
		obj->setVisible(true);
		obj->setOpacity(255);
	}
}

void LevelEditorLayer::rebuildPlaytestSections()
{
	_sectionObjects.clear();
	_lastObjXPos = 570.f;
	for (GameObject* obj : _allObjects)
	{
		if (!obj)
			continue;
	int section = sectionForPos(obj->getPositionX());
		section = section - 1 < 0 ? 0 : section - 1;
		obj->_section = section;
		ensureSectionCapacity(section);
		_sectionObjects[section].push_back(obj);
		if (obj->getPositionX() > _lastObjXPos)
			_lastObjXPos = obj->getPositionX();
	}
	rebuildObjectCache();
}

void LevelEditorLayer::recordPlaytestTrail(const Vec2& pos)
{
	if (_playtestTrailPts.empty())
	{
		_playtestTrailPts.push_back(pos);
		return;
	}
	const Vec2& last = _playtestTrailPts.back();
	if (last.distance(pos) < 3.f)
		return;
	if (_playtestTrail)
		_playtestTrail->drawSegment(last, pos, 3.6f, Color4F(0.12f, 0.95f, 0.22f, 0.88f));
	_playtestTrailPts.push_back(pos);
	if (_playtestTrailPts.size() > 4000)
	{
		_playtestTrailPts.erase(_playtestTrailPts.begin(), _playtestTrailPts.begin() + 1000);
	}
}

void LevelEditorLayer::spawnPlaytestDeathCross(const Vec2& pos)
{
	if (!_playtestDeathMarks)
		return;
	auto* cross = DrawNode::create();
	constexpr float arm = 14.f;
	constexpr float thick = 3.2f;
	const Color4F red(1.f, 0.12f, 0.12f, 1.f);
	cross->drawSegment({-arm, -arm}, {arm, arm}, thick, red);
	cross->drawSegment({-arm, arm}, {arm, -arm}, thick, red);
	cross->setPosition(pos);
	_playtestDeathMarks->addChild(cross);
}

void LevelEditorLayer::focusCameraOnPlaytestDeath()
{
	if (!_hasPlaytestDeathPos)
		return;

	const auto winSize = Director::getInstance()->getWinSize();
	const float maxCamX = std::max(0.f, _lastObjXPos - winSize.width * 0.35f);
	constexpr float kMinEditorCamY = -240.f;

	m_obCamPos.x = _playtestDeathPos.x - winSize.width / 2.5f;
	m_obCamPos.y = _playtestDeathPos.y - winSize.height * 0.45f;
	m_obCamPos.x = clampf(m_obCamPos.x, 0.f, maxCamX);
	m_obCamPos.y = clampf(m_obCamPos.y, kMinEditorCamY, 1140.f - winSize.height);
	_lastGroundCamX = m_obCamPos.x;
}

void LevelEditorLayer::startEditorPlaytest()
{
	if (_playtesting)
		stopEditorPlaytest(false);

	stopEditorMusic();
	saveLevelString();
	_instance = this;
	_playtesting = true;
	_playtestDead = false;
	_playtestJumpHeld = false;
	_playtestCamYCenter = 0.f;
	_editorCamBeforePlaytest = m_obCamPos;
	_editorZoomBeforePlaytest = _editorZoom;
	_hasPlaytestDeathPos = false;
	// Objects live under scaled _gameLayer; player/ground do not — force 1x for a fair playtest.
	_editorZoom = 1.f;
	applyEditorZoom();
	m_camDelta = {0.f, 0.f};
	m_obCamPos = {0.f, 0.f};
	_lastGroundCamX = 0.f;

	if (_playtestTrail)
		_playtestTrail->clear();
	_playtestTrailPts.clear();
	if (_playtestDeathMarks)
		_playtestDeathMarks->removeAllChildren();

	rebuildPlaytestSections();
	resetPlaytestObjectState();
	setEditorHudVisible(false);
	refreshPlaytestButton();

	if (_player1)
	{
		_player1->removeFromParent();
		_player1 = nullptr;
	}
	if (_player2)
	{
		_player2->removeFromParent();
		_player2 = nullptr;
	}
	_playtestDual = false;

	auto* gm = GameManager::getInstance();
	int playerIcon = gm ? gm->getSelectedIcon(IconType::kIconTypeCube) : 1;
	if (playerIcon < 1)
		playerIcon = 1;

	_player1 = PlayerObject::create(playerIcon, this);
	if (!_player1)
	{
		stopEditorPlaytest(false);
		return;
	}
	_player1->setPosition({2.f, 105.f});
	addChild(_player1, 101);
	_player2 = PlayerObject::create(playerIcon, this);
	if (_player2)
	{
		_player2->setPosition({2.f, 105.f});
		addChild(_player2, 101);
		_player2->setVisible(false);
		_player2->setActive(false);
	}
	if (gm)
	{
		_player1->setMainColor(gm->getPlayerMainColor());
		_player1->setSecondaryColor(gm->getPlayerSecondaryColor());
		_player1->setGlowColor(gm->getPlayerGlowColor());
		_player1->setGlow(gm->isPlayerGlowEnabled());
		if (_player2)
		{
			_player2->setMainColor(gm->getPlayerMainColor());
			_player2->setSecondaryColor(gm->getPlayerSecondaryColor());
			_player2->setGlowColor(gm->getPlayerGlowColor());
			_player2->setGlow(gm->isPlayerGlowEnabled());
		}
	}
	_player1->reset();
	_player1->setPosition({2.f, 105.f});
	_player1->setVisible(true);
	_player1->setGamemode(_levelSettings.gamemode);
	_player1->toggleMini(_levelSettings.mini);
	_player1->m_bIsPlatformer = _levelSettings.platformer;
	_player1->direction = _levelSettings.platformer ? 0.f : 1.f;
	if (_levelSettings.flipGravity)
		_player1->flipGravity(true);
	applyPlaytestSpeed(_levelSettings.speed);
	if (_levelSettings.gamemode == PlayerGamemodeShip || _levelSettings.gamemode == PlayerGamemodeUFO ||
		_levelSettings.gamemode == PlayerGamemodeWave || _levelSettings.gamemode == PlayerGamemodeSwing)
		_playtestCamYCenter = 240.f;
	else if (_levelSettings.gamemode == PlayerGamemodeBall)
		_playtestCamYCenter = 210.f;

	if (_bottomGround && _cameraFollow)
	{
		_bottomGround->setPositionX(0.f);
		_bottomGround->setPositionY(-_cameraFollow->getPositionY() + 12.f);
	}

	if (_levelSettings.dual)
		setPlaytestDualMode(true);

	recordPlaytestTrail(_player1->getPosition());
	updatePlaytestCamera(0.f);
}

void LevelEditorLayer::stopEditorPlaytest(bool died)
{
	unschedule("editor_playtest_stop");
	if (!_playtesting && !_player1)
		return;

	if (_player1 && _player1->m_bIsHolding)
		_player1->releaseButton();
	if (_player2 && _player2->m_bIsHolding)
		_player2->releaseButton();

	if (_player1)
	{
		_player1->removeFromParent();
		_player1 = nullptr;
	}
	if (_player2)
	{
		_player2->removeFromParent();
		_player2 = nullptr;
	}
	_playtestDual = false;
	if (_ceiling)
		_ceiling->setVisible(false);

	_playtesting = false;
	_playtestDead = false;
	_playtestJumpHeld = false;
	m_camDelta = {0.f, 0.f};
	_editorZoom = _editorZoomBeforePlaytest;
	applyEditorZoom();

	if (died && _hasPlaytestDeathPos)
	{
		// Keep green trail + death cross, jump editor cam to the death spot.
		focusCameraOnPlaytestDeath();
	}
	else
	{
		m_obCamPos = _editorCamBeforePlaytest;
		_lastGroundCamX = m_obCamPos.x;
		if (_playtestTrail)
			_playtestTrail->clear();
		_playtestTrailPts.clear();
		if (_playtestDeathMarks)
			_playtestDeathMarks->removeAllChildren();
		_hasPlaytestDeathPos = false;
	}

	resetPlaytestObjectState();
	applyEditorLayerVisibility();
	setEditorHudVisible(true);
	refreshPlaytestButton();
	updateCamera(0.f);
}

void LevelEditorLayer::destroyPlayer(PlayerObject* player)
{
	if (!_playtesting || _playtestDead || !player || player->isDead())
		return;

	_playtestDead = true;
	_playtestDeathPos = player->getPosition();
	_hasPlaytestDeathPos = true;
	player->setIsDead(true);
	player->playDeathEffect();
	player->stopRotation();
	player->setVisible(false);
	recordPlaytestTrail(_playtestDeathPos);
	spawnPlaytestDeathCross(_playtestDeathPos);

	scheduleOnce([this](float) { stopEditorPlaytest(true); }, 0.7f, "editor_playtest_stop");
}

void LevelEditorLayer::updatePlaytest(float dt)
{
	float step = std::min(2.0f, dt * 60.0f);
	if (_player1 && !_playtestDead && !_player1->isDead())
	{
		_player1->m_bIsPlatformer = _levelSettings.platformer;
		_player1->storeShipRotationPos();
		if (_playtestDual && _player2)
		{
			_player2->m_bIsPlatformer = _levelSettings.platformer;
			_player2->storeShipRotationPos();
		}
		step /= 4.0f;
		for (int i = 0; i < 4; i++)
		{
			_player1->update(step);
			_player1->setOuterBounds(Rect(_player1->getPosition() - Vec2(15.f, 15.f), Vec2(30.f, 30.f)));
			_player1->setInnerBounds(Rect(_player1->getPosition() - Vec2(3.75f, 3.75f), Vec2(7.5f, 7.5f)));
			checkPlaytestCollisions(_player1, step);
			if (!_player1 || _player1->isDead() || _playtestDead)
				break;
			recordPlaytestTrail(_player1->getPosition());

			if (!_playtestDual || !_player2)
				continue;
			_player2->update(step);
			_player2->setOuterBounds(Rect(_player2->getPosition() - Vec2(15.f, 15.f), Vec2(30.f, 30.f)));
			_player2->setInnerBounds(Rect(_player2->getPosition() - Vec2(3.75f, 3.75f), Vec2(7.5f, 7.5f)));
			checkPlaytestCollisions(_player2, step);
			if (!_player2 || _player2->isDead() || _playtestDead)
				break;
		}
		step *= 4.0f;
		if (_player1 && !_playtestDead &&
			(_player1->_currentGamemode == PlayerGamemodeShip || _player1->_currentGamemode == PlayerGamemodeUFO))
			_player1->updateShipRotation(step);
		if (_playtestDual && _player2 && !_playtestDead &&
			(_player2->_currentGamemode == PlayerGamemodeShip || _player2->_currentGamemode == PlayerGamemodeUFO))
			_player2->updateShipRotation(step);
	}
	updatePlaytestCamera(step);
}

void LevelEditorLayer::checkPlaytestCollisions(PlayerObject* player, float dt)
{
	if (!player)
		return;

	player->beginSlopePass();

	auto playerOuterBounds = player->_mini ? player->getOuterBounds(0.6f, 0.6f) : player->getOuterBounds();
	player->setTouchedRing(nullptr);

	if (player->getPositionY() < (player->_mini ? 99.f : 105.0f) && player->isGroundedMode())
	{
		if (player->isGravityFlipped())
		{
			destroyPlayer(player);
			return;
		}
		player->setPositionY(player->_mini ? 99.f : 105.0f);
		player->hitGround(false);
	}
	else if (player->getPositionY() > 1290.0f)
	{
		destroyPlayer(player);
			return;
		}

	if (_playtestDual && player->isGroundedMode())
	{
		const float shift = _playtestCamYCenter > 0.f ? (_playtestCamYCenter - 240.f) : 0.f;
		const float dualCeil = (player->_mini ? 249.f : 255.f) + shift;
		if (player->getPositionY() > dualCeil)
		{
			if (!player->isGravityFlipped())
			{
				destroyPlayer(player);
				return;
			}
			player->setPositionY(dualCeil);
			player->hitGround(true);
		}
	}

	if (player->isFlying() || player->_currentGamemode == PlayerGamemodeBall)
	{
		const float camY = _cameraFollow ? _cameraFollow->getPositionY() : m_obCamPos.y;
		const float groundY = _bottomGround ? _bottomGround->getPositionY() : 0.f;
		const float worldFloor = groundY + camY + (player->_mini ? 87.f : 93.f);
		const float worldCeil = _playtestCamYCenter + (player->_mini ? 69.f : 75.f);
		if (player->getPositionY() < worldFloor)
		{
			player->setPositionY(worldFloor);
			if (!player->isGravityFlipped())
				player->hitGround(false);
			player->setYVel(0.f);
		}
		if (player->getPositionY() > worldCeil)
		{
			player->setPositionY(worldCeil);
			if (player->isGravityFlipped())
				player->hitGround(true);
			player->setYVel(0.f);
		}
	}

	player->setOuterBounds(Rect(player->getPosition() - Vec2(15.f, 15.f), Vec2(30.f, 30.f)));
	player->setInnerBounds(Rect(player->getPosition() - Vec2(3.75f, 3.75f), Vec2(7.5f, 7.5f)));
	playerOuterBounds = player->_mini ? player->getOuterBounds(0.6f, 0.6f) : player->getOuterBounds();

	const int currentSection = sectionForPos(player->getPositionX());
	std::vector<GameObject*> hazards;
	player->clearLetterBlockFlags();

	auto enterGamemode = [this, player](GameObject* obj, PlayerGamemode mode) {
		obj->triggerActivated(player);
		player->setPortalP(obj->getPosition());
		player->setPortalObject(obj);
		player->setRotation(0.f);
		player->setGamemode(mode);
		if (mode == PlayerGamemodeShip || mode == PlayerGamemodeUFO || mode == PlayerGamemodeWave ||
			mode == PlayerGamemodeSwing)
			_playtestCamYCenter = obj->getPositionY() < 270.f ? 240.f : (std::floor(obj->getPositionY() / 30.f) * 30.f);
		else if (mode == PlayerGamemodeBall)
			_playtestCamYCenter = obj->getPositionY() < 240.f ? 210.f : (std::floor(obj->getPositionY() / 30.f) * 30.f);
		else if (_playtestDual)
			_playtestCamYCenter = 240.f;
		else
			_playtestCamYCenter = 0.f;
	};

	for (int i = currentSection - 2; i <= currentSection + 1; i++)
	{
		if (i < 0 || i >= static_cast<int>(_sectionObjects.size()))
			continue;
		for (GameObject* obj : _sectionObjects[i])
		{
			if (!obj)
				continue;
			if (obj->wantsCollisionBounds())
				obj->refreshCollisionBounds();

			if (obj->isLetterBlock())
			{
				if (playerOuterBounds.intersectsRect(obj->getLetterBlockBounds()))
					player->applyLetterBlock(obj);
				continue;
			}

			const GameObjectType earlyType = obj->getGameObjectType();
			if (earlyType == kGameObjectTypeCubePortal || earlyType == kGameObjectTypeShipPortal ||
				earlyType == kGameObjectTypeBallPortal || earlyType == kGameObjectTypeUfoPortal ||
				earlyType == kGameObjectTypeWavePortal || earlyType == kGameObjectTypeRobotPortal ||
				earlyType == kGameObjectTypeSpiderPortal || earlyType == kGameObjectTypeSwingPortal ||
				earlyType == kGameObjectTypeDualPortal || earlyType == kGameObjectTypeSoloPortal)
			{
				if (obj->hasBeenActivatedByPlayer(player))
					continue;
				Rect portalBounds = obj->getOuterBounds();
				if (portalBounds.size.width <= 0.f || portalBounds.size.height <= 0.f)
					portalBounds = Rect(obj->getPosition() - Vec2(20.f, 50.f), Vec2(40.f, 100.f));
				portalBounds.origin -= Vec2(8.f, 16.f);
				portalBounds.size += Vec2(16.f, 32.f);
				if (playerOuterBounds.intersectsRect(portalBounds))
				{
					switch (earlyType)
					{
					case kGameObjectTypeShipPortal:
						enterGamemode(obj, PlayerGamemodeShip);
						break;
					case kGameObjectTypeBallPortal:
						enterGamemode(obj, PlayerGamemodeBall);
						break;
					case kGameObjectTypeUfoPortal:
						enterGamemode(obj, PlayerGamemodeUFO);
						break;
					case kGameObjectTypeCubePortal:
						enterGamemode(obj, PlayerGamemodeCube);
						break;
					case kGameObjectTypeWavePortal:
						enterGamemode(obj, PlayerGamemodeWave);
						break;
					case kGameObjectTypeRobotPortal:
						enterGamemode(obj, PlayerGamemodeRobot);
						break;
					case kGameObjectTypeSpiderPortal:
						enterGamemode(obj, PlayerGamemodeSpider);
						break;
					case kGameObjectTypeSwingPortal:
						enterGamemode(obj, PlayerGamemodeSwing);
						break;
					case kGameObjectTypeDualPortal:
						obj->triggerActivated(player);
						player->setPortalP(obj->getPosition());
						player->setPortalObject(obj);
						setPlaytestDualMode(true);
						break;
					case kGameObjectTypeSoloPortal:
						obj->triggerActivated(player);
						player->setPortalP(obj->getPosition());
						player->setPortalObject(obj);
						setPlaytestDualMode(false);
						break;
					default:
						break;
					}
				}
				continue;
			}

			auto objBounds = obj->getOuterBounds();
			const GameObjectType objType = obj->getGameObjectType();
			if (objType == kGameObjectTypeDecoration || objType == kGameObjectTypeSpecial ||
				objType == kGameObjectTypeLetterD || objType == kGameObjectTypeLetterJ ||
				objType == kGameObjectTypeLetterS || objType == kGameObjectTypeLetterH ||
				objType == kGameObjectTypeLetterF || !obj->wantsCollisionBounds())
				continue;
			if (obj->_isTrigger)
				continue;

			if (objType == kGameObjectTypeHazard)
			{
				if (objBounds.size.width <= 0.f || objBounds.size.height <= 0.f)
				{
					const Hitbox hb = GameObject::resolveHitbox(obj->getID());
					if (hb.w > 0.f && hb.h > 0.f)
						objBounds = Rect(obj->getPosition() + Vec2(hb.x, hb.y), Vec2(hb.w, hb.h));
					else
						objBounds = Rect(obj->getPosition() + Vec2(-3.f, -6.f), Vec2(6.f, 12.f));
					obj->setOuterBounds(objBounds);
				}
			}

			if ((objBounds.size.width <= 0 || objBounds.size.height <= 0) && obj->_radius <= 0)
				continue;

			if (objType == kGameObjectTypeHazard)
			{
				hazards.push_back(obj);
			}
			else if (objType == kGameObjectTypeSolid || objType == kGameObjectTypeSlope ||
					 objType == kGameObjectTypeCollisionObject)
			{
				if (playerOuterBounds.intersectsRect(objBounds))
				{
					if (objType == kGameObjectTypeSlope)
						player->collidedWithSlope(dt, obj);
					else
						player->collidedWithObject(dt, obj);
				}
			}
			else if (playerOuterBounds.intersectsRect(objBounds))
			{
				if (objType != kGameObjectTypeSolid && objType != kGameObjectTypeSlope &&
					obj->hasBeenActivatedByPlayer(player))
					continue;

				switch (objType)
				{
				case kGameObjectTypeInverseGravityPortal:
					obj->triggerActivated(player);
					player->setPortalP(obj->getPosition());
					player->setPortalObject(obj);
					player->flipGravity(true);
					break;
				case kGameObjectTypeNormalGravityPortal:
					obj->triggerActivated(player);
					player->setPortalP(obj->getPosition());
					player->setPortalObject(obj);
					player->flipGravity(false);
					break;
				case kGameObjectTypeYellowJumpPad:
					player->setPortalP(obj->getPosition());
					player->setPortalObject(obj);
					obj->triggerActivated(player);
					player->propellPlayer(1);
					player->_touchedPadObject = obj;
					break;
				case kGameObjectTypeGravityPad: {
					if (player->_touchedPadObject)
						break;
					auto pos = obj->getPosition();
					pos.y -= 10;
					player->setPortalP(pos);
					player->setPortalObject(obj);
					obj->triggerActivated(player);
					player->propellPlayer(0.8);
					player->_touchedPadObject = obj;
					player->flipGravity(!player->isGravityFlipped());
					break;
				}
				case kGameObjectTypePinkJumpPad:
					player->setPortalP(obj->getPosition());
					player->setPortalObject(obj);
					obj->triggerActivated(player);
					player->propellPlayer(0.65);
					player->_touchedPadObject = obj;
					break;
				case kGameObjectTypeRedJumpPad:
					player->setPortalP(obj->getPosition());
					player->setPortalObject(obj);
					obj->triggerActivated(player);
					player->propellPlayer(1.25);
					player->_touchedPadObject = obj;
					break;
				case kGameObjectTypeYellowJumpRing:
				case kGameObjectTypeDashRing:
				case kGameObjectTypeGravityDashRing:
				case kGameObjectTypeGravityRing:
				case kGameObjectTypeRedJumpRing:
				case kGameObjectTypePinkJumpRing:
				case kGameObjectTypeDropRing:
				case kGameObjectTypeGreenRing:
				case kGameObjectTypeSpiderRing:
				case kGameObjectTypeCustomRing:
					player->setPortalP(obj->getPosition());
					player->setPortalObject(obj);
					player->setTouchedRing(obj);
					if (player->m_bIsHolding)
						player->_queuedHold = true;
					player->ringJump(obj);
					break;
				case kGameObjectTypeModifier:
					obj->triggerActivated(player);
					switch (obj->getID())
					{
					case 201:
						applyPlaytestSpeed(0);
						break;
					case 200:
						applyPlaytestSpeed(1);
						break;
					case 202:
						applyPlaytestSpeed(2);
						break;
					case 203:
						applyPlaytestSpeed(3);
						break;
					case 1334:
						applyPlaytestSpeed(4);
						break;
					default:
						break;
					}
					break;
				case kGameObjectTypeMiniSizePortal:
					obj->triggerActivated(player);
					player->setPortalP(obj->getPosition());
					player->setPortalObject(obj);
					player->toggleMini(true);
					break;
				case kGameObjectTypeRegularSizePortal:
					obj->triggerActivated(player);
					player->setPortalP(obj->getPosition());
					player->setPortalObject(obj);
					player->toggleMini(false);
					break;
				default:
					break;
				}
			}
			if (player->isDead() || _playtestDead)
			{
				player->endSlopePass(dt);
	return;
			}
		}
	}

	for (GameObject* hazard : hazards)
	{
		if (!hazard)
			continue;
		if (hazard->_radius > 0)
		{
			if (playerOuterBounds.intersectsCircle(hazard->getPosition(), hazard->_radius))
			{
				destroyPlayer(player);
				player->endSlopePass(dt);
				return;
			}
		}
		else
		{
			Rect hazardBounds = hazard->getOuterBounds();
			if (hazardBounds.size.width <= 0.f || hazardBounds.size.height <= 0.f)
				hazardBounds = Rect(hazard->getPosition() - Vec2(9.f, 10.f), Vec2(18.f, 20.f));
			if (playerOuterBounds.intersectsRect(hazardBounds))
			{
				destroyPlayer(player);
				player->endSlopePass(dt);
				return;
			}
		}
	}

	player->endSlopePass(dt);
	if (player->_currentGamemode == PlayerGamemodeShip)
		player->_queuedHold = false;
}

void LevelEditorLayer::onKeyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
	if (_menuOpen)
	{
		if (keyCode == EventKeyboard::KeyCode::KEY_ESCAPE)
			hideEditorMenu();
			return;
		}

	if (_playtesting)
	{
		if (keyCode == EventKeyboard::KeyCode::KEY_ESCAPE)
		{
			stopEditorPlaytest(false);
			return;
		}
		if (!_playtestDead && _player1)
		{
			if (keyCode == EventKeyboard::KeyCode::KEY_SPACE || keyCode == EventKeyboard::KeyCode::KEY_UP_ARROW)
			{
				if (!_player1->m_bIsHolding)
					_player1->pushButton();
				if (_playtestDual && _player2 && !_player2->m_bIsHolding)
					_player2->pushButton();
				_playtestJumpHeld = true;
			}
			if (_player1->m_bIsPlatformer)
			{
				if (keyCode == EventKeyboard::KeyCode::KEY_A || keyCode == EventKeyboard::KeyCode::KEY_LEFT_ARROW)
					_player1->direction = -1.f;
				else if (keyCode == EventKeyboard::KeyCode::KEY_D || keyCode == EventKeyboard::KeyCode::KEY_RIGHT_ARROW)
					_player1->direction = 1.f;
			}
		}
		return;
	}

	if (_ctrlPressed)
	{
		if (keyCode == EventKeyboard::KeyCode::KEY_Z)
		{
			editorUndo();
			return;
		}
		if (keyCode == EventKeyboard::KeyCode::KEY_Y)
		{
			editorRedo();
			return;
		}
		if (keyCode == EventKeyboard::KeyCode::KEY_C)
		{
			copySelection();
			return;
		}
		if (keyCode == EventKeyboard::KeyCode::KEY_V)
		{
			pasteClipboard();
			return;
		}
		if (keyCode == EventKeyboard::KeyCode::KEY_D)
		{
			duplicateSelection();
			return;
		}
	}

	float step = _shiftPressed ? 2.f : kGridSize;
	float scaling = _shiftPressed ? 0.1f : 1.f;
	bool movedX = false;
	bool movedY = false;
	float appliedStep = step;

	switch (keyCode)
	{
	case EventKeyboard::KeyCode::KEY_B:
		setToolMode(EditorToolMode::Build);
		break;
	case EventKeyboard::KeyCode::KEY_D:
		// Always pan with D when nothing valid is selected; never switch tool modes.
		if (_selectedObjectReal &&
			std::find(_allObjects.begin(), _allObjects.end(), _selectedObjectReal) != _allObjects.end())
			moveSelected(step, 0.f);
		else
		{
			clearSelection();
			m_camDelta.x = 7.f;
		}
		break;
	case EventKeyboard::KeyCode::KEY_A:
		if (_selectedObjectReal &&
			std::find(_allObjects.begin(), _allObjects.end(), _selectedObjectReal) != _allObjects.end())
			moveSelected(-step, 0.f);
		else
		{
			clearSelection();
			m_camDelta.x = -7.f;
		}
		break;
	case EventKeyboard::KeyCode::KEY_W:
		if (_selectedObjectReal &&
			std::find(_allObjects.begin(), _allObjects.end(), _selectedObjectReal) != _allObjects.end())
			moveSelected(0.f, step);
		else
		{
			clearSelection();
			m_camDelta.y = 7.f;
		}
		break;
	case EventKeyboard::KeyCode::KEY_S:
		if (_selectedObjectReal &&
			std::find(_allObjects.begin(), _allObjects.end(), _selectedObjectReal) != _allObjects.end())
			moveSelected(0.f, -step);
		else
		{
			clearSelection();
			m_camDelta.y = -7.f;
		}
		break;
	case EventKeyboard::KeyCode::KEY_Q:
		rotateSelected(-90.f);
		break;
	case EventKeyboard::KeyCode::KEY_E:
		if (_selectedObjectReal)
			rotateSelected(90.f);
		else
			setToolMode(EditorToolMode::Edit);
		break;
	case EventKeyboard::KeyCode::KEY_R:
		rotateSelected(90.f);
		break;
	case EventKeyboard::KeyCode::KEY_ESCAPE:
		showEditorMenu();
		break;
	case EventKeyboard::KeyCode::KEY_CTRL:
		_ctrlPressed = true;
		break;
	case EventKeyboard::KeyCode::KEY_SHIFT:
		_shiftPressed = true;
		break;
	case EventKeyboard::KeyCode::KEY_SPACE:
		_spacePressed = true;
		break;
	case EventKeyboard::KeyCode::KEY_LEFT_ARROW:
		if (_selectedObjectReal)
		{
			appliedStep = -step;
			auto pos = _selectedObjectReal->getPosition();
			pos.x += appliedStep;
			_selectedObjectReal->setPosition(pos);
			_selectedObjectReal->setStartPositionX(pos.x);
			movedX = true;
		}
		break;
	case EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
		if (_selectedObjectReal)
		{
			auto pos = _selectedObjectReal->getPosition();
			pos.x += step;
			_selectedObjectReal->setPosition(pos);
			_selectedObjectReal->setStartPositionX(pos.x);
			movedX = true;
			appliedStep = step;
		}
		break;
	case EventKeyboard::KeyCode::KEY_UP_ARROW:
		if (_selectedObjectReal)
		{
			auto pos = _selectedObjectReal->getPosition();
			pos.y += step;
			_selectedObjectReal->setPosition(pos);
			_selectedObjectReal->setStartPositionY(pos.y);
			movedY = true;
			appliedStep = step;
		}
		break;
	case EventKeyboard::KeyCode::KEY_DOWN_ARROW:
		if (_selectedObjectReal)
		{
			appliedStep = -step;
			auto pos = _selectedObjectReal->getPosition();
			pos.y += appliedStep;
			_selectedObjectReal->setPosition(pos);
			_selectedObjectReal->setStartPositionY(pos.y);
			movedY = true;
		}
		break;
	case EventKeyboard::KeyCode::KEY_KP_PLUS:
		if (_selectedObjectReal)
		{
			auto scale = _selectedObjectReal->getStartScale();
			scale.x += scaling;
			scale.y += scaling;
			_selectedObjectReal->setStartScale(scale);
		}
		break;
	case EventKeyboard::KeyCode::KEY_KP_MINUS:
		if (_selectedObjectReal)
		{
			auto scale = _selectedObjectReal->getStartScale();
			scale.x -= scaling;
			scale.y -= scaling;
			_selectedObjectReal->setStartScale(scale);
		}
		break;
	case EventKeyboard::KeyCode::KEY_DELETE:
		if (_selectedObjectReal)
			removeEditorObject(_selectedObjectReal);
		break;
	default:
		break;
	}

	if (_selectedObjectReal && (movedX || movedY))
	{
		const Vec2 current = _selectedObjectReal->getPosition();
		Vec2 oldPos = current;
		if (movedX)
			oldPos.x -= appliedStep;
		if (movedY)
			oldPos.y -= appliedStep;
		_objectPositionCache.erase(fmt::format("{}x{}", oldPos.x, oldPos.y));
		_objectPositionCache[fmt::format("{}x{}", current.x, current.y)] = _selectedObjectReal;
	}
}

void LevelEditorLayer::onKeyReleased(EventKeyboard::KeyCode keyCode, Event* event)
{
	if (_playtesting)
	{
		if (keyCode == EventKeyboard::KeyCode::KEY_SPACE || keyCode == EventKeyboard::KeyCode::KEY_UP_ARROW)
		{
			if (_player1 && _player1->m_bIsHolding)
				_player1->releaseButton();
			if (_playtestDual && _player2 && _player2->m_bIsHolding)
				_player2->releaseButton();
			_playtestJumpHeld = false;
		}
		if (_player1 && _player1->m_bIsPlatformer)
		{
			const bool left = keyCode == EventKeyboard::KeyCode::KEY_A || keyCode == EventKeyboard::KeyCode::KEY_LEFT_ARROW;
			const bool right = keyCode == EventKeyboard::KeyCode::KEY_D || keyCode == EventKeyboard::KeyCode::KEY_RIGHT_ARROW;
			if ((left && _player1->direction < 0.f) || (right && _player1->direction > 0.f))
				_player1->direction = 0.f;
		}
		return;
	}

	switch (keyCode)
	{
	case EventKeyboard::KeyCode::KEY_A:
	case EventKeyboard::KeyCode::KEY_D:
		m_camDelta.x = 0.f;
		break;
	case EventKeyboard::KeyCode::KEY_W:
	case EventKeyboard::KeyCode::KEY_S:
		m_camDelta.y = 0.f;
		break;
	case EventKeyboard::KeyCode::KEY_SHIFT:
		_shiftPressed = false;
		break;
	case EventKeyboard::KeyCode::KEY_CTRL:
		_ctrlPressed = false;
		break;
	case EventKeyboard::KeyCode::KEY_SPACE:
		_spacePressed = false;
		break;
	default:
		break;
	}
}

void LevelEditorLayer::onTouchMoved(Touch* touch, Event* event)
{
	if (_playtesting)
		return;

	const Vec2 screen = touch->getLocation();

	if (_leftPanning || _cameraPanning)
	{
		applyEditorPan(screen);
	return;
}

	if (_pendingTap)
	{
		if (screen.distance(_panTouchStart) < kPanDragThreshold)
			return;

		_pendingTap = false;

		if (_spacePressed)
		{
			_leftPanning = true;
			applyEditorPan(screen);
			return;
		}

		if (_toolMode == EditorToolMode::Build && _swipeEnabled)
		{
		_inSwapMode = true;
		}
		else if (_toolMode == EditorToolMode::Delete && _swipeEnabled)
		{
			// swipe-delete continues below
		}
		else if (_toolMode == EditorToolMode::Edit && _swipeEnabled)
		{
			// marquee select continues below
		}
		else if (_toolMode == EditorToolMode::Edit && _draggingSelection &&
				 (_freeMoveEnabled || _freeRotateEnabled))
		{
			// object drag / rotate continues below
		}
		else
		{
			_leftPanning = true;
			_draggingSelection = false;
			applyEditorPan(screen);
			return;
		}
	}

	if (screen.y < _toolbarHeight + _tabStripHeight)
		return;

	const bool useSnap = _snapEnabled && !_freeMoveEnabled;
	const Vec2 world = touchToWorld(screen, useSnap);

	if (_toolMode == EditorToolMode::Build && (_inSwapMode || _swipeEnabled))
	{
		auto* existing = findObject(world.x, world.y);
		if (existing && existing->getID() == _selectedObject)
			return;
		placeObjectAt(useSnap ? world : snapTouchToGrid(screen));
		return;
	}

	if (_toolMode == EditorToolMode::Delete && _swipeEnabled)
	{
		deleteObjectAt(world);
		return;
	}

	if (_toolMode == EditorToolMode::Edit && _swipeEnabled && _marqueeSelecting)
	{
		_marqueeEndWorld = touchToWorld(screen, false);
		refreshMarqueeVisual();
		return;
	}

	if (_toolMode != EditorToolMode::Edit || !_selectedObjectReal || !_draggingSelection)
		return;

	if (_freeRotateEnabled)
	{
		const Vec2 center = _selectedObjectReal->getPosition();
		const float angle = AX_RADIANS_TO_DEGREES(std::atan2(world.y - center.y, world.x - center.x));
		float delta = angle - _lastRotateAngle;
		while (delta > 180.f)
			delta -= 360.f;
		while (delta < -180.f)
			delta += 360.f;
		_selectedObjectReal->setRotation(_selectedObjectReal->getRotation() + delta);
		if (_snapEnabled)
		{
			const float snapped = std::round(_selectedObjectReal->getRotation() / 15.f) * 15.f;
			_selectedObjectReal->setRotation(snapped);
		}
		_lastRotateAngle = angle;
		return;
	}

	if (_freeMoveEnabled || (_swipeEnabled && !_marqueeSelecting))
	{
		const Vec2 oldPos = _selectedObjectReal->getPosition();
		Vec2 newPos = _freeMoveEnabled ? (world) : world;
		if (_freeMoveEnabled)
		{
			const Vec2 delta = touch->getLocation() - _dragTouchStart;
			newPos = _dragObjStart + delta;
			if (_snapEnabled)
			{
				newPos.x = std::floor(newPos.x / kGridSize) * kGridSize + 15.f;
				newPos.y = std::floor((newPos.y - kGroundOffset) / kGridSize) * kGridSize + 15.f + kGroundOffset;
			}
		}

		_selectedObjectReal->setPosition(newPos);
		_selectedObjectReal->setStartPosition(newPos);
		_objectPositionCache.erase(fmt::format("{}x{}", oldPos.x, oldPos.y));
		_objectPositionCache[fmt::format("{}x{}", newPos.x, newPos.y)] = _selectedObjectReal;
	}
}

bool LevelEditorLayer::onTouchBegan(Touch* touch, Event* event)
{
	if (_playtesting)
	{
		if (!_playtestDead && _player1 && !_player1->m_bIsHolding)
		_player1->pushButton();
		if (!_playtestDead && _playtestDual && _player2 && !_player2->m_bIsHolding)
			_player2->pushButton();
		_playtestJumpHeld = true;
		return true;
	}

	if (touch->getLocation().y < _toolbarHeight + _tabStripHeight)
		return false;

	_draggingSelection = false;
	_leftPanning = false;
	_pendingTap = true;
	_panTouchStart = touch->getLocation();
	_camAtPanStart = m_obCamPos;

	if (_spacePressed)
	{
		_leftPanning = true;
		_pendingTap = false;
	return true;
}

	if (_toolMode == EditorToolMode::Edit)
	{
		if (_swipeEnabled)
		{
			const Vec2 world = touchToWorld(touch->getLocation(), false);
			_marqueeStartWorld = world;
			_marqueeEndWorld = world;
			_marqueeSelecting = true;
			_draggingSelection = false;
			refreshMarqueeVisual();
			return true;
		}

		const bool useSnap = _snapEnabled && !_freeMoveEnabled;
		const Vec2 world = touchToWorld(touch->getLocation(), useSnap);
		GameObject* hit = nullptr;
		if (_snapEnabled && !_freeMoveEnabled)
			hit = findObject(world.x, world.y);
		if (!hit)
			hit = findObjectNear(world, kGridSize * 0.65f);
		if (hit)
		{
			setSelectedObject(hit);
			_draggingSelection = true;
			_dragTouchStart = touch->getLocation();
			_dragObjStart = hit->getPosition();
			_lastRotateAngle = AX_RADIANS_TO_DEGREES(std::atan2(world.y - hit->getPositionY(), world.x - hit->getPositionX()));
		}
	}
	return true;
}

void LevelEditorLayer::onTouchEnded(Touch* touch, Event* event)
{
	if (_playtesting)
	{
		if (_player1 && _player1->m_bIsHolding)
		_player1->releaseButton();
		if (_playtestDual && _player2 && _player2->m_bIsHolding)
			_player2->releaseButton();
		_playtestJumpHeld = false;
		return;
	}

	if (_marqueeSelecting)
	{
		const Vec2 screen = touch->getLocation();
		if (screen.distance(_panTouchStart) < kPanDragThreshold)
		{
			const bool useSnap = _snapEnabled && !_freeMoveEnabled;
			selectObjectAt(touchToWorld(screen, useSnap));
		}
		else
		{
			selectObjectsInMarquee(_marqueeStartWorld, _marqueeEndWorld);
		}
		_marqueeSelecting = false;
		refreshMarqueeVisual();
		_pendingTap = false;
		_leftPanning = false;
	_inSwapMode = false;
		_draggingSelection = false;
		return;
	}

	if (_pendingTap && !_leftPanning && !_cameraPanning)
		commitEditorTap(touch->getLocation());

	_pendingTap = false;
	_leftPanning = false;
	_inSwapMode = false;
	_draggingSelection = false;
}
