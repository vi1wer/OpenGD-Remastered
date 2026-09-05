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

#include "GameObject.h"
#include "2d/ActionInstant.h"
#include "2d/ActionInterval.h"
#include "2d/Animation.h"
#include "2d/ParticleSystemQuad.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "renderer/TextureCache.h"
#include "BaseGameLayer.h"
#include "EffectGameObject.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/log.h"
#include "GameToolbox/nodes.h"
#include "PlayLayer.h"
#include "PlayerObject.h"
#include "platform/FileUtils.h"
#include <fmt/format.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_set>

USING_NS_AX;

ax::Color3B _tempColor;

namespace
{
std::unordered_map<int, std::string> s_blockFrames;
std::unordered_set<int> s_triggerIds;
bool s_blockDataLoaded = false;

void loadBlockDatabase()
{
	if (s_blockDataLoaded)
		return;
	s_blockDataLoaded = true;

	auto* fu = FileUtils::getInstance();

	auto looksLikeTexture = [](std::string_view tex) -> bool {
		if (tex.size() > 4 && tex.compare(tex.size() - 4, 4, ".png") == 0)
			tex.remove_suffix(4);
		if (tex.rfind("edit_", 0) == 0)
			return true;
		return tex.find('_') != std::string_view::npos;
	};

	// Full GD 2.2 ObjectToolbox dump (id -> texture without .png).
	try
	{
		auto blocksJson = nlohmann::json::parse(fu->getStringFromFile("Custom/blocks.json"));
		for (auto& [key, val] : blocksJson.items())
		{
			int id = GameToolbox::stoi(key);
			std::string tex = val.is_string() ? val.get<std::string>() : val.dump();
			if (tex.size() > 4 && tex.compare(tex.size() - 4, 4, ".png") == 0)
				tex.resize(tex.size() - 4);
			if (!looksLikeTexture(tex))
				continue;
			s_blockFrames[id] = tex;

			if (id != 1586 && (tex.rfind("edit_e", 0) == 0 || tex.rfind("edit_ee", 0) == 0))
				s_triggerIds.insert(id);
		}
		GameToolbox::log("Loaded {} block frames from Custom/blocks.json", s_blockFrames.size());
	}
	catch (const std::exception& e)
	{
		GameToolbox::log("Failed to load Custom/blocks.json: {}", e.what());
	}

	// Supplemental entries from object.json (children / metadata textures).
	try
	{
		auto json = nlohmann::json::parse(fu->getStringFromFile("Custom/object.json"));
		for (auto& [key, val] : json.items())
		{
			if (!val.contains("texture_name") || !val["texture_name"].is_string())
				continue;

			int id = GameToolbox::stoi(key);
			if (s_blockFrames.contains(id) || GameObject::_pBlocks.contains(id))
				continue;

			std::string tex = val["texture_name"].get<std::string>();
			if (tex.size() > 4 && tex.compare(tex.size() - 4, 4, ".png") == 0)
				tex.resize(tex.size() - 4);
			if (!looksLikeTexture(tex))
				continue;

			s_blockFrames[id] = std::move(tex);
		}
	}
	catch (const std::exception& e)
	{
		GameToolbox::log("Failed to load block frames from Custom/object.json: {}", e.what());
	}

	for (int id : GameObject::_pTriggers)
		s_triggerIds.insert(id);
}
} // namespace

std::string_view GameObject::getBlockFrame(int objectID)
{
	loadBlockDatabase();

	// Built-in LongData map is authoritative for classic IDs.
	if (_pBlocks.contains(objectID))
	{
		std::string_view frame = _pBlocks.at(objectID);
		if (frame.size() > 4 && frame.compare(frame.size() - 4, 4, ".png") == 0)
			frame.remove_suffix(4);
		return frame;
	}

	auto it = s_blockFrames.find(objectID);
	if (it != s_blockFrames.end())
		return it->second;

	return {};
}

bool GameObject::isTriggerID(int objectID)
{
	if (objectID == 1586)
		return false;
	loadBlockDatabase();
	if (s_triggerIds.contains(objectID))
		return true;
	return std::find(_pTriggers.begin(), _pTriggers.end(), objectID) != _pTriggers.end();
}

bool GameObject::isSlopeFrame(std::string_view frame)
{
	return frame.find("slope") != std::string_view::npos;
}

bool GameObject::isPassableDecorationFrame(std::string_view frame)
{
	if (frame.empty())
		return false;
	if (isPixelArtFrame(frame))
		return true;
	static constexpr const char* kDecoPrefixes[] = {
		"d_", "chain_", "rod_", "smallOutline_", "block008_", "d_link", "d_bar", "d_ball", "d_wheel",
		"d_cloud", "d_grass", "d_03_", "plank_01_small", "invis_plank", "d_art", "d_round", "persp_outline_"};
	for (const char* prefix : kDecoPrefixes)
	{
		const size_t len = std::strlen(prefix);
		if (frame.size() >= len && frame.compare(0, len, prefix) == 0)
			return true;
	}
	return false;
}

bool GameObject::isPixelArtFrame(std::string_view frame)
{
	if (frame.empty())
		return false;
	// GD pixel blocks / pixel art are visual-only (no collision).
	return frame.rfind("pixelart_", 0) == 0 || frame.rfind("pixelb_", 0) == 0 || frame.rfind("pixel_", 0) == 0 ||
		   frame.rfind("d_pixelArt", 0) == 0 || frame.rfind("d_pixelart", 0) == 0 ||
		   frame.find("pixelart_") != std::string_view::npos || frame.find("pixelb_") != std::string_view::npos;
}

bool GameObject::isCollidableOutlineFrame(std::string_view frame)
{
	if (frame.empty())
		return false;
	// Decorative outlines — no collision.
	if (frame.find("smallOutline") != std::string_view::npos)
		return false;
	if (frame.find("persp_outline") != std::string_view::npos)
		return false;
	// Solid outline lines used as platforms / edges in GD.
	return frame.find("blockOutline") != std::string_view::npos ||
		   frame.find("invisibleOutline") != std::string_view::npos;
}

bool GameObject::isCoin() const
{
	return _pObjectType == kGameObjectTypeSecretCoin || _pObjectType == kGameObjectTypeUserCoin
		|| m_pId == 142 || m_pId == 1329;
}

void GameObject::startCoinAnimation()
{
	const char* prefix = (getID() == 1329) ? "secretCoin_2_01" : "secretCoin_01";
	auto* cache = SpriteFrameCache::getInstance();
	Vector<SpriteFrame*> frames;
	for (int i = 1; i <= 4; i++)
	{
		if (auto* frame = cache->getSpriteFrameByName(fmt::format("{}_{:03}.png", prefix, i)))
			frames.pushBack(frame);
	}
	if (frames.empty())
		return;

	stopActionByTag(80);
	auto* animate = RepeatForever::create(Animate::create(Animation::createWithSpriteFrames(frames, 0.1f)));
	animate->setTag(80);
	runAction(animate);
	_hasIdleAnim = true;
}

bool GameObject::wantsCollisionBounds() const
{
	if (_isNoTouch)
		return false;
	if (isPixelArtFrame(getBlockFrame(m_pId)))
		return false;
	if (_pObjectType == kGameObjectTypeDecoration || _pObjectType == kGameObjectTypeSpecial)
		return false;
	if (isPassableDecorationFrame(getBlockFrame(m_pId)))
		return false;
	if (_isTrigger)
	{
		auto* trigger = dynamic_cast<const EffectGameObject*>(this);
		return trigger && trigger->_touchTriggered;
	}
	return true;
}

