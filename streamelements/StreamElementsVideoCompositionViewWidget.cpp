#include "graphics/matrix4.h"

#include "StreamElementsVideoCompositionViewWidget.hpp"
#include <QWindow>
#include <QScreen>
#include <QMouseEvent>
#include <QColor>

static void
scanGroupSceneItems(obs_sceneitem_t *group,
		    std::function<void(obs_sceneitem_t * /*item*/)> callback,
		    bool recursive)
{
	struct data_t {
		std::function<void(obs_sceneitem_t * /*item*/)> callback;
		bool recursive;
	};

	data_t data = {};
	data.callback = callback;
	data.recursive = recursive;

	obs_sceneitem_group_enum_items(
		group,
		[](obs_scene_t *scene, obs_sceneitem_t *item, void *data_p) {
			auto data = (data_t *)data_p;

			data->callback(item);

			if (data->recursive && obs_sceneitem_is_group(item)) {
				scanGroupSceneItems(item, data->callback, data->recursive);
			}

			return true;
		},
		&data);
}

static void scanSceneItems(
	obs_scene_t* scene,
	std::function<void(obs_sceneitem_t * /*item*/)> callback,
	bool recursive)
{
	struct data_t {
		std::function<void(obs_sceneitem_t * /*item*/)> callback;
		bool recursive;
	};

	data_t data = {};
	data.callback = callback;
	data.recursive = recursive;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t * /* scene */, obs_sceneitem_t *item,
		   void *data_p) {
			auto data = (data_t *)data_p;

			data->callback(item);

			if (data->recursive && obs_sceneitem_is_group(item)) {
				scanGroupSceneItems(item, data->callback,
						    data->recursive);
			}

			return true;
		},
		&data);
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
static inline void startProjectionRegion(int vX, int vY, int vCX, int vCY, float oL,
			       float oR, float oT, float oB)
{
	gs_projection_push();
	gs_viewport_push();
	gs_set_viewport(vX, vY, vCX, vCY);

	// We're projecting source-view-sized rectangle to the viewport rectangle set above
	gs_ortho(oL, oR, oT, oB, -100.0f, 100.0f);
}

static inline void endProjectionRegion()
{
	gs_viewport_pop();
	gs_projection_pop();
}

static inline void colorToVec4(QColor color, vec4 *vec) {
	vec4_set(vec, color.redF(), color.greenF(),
		 color.blueF(), color.alphaF());
}

