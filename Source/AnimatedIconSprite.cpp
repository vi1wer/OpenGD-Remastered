/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "AnimatedIconSprite.h"

#include "GameToolbox/conv.h"
#include "GameToolbox/log.h"
#include "GameToolbox/math.h"
#include "UTF8.h"
#include "base/Director.h"
#include "base/Value.h"
#include "platform/FileUtils.h"
#include "2d/Sprite.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <unordered_map>

USING_NS_AX;

namespace
{
std::string_view trimView(std::string_view s)
{
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		s.remove_prefix(1);
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
		s.remove_suffix(1);
	return s;
}

Vec2 parseBraceVec2(const std::string& raw, const Vec2& fallback = Vec2::ZERO)
{
	std::string_view s = trimView(raw);
	if (s.size() >= 2 && s.front() == '{' && s.back() == '}')
		s = s.substr(1, s.size() - 2);
	const auto comma = s.find(',');
	if (comma == std::string_view::npos)
		return fallback;
	return Vec2(GameToolbox::stof(trimView(s.substr(0, comma))),
				GameToolbox::stof(trimView(s.substr(comma + 1))));
}

int parsePartIndex(const std::string& textureName)
{
	// robot_01_03_001.png / spider_01_02_001.png
	const auto first = textureName.find('_');
	if (first == std::string::npos)
		return 1;
	const auto second = textureName.find('_', first + 1);
	if (second == std::string::npos)
		return 1;
	const auto third = textureName.find('_', second + 1);
	const auto part = textureName.substr(second + 1, (third == std::string::npos ? textureName.size() : third) - second - 1);
	const int value = GameToolbox::stoi(part);
	return value > 0 ? value : 1;
}

bool parseFrameKey(const std::string& key, const char* kindPrefix, std::string& animName, int& frameIndex)
{
	const std::string prefix = std::string(kindPrefix) + "_";
	if (key.size() <= prefix.size() + 4 || key.compare(0, prefix.size(), prefix) != 0)
		return false;
	if (key.compare(key.size() - 4, 4, ".png") != 0)
		return false;

	const std::string body = key.substr(prefix.size(), key.size() - prefix.size() - 4);
	const auto us = body.rfind('_');
	if (us == std::string::npos || us + 1 >= body.size())
		return false;

	animName = body.substr(0, us);
	frameIndex = GameToolbox::stoi(std::string_view(body).substr(us + 1));
	return frameIndex > 0 && !animName.empty();
}

Sprite* tryFrame(const std::string& name, const std::string& fallback)
{
	Sprite* spr = Sprite::createWithSpriteFrameName(name);
	if (!spr && !fallback.empty() && fallback != name)
		spr = Sprite::createWithSpriteFrameName(fallback);
	return spr;
}

void tintSprite(Sprite* spr, const Color3B& color)
{
	if (spr)
		spr->setColor(color);
}
} // namespace

AnimatedIconSprite* AnimatedIconSprite::create(IconType type, int iconId)
{
	auto* ret = new (std::nothrow) AnimatedIconSprite();
	if (ret && ret->init(type, iconId))
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
	return nullptr;
}

bool AnimatedIconSprite::init(IconType type, int iconId)
{
	if (!Node::init())
		return false;

	_type = (type == IconType::kIconTypeSpider) ? IconType::kIconTypeSpider : IconType::kIconTypeRobot;
	_data = loadAnimData(_type);
	if (!_data || _data->parts.empty())
		return false;

	setIconId(iconId);
	setCascadeOpacityEnabled(true);
	return true;
}