void GameObject::refreshCollisionBounds()
{
	if (!_hasCollisionHitbox)
		return;

	const Hitbox hb = _collisionHitbox;
	Mat4 tr;
	Rect rec = {hb.x, hb.y, hb.w, hb.h};
	tr.rotate(getRotationQuat());
	tr.scale(getScaleX() * (isFlippedX() ? -1.f : 1.f), getScaleY() * (isFlippedY() ? -1.f : 1.f), 1);
	rec = RectApplyTransform(rec, tr);
	setOuterBounds(Rect(getPosition() + Vec2(rec.origin.x, rec.origin.y), {rec.size.width, rec.size.height}));
}

Hitbox GameObject::resolveObjectHitbox(int objectID, GameObject* obj)
{
	Hitbox hb = resolveHitbox(objectID);
	if ((hb.w <= 0.f || hb.h <= 0.f) && obj)
	{
		std::string_view createdFrame = getBlockFrame(objectID);
		// Pixel art never gets a synthetic hitbox.
		if (isPixelArtFrame(createdFrame))
			return {0, 0, 0, 0};

		// Collidable outline lines without a table entry: full 30x30 block.
		if (isCollidableOutlineFrame(createdFrame))
			return {30.f, 30.f, -15.f, -15.f};

		if (hb.w <= 0.f || hb.h <= 0.f)
		{
			switch (obj->getGameObjectType())
			{
			case kGameObjectTypeHazard:
				hb = {12.f, 6.f, -3.f, -6.f};
				break;
			case kGameObjectTypeCubePortal:
			case kGameObjectTypeShipPortal:
			case kGameObjectTypeBallPortal:
			case kGameObjectTypeUfoPortal:
			case kGameObjectTypeWavePortal:
			case kGameObjectTypeRobotPortal:
			case kGameObjectTypeSpiderPortal:
			case kGameObjectTypeSwingPortal:
			case kGameObjectTypeDualPortal:
			case kGameObjectTypeSoloPortal:
				hb = {86.f, 34.f, -17.f, -43.f};
				break;
			default:
				break;
			}
		}
	}
	return hb;
}

void GameObject::applyLoadedHitbox(Hitbox hb)
{
	if (!wantsCollisionBounds())
	{
		_radius = 0.f;
		_hasCollisionHitbox = false;
		setOuterBounds(Rect());
		return;
	}

	if (_pHitboxRadius.contains(getID()) && getGameObjectType() == kGameObjectTypeHazard)
		_radius = _pHitboxRadius.at(getID());

	if (hb.w <= 0.f || hb.h <= 0.f)
	{
		_hasCollisionHitbox = false;
		return;
	}

	_collisionHitbox = hb;
	_hasCollisionHitbox = true;
	refreshCollisionBounds();
}

void GameObject::startIdleAnimation()
{
	if (_isTrigger)
		return;

	if (isCoin())
	{
		startCoinAnimation();
		return;
	}

	stopActionByTag(80);
	stopActionByTag(81);
	for (auto* child : _childSprites)
	{
		if (child)
			child->stopActionByTag(80);
	}

	bool started = false;
	const int objectID = getID();
	std::string frame(getBlockFrame(objectID));
	if (frame.size() > 4 && frame.compare(frame.size() - 4, 4, ".png") == 0)
		frame.resize(frame.size() - 4);

	auto animateSprite = [&](Sprite* target, const std::string& prefix) -> bool {
		if (!target || prefix.empty())
			return false;
		auto* cache = SpriteFrameCache::getInstance();
		Vector<SpriteFrame*> frames;
		for (int i = 1; i <= 24; i++)
		{
			auto* sf = cache->getSpriteFrameByName(fmt::format("{}_{:03}.png", prefix, i));
			if (!sf)
				break;
			frames.pushBack(sf);
		}
		if (frames.size() < 2)
			return false;
		auto* animate = RepeatForever::create(Animate::create(Animation::createWithSpriteFrames(frames, 0.06f)));
		animate->setTag(80);
		target->runAction(animate);
		return true;
	};

	const bool canFrameAnim =
		!_animateOnTrigger && _pObjectType != kGameObjectTypeSolid && _pObjectType != kGameObjectTypeSlope &&
		_pObjectType != kGameObjectTypeSpecial;
	if (canFrameAnim)
	{
		const auto us = frame.rfind('_');
		if (us != std::string::npos && us + 1 < frame.size())
		{
			const std::string numPart = frame.substr(us + 1);
			bool numeric = numPart.size() == 3;
			for (char c : numPart)
			{
				if (!std::isdigit(static_cast<unsigned char>(c)))
				{
					numeric = false;
					break;
				}
			}
			if (numeric)
			{
				const std::string prefix = frame.substr(0, us);
				if (animateSprite(this, prefix))
					started = true;
				const std::string colorPrefix = prefix + "_color";
				for (auto* child : _childSprites)
				{
					if (animateSprite(child, colorPrefix))
						started = true;
				}
			}
		}
	}

	bool spin = false;
	if (_pHitboxRadius.contains(objectID) && objectID != 1582 && objectID != 1583)
		spin = true;
	else if (frame.find("sawblade") != std::string::npos || frame.find("blade_") != std::string::npos ||
			 frame.find("cogwheel") != std::string::npos || frame.find("spikewheel") != std::string::npos)
		spin = true;

	if (spin)
	{
		const float radius = _radius > 0.f ? _radius : 20.f;
		float duration = 0.35f + radius * 0.012f;
		if (duration < 0.4f)
			duration = 0.4f;
		if (duration > 1.15f)
			duration = 1.15f;
		const float dir = (objectID % 2 == 0) ? 360.f : -360.f;
		auto* rotate = RepeatForever::create(RotateBy::create(duration, dir));
		rotate->setTag(81);
		runAction(rotate);
		started = true;
	}

	_hasIdleAnim = started;
}

Hitbox GameObject::resolveHitbox(int objectID)
{
	if (_pHitboxes.contains(objectID))
		return _pHitboxes.at(objectID);

	if (objectID == 142 || objectID == 1329)
		return {40.f, 40.f, -20.f, -20.f};

	std::string_view frame = getBlockFrame(objectID);
	if (isPassableDecorationFrame(frame) || isPixelArtFrame(frame))
		return {0, 0, 0, 0};

	const bool decoSpike = frame.find("spikeart") != std::string_view::npos ||
						   frame.find("spikewheel") != std::string_view::npos;
	if (!decoSpike && frame.find("spike") != std::string_view::npos)
		return {12.f, 6.f, -3.f, -6.f};

	if (frame.find("lava") != std::string_view::npos)
		return {12.f, 30.f, -15.f, -6.f};

	if (isCollidableOutlineFrame(frame))
		return {30.f, 30.f, -15.f, -15.f};

	if (!isSlopeFrame(frame))
		return {0, 0, 0, 0};

	// Missing slope hitboxes: 45° ≈ 30x30, 26° ≈ 60x30.
	const bool wide =
		(frame.find("square") == std::string_view::npos) &&
		(frame.find("slope_02") != std::string_view::npos || frame.find("slope_04") != std::string_view::npos ||
		 frame.find("slope_06") != std::string_view::npos || frame.find("slope_02b") != std::string_view::npos ||
		 frame.find("slope_02c") != std::string_view::npos || frame.find("slope_02d") != std::string_view::npos);

	if (wide)
		return {30.f, 60.f, -30.f, -15.f};
	return {30.f, 30.f, -15.f, -15.f};
}