static inline void fillRect(float x1, float y1, float x2, float y2, QColor color)
{
	x1 = std::round(x1);
	x2 = std::round(x2);
	y1 = std::round(y1);
	y2 = std::round(y2);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *colParam = gs_effect_get_param_by_name(gs_get_effect(), "color");

	vec4 colorVec4;
	colorToVec4(color, &colorVec4);

	gs_matrix_push();
	gs_matrix_identity();
	
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

	gs_matrix_push();
	gs_matrix_identity();

	//gs_matrix_translate3f(x1, y1, 0.0f);       // Start top/left position
	//gs_matrix_scale3f(x2 - x1, y2 - y1, 1.0f); // Width/height

	gs_effect_set_vec4(colParam, &colorVec4);

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

static vec3 getTransformedPosition(float x, float y, const matrix4 &mat)
{
	vec3 result;
	vec3_set(&result, x, y, 0.0f);
	vec3_transform(&result, &result, &mat);
	return result;
}

class ControlPoint {
public:
	obs_scene_t *m_scene;
	obs_sceneitem_t *m_sceneItem;

	double m_x;
	double m_y;
	double m_width;
	double m_height;

	bool m_modifyX;
	bool m_modifyY;
	bool m_modifyRotation;

	std::shared_ptr<ControlPoint> m_lineTo;

public:
	ControlPoint(obs_scene_t *scene, obs_sceneitem_t *sceneItem, double x,
		     double y, double width, double height, bool modifyX,
		     bool modifyY, bool modifyRotation,
		     std::shared_ptr<ControlPoint> lineTo = nullptr)
		: m_scene(scene),
		  m_sceneItem(sceneItem),
		  m_x(x),
		  m_y(y),
		  m_width(width),
		  m_height(height),
		  m_modifyX(modifyX),
		  m_modifyY(modifyY),
		  m_modifyRotation(modifyRotation),
		  m_lineTo(lineTo)
	{
		obs_sceneitem_addref(m_sceneItem);
	}

	~ControlPoint() { obs_sceneitem_release(m_sceneItem); }

	virtual void Draw()
	{
		matrix4 transform;
		obs_sceneitem_get_box_transform(m_sceneItem, &transform);

		/*
		auto parentSceneItem = obs_sceneitem_get_group(m_scene, m_sceneItem);
		if (parentSceneItem) {
			matrix4 parentTransform;
			obs_sceneitem_get_draw_transform(parentSceneItem,
							 &parentTransform);
			
			matrix4_mul(&transform, &transform, &parentTransform);
		}
		*/

		//auto pos = getTransformedPosition(m_x, m_y, transform);
		//auto pos = getTransformedPosition(0.0f, 0.0f, transform);
		//auto pos2 = getTransformedPosition(1.0f, 1.0f, transform);

		//drawRect(pos.x, pos.x, pos2.x, pos2.y, 20,
		//	 QColor(255, 0, 0, 255));

		obs_transform_info info;
		obs_sceneitem_get_info(m_sceneItem, &info);

		matrix4 projection;
		gs_matrix_get(&projection);

		gs_matrix_push();
		gs_matrix_mul(&transform); // this works with rotation = 0
		drawRect(info.pos.x, info.pos.y, info.pos.x + info.bounds.x, info.pos.y + info.bounds.y, 20, QColor(255, 0, 0, 255));
		gs_matrix_pop();

		// This behaves differently when rotated
		auto topLeft = getTransformedPosition(0.0f, 0.0f, transform);
		auto bottomRight =
			getTransformedPosition(1.0f, 1.0f, transform);

		drawRect(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y, 10,
			 QColor(0, 0, 255, 255));

		char buf[512];
		sprintf(buf, "topLeft: (%d x %d) | bottomRight: (%d x %d)\n",
			(int)topLeft.x, (int)topLeft.y, (int)bottomRight.x,
			(int)bottomRight.y);
		OutputDebugStringA(buf);
	}
};

std::vector<std::shared_ptr<ControlPoint>>
createSceneItemControlPoints(obs_scene_t* scene, obs_sceneitem_t *item)
{
	const float thickness = 20.0f;
	const float rotationDistance = 100.0f;

	obs_transform_info info;
	obs_sceneitem_get_info(item, &info);

	vec2 bounds;
	auto source = obs_sceneitem_get_source(item);
	vec2_set(&bounds, obs_source_get_width(source) * info.scale.x,
		 obs_source_get_height(source) * info.scale.y);

	std::vector<std::shared_ptr<ControlPoint>> result;

	// Top-left
	result.push_back(std::make_shared<ControlPoint>(
		scene, item, -thickness / 2.0f, -thickness / 2.0f,
		thickness / 2.0f, thickness / 2.0f, true, true, false));

	// Top-right
	result.push_back(std::make_shared<ControlPoint>(
		scene, item, bounds.x - thickness / 2.0f, -thickness / 2.0f,
		bounds.x + thickness / 2.0f, thickness / 2.0f, true, true,
		false));

	// Bottom-Left
	result.push_back(std::make_shared<ControlPoint>(
		scene, item, -thickness / 2.0f, bounds.y - thickness / 2.0f,
		thickness / 2.0f, bounds.y + thickness / 2.0f, true, true,
		false));

	// Bottom-Right
	result.push_back(std::make_shared<ControlPoint>(
		scene, item, bounds.x - thickness / 2.0f,
		bounds.y - thickness / 2.0f, bounds.x + thickness / 2.0f,
		bounds.y + thickness / 2.0f, true, true, false));

	// Top
	result.push_back(std::make_shared<ControlPoint>(
		scene, item, (bounds.x / 2.0f) - thickness / 2.0f,
		-thickness / 2.0f, (bounds.x / 2.0f) + thickness / 2.0f,
		thickness / 2.0f, false, true, false));

	// Bottom
	auto bottomPoint = std::make_shared<ControlPoint>(
		scene, item, (bounds.x / 2.0f) - thickness / 2.0f,
		bounds.y - thickness / 2.0f,
		(bounds.x / 2.0f) + thickness / 2.0f,
		bounds.y + thickness / 2.0f, false, true, false);

	result.push_back(bottomPoint);

	// Left
	result.push_back(std::make_shared<ControlPoint>(
		scene, item, -thickness / 2.0f,
		(bounds.y / 2.0f) - thickness / 2.0f, thickness / 2.0f,
		(bounds.y / 2.0f) + thickness / 2.0f, true, false, false));

	// Right
	result.push_back(std::make_shared<ControlPoint>(
		scene, item, bounds.x - thickness / 2.0f,
		(bounds.y / 2.0f) - thickness / 2.0f,
		bounds.x + thickness / 2.0f,
		(bounds.y / 2.0f) + thickness / 2.0f, true, false, false));

	// Rotation
	result.push_back(std::make_shared<ControlPoint>(
		scene, item, (bounds.x / 2.0f) - thickness / 2.0f,
		bounds.y + rotationDistance - thickness / 2.0f,
		(bounds.x / 2.0f) + thickness / 2.0f,
		bounds.y + rotationDistance + thickness / 2.0f, false, false,
		true, bottomPoint));

	return result;
}

std::vector<std::shared_ptr<ControlPoint>>
createSceneItemsControlPoints(obs_scene_t *scene)
{
	std::vector<std::shared_ptr<ControlPoint>> result;

	scanSceneItems(
		scene, [&](obs_sceneitem_t *item) {
			// TODO: check for selection

			for (auto point :
			     createSceneItemControlPoints(scene, item)) {
				result.push_back(point);
			}
		}, true);

	return result;
}

/*
static inline void GetScaleAndViewPos(int sourceWidth, int sourceHeight, int viewportWidth,
					int viewportHeight, int &viewX, int &viewY,
					float &scale)
{
	double windowAspect, baseAspect;
	int newCX, newCY;

	windowAspect = double(viewportWidth) / double(viewportHeight);
	baseAspect = double(sourceWidth) / double(sourceHeight);

	if (windowAspect > baseAspect) {
		scale = float(viewportHeight) / float(sourceHeight);
		newCX = int(double(viewportHeight) * baseAspect);
		newCY = viewportHeight;
	} else {
		scale = float(viewportWidth) / float(sourceWidth);
		newCX = viewportWidth;
		newCY = int(float(viewportWidth) / baseAspect);
	}

	viewX = viewportWidth / 2 - newCX / 2;
	viewY = viewportHeight / 2 - newCY / 2;
}
*/

static inline QSize GetPixelSize(QWidget *widget)
{
	return widget->size() * widget->devicePixelRatioF();
}

static bool QTToGSWindow(QWindow *window, gs_window &gswindow)
{
	bool success = true;

#ifdef _WIN32
	gswindow.hwnd = (HWND)window->winId();
#elif __APPLE__
	gswindow.view = (id)window->winId();
#else
	switch (obs_get_nix_platform()) {
	case OBS_NIX_PLATFORM_X11_EGL:
		gswindow.id = window->winId();
		gswindow.display = obs_get_nix_platform_display();
		break;
#ifdef ENABLE_WAYLAND
	case OBS_NIX_PLATFORM_WAYLAND: {
		QPlatformNativeInterface *native =
			QGuiApplication::platformNativeInterface();
		gswindow.display =
			native->nativeResourceForWindow("surface", window);
		success = gswindow.display != nullptr;
		break;
	}
#endif
	default:
		success = false;
		break;
	}
#endif
	return success;
}

StreamElementsVideoCompositionViewWidget::StreamElementsVideoCompositionViewWidget(
	QWidget* parent,
	std::shared_ptr<StreamElementsVideoCompositionBase> videoComposition)
	: m_videoComposition(videoComposition), QWidget(parent)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setMouseTracking(true);

	m_videoCompositionInfo = m_videoComposition->GetCompositionInfo(this);

	auto windowVisible = [this](bool visible) {
		if (!visible) {
#if !defined(_WIN32) && !defined(__APPLE__)
			//m_display = nullptr;
#endif
			return;
		}

		if (!m_display) {
			//CreateDisplay();
		} else {
			QSize size = GetPixelSize(this);
			obs_enter_graphics();
			obs_display_resize(m_display, size.width(),
					   size.height());
			obs_leave_graphics();
		}
	};

	auto screenChanged = [this](QScreen *) {
		//CreateDisplay();

		if (!m_display)
			return;

		QSize size = GetPixelSize(this);
		obs_enter_graphics();
		obs_display_resize(m_display, size.width(), size.height());
		obs_leave_graphics();
	};

	connect(windowHandle(), &QWindow::visibleChanged, windowVisible);
	connect(windowHandle(), &QWindow::screenChanged, screenChanged);

#ifdef ENABLE_WAYLAND
	if (obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND)
		windowHandle()->installEventFilter(
			new SurfaceEventFilter(this));
#endif

	//CreateDisplay();
}