const AnimatedIconSprite::AnimData* AnimatedIconSprite::loadAnimData(IconType type)
{
	static AnimData robotData;
	static AnimData spiderData;
	static bool robotTried = false;
	static bool spiderTried = false;

	AnimData* data = (type == IconType::kIconTypeSpider) ? &spiderData : &robotData;
	bool& tried = (type == IconType::kIconTypeSpider) ? spiderTried : robotTried;
	if (tried)
		return data->parts.empty() ? nullptr : data;
	tried = true;

	const char* plist = (type == IconType::kIconTypeSpider) ? "Spider_AnimDesc.plist" : "Robot_AnimDesc.plist";
	const char* kindPrefix = (type == IconType::kIconTypeSpider) ? "Spider" : "Robot";
	auto* fu = FileUtils::getInstance();
	ValueMap root = fu->getValueMapFromFile(plist);
	if (root.empty())
	{
		GameToolbox::log("Failed to load {}", plist);
		return nullptr;
	}

	const auto usedIt = root.find("usedTextures");
	if (usedIt == root.end() || usedIt->second.getTypeFamily() != Value::Type::MAP)
		return nullptr;

	std::map<int, PartDef> orderedParts;
	for (const auto& [key, value] : usedIt->second.asValueMap())
	{
		if (value.getTypeFamily() != Value::Type::MAP)
			continue;
		const ValueMap& partMap = value.asValueMap();
		PartDef def;
		if (const auto tagIt = partMap.find("tag"); tagIt != partMap.end())
			def.tag = GameToolbox::stoi(tagIt->second.asString());
		if (const auto texIt = partMap.find("texture"); texIt != partMap.end())
			def.partIndex = parsePartIndex(texIt->second.asString());
		orderedParts.emplace(def.tag, def);
	}

	data->parts.reserve(orderedParts.size());
	int maxTag = -1;
	for (const auto& [tag, def] : orderedParts)
	{
		data->parts.push_back(def);
		maxTag = std::max(maxTag, tag);
	}

	const auto animIt = root.find("animationContainer");
	if (animIt == root.end() || animIt->second.getTypeFamily() != Value::Type::MAP)
		return data->parts.empty() ? nullptr : data;

	std::unordered_map<std::string, std::map<int, AnimFrame>> collected;
	for (const auto& [frameKey, frameVal] : animIt->second.asValueMap())
	{
		if (frameVal.getTypeFamily() != Value::Type::MAP)
			continue;
		std::string animName;
		int frameIndex = 0;
		if (!parseFrameKey(frameKey, kindPrefix, animName, frameIndex))
			continue;

		AnimFrame frame;
		frame.parts.assign(static_cast<size_t>(std::max(maxTag + 1, 1)), PartPose{});
		for (const auto& [spriteKey, spriteVal] : frameVal.asValueMap())
		{
			if (spriteVal.getTypeFamily() != Value::Type::MAP)
				continue;
			const ValueMap& spr = spriteVal.asValueMap();
			int tag = 0;
			if (const auto tagIt = spr.find("tag"); tagIt != spr.end())
				tag = GameToolbox::stoi(tagIt->second.asString());
			if (tag < 0)
				continue;
			if (tag >= static_cast<int>(frame.parts.size()))
				frame.parts.resize(static_cast<size_t>(tag) + 1);

			PartPose pose;
			pose.valid = true;
			if (const auto posIt = spr.find("position"); posIt != spr.end())
				pose.position = parseBraceVec2(posIt->second.asString());
			if (const auto scaleIt = spr.find("scale"); scaleIt != spr.end())
				pose.scale = parseBraceVec2(scaleIt->second.asString(), Vec2(1.f, 1.f));
			if (const auto flipIt = spr.find("flipped"); flipIt != spr.end())
				pose.flipped = parseBraceVec2(flipIt->second.asString());
			if (const auto rotIt = spr.find("rotation"); rotIt != spr.end())
				pose.rotation = GameToolbox::stof(rotIt->second.asString());
			if (const auto zIt = spr.find("zValue"); zIt != spr.end())
				pose.z = GameToolbox::stoi(zIt->second.asString());
			frame.parts[static_cast<size_t>(tag)] = pose;
		}
		collected[animName].emplace(frameIndex, std::move(frame));
	}

	for (auto& [name, frames] : collected)
	{
		Animation anim;
		anim.frames.reserve(frames.size());
		for (auto& [index, frame] : frames)
			anim.frames.push_back(std::move(frame));
		data->animations.emplace(name, std::move(anim));
	}

	return data->parts.empty() ? nullptr : data;
}

void AnimatedIconSprite::setIconId(int iconId)
{
	const int maxIcon = GameToolbox::getValueForGamemode(_type);
	_iconId = GameToolbox::inRange(iconId, 1, maxIcon > 0 ? maxIcon : 1);
	rebuildParts();

	if (!_currentAnim.empty() && hasAnimation(_currentAnim))
	{
		const auto& frames = _data->animations.at(_currentAnim).frames;
		if (!frames.empty())
			applyFrame(frames[static_cast<size_t>(std::clamp(_frameIndex, 0, static_cast<int>(frames.size()) - 1))]);
	}
	else if (hasAnimation("idle"))
		playAnimation("idle", false, true);
	else if (hasAnimation("idle01"))
		playAnimation("idle01", false, true);
}