void GameObject::determineSlopeDirection()
{
	if (getGameObjectType() != kGameObjectTypeSlope && !isSlopeFrame(getBlockFrame(getID())))
		return;

	const bool flipX = getScaleX() < 0.f || isFlippedX();
	const bool flipY = getScaleY() < 0.f || isFlippedY();

	// Match camila314/pathfinder + RobTop flip→orientation mapping.
	int orientation = static_cast<int>(std::lround(getRotation() / 90.f));
	if (flipX && flipY)
		orientation += 2;
	else if (flipX)
		orientation += 1;
	else if (flipY)
		orientation += 3;
	orientation %= 4;
	if (orientation < 0)
		orientation += 4;

	_slopeDirection = orientation;
	// 0: floor /   1: floor \   2: ceiling \   3: ceiling /
	_slopeFloorTop = orientation >= 2;
	_slopeUphill = (orientation == 0 || orientation == 3);

	std::string_view frame = getBlockFrame(getID());
	_slopeIsHazard = frame.find("pit_") != std::string_view::npos;
}

float GameObject::getSlopeAngle() const
{
	const Rect r = getOuterBounds();
	if (r.size.width <= 0.f)
		return 0.f;
	return std::atan(r.size.height / r.size.width);
}

double GameObject::slopeYPos(float playerX) const
{
	// From camila314/gdp GameObject_slopeYPos (2.2).
	const Rect objectRect = getOuterBounds();
	const float slopeLeft = objectRect.getMinX();
	const float slopeRight = objectRect.getMaxX();
	const float slopeBottom = objectRect.getMinY();
	const float slopeTop = objectRect.getMaxY();
	const float slopeRatio = objectRect.size.height / objectRect.size.width;

	double result;
	if (slopeLeft < playerX)
	{
		const float distanceFromRight = playerX - slopeRight;
		if (_slopeUphill)
			result = slopeTop + distanceFromRight * slopeRatio;
		else
			result = slopeBottom - distanceFromRight * slopeRatio;
	}
	else
	{
		const float distanceFromLeft = slopeLeft - playerX;
		if (!_slopeUphill)
			result = slopeTop + distanceFromLeft * slopeRatio;
		else
			result = slopeBottom - distanceFromLeft * slopeRatio;
	}

	if (_slopeIsHazard)
		result += (_slopeFloorTop ? -4.0 : 4.0);

	return result;
}

bool GameObject::init(std::string_view frame, std::string_view glowFrame)
{
	if (frame.find("player") != std::string::npos)
		return true;

	if (frame.empty())
	{
		// GameToolbox::log("false cuz frame is empty");
		return false;
	}

	if (!Sprite::initWithSpriteFrameName(fmt::format("{}.png", frame)))
	{
		// GameToolbox::log("false cuz sprite failed");
		return false;
	}

	_pOuterBounds = Rect();
	_pInnerBounds = Rect();

	_glowSprite = nullptr;

	if (!glowFrame.empty())
	{
		_glowSprite = Sprite::createWithSpriteFrameName(fmt::format("{}.png", glowFrame));
		if (_glowSprite)
		{
			_hasGlow = true;
			_glowSprite->setBlendFunc(GameToolbox::getBlending());
			_glowSprite->setStretchEnabled(false);
			_glowSprite->setLocalZOrder(-1);
			_glowSprite->retain();
		}
	}

	if (auto* tex = getTexture())
		_texturePath = tex->getPath();
	else
		_texturePath.clear();

	setCascadeColorEnabled(false);
	setCascadeOpacityEnabled(false);

	return true;
}

void GameObject::addCustomSprites(nlohmann::json j, ax::Sprite* parent)
{
	for (nlohmann::json jsonObj : j)
	{
		ax::Sprite* s = ax::Sprite::createWithSpriteFrameName(jsonObj["texture_name"].get<std::string>());
		if (!s)
			continue;
		parent->addChild(s);
		s->setStretchEnabled(false);
		s->setAnchorPoint({static_cast<float>(jsonObj["anchor_x"]), static_cast<float>(jsonObj["anchor_y"])});
		s->setFlippedX(static_cast<bool>(jsonObj["flip_x"]));
		s->setFlippedY(static_cast<bool>(jsonObj["flip_y"]));
		s->setPosition({static_cast<float>(jsonObj["x"]), static_cast<float>(jsonObj["y"])});
		s->setLocalZOrder(static_cast<int>(jsonObj["z"]));
		s->setRotation(static_cast<float>(jsonObj["rot"]));
		s->setScaleX(static_cast<float>(jsonObj["scale_x"]));
		s->setScaleY(static_cast<float>(jsonObj["scale_y"]));
		if (jsonObj.contains("content_x"))
			s->setContentSize({static_cast<float>(jsonObj["content_x"]), static_cast<float>(jsonObj["content_y"])});
		s->setCascadeColorEnabled(false);
		s->setCascadeOpacityEnabled(false);
		if (jsonObj.contains("color_channel"))
		{
			if (jsonObj["color_channel"] == "base")
				_childSpritesChannel.push_back(0);
			else if (jsonObj["color_channel"] == "detail")
				_childSpritesChannel.push_back(1);
			else if (jsonObj["color_channel"] == "black")
				_childSpritesChannel.push_back(2);
		}
		else
			_childSpritesChannel.push_back(404);

		_childSprites.push_back(s);

		if (jsonObj.contains("children"))
			addCustomSprites(jsonObj["children"], s);
	}
}

