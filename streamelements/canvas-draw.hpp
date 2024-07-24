#pragma once

#include <obs.h>
#include <graphics/matrix4.h>

#include "canvas-math.hpp"

//
// Calculate video viewport position and size based on world width & height, and
// viewport width & height constraints.
//
// Adds mandatory padding, so video will not occupy 100% of either X or Y axis
// of the viewport.
//
// The output of this function can be used as input to startProjectionRegion()
// output position and dimensions.
//
static inline void calculateVideoViewportPositionAndSize(
	StreamElementsVideoCompositionViewWidget *self, double worldWidth,
	double worldHeight, double unpaddedViewportWidth, double unpaddedViewportHeight,
	double *viewX, double *viewY, double *viewWidth, double *viewHeight)
{
	double pixelDensity = self->devicePixelRatioF();

	double paddingX = 20.0f * pixelDensity;
	double paddingY = 20.0f * pixelDensity;

	double viewportWidth = unpaddedViewportWidth - (paddingX * 2.0f);
	double viewportHeight = unpaddedViewportHeight - (paddingY * 2.0f);

	*viewWidth = worldWidth;
	*viewHeight = worldHeight;

	if (*viewWidth > viewportWidth) {
		double ratio = viewportWidth / (*viewWidth);

		*viewWidth *= ratio;
		*viewHeight *= ratio;
	}

	if (*viewHeight > viewportHeight) {
		double ratio = viewportHeight / (*viewHeight);

		*viewWidth *= ratio;
		*viewHeight *= ratio;
	}

	*viewWidth = std::floor(*viewWidth);
	*viewHeight = std::floor(*viewHeight);

	*viewX = std::floor((unpaddedViewportWidth / 2.0f) - (*viewWidth / 2.0f));
	*viewY = std::floor((unpaddedViewportHeight / 2.0f) - (*viewHeight / 2.0f));
}

//
// Define a projection region for drawing a view texture.
//
// The projection region maps a rectangle of the view source dimensions onto
// a rectangle on the destination display viewport.
//
// That way when obs_render_main_texture() or obs_view_render() are called,
// they can just draw the source scene/transition and not bother themselves
// with scaling and projection onto the target surface at the correct
// coordinates: everything is predetermined.
//
static inline void startProjectionRegion(int vX, int vY, int vCX, int vCY,
					 float oL, float oT, float oR, float oB)
{
	gs_projection_push();
	gs_viewport_push();
	gs_set_viewport(vX, vY, vCX, vCY);

	// We're projecting source-view-sized rectangle to the viewport rectangle set above
	gs_ortho(oL, oR, oT, oB, -100.0f, 100.0f);
}

//
// Restore projection to whatever state it was before call to startProjectionRegion()
// above.
//
static inline void endProjectionRegion()
{
	gs_viewport_pop();
	gs_projection_pop();
}

//
// Transform QColor to format suitable for use with gs_XXX functions
//
static inline void colorToVec4(QColor color, vec4 *vec) {
	vec4_set(vec, color.redF(), color.greenF(),
		 color.blueF(), color.alphaF());
}

static inline void fillRect(float x1, float y1, float x2, float y2, QColor color)
{
	//x1 = std::round(x1);
	//x2 = std::round(x2);
	//y1 = std::round(y1);
	//y2 = std::round(y2);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *colParam = gs_effect_get_param_by_name(gs_get_effect(), "color");

	vec4 colorVec4;
	colorToVec4(color, &colorVec4);

	gs_matrix_push();
	//gs_matrix_identity();
	
	gs_matrix_translate3f(x1, y1, 0.0f); // Start top/left position
	gs_matrix_scale3f(x2 - x1, y2 - y1, 1.0f); // Width/height

	gs_effect_set_vec4(colParam, &colorVec4);

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f);
	auto vertexBuffer = gs_render_save();

	gs_load_vertexbuffer(vertexBuffer); // tl, tr, br, bl
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(vertexBuffer);

	gs_matrix_pop();

	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static inline void drawRect(float x1, float y1, float x2, float y2,
			    float thickness, QColor color)
{
	x1 = std::round(x1);
	x2 = std::round(x2);
	y1 = std::round(y1);
	y2 = std::round(y2);

	fillRect(x1, y1, x2, y1 + thickness, color);
	fillRect(x1, y2 - thickness, x2, y2, color);
	fillRect(x1, y1 + thickness, x1 + thickness, y2 - thickness, color);
	fillRect(x2 - thickness, y1 + thickness, x2, y2 - thickness, color);
}