StreamElementsVideoCompositionViewWidget::
~StreamElementsVideoCompositionViewWidget()
{
	if (m_display) {
		obs_enter_graphics();
		obs_display_remove_draw_callback(
			m_display, obs_display_draw_callback, this);

		obs_display_destroy(m_display);
		obs_leave_graphics();

		m_display = nullptr;
	}

	m_videoCompositionInfo = nullptr;
}

void StreamElementsVideoCompositionViewWidget::CreateDisplay()
{
	if (m_display)
		return;

	if (!isVisible())
		return;

	if (!windowHandle()->isExposed())
		return;

	QSize size = GetPixelSize(this);

	gs_init_data info = {};
	info.cx = size.width();
	info.cy = size.height();
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;

	if (!QTToGSWindow(windowHandle(), info.window))
		return;

	obs_enter_graphics();
	m_display = obs_display_create(&info, 0x008000L);

	obs_display_add_draw_callback(m_display, obs_display_draw_callback, this);
	obs_leave_graphics();
}

void StreamElementsVideoCompositionViewWidget::paintEvent(QPaintEvent *event)
{
	CreateDisplay();

	QWidget::paintEvent(event);
}

void StreamElementsVideoCompositionViewWidget::moveEvent(QMoveEvent *event)
{
	QWidget::moveEvent(event);

	OnMove();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool StreamElementsVideoCompositionViewWidget::nativeEvent(const QByteArray &,
							   void *message,
							   qintptr *)
#else
bool StreamElementsVideoCompositionViewWidget::nativeEvent(const QByteArray &,
							   void *message,
							   long *)
#endif
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	switch (msg.message) {
	case WM_DISPLAYCHANGE:
		OnDisplayChange();
	}
#else
	UNUSED_PARAMETER(message);
#endif

	return false;
}

void StreamElementsVideoCompositionViewWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	CreateDisplay();

	if (isVisible() && m_display) {
		QSize size = GetPixelSize(this);
		obs_display_resize(m_display, size.width(), size.height());
	}
}

QPaintEngine *StreamElementsVideoCompositionViewWidget::paintEngine() const
{
	return nullptr;
}

void StreamElementsVideoCompositionViewWidget::OnMove()
{
	if (m_display)
		obs_display_update_color_space(m_display);
}

void StreamElementsVideoCompositionViewWidget::OnDisplayChange()
{
	if (m_display)
		obs_display_update_color_space(m_display);
}

void StreamElementsVideoCompositionViewWidget::viewportToWorldCoords(
	uint32_t viewportX, uint32_t viewportY,
	uint32_t* worldX, uint32_t* worldY)
{
	auto displaySize = GetPixelSize(this);

	auto ovi = video_output_get_info(m_videoCompositionInfo->GetVideo());

	auto displayWidth = (uint32_t)displaySize.width();
	auto displayHeight = (uint32_t)displaySize.height();

	auto worldWidth = ovi->width;
	auto worldHeight = ovi->height;

	auto widthRatio = (double)displayWidth / (double)worldWidth;
	auto heightRatio = (double)displayHeight / (double)worldHeight;

	*worldX = (uint32_t)((double)viewportX / widthRatio);
	*worldY = (uint32_t)((double)viewportY / heightRatio);
}

void StreamElementsVideoCompositionViewWidget::mouseMoveEvent(QMouseEvent *event)
{
	QWidget::mouseMoveEvent(event);

	viewportToWorldCoords(event, &m_currMouseWorldX, &m_currMouseWorldY);
}