void AnimatedIconSprite::rebuildParts()
{
	removeAllChildren();
	_parts.clear();
	if (!_data)
		return;

	const char* prefix = GameToolbox::getNameGamemode(_type);
	_parts.resize(_data->parts.size());

	int maxTag = 0;
	for (const auto& def : _data->parts)
		maxTag = std::max(maxTag, def.tag);
	std::vector<PartSprites> byTag(static_cast<size_t>(maxTag + 1));

	for (size_t i = 0; i < _data->parts.size(); ++i)
	{
		const PartDef& def = _data->parts[i];
		PartSprites sprites;
		const auto mainName = StringUtils::format("%s_%02d_%02d_001.png", prefix, _iconId, def.partIndex);
		const auto mainFb = StringUtils::format("%s_01_%02d_001.png", prefix, def.partIndex);
		sprites.main = tryFrame(mainName, mainFb);
		if (!sprites.main)
			continue;
		sprites.main->setStretchEnabled(false);
		sprites.main->setAnchorPoint({0.5f, 0.5f});
		sprites.main->setColor(_mainColor);
		addChild(sprites.main, def.tag);

		const auto secName = StringUtils::format("%s_%02d_%02d_2_001.png", prefix, _iconId, def.partIndex);
		const auto secFb = StringUtils::format("%s_01_%02d_2_001.png", prefix, def.partIndex);
		sprites.second = tryFrame(secName, secFb);
		if (sprites.second)
		{
			sprites.second->setStretchEnabled(false);
			sprites.second->setAnchorPoint({0.5f, 0.5f});
			sprites.second->setColor(_secondColor);
			addChild(sprites.second, def.tag - 1);
		}

		const auto glowName = StringUtils::format("%s_%02d_%02d_glow_001.png", prefix, _iconId, def.partIndex);
		const auto glowFb = StringUtils::format("%s_01_%02d_glow_001.png", prefix, def.partIndex);
		sprites.glow = tryFrame(glowName, glowFb);
		if (sprites.glow)
		{
			sprites.glow->setStretchEnabled(false);
			sprites.glow->setAnchorPoint({0.5f, 0.5f});
			sprites.glow->setBlendFunc(GameToolbox::getBlending());
			sprites.glow->setColor(_glowColor);
			sprites.glow->setVisible(_hasGlow);
			addChild(sprites.glow, def.tag - 2);
		}

		if (_type == IconType::kIconTypeSpider && def.partIndex == 1)
		{
			const auto extraName = StringUtils::format("%s_%02d_01_extra_001.png", prefix, _iconId);
			sprites.extra = tryFrame(extraName, "");
			if (sprites.extra)
			{
				sprites.extra->setStretchEnabled(false);
				sprites.extra->setAnchorPoint({0.5f, 0.5f});
				addChild(sprites.extra, def.tag + 1);
			}
		}

		byTag[static_cast<size_t>(def.tag)] = sprites;
		_parts[i] = sprites;
	}

	_parts = std::move(byTag);
}

void AnimatedIconSprite::applySpritePose(Sprite* spr, const PartPose& pose, int extraZ)
{
	if (!spr)
		return;
	spr->setPosition(pose.position);
	spr->setRotation(pose.rotation);
	spr->setScale(pose.scale.x, pose.scale.y);
	spr->setFlippedX(pose.flipped.x > 0.5f);
	spr->setFlippedY(pose.flipped.y > 0.5f);
	spr->setLocalZOrder(pose.z + extraZ);
}

void AnimatedIconSprite::applyFrame(const AnimFrame& frame)
{
	const size_t count = std::min(_parts.size(), frame.parts.size());
	for (size_t i = 0; i < count; ++i)
	{
		const PartPose& pose = frame.parts[i];
		if (!pose.valid)
			continue;
		applySpritePose(_parts[i].main, pose, 0);
		applySpritePose(_parts[i].second, pose, -1);
		applySpritePose(_parts[i].glow, pose, -2);
		applySpritePose(_parts[i].extra, pose, 1);
	}
}