static void drawStripedLine(float x1, float y1, float x2, float y2,
			    float thickness, vec2 scale, QColor color)
{
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *colParam =
		gs_effect_get_param_by_name(gs_get_effect(), "color");

	vec4 colorVec4;
	colorToVec4(color, &colorVec4);

	gs_effect_set_vec4(colParam, &colorVec4);

	float ySide = (y1 == y2) ? (y1 < 0.5f ? 1.0f : -1.0f) : 0.0f;
	float xSide = (x1 == x2) ? (x1 < 0.5f ? 1.0f : -1.0f) : 0.0f;

	float dist =
		sqrt(pow((x1 - x2) * scale.x, 2) + pow((y1 - y2) * scale.y, 2));
	float offX = (x2 - x1) / dist;
	float offY = (y2 - y1) / dist;

	for (int i = 0, l = ceil(dist / 15); i < l; i++) {
		gs_render_start(true);

		float xx1 = x1 + i * 15 * offX;
		float yy1 = y1 + i * 15 * offY;

		float dx;
		float dy;

		if (x1 < x2) {
			dx = std::min(xx1 + 7.5f * offX, x2);
		} else {
			dx = std::max(xx1 + 7.5f * offX, x2);
		}

		if (y1 < y2) {
			dy = std::min(yy1 + 7.5f * offY, y2);
		} else {
			dy = std::max(yy1 + 7.5f * offY, y2);
		}

		gs_vertex2f(xx1, yy1);
		gs_vertex2f(xx1 + (xSide * (thickness / scale.x)),
			    yy1 + (ySide * (thickness / scale.y)));
		gs_vertex2f(dx, dy);
		gs_vertex2f(dx + (xSide * (thickness / scale.x)),
			    dy + (ySide * (thickness / scale.y)));

		gs_vertbuffer_t *line = gs_render_save();

		gs_load_vertexbuffer(line);
		gs_draw(GS_TRISTRIP, 0, 0);
		gs_vertexbuffer_destroy(line);
	}

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static inline void drawLine(float x1, float y1, float x2, float y2, float thickness,
		     vec2 scale, QColor color)
{
	float ySide = (y1 == y2) ? (y1 < 0.5f ? 1.0f : -1.0f) : 0.0f;
	float xSide = (x1 == x2) ? (x1 < 0.5f ? 1.0f : -1.0f) : 0.0f;

	gs_render_start(true);

	gs_vertex2f(x1, y1);
	gs_vertex2f(x1 + (xSide * (thickness / scale.x)),
		    y1 + (ySide * (thickness / scale.y)));
	gs_vertex2f(x2 + (xSide * (thickness / scale.x)),
		    y2 + (ySide * (thickness / scale.y)));
	gs_vertex2f(x2, y2);
	gs_vertex2f(x1, y1);

	gs_vertbuffer_t *line = gs_render_save();

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *colParam =
		gs_effect_get_param_by_name(gs_get_effect(), "color");

	vec4 colorVec4;
	colorToVec4(color, &colorVec4);

	gs_effect_set_vec4(colParam, &colorVec4);

	gs_load_vertexbuffer(line);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(line);
	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static inline void drawLine(float x1, float y1, float x2, float y2,
			    float thickness,
			    QColor color)
{
	x1 = std::round(x1);
	x2 = std::round(x2);
	y1 = std::round(y1);
	y2 = std::round(y2);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *colParam =
		gs_effect_get_param_by_name(gs_get_effect(), "color");

	vec4 colorVec4;
	colorToVec4(color, &colorVec4);

	gs_effect_set_vec4(colParam, &colorVec4);

	gs_matrix_push();
	//gs_matrix_identity();

	gs_render_start(true);
	gs_vertex2f(x1 - (thickness / 2.0f), y1 - (thickness / 2.0f));
	gs_vertex2f(x1 + thickness, y1 + thickness);
	gs_vertex2f(x2 + thickness, y2 + thickness);
	gs_vertex2f(x2, y2);
	gs_vertex2f(x1, y1);
	auto vertexBuffer = gs_render_save();

	gs_load_vertexbuffer(vertexBuffer);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(vertexBuffer);

	gs_matrix_pop();

	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}