void GameObject::customSetup()
{
	static nlohmann::json childJson;

	if (childJson.empty())
	{
		childJson = nlohmann::json::parse(FileUtils::getInstance()->getStringFromFile("Custom/object.json"));
	}

	const std::string idKey = std::to_string(getID());
	nlohmann::json objJson = nlohmann::json::object();
	if (childJson.is_object() && childJson.contains(idKey) && childJson[idKey].is_object())
		objJson = childJson[idKey];

	if (objJson.contains("default_z_order"))
		setGlobalZOrder((int)objJson["default_z_order"]);
	if (objJson.contains("default_z_layer"))
		_zLayer = (int)objJson["default_z_layer"];
	if (objJson.contains("default_primary_channel"))
		_mainColorChannel = (int)objJson["default_primary_channel"];
	if (objJson.contains("default_secondary_channel"))
		_secColorChannel = (int)objJson["default_secondary_channel"];

	if (objJson.contains("color_channel"))
	{
		if (objJson["color_channel"] == "base")
			_childSpritesChannel.push_back(0);
		else if (objJson["color_channel"] == "detail")
			_childSpritesChannel.push_back(1);
		else if (objJson["color_channel"] == "black")
			_childSpritesChannel.push_back(2);
		else
			_childSpritesChannel.push_back(404);
	}
	else
		_childSpritesChannel.push_back(404);

	if (objJson.contains("object_type"))
		setGameObjectType((GameObjectType)objJson["object_type"]);
	else if (std::find(_pSolids.begin(), _pSolids.end(), getID()) != _pSolids.end())
		setGameObjectType(kGameObjectTypeSolid);
	else
		setGameObjectType(kGameObjectTypeDecoration);

	// Slope textures must collide even if object.json still marks them as decorations.
	if (isSlopeFrame(getBlockFrame(getID())))
		setGameObjectType(kGameObjectTypeSlope);

	{
		std::string_view frame = getBlockFrame(getID());
		// Pixel objects are decoration-only (no hitboxes), regardless of object.json.
		if (isPixelArtFrame(frame))
			setGameObjectType(kGameObjectTypeDecoration);
		// Outline lines that collide in GD (blockOutline / invisibleOutline / thick / outer).
		else if (isCollidableOutlineFrame(frame))
			setGameObjectType(kGameObjectTypeSolid);
	}

	// Spikes / pits must kill even if object.json marks them as solids.
	{
		std::string_view frame = getBlockFrame(getID());
		if (!isPassableDecorationFrame(frame) && !isPixelArtFrame(frame))
		{
			const bool decoSpike = frame.find("spikeart") != std::string_view::npos ||
								   frame.find("spikewheel") != std::string_view::npos;
			if (!decoSpike && (frame.find("spike") != std::string_view::npos ||
							   frame.find("lava") != std::string_view::npos ||
							   (frame.find("pit_") != std::string_view::npos && frame.find("slope") == std::string_view::npos)))
				setGameObjectType(kGameObjectTypeHazard);
		}
	}

	// Always apply known interactive IDs (object.json guesses can be wrong).
	switch (getID())
	{
	case 35:
		setGameObjectType(kGameObjectTypeYellowJumpPad);
		break;
	case 67:
		setGameObjectType(kGameObjectTypeGravityPad);
		break;
	case 140:
		setGameObjectType(kGameObjectTypePinkJumpPad);
		break;
	case 1332:
		setGameObjectType(kGameObjectTypeRedJumpPad);
		break;
	case 3005: // spider pad (2.2)
		setGameObjectType(kGameObjectTypeSpiderPad);
		break;
	case 36:
		setGameObjectType(kGameObjectTypeYellowJumpRing);
		break;
	case 84:
		setGameObjectType(kGameObjectTypeGravityRing);
		break;
	case 141:
		setGameObjectType(kGameObjectTypePinkJumpRing);
		break;
	case 1022:
		setGameObjectType(kGameObjectTypeGreenRing);
		break;
	case 1330:
		setGameObjectType(kGameObjectTypeDropRing);
		break;
	case 1333:
		setGameObjectType(kGameObjectTypeRedJumpRing);
		break;
	case 1594:
		setGameObjectType(kGameObjectTypeCustomRing);
		break;
	case 1704:
		setGameObjectType(kGameObjectTypeDashRing);
		break;
	case 1751:
		setGameObjectType(kGameObjectTypeGravityDashRing);
		break;
	case 3004: // spider orb
		setGameObjectType(kGameObjectTypeSpiderRing);
		break;
	case 3027: // teleport orb
		setGameObjectType(kGameObjectTypeCustomRing);
		break;
	case 10:
		setGameObjectType(kGameObjectTypeNormalGravityPortal);
		break;
	case 11:
		setGameObjectType(kGameObjectTypeInverseGravityPortal);
		break;
	case 12:
		setGameObjectType(kGameObjectTypeCubePortal);
		break;
	case 13:
		setGameObjectType(kGameObjectTypeShipPortal);
		break;
	case 45:
		setGameObjectType(kGameObjectTypeInverseMirrorPortal);
		break;
	case 46:
		setGameObjectType(kGameObjectTypeNormalMirrorPortal);
		break;
	case 47:
		setGameObjectType(kGameObjectTypeBallPortal);
		break;
	case 99:
		setGameObjectType(kGameObjectTypeRegularSizePortal);
		break;
	case 101:
		setGameObjectType(kGameObjectTypeMiniSizePortal);
		break;
	case 111:
		setGameObjectType(kGameObjectTypeUfoPortal);
		break;
	case 286:
		setGameObjectType(kGameObjectTypeDualPortal);
		break;
	case 287:
		setGameObjectType(kGameObjectTypeSoloPortal);
		break;
	case 660:
		setGameObjectType(kGameObjectTypeWavePortal);
		break;
	case 745:
		setGameObjectType(kGameObjectTypeRobotPortal);
		break;
	case 747:
	case 749:
		setGameObjectType(kGameObjectTypeTeleportPortal);
		break;
	case 1331:
		setGameObjectType(kGameObjectTypeSpiderPortal);
		break;
	case 1933:
		setGameObjectType(kGameObjectTypeSwingPortal);
		break;
	case 200:
	case 201:
	case 202:
	case 203:
	case 1334:
		setGameObjectType(kGameObjectTypeModifier);
		break;
	case 142:
		setGameObjectType(kGameObjectTypeSecretCoin);
		break;
	case 1329:
		setGameObjectType(kGameObjectTypeUserCoin);
		break;
	default:
		break;
	}

	// object.json type 7 (Details tab) must stay non-collidable unless an ID above overrides it.
	if (objJson.contains("object_type") && objJson["object_type"].get<int>() == 7)
		setGameObjectType(kGameObjectTypeDecoration);

	if (isPassableDecorationFrame(getBlockFrame(getID())))
		setGameObjectType(kGameObjectTypeDecoration);

	if (isCoin())
		startCoinAnimation();

	std::string_view hideFrame = getBlockFrame(getID());
	if (_isTrigger || hideFrame.rfind("edit_e", 0) == 0 || hideFrame.rfind("edit_ee", 0) == 0)
	{
		setVisible(false);
		setOpacity(0);
	}

	if (objJson.contains("children"))
		addCustomSprites(objJson["children"], this);

	for (auto obj : _childSprites)
		obj->setAdditionalTransform(&_parentMatrix);

	_isOnlyDetail = true;

	for (size_t type : _childSpritesChannel)
	{
		if (type == 0)
			_isOnlyDetail = false;
	}

	_primaryInvisible = false;

	// robtop made 3d parts have lines by default but then decided to make them invisible instead of removing them for
	// whatever reason
	if (getID() >= 515 && getID() <= 640)
	{
		_primaryInvisible = true;
		setOpacity(0);
	}

	switch (getID())
	{
	case 10:
		createAndAddParticle("portalEffect01.plist", 3);
		break;
	case 11:
		createAndAddParticle("portalEffect02.plist", 3);
		break;
	case 12:
		createAndAddParticle("portalEffect03.plist", 3);
		break;
	case 13:
		createAndAddParticle("portalEffect04.plist", 3);
		break;
	case 35: // yellow pad
		createAndAddParticle("bumpEffect.plist", 0);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(1.f, 1.f, 0.f, 1.f));
		break;
	case 67: // blue pad
		createAndAddParticle("bumpEffect.plist", 0);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(0.f, 1.f, 1.f, 1.f));
		break;
	case 140: // pink pad
		createAndAddParticle("bumpEffect.plist", 0);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(1.f, 0.f, 1.f, 1.f));
		break;
	case 1332: // red pad
		createAndAddParticle("bumpEffect.plist", 0);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(1.f, 0.15f, 0.15f, 1.f));
		break;
	case 36: // yellow orb
		createAndAddParticle("ringEffect.plist", 3);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(1.f, 1.f, 0.f, 1.f));
		break;
	case 84: // blue orb
		createAndAddParticle("ringEffect.plist", 3);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(0.f, 1.f, 1.f, 1.f));
		break;
	case 141: // pink orb
		createAndAddParticle("ringEffect.plist", 3);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(1.f, 0.f, 1.f, 1.f));
		break;
	case 1022: // green orb
		createAndAddParticle("ringEffect.plist", 3);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(0.f, 1.f, 0.2f, 1.f));
		break;
	case 1330: // black / drop orb
		createAndAddParticle("ringEffect.plist", 3);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(0.35f, 0.2f, 0.55f, 1.f));
		break;
	case 1333: // red orb
		createAndAddParticle("ringEffect.plist", 3);
		_animateOnTrigger = true;
		if (_particle)
			_particle->setStartColor(ax::Color4F(1.f, 0.15f, 0.15f, 1.f));
		break;
	case 1582:
	case 1583:
		createAndAddParticle("fireballEffect.plist", 3);
		break;
	case 1704:
	case 1751:
		createAndAddParticle("ringEffect.plist", 3);
		break;
	case 366:
	case 367:
	case 1717:
	case 1718:
	case 1723:
	case 1724:
		_primaryInvisible = true;
		break;
	case 1586:
		_primaryInvisible = true;
		setOpacity(0);
		break;
	}
}