std::string AnimatedIconSprite::resolveAnimationName(const std::string& name) const
{
	if (!_data)
		return {};
	if (_data->animations.count(name))
		return name;
	if (name == "run" && _data->animations.count("walk"))
		return "walk";
	if (name == "idle01" && _data->animations.count("idle"))
		return "idle";
	if (name == "idle" && _data->animations.count("idle01"))
		return "idle01";
	if (name == "jump_start" && _data->animations.count("jump"))
		return "jump";
	if (name == "jump_loop" && _data->animations.count("jump"))
		return "jump";
	if (name == "fall_start" && _data->animations.count("fall_loop"))
		return "fall_loop";
	return {};
}

bool AnimatedIconSprite::hasAnimation(const std::string& name) const
{
	return _data && _data->animations.count(name) > 0;
}

void AnimatedIconSprite::playAnimation(const std::string& name, bool loop, bool restart)
{
	const std::string resolved = resolveAnimationName(name);
	if (resolved.empty())
		return;
	if (!restart && _playing && _currentAnim == resolved)
		return;

	_currentAnim = resolved;
	_loop = loop;
	_frameIndex = 0;
	_frameTime = 0.f;
	_playing = true;

	const Animation& anim = _data->animations.at(_currentAnim);
	if (!anim.frames.empty())
		applyFrame(anim.frames.front());

	if (anim.frames.size() <= 1)
	{
		_playing = false;
		unscheduleUpdate();
		if (!loop)
			finishNonLooping();
		return;
	}
	scheduleUpdate();
}

void AnimatedIconSprite::stopAnimation()
{
	_playing = false;
	unscheduleUpdate();
}

void AnimatedIconSprite::finishNonLooping()
{
	_playing = false;
	if (_currentAnim == "jump_start")
	{
		playAnimation("jump_loop", true, true);
		return;
	}
	if (_currentAnim == "fall_start")
	{
		playAnimation("fall_loop", true, true);
		return;
	}
	unscheduleUpdate();
}

void AnimatedIconSprite::update(float dt)
{
	if (!_playing || !_data)
		return;
	const auto it = _data->animations.find(_currentAnim);
	if (it == _data->animations.end() || it->second.frames.empty())
		return;

	const auto& frames = it->second.frames;
	_frameTime += dt;
	while (_frameTime >= _frameDuration)
	{
		_frameTime -= _frameDuration;
		++_frameIndex;
		if (_frameIndex >= static_cast<int>(frames.size()))
		{
			if (_loop)
			{
				_frameIndex = 0;
			}
			else
			{
				_frameIndex = static_cast<int>(frames.size()) - 1;
				applyFrame(frames[static_cast<size_t>(_frameIndex)]);
				finishNonLooping();
				return;
			}
		}
		applyFrame(frames[static_cast<size_t>(_frameIndex)]);
	}
}

void AnimatedIconSprite::setMainColor(const Color3B& color)
{
	_mainColor = color;
	for (auto& part : _parts)
		tintSprite(part.main, color);
}

void AnimatedIconSprite::setSecondaryColor(const Color3B& color)
{
	_secondColor = color;
	for (auto& part : _parts)
		tintSprite(part.second, color);
}

void AnimatedIconSprite::setGlowColor(const Color3B& color)
{
	_glowColor = color;
	for (auto& part : _parts)
		tintSprite(part.glow, color);
}

void AnimatedIconSprite::setGlow(bool glow)
{
	_hasGlow = glow;
	for (auto& part : _parts)
	{
		if (part.glow)
			part.glow->setVisible(glow);
	}
}

Rect AnimatedIconSprite::computeBounds() const
{
	Rect bounds;
	bool hasBounds = false;
	auto includeSprite = [&](Sprite* spr) {
		if (!spr || !spr->isVisible())
			return;
		const Rect box = spr->getBoundingBox();
		if (box.size.width <= 0.f && box.size.height <= 0.f)
			return;
		if (!hasBounds)
		{
			bounds = box;
			hasBounds = true;
		}
		else
		{
			const float minX = std::min(bounds.getMinX(), box.getMinX());
			const float minY = std::min(bounds.getMinY(), box.getMinY());
			const float maxX = std::max(bounds.getMaxX(), box.getMaxX());
			const float maxY = std::max(bounds.getMaxY(), box.getMaxY());
			bounds = Rect(minX, minY, maxX - minX, maxY - minY);
		}
	};

	for (const auto& part : _parts)
	{
		includeSprite(part.main);
		includeSprite(part.second);
		includeSprite(part.extra);
	}
	if (!hasBounds)
		return Rect(-15.f, -15.f, 30.f, 30.f);
	return bounds;
}
