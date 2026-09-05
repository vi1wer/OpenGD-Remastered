/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "EditorPropertyLayer.h"

#include "ButtonSprite.h"
#include "GameObject.h"
#include "GameToolbox/getTextureString.h"
#include "LevelEditorLayer.h"
#include "MenuItemSpriteExtra.h"

#include "2d/Label.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "ui/UIScale9Sprite.h"

#include <cmath>
#include <fmt/format.h>

USING_NS_AX;

EditorPropertyLayer* EditorPropertyLayer::create(LevelEditorLayer* editor, EditorPropertyKind kind, GameObject* obj)
{
	auto* ret = new (std::nothrow) EditorPropertyLayer();
	if (ret && ret->init(editor, kind, obj))
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
	return nullptr;
}

bool EditorPropertyLayer::init(LevelEditorLayer* editor, EditorPropertyKind kind, GameObject* obj)
{
	if (!PopupLayer::init())
		return false;

	_editor = editor;
	_target = obj;
	_kind = kind;

	const auto winSize = Director::getInstance()->getWinSize();
	auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
	if (!bg)
		return false;
	bg->setContentSize({320.f, 200.f});
	bg->setPosition(winSize * 0.5f);
	_mainLayer->addChild(bg);

	const char* title = "Edit Object";
	switch (_kind)
	{
	case EditorPropertyKind::Group:
		title = "Edit Group";
		break;
	case EditorPropertyKind::Special:
		title = "Edit Special";
		break;
	case EditorPropertyKind::Color:
		title = "Edit Color";
		break;
	default:
		break;
	}

	if (auto* lbl = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), title))
	{
		lbl->setScale(0.55f);
		lbl->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 70.f});
		_mainLayer->addChild(lbl);
	}

	auto* menu = Menu::create();
	menu->setPosition({0.f, 0.f});
	_mainLayer->addChild(menu);

	auto addBtn = [&](const char* label, float y, std::function<void()> action) {
		auto* spr = ButtonSprite::create(label, 120, 0, 0.45f);
		auto* btn = MenuItemSpriteExtra::create(spr, [action](Node*) {
			if (action)
				action();
		});
		btn->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + y});
		menu->addChild(btn);
	};

	if (!_target)
	{
		addBtn("No object selected", 0.f, [this] { close(); });
		addBtn("Close", -40.f, [this] { close(); });
		return true;
	}

	auto addArrowRow = [&](const char* label, float y, std::function<void(int)> onChange, int value) {
		if (auto* rowLbl = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"),
				fmt::format("{}: {}", label, value)))
		{
			rowLbl->setScale(0.4f);
			rowLbl->setPosition({winSize.width * 0.5f - 40.f, winSize.height * 0.5f + y});
			_mainLayer->addChild(rowLbl);
		}
		auto makeArrow = [&](const char* frame, float x, int delta) {
			Node* spr = nullptr;
			if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
				spr = Sprite::createWithSpriteFrameName(frame);
			if (!spr)
				return;
			spr->setScale(0.55f);
			auto* btn = MenuItemSpriteExtra::create(spr, [onChange, delta](Node*) { onChange(delta); });
			btn->setPosition({x, winSize.height * 0.5f + y});
			menu->addChild(btn);
		};
		makeArrow("edit_leftBtn2_001.png", winSize.width * 0.5f + 50.f, -1);
		makeArrow("edit_rightBtn2_001.png", winSize.width * 0.5f + 90.f, 1);
	};

	switch (_kind)
	{
	case EditorPropertyKind::Object:
		addArrowRow("X", 20.f, [this](int d) {
			if (!_target)
				return;
			auto p = _target->getPosition();
			p.x += d * 30.f;
			_target->setPosition(p);
			_target->setStartPosition(p);
		}, static_cast<int>(_target->getPositionX()));
		addArrowRow("Y", -10.f, [this](int d) {
			if (!_target)
				return;
			auto p = _target->getPosition();
			p.y += d * 30.f;
			_target->setPosition(p);
			_target->setStartPosition(p);
		}, static_cast<int>(_target->getPositionY()));
		addArrowRow("Rot", -40.f, [this](int d) {
			if (!_target)
				return;
			_target->setRotation(_target->getRotation() + d * 15.f);
		}, static_cast<int>(_target->getRotation()));
		addBtn("Flip X", -70.f, [this] {
			if (!_target)
				return;
			_target->setScaleX(-_target->getScaleX());
			_target->setStartScaleX(_target->getScaleX());
		});
		addBtn("Flip Y", -100.f, [this] {
			if (!_target)
				return;
			_target->setScaleY(-_target->getScaleY());
			_target->setStartScaleY(_target->getScaleY());
		});
		break;
	case EditorPropertyKind::Group:
		addArrowRow("Group", 10.f, [this](int d) {
			if (!_target)
				return;
			if (d > 0)
				_target->_groups.push_back(_target->_groups.empty() ? 1 : _target->_groups.back() + 1);
			else if (!_target->_groups.empty())
				_target->_groups.pop_back();
		}, _target->_groups.empty() ? 0 : _target->_groups.back());
		break;
	case EditorPropertyKind::Special:
		if (auto* info = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"),
				fmt::format("Object ID: {}", _target->getID())))
		{
			info->setScale(0.4f);
			info->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 10.f});
			_mainLayer->addChild(info);
		}
		if (auto* hint = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Trigger params: coming soon"))
		{
			hint->setScale(0.35f);
			hint->setPosition({winSize.width * 0.5f, winSize.height * 0.5f - 20.f});
			_mainLayer->addChild(hint);
		}
		break;
	case EditorPropertyKind::Color:
		addArrowRow("Main Ch", 10.f, [this](int d) {
			if (!_target)
				return;
			_target->_mainColorChannel = std::max(-1, _target->_mainColorChannel + d);
		}, _target->_mainColorChannel);
		addArrowRow("Detail Ch", -20.f, [this](int d) {
			if (!_target)
				return;
			_target->_secColorChannel = std::max(-1, _target->_secColorChannel + d);
		}, _target->_secColorChannel);
		break;
	}

	addBtn("Close", -130.f, [this] { close(); });

	auto* closeMenu = Menu::create();
	auto* closeSpr = Sprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
	if (closeSpr)
	{
		auto* closeBtn = MenuItemSpriteExtra::create(closeSpr, [this](Node*) { close(); });
		closeMenu->addChild(closeBtn);
		closeBtn->setPosition({winSize.width * 0.5f + 145.f, winSize.height * 0.5f + 85.f});
		_mainLayer->addChild(closeMenu);
	}

	return true;
}