void GameObject::applyParticleTexture(ParticleSystemQuad* particle)
{
	if (!particle)
		return;
	if (auto* tex = Director::getInstance()->getTextureCache()->addImage("square.png"))
		particle->setTexture(tex);
}

void GameObject::createAndAddParticle(const char* path, int zOrder)
{
	if (_particle)
	{
		_particle->cleanup();
		AX_SAFE_RELEASE_NULL(_particle);
	}

	_particle = ParticleSystemQuad::create(path);
	if (!_particle)
	{
		_hasParticle = false;
		GameToolbox::log("Failed to create particle {}", path);
		return;
	}

	_hasParticle = true;
	applyParticleTexture(_particle);
	_particle->setGlobalZOrder(static_cast<float>(zOrder));
	_particle->retain();
	_particle->setPositionType(ParticleSystem::PositionType::GROUPED);
	_particle->setBlendFunc(GameToolbox::getBlending());
	_particle->setRotation(getRotation());
	// Don't play at level load — only when the pad/orb is activated.
	_particle->stopSystem();
	_particle->setVisible(false);
}

void GameObject::setupCustomParticle(std::string_view data)
{
	if (_particle)
	{
		if (_particle->getParent())
		{
			AX_SAFE_RETAIN(_particle);
			_particle->removeFromParentAndCleanup(false);
		}
		AX_SAFE_RELEASE(_particle);
		_particle = nullptr;
	}

	auto parts = GameToolbox::splitByDelimStringView(data, 'a');
	auto num = [&](size_t i, float fallback) -> float {
		if (i >= parts.size() || parts[i].empty())
			return fallback;
		return GameToolbox::stof(parts[i]);
	};
	auto channel = [&](size_t i, float fallback) -> float {
		float v = num(i, fallback);
		return v > 1.5f ? v / 255.f : v;
	};

	const int maxParticles = std::clamp(static_cast<int>(num(0, 30.f)), 1, 400);
	auto* ps = ParticleSystemQuad::createWithTotalParticles(maxParticles);
	if (!ps)
	{
		_hasParticle = false;
		return;
	}

	const float duration = num(1, -1.f);
	ps->setDuration(duration < 0.f ? -1.f : (duration == 0.f ? -1.f : duration));

	const float life = std::max(num(2, 1.f), 0.05f);
	ps->setLife(life);
	ps->setLifeVar(num(3, 0.f));

	float emission = num(4, 0.f);
	if (emission <= 0.f)
		emission = static_cast<float>(maxParticles) / life;
	ps->setEmissionRate(emission);

	ps->setAngle(num(5, 90.f));
	ps->setAngleVar(num(6, 0.f));
	ps->setSpeed(num(7, 20.f));
	ps->setSpeedVar(num(8, 0.f));
	ps->setPosVar({num(9, 0.f), num(10, 0.f)});
	ps->setGravity({num(11, 0.f), num(12, 0.f)});
	ps->setRadialAccel(num(13, 0.f));
	ps->setRadialAccelVar(num(14, 0.f));
	ps->setTangentialAccel(num(15, 0.f));
	ps->setTangentialAccelVar(num(16, 0.f));
	ps->setStartSize(std::max(num(17, 4.f), 0.5f));
	ps->setStartSizeVar(num(18, 0.f));
	ps->setStartSpin(num(19, 0.f));
	ps->setStartSpinVar(num(20, 0.f));
	ps->setStartColor(Color4F(channel(21, 1.f), channel(23, 1.f), channel(25, 1.f), channel(27, 1.f)));
	ps->setStartColorVar(Color4F(channel(22, 0.f), channel(24, 0.f), channel(26, 0.f), channel(28, 0.f)));

	float endSize = num(29, 0.f);
	ps->setEndSize(endSize < 0.f ? ParticleSystem::START_SIZE_EQUAL_TO_END_SIZE : endSize);
	ps->setEndSizeVar(num(30, 0.f));
	ps->setEndSpin(num(31, 0.f));
	ps->setEndSpinVar(num(32, 0.f));
	ps->setEndColor(Color4F(channel(33, 1.f), channel(35, 1.f), channel(37, 1.f), channel(39, 0.f)));
	ps->setEndColorVar(Color4F(channel(34, 0.f), channel(36, 0.f), channel(38, 0.f), channel(40, 0.f)));

	if (num(51, 0.f) > 0.5f)
	{
		ps->setEmitterMode(ParticleSystem::Mode::RADIUS);
		ps->setStartRadius(num(45, 0.f));
		ps->setStartRadiusVar(num(46, 0.f));
	}
	else
		ps->setEmitterMode(ParticleSystem::Mode::GRAVITY);

	const int posType = static_cast<int>(num(52, 2.f));
	if (posType == 0)
		ps->setPositionType(ParticleSystem::PositionType::FREE);
	else if (posType == 1)
		ps->setPositionType(ParticleSystem::PositionType::RELATIVE);
	else
		ps->setPositionType(ParticleSystem::PositionType::GROUPED);

	ps->setBlendAdditive(num(53, 0.f) > 0.5f);
	applyParticleTexture(ps);
	ps->setPosition(getPosition());
	ps->setRotation(getRotation());
	ps->setGlobalZOrder(6.f);
	ps->retain();

	_particle = ps;
	_hasParticle = true;

	if (_animateOnTrigger)
		ps->stopSystem();
	else
		ps->resetSystem();
}

GameObject* GameObject::createObject(std::string_view frame, std::string_view glowFrame)
{
	if (frame.find("ring_01_001") != std::string::npos)
		return GameObject::create(frame, glowFrame);
	else
		return nullptr;
}

GameObject* GameObject::create(std::string_view frame, std::string_view glowFrame)
{
	auto pRet = new (std::nothrow) GameObject();

	if (pRet && pRet->init(frame, glowFrame))
	{
		pRet->autorelease();
		return pRet;
	}
	AX_SAFE_DELETE(pRet);
	return nullptr;
}