void StreamElementsVideoCompositionViewWidget::mousePressEvent(
	QMouseEvent *event)
{
	QWidget::mousePressEvent(event);

	viewportToWorldCoords(event, &m_currMouseWorldX, &m_currMouseWorldY);
}

void StreamElementsVideoCompositionViewWidget::mouseReleaseEvent(
	QMouseEvent *event)
{
	QWidget::mouseReleaseEvent(event);

	viewportToWorldCoords(event, &m_currMouseWorldX, &m_currMouseWorldY);
}

void StreamElementsVideoCompositionViewWidget::obs_display_draw_callback(void* data, uint32_t viewportWidth,
	uint32_t viewportHeight)
{
	//OutputDebugStringA("obs_display_draw_callback\n");
	StreamElementsVideoCompositionViewWidget *self =
		reinterpret_cast<StreamElementsVideoCompositionViewWidget *>(
			data);

	uint32_t worldWidth;
	uint32_t worldHeight;
	//int viewX, viewY;
	//int viewWidth, viewHeight;
	//float scale;

	auto video = self->m_videoCompositionInfo->GetVideo();
	auto ovi = video_output_get_info(video);

	// TODO: Figure out what to do with scaled output size: i.e. we probably want to get the video dimension info from the view, or maybe from another place
	worldWidth = ovi->width;
	worldHeight = ovi->height;

	//GetScaleAndViewPos(sourceWidth, sourceHeight, viewportWidth, viewportHeight, viewX, viewY, scale);

	//viewWidth = int(scale * float(sourceWidth));
	//viewHeight = int(scale * float(sourceHeight));

	//startProjectionRegion(viewX, viewY, viewWidth, viewHeight, 0.0f, float(sourceWidth), 0.0f,
	//	    float(sourceHeight));

	startProjectionRegion(0, 0, viewportWidth, viewportHeight, 0.0f,
			float(worldWidth), 0.0f, float(worldHeight));

	// Render the view into the region set above
	self->m_videoCompositionInfo->Render();

	if (self->m_currUnderMouse) {
		fillRect(self->m_currMouseWorldX, self->m_currMouseWorldY,
			 self->m_currMouseWorldX + 50,
			 self->m_currMouseWorldY + 50,
			 QColor(255, 255, 0, 175));

		drawRect(self->m_currMouseWorldX, self->m_currMouseWorldY,
			 self->m_currMouseWorldX + 50,
			 self->m_currMouseWorldY + 50, 5.0f,
			 QColor(255, 0, 0, 175));

		drawLine(0, 0, self->m_currMouseWorldX, self->m_currMouseWorldY,
			 5.0f, QColor(0, 255, 0, 175));

		drawLine(0, worldHeight, self->m_currMouseWorldX, self->m_currMouseWorldY,
			 5.0f, QColor(0, 255, 0, 175));

		drawLine(worldWidth, worldHeight, self->m_currMouseWorldX,
			 self->m_currMouseWorldY, 5.0f, QColor(0, 255, 0, 175));

		drawLine(worldWidth, 0, self->m_currMouseWorldX,
			 self->m_currMouseWorldY, 5.0f, QColor(0, 255, 0, 175));
	}

	auto controlPoints = createSceneItemsControlPoints(
	self->m_videoComposition->GetCurrentScene());


	/*
	std::string buf;
	for (auto controlPoint : controlPoints) {
		controlPoint->Draw();

		char buffer[512];
		sprintf(buffer, "(%d, %d, %d, %d)", (int)controlPoint->m_x,
			(int)controlPoint->m_y, (int)controlPoint->m_width,
			(int)controlPoint->m_height);

		buf += buffer;
		buf += "    ";
	}

	OutputDebugStringA(("controlPoints: " + buf + "\n").c_str());
	*/
	if (controlPoints.size() > 0)
		controlPoints[0]->Draw();

	endProjectionRegion();

	/*
	char buffer[512];
	sprintf(buffer, "under mouse: %s | world pos: %d x %d\n",
		self->m_currUnderMouse ? "Y" : "N", self->m_currMouseWorldX,
		self->m_currMouseWorldY);

	OutputDebugStringA(buffer);
	*/
}
