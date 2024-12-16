#pragma once

#include <obs.h>

#include "canvas-math.hpp"

static void setSceneItemOwnRotationDegrees(obs_sceneitem_t* sceneItem, double newAngle)
{
	while (newAngle >= 360.0f)
		newAngle -= 360.0f;

	while (newAngle < 0.0f)
		newAngle += 360.0f;

	if (newAngle == obs_sceneitem_get_rot(sceneItem))
		return;

	//
	// This piece of code was copied directly from OBS code.
	//
	// I don't know exactly how it works, but apparently it does in both
	// cases where there is a parent group, and where there is none.
	//
	// TODO: Figure this out later
	//

	auto rotatePos = [](vec2 *pos, float rot) -> void {
		float cosR = cos(rot);
		float sinR = sin(rot);

		vec2 newPos;

		newPos.x = cosR * pos->x - sinR * pos->y;
		newPos.y = sinR * pos->x + cosR * pos->y;

		vec2_copy(pos, &newPos);
	};

	matrix4 transform;
	obs_sceneitem_get_box_transform(sceneItem, &transform);
	vec2 rotatePoint;
	vec2_set(&rotatePoint,
		 transform.t.x + transform.x.x / 2 + transform.y.x / 2,
		 transform.t.y + transform.x.y / 2 + transform.y.y / 2);
	vec2 offsetPoint;
	obs_sceneitem_get_pos(sceneItem, &offsetPoint);

	offsetPoint.x -= rotatePoint.x;
	offsetPoint.y -= rotatePoint.y;

	rotatePos(&offsetPoint,
		  -degreesToRadians(obs_sceneitem_get_rot(sceneItem)));

	vec2 newPosition;
	vec2_copy(&newPosition, &offsetPoint);
	rotatePos(&newPosition, degreesToRadians(newAngle));
	newPosition.x += rotatePoint.x;
	newPosition.y += rotatePoint.y;

	obs_sceneitem_set_rot(sceneItem, newAngle);
	obs_sceneitem_set_pos(sceneItem, &newPosition);
}

static void setSceneItemViewportRotationDegrees(obs_scene_t *scene,
					   obs_sceneitem_t *sceneItem,
					   double newAngle)
{
	obs_sceneitem_t *parentSceneItem =
		obs_sceneitem_get_group(scene, sceneItem);

	if (parentSceneItem) {
		setSceneItemOwnRotationDegrees(
			sceneItem,
			newAngle - obs_sceneitem_get_rot(parentSceneItem));
	} else {
		setSceneItemOwnRotationDegrees(sceneItem, newAngle);
	}
}

static double getSceneItemViewportRotationDegrees(obs_scene_t *scene,
						obs_sceneitem_t *sceneItem)
{
	obs_sceneitem_t *parentSceneItem =
		obs_sceneitem_get_group(scene, sceneItem);

	double result = obs_sceneitem_get_rot(sceneItem);

	if (parentSceneItem) {
		result += obs_sceneitem_get_rot(parentSceneItem);
	}

	return result;
}

static void getSceneItemViewportBoundingBoxCoords(obs_scene_t *scene,
				       obs_sceneitem_t *sceneItem, vec2 *tl,
				       vec2 *br)
{
	bool isFirst = true;

	vec2_set(tl, 0.0f, 0.0f);
	vec2_set(br, 0.0f, 0.0f);

	obs_sceneitem_t *parentSceneItem =
		obs_sceneitem_get_group(scene, sceneItem);

	matrix4 transform, inv_transform;
	getSceneItemBoxTransformMatrices(sceneItem, parentSceneItem, &transform,
					 &inv_transform);

	auto check = [&](double x, double y) {
		auto pos = getTransformedPosition(x, y,
						  transform); // To world coords

		if (isFirst || pos.x < tl->x)
			tl->x = pos.x;
		if (isFirst || pos.y < tl->y)
			tl->y = pos.y;
		if (isFirst || pos.x > br->x)
			br->x = pos.x;
		if (isFirst || pos.y > br->y)
			br->y = pos.y;

		isFirst = false;
	};

	check(0.0f, 0.0f);
	check(1.0f, 0.0f);
	check(1.0f, 1.0f);
	check(0.0f, 1.0f);

	if (tl->x > br->x) {
		auto tmp = br->x;
		br->x = tl->x;
		tl->x = tmp;
	}

	if (tl->y > br->y) {
		auto tmp = br->y;
		br->y = tl->y;
		tl->y = tmp;
	}
}

static void setSceneItemViewportBoundingBoxPosition(obs_scene_t *scene,
					     obs_sceneitem_t *sceneItem,
					     double x, double y)
{
	obs_sceneitem_t *parentSceneItem =
		obs_sceneitem_get_group(scene, sceneItem);

	vec2 box_tl, box_br;
	getSceneItemViewportBoundingBoxCoords(scene, sceneItem, &box_tl,
					      &box_br);

	auto box_width = box_br.x - box_tl.x;
	auto box_height = box_br.y - box_tl.y;

	vec2 dst_box_tl, dst_box_br;
	vec2_set(&dst_box_tl, x, y);
	vec2_set(&dst_box_br, x + box_width, y + box_height);

	vec2 move_delta;

	// Calculate move delta, applying parent transformation if necessary
	if (parentSceneItem) {
		matrix4 parentTransform, invParentTransform;
		obs_sceneitem_get_draw_transform(parentSceneItem,
						 &parentTransform);

		matrix4_inv(&invParentTransform, &parentTransform);

		vec3 parent_box_tl = getTransformedPosition(box_tl.x, box_tl.y,
							    invParentTransform);

		vec3 parent_dst_box_tl = getTransformedPosition(
			dst_box_tl.x, dst_box_tl.y, invParentTransform);

		vec2_set(&move_delta, parent_dst_box_tl.x - parent_box_tl.x,
			 parent_dst_box_tl.y - parent_box_tl.y);
	} else {
		vec2_set(&move_delta, dst_box_tl.x - box_tl.x,
			 dst_box_tl.y - box_tl.y);
	}

	vec2 pos;
	obs_sceneitem_get_pos(sceneItem, &pos);
	pos.x += move_delta.x;
	pos.y += move_delta.y;
	obs_sceneitem_set_pos(sceneItem, &pos);
}