void GameObject::setPosition(const ax::Vec2& pos)
{
	Sprite::setPosition(pos);
	_parentMatrix = getNodeToParentTransform();
	if (_hasGlow && _glowSprite)
		_glowSprite->setPosition(pos);
	if (_hasParticle && _particle)
		_particle->setPosition(pos);
	refreshCollisionBounds();
}
void GameObject::setRotation(float rotation)
{
	Sprite::setRotation(rotation);
	_parentMatrix = getNodeToParentTransform();
	if (_hasGlow && _glowSprite)
		_glowSprite->setRotation(rotation);
	if (_hasParticle && _particle)
		_particle->setRotation(rotation);
	refreshCollisionBounds();
}
void GameObject::setScaleX(float scalex)
{
	Sprite::setScaleX(scalex);
	_parentMatrix = getNodeToParentTransform();
	if (_hasGlow && _glowSprite)
		_glowSprite->setScaleX(scalex);
	if (_hasParticle && _particle)
		_particle->setRotation(scalex * (isFlippedX() ? -1.f : 1.f));
	refreshCollisionBounds();
}
void GameObject::setScaleY(float scaley)
{
	Sprite::setScaleY(scaley);
	_parentMatrix = getNodeToParentTransform();
	if (_hasGlow && _glowSprite)
		_glowSprite->setScaleY(scaley);
	if (_hasParticle && _particle)
		_particle->setScaleY(scaley * (isFlippedX() ? -1.f : 1.f));
	refreshCollisionBounds();
}
void GameObject::setOpacity(uint8_t opacity)
{
	if (_primaryInvisible) opacity = 0;
	Sprite::setOpacity(opacity);
	if (_hasGlow && _glowSprite)
		_glowSprite->setOpacity(opacity);
	if (_hasParticle && _particle)
		_particle->setOpacity(opacity);
}

Color3B GameObject::getChannelColor(SpriteColor *colorChannel)
{
	auto bgl = BaseGameLayer::getInstance();
	if (!bgl || !colorChannel)
		return Color3B::WHITE;

	Color3B returnCol;

	int copyId = colorChannel->_copyingColorID;
	if (copyId != -1 && bgl->_colorChannels.contains(copyId) && &bgl->_colorChannels[copyId] != colorChannel)
		returnCol = getChannelColor(&bgl->_colorChannels[copyId]);
	else
		returnCol = colorChannel->_color;

	if (colorChannel->_applyHsv)
		GameToolbox::applyHSV(colorChannel->_hsvModifier, &returnCol);

	return returnCol;
}



void GameObject::applyColorChannel(ax::Sprite* sprite, int channelType, float opacityMultiplier, SpriteColor* col)
{
	if (!sprite || !col) return;
	
	float op = col->_opacity * opacityMultiplier * _effectOpacityMultipler;

	switch (channelType)
	{
	case 0:
		if (getOpacity() != op)
			sprite->setOpacity(op);
		break;
	case 1:
		if (getOpacity() != op)
			sprite->setOpacity(op);
		break;
	default:
		if (getOpacity() != op)
			sprite->setOpacity(op);
		break;
	}

	if (col->_blending)
		sprite->setBlendFunc(GameToolbox::getBlending());
	else
		sprite->setBlendFunc(BlendFunc::DISABLE);

	Color3B finalColor = getChannelColor(col);

	_tempColor = finalColor;
}

void GameObject::update()
{
	std::string_view frame = getBlockFrame(getID());
	if (_isTrigger || frame.rfind("edit_e", 0) == 0 || frame.rfind("edit_ee", 0) == 0)
	{
		setVisible(false);
		setOpacity(0);
		return;
	}

	if (isCoin() && (_hasBeenActivatedP1 || _hasBeenActivatedP2))
	{
		setVisible(false);
		return;
	}

	if (getEnterEffectID() == 0)
	{
		// setPosition(_startPosition);
		setScaleX(_startScale.x);
		setScaleY(_startScale.y);
	}

	auto bgl = BaseGameLayer::getInstance();
	if (!bgl)
		return;

	if (!_isTrigger)
		this->setPosition(this->_startPosition + this->_startPosOffset);

	float opacityMultiplier = 1.f;

	ax::Color3B groupColor;
	GroupProperties::GroupState state = GroupProperties::GroupState::NOT_CHANGING;

	for (int i : _groups | std::views::reverse)
	{
		auto it = bgl->_groups.find(i);
		if (it == bgl->_groups.end())
			continue;
		opacityMultiplier *= it->second._alpha;
		if (it->second.groupState != GroupProperties::GroupState::NOT_CHANGING)
		{
			groupColor = it->second._color;
			state = it->second.groupState;
		}
	}

	_mainColor = bgl->_colorChannels.contains(_mainColorChannel) ? &bgl->_colorChannels[_mainColorChannel] : nullptr;
	_secColor = bgl->_colorChannels.contains(_secColorChannel) ? &bgl->_colorChannels[_secColorChannel] : nullptr;

	if (_childSpritesChannel.empty())
		return;

	switch (_childSpritesChannel[0])
	{
	case 0:

		if (state == GroupProperties::GroupState::MAIN_ONLY || state == GroupProperties::GroupState::MAIN_DETAIL)
			_tempColor = groupColor;
		else
			applyColorChannel(this, 0, opacityMultiplier, _mainColor);

		if (_mainHSVEnabled)
			GameToolbox::applyHSV(_mainHSV, &_tempColor);

		if (getColor() != _tempColor)
			setColor(_tempColor);
		break;
	case 1:

		if (state == GroupProperties::GroupState::DETAIL_ONLY || state == GroupProperties::GroupState::MAIN_DETAIL)
			_tempColor = groupColor;
		else if (_secColor)
			applyColorChannel(this, 1, opacityMultiplier, _secColor);
		else
			applyColorChannel(this, 1, opacityMultiplier, _mainColor);

		if (_secondaryHSVEnabled)
			GameToolbox::applyHSV(_secondaryHSV, &_tempColor);
		else if (_isOnlyDetail)
			GameToolbox::applyHSV(_mainHSV, &_tempColor);

		if (getColor() != _tempColor)
			setColor(_tempColor);
		break;
	case 2:
		if (bgl->_colorChannels.contains(1010))
			applyColorChannel(this, 2, opacityMultiplier, &bgl->_colorChannels[1010]);
		if (getColor() != _tempColor)
			setColor(_tempColor);
		break;
	}

	for (size_t i = 0; i < _childSprites.size(); i++)
	{
		if (i + 1 >= _childSpritesChannel.size() || !_childSprites[i])
			continue;
		switch (_childSpritesChannel[i + 1])
		{
		case 0:

			if (state == GroupProperties::GroupState::MAIN_ONLY || state == GroupProperties::GroupState::MAIN_DETAIL)
				_tempColor = groupColor;
			else
				applyColorChannel(_childSprites[i], 0, opacityMultiplier, _mainColor);

			if (_mainHSVEnabled)
				GameToolbox::applyHSV(_mainHSV, &_tempColor);
			if (_childSprites[i]->getColor() != _tempColor)
				_childSprites[i]->setColor(_tempColor);
			break;
		case 1:

			if (state == GroupProperties::GroupState::DETAIL_ONLY || state == GroupProperties::GroupState::MAIN_DETAIL)
				_tempColor = groupColor;
			else if (_secColor)
				applyColorChannel(_childSprites[i], 1, opacityMultiplier, _secColor);
			else
				applyColorChannel(_childSprites[i], 1, opacityMultiplier, _mainColor);

			if (_secondaryHSVEnabled)
				GameToolbox::applyHSV(_secondaryHSV, &_tempColor);
			else if (_isOnlyDetail)
				GameToolbox::applyHSV(_mainHSV, &_tempColor);
			if (_childSprites[i]->getColor() != _tempColor)
				_childSprites[i]->setColor(_tempColor);
			break;
		case 2:
			if (bgl->_colorChannels.contains(1010))
				applyColorChannel(_childSprites[i], 2, opacityMultiplier, &bgl->_colorChannels[1010]);
			if (_childSprites[i]->getColor() != _tempColor)
				_childSprites[i]->setColor(_tempColor);
			break;
		}
	}
}

