#pragma once

#include <obs.h>
#include <graphics/matrix4.h>

#define PI (3.1415926535f)

static inline double radiansToDegrees(double radians) {
	return radians * 180.0f / PI;
}

static inline double degreesToRadians(double degrees) {
	return degrees * PI / 180.0f;
}

static inline double normalizeDegrees(double degrees) {
	while (degrees >= 360.0f)
		degrees -= 360.0f;

	while (degrees < 0.0f)
		degrees += 360.0f;

	return degrees;
}

static inline double getCircleDegrees(vec2 center, vec2 periphery) {
	double adjacent = periphery.x - center.x;
	double opposite = periphery.y - center.y;

	if (adjacent == 0.0f) {
		return opposite > 0.0f ? 0.0f : 180.0f;
	} else {
		auto result = radiansToDegrees(atan(fabsf(opposite) / fabsf(adjacent)));

		if (opposite > 0.0f) {
			// bottom
			if (adjacent > 0.0f) {
				// right
				result += 270.0f;
			} else {
				// left
				result = 90.0f - result;
			}
		} else {
			// top
			if (adjacent > 0.0f) {
				// right
				result = 90.0f - result + 180.0f;
			} else {
				// left
				result += 90.0f;
			}
		}

		return normalizeDegrees(result + 180.0f);
	}
}

static inline double getDistanceBetweenTwoPoints(double x1, double y1, double x2, double y2) {
	return sqrt(pow((x2 - x1), 2) + pow((y2 - y1), 2));
}

static inline double getDistanceBetweenTwoPoints(vec2 &a, vec2 &b)
{
	return sqrt(pow((b.x - a.x), 2) + pow((b.y - a.y), 2));
}

static void applyParentSceneItemTransform(obs_sceneitem_t *childSceneItem,
					  obs_sceneitem_t *parentSceneItem,
					  matrix4 *transform)
{
	if (parentSceneItem) {
		matrix4 parentTransform;
		obs_sceneitem_get_draw_transform(parentSceneItem,
						 &parentTransform);

		matrix4_mul(transform, transform, &parentTransform);
	}
}

static void getSceneItemDrawTransformMatrices(obs_sceneitem_t *sceneItem,
					      obs_sceneitem_t *parentSceneItem,
					      matrix4 *transform,
					      matrix4 *inv_transform)
{
	obs_sceneitem_get_draw_transform(sceneItem, transform);

	applyParentSceneItemTransform(sceneItem, parentSceneItem, transform);

	matrix4_inv(inv_transform, transform);
}

static void getSceneItemParentDrawTransformMatrices(
	obs_sceneitem_t *sceneItem, obs_sceneitem_t *parentSceneItem,
	matrix4 *transform, matrix4 *inv_transform)
{
	matrix4_identity(transform);

	applyParentSceneItemTransform(sceneItem, parentSceneItem, transform);

	matrix4_inv(inv_transform, transform);
}

static void getSceneItemBoxTransformMatrices(obs_sceneitem_t *sceneItem,
					     obs_sceneitem_t *parentSceneItem,
					     matrix4 *transform,
					     matrix4 *inv_transform)
{
	obs_sceneitem_get_box_transform(sceneItem, transform);

	applyParentSceneItemTransform(sceneItem, parentSceneItem, transform);

	matrix4_inv(inv_transform, transform);
}

static vec2 getSceneItemFinalScale(obs_sceneitem_t *sceneItem,
				   obs_sceneitem_t *parentSceneItem)
{
	obs_transform_info info;
	obs_sceneitem_get_info(sceneItem, &info);

	if (parentSceneItem) {
		obs_transform_info parentInfo;
		obs_sceneitem_get_info(parentSceneItem, &parentInfo);

		info.scale.x *= parentInfo.scale.x;
		info.scale.y *= parentInfo.scale.y;
	}

	return info.scale;
}

static vec2 getSceneItemFinalBoxScale(obs_sceneitem_t *sceneItem,
				      obs_sceneitem_t *parentSceneItem)
{
	matrix4 transform, inv_transform;
	getSceneItemBoxTransformMatrices(sceneItem, parentSceneItem, &transform,
					 &inv_transform);

	vec2 scale;
	vec2_set(&scale, transform.x.x, transform.y.y);

	return scale;
}

static vec3 getTransformedPosition(float x, float y, const matrix4 &mat)
{
	vec3 result;
	vec3_set(&result, x, y, 0.0f);
	vec3_transform(&result, &result, &mat);
	return result;
}