GameObject* GameObject::createFromString(std::string_view data)
{
	// data = 1,2,3,4,5,6,7 where [key,value,key,value]
	auto properties = GameToolbox::splitByDelimStringView(data, ',');

	GameObject* obj = nullptr;

	// Need at least key,value for object id (properties[0]=1, properties[1]=id).
	if (properties.size() < 2)
		return nullptr;

	int objectID = GameToolbox::stoi(properties[1]);

	std::string_view frame = GameObject::getBlockFrame(objectID);
	if (frame.empty())
	{
		GameToolbox::log("Could not resolve block frame for object id {}", objectID);
		return nullptr;
	}

	// actually create the object
	if (objectID != 1 && GameObject::isTriggerID(objectID))
	{
		obj = EffectGameObject::create(frame);
		// mylock.unlock();
		if(obj)
		{
			obj->_isTrigger = true;
		}
	}
	else
	{
		// mylock.lock();
		obj = GameObject::create(frame, GameObject::getGlowFrame(objectID));
		// mylock.unlock();
	}

	if (!obj)
	{
		GameToolbox::log("Could not create object from {}", frame);
		return nullptr;
	}

	AX_SAFE_RETAIN(obj);

	obj->setStretchEnabled(false);
	obj->setActive(true);
	obj->setID(objectID);
	obj->customSetup();

	auto bgl = BaseGameLayer::getInstance();
	// TODO: set uniqueID in base layer

	auto parseHSV = [](std::string_view raw, GDHSV& out) {
		auto hsv = GameToolbox::splitByDelimStringView(raw, 'a');
		if (hsv.size() < 5)
			return;
		out.h = GameToolbox::stof(hsv[0]);
		out.s = GameToolbox::stof(hsv[1]);
		out.v = GameToolbox::stof(hsv[2]);
		out.sChecked = GameToolbox::stoi(hsv[3]) != 0;
		out.vChecked = GameToolbox::stoi(hsv[4]) != 0;
	};

	// iterate over every key
	for (size_t i = 0; i + 1 < properties.size(); i += 2)
	{
		int key = GameToolbox::stoi(properties[i]);
		switch (key)
		{
		case 2:
			obj->setPositionX(GameToolbox::stof(properties[i + 1]));
			break;
		case 3:
			obj->setPositionY(GameToolbox::stof(properties[i + 1]) + 90.0f);
			break;
		case 4:
			obj->setScaleX(-1.f * GameToolbox::stof(properties[i + 1]));
			break;
		case 5:
			obj->setScaleY(-1.f * GameToolbox::stof(properties[i + 1]));
			break;
		case 6:
			obj->setRotation(GameToolbox::stof(properties[i + 1]));
			break;
		case 7:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_color.r = GameToolbox::stoi(properties[i + 1]);
			break;
		case 8:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_color.g = GameToolbox::stoi(properties[i + 1]);
			break;
		case 9:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_color.b = GameToolbox::stoi(properties[i + 1]);
			break;
		case 10:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_duration = GameToolbox::stof(properties[i + 1]);
			break;
		case 17:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_blending = GameToolbox::stoi(properties[i + 1]);
		case 20:
			obj->_editorLayer = GameToolbox::stoi(properties[i + 1]);
			break;
		case 21:
			obj->_mainColorChannel = GameToolbox::stoi(properties[i + 1]);
			if (bgl && !bgl->_colorChannels.contains(obj->_mainColorChannel))
			{
				bgl->_colorChannels.insert({obj->_mainColorChannel, SpriteColor(Color3B::WHITE, 255, 0)});
				bgl->_originalColors.insert({obj->_mainColorChannel, SpriteColor(Color3B::WHITE, 255, 0)});
			}
			break;
		case 22:
			obj->_secColorChannel = GameToolbox::stoi(properties[i + 1]);
			if (bgl && !bgl->_colorChannels.contains(obj->_secColorChannel))
			{
				bgl->_colorChannels.insert({obj->_secColorChannel, SpriteColor(Color3B::WHITE, 255, 0)});
				bgl->_originalColors.insert({obj->_secColorChannel, SpriteColor(Color3B::WHITE, 255, 0)});
			}
			break;
		case 23:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_targetColorId = GameToolbox::stoi(properties[i + 1]);
			break;
		case 24:
			obj->_zLayer = GameToolbox::stoi(properties[i + 1]);
			break;
		case 25:
			obj->setGlobalZOrder(static_cast<float>(GameToolbox::stoi(properties[i + 1])));
			break;
		case 28:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_offset.x = GameToolbox::stof(properties[i + 1]);
			break;
		case 29:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_offset.y = GameToolbox::stof(properties[i + 1]);
			break;
		case 30:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_easing = GameToolbox::stoi(properties[i + 1]);
			break;
		case 85:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_easeRate = GameToolbox::stof(properties[i + 1]);
			break;
		case 32:
			obj->setScaleX(obj->getScaleX() * GameToolbox::stof(properties[i + 1]));
			obj->setScaleY(obj->getScaleY() * GameToolbox::stof(properties[i + 1]));
			break;
		case 35:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_opacity = GameToolbox::stof(properties[i + 1]);
			break;
		case 41:
			obj->_mainHSVEnabled = GameToolbox::stoi(properties[i + 1]);
			break;
		case 42:
			obj->_secondaryHSVEnabled = GameToolbox::stoi(properties[i + 1]);
			break;
		case 43:
			parseHSV(properties[i + 1], obj->_mainHSV);
			break;
		case 44:
			parseHSV(properties[i + 1], obj->_secondaryHSV);
			break;
		case 45:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_fadeIn = GameToolbox::stof(properties[i + 1]);
			break;
		case 46:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_hold = GameToolbox::stof(properties[i + 1]);
			break;
		case 47:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_fadeOut = GameToolbox::stof(properties[i + 1]);
			break;
		case 48:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_pulseMode = GameToolbox::stoi(properties[i + 1]);
			break;
		case 49: {
			if (obj->_isTrigger)
			{
				if (auto* trigger = dynamic_cast<EffectGameObject*>(obj))
					parseHSV(properties[i + 1], trigger->_hsv);
			}
			break;
		}

		case 50:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_copiedColorId = GameToolbox::stoi(properties[i + 1]);
			break;
		case 52:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_pulseType = GameToolbox::stoi(properties[i + 1]);
			break;
		case 51:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_targetGroupId = GameToolbox::stoi(properties[i + 1]);
			else
				obj->_teleportTargetGroupId = GameToolbox::stoi(properties[i + 1]);
			break;
		case 54:
			// TeleportPortalObject::m_teleportYOffset (classic 747 portals in Deadlocked etc.)
			obj->_teleportYOffset = GameToolbox::stof(properties[i + 1]);
			break;
		case 56:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_activateGroup = GameToolbox::stoi(properties[i + 1]);
			break;
		case 58:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_lockToPlayerX = GameToolbox::stoi(properties[i + 1]);
			break;
		case 59:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_lockToPlayerY = GameToolbox::stoi(properties[i + 1]);
			break;
		case 123:
			obj->_animateOnTrigger = GameToolbox::stoi(properties[i + 1]) != 0;
			break;
		case 145:
			obj->_particleData = std::string(properties[i + 1]);
			break;
		case 57: {
			auto groups = GameToolbox::splitByDelimStringView(properties[i + 1], '.');
			// pre-allocate
			obj->_groups.reserve(groups.size());
			for (std::string_view groupStr : groups)
			{
				int group = GameToolbox::stoi(groupStr);
				if (auto* pl = PlayLayer::getInstance())
					pl->_groups[group]._objects.push_back(obj);
				else if (bgl)
					bgl->_groups[group]._objects.push_back(obj);
				obj->_groups.push_back(group);
			}
			break;
		}
		case 62:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_spawnTriggered = GameToolbox::stoi(properties[i + 1]);
			break;
		case 63:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_spawnDelay = GameToolbox::stof(properties[i + 1]);
			break;
		case 65:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_mainOnly = GameToolbox::stoi(properties[i + 1]);
			break;
		case 66:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_detailOnly = GameToolbox::stoi(properties[i + 1]);
			break;
		case 67: // dont enter
		case 64: // dont exit
			obj->setDontTransform(true);
			break;
		case 121: // noTouch — disable collision (GD 2.1+)
			obj->_isNoTouch = GameToolbox::stoi(properties[i + 1]) != 0;
			if (obj->_isNoTouch)
				obj->setGameObjectType(kGameObjectTypeDecoration);
			break;
		case 87:
			if (obj->_isTrigger)
				dynamic_cast<EffectGameObject*>(obj)->_multiTriggered = GameToolbox::stoi(properties[i + 1]);
			break;
		case 108:
			obj->_linkedGroupId = GameToolbox::stoi(properties[i + 1]);
			break;
		case 352:
			obj->_teleportIgnoreX = GameToolbox::stoi(properties[i + 1]) != 0;
			break;
		case 353:
			obj->_teleportIgnoreY = GameToolbox::stoi(properties[i + 1]) != 0;
			break;
		} // switch end
	}	  // for end

	if (obj)
	{
		const Hitbox hb = resolveObjectHitbox(objectID, obj);
		obj->applyLoadedHitbox(hb);
		if (obj->getGameObjectType() == kGameObjectTypeSlope)
			obj->determineSlopeDirection();
		obj->setStartPosition(obj->getPosition());
		obj->setStartScaleX(obj->getScaleX());
		obj->setStartScaleY(obj->getScaleY());
		if (obj->getID() == 1586)
			obj->setupCustomParticle(obj->_particleData);
		return obj;
	}

	return nullptr;
}

void GameObject::removeFromGameLayer()
{
	setActive(false);

	for (auto sprite : _childSprites)
	{
		if (!sprite)
			continue;
		if (sprite->getBlendFunc() != getBlendFunc() && sprite->getParent())
		{
			AX_SAFE_RETAIN(sprite);
			sprite->removeFromParentAndCleanup(true);
		}
	}

	AX_SAFE_RETAIN(this);
	if (_particle && _particle->getParent())
	{
		AX_SAFE_RETAIN(_particle);
		_particle->removeFromParentAndCleanup(true);
	}
	if (_glowSprite && _glowSprite->getParent())
	{
		AX_SAFE_RETAIN(_glowSprite);
		_glowSprite->removeFromParentAndCleanup(true);
	}

	if (getParent())
		removeFromParentAndCleanup(true);
}

void GameObject::updateTweenAction(float value, std::string_view key)
{
}

std::string GameObject::keyToFrame(int key)
{
	return key > 0 && key < 8 ? fmt::format("square_{}_001.png", key) : "";
}

// couldn't understand jack shit from the original
// so I recreated from 2.1's GameToolbox::stringSetupToMap
std::map<std::string, std::string> GameObject::stringSetupToDict(std::string str)
{
	size_t index = 0;
	size_t length = str.length();

	std::map<std::string, std::string> output;

	size_t currentComma = str.find(',');
	while (true)
	{
		auto key = str.substr(index, currentComma - index);

		if (currentComma == str.npos)
			break;

		// find new comma
		index = currentComma + 1;
		currentComma = str.find(',', index);

		// set values
		output.insert(std::make_pair(key, str.substr(index, currentComma - index)));

		if (currentComma == str.npos)
			break;

		// find new comma (again)
		index = currentComma + 1;
		currentComma = str.find(',', index);
	}

	return output;
}
void GameObject::triggerActivated(PlayerObject* player)
{
	auto* bgl = PlayLayer::getInstance();
	if (!bgl || !player)
		return;
	if (player == bgl->_player1)
		_hasBeenActivatedP1 = true;
	else
		_hasBeenActivatedP2 = true;

	// Pad/orb particle burst on bounce/click (object's bumpEffect / ringEffect).
	if (_particle && _animateOnTrigger)
	{
		const bool isPad = _pObjectType == kGameObjectTypeYellowJumpPad || _pObjectType == kGameObjectTypeGravityPad ||
						   _pObjectType == kGameObjectTypePinkJumpPad || _pObjectType == kGameObjectTypeRedJumpPad ||
						   _pObjectType == kGameObjectTypeSpiderPad;
		_particle->setPosition(getPosition() + ax::Vec2(0.f, isPad ? -2.f : 0.f));
		_particle->setVisible(true);
		if (!_particle->getParent())
		{
			ax::Node* parent = getParent();
			if (!parent)
				parent = bgl;
			if (parent)
				parent->addChild(_particle, 80);
		}
		_particle->setDuration(0.05f);
		_particle->stopSystem();
		_particle->resetSystem();
		_particle->resumeEmissions();

		// Stop emitting so duration -1 plists don't keep spraying forever.
		_particle->runAction(Sequence::create(DelayTime::create(0.08f), CallFunc::create([this]() {
												  if (_particle)
													  _particle->stopSystem();
											  }),
											  nullptr));
	}
}

bool GameObject::hasBeenActivatedByPlayer(PlayerObject* player)
{
	auto* bgl = PlayLayer::getInstance();
	if (!bgl || !player)
		return false;
	return player == bgl->_player1 ? _hasBeenActivatedP1 : _hasBeenActivatedP2;
}

ax::Rect GameObject::getOuterBounds(float a, float b)
{
	ax::Rect r = getOuterBounds();
	r.origin.x += r.size.width / 2;
	r.origin.y += r.size.height / 2;
	r.size.width *= a;
	r.size.height *= b;
	r.origin.x -= r.size.width / 2;
	r.origin.y -= r.size.height / 2;
	return r;
}

std::string_view GameObject::getGlowFrame(int objectID)
{
	 // return "";

	 switch(objectID)
	 {
	 [[likely]] default: return "";
	 case 44: return "checkpoint_01_glow_001";
	 [[likely]] case 1: return "square_01_glow_001";
	 [[likely]] case 2: return "square_02_glow_001";
	 [[likely]] case 3: return "square_03_glow_001";
	 [[likely]] case 4: return "square_04_glow_001";
	 [[likely]] case 6: return "square_06_glow_001";
	 [[likely]] case 7: return "square_07_glow_001";
	 [[likely]] case 8: return "spike_01_glow_001";
	 case 35: return "bump_01_glow_001";
	 case 39: return "spike_02_glow_001";
	 case 40: return "plank_01_glow_001";
	 [[unlikely]] case 1903: return "plank_01_glow_001";
	 }
}