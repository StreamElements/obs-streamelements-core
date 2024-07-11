#include "StreamElementsVideoCompositionViewWidget.hpp"
#include <QWindow>
#include <QScreen>

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

	m_videoCompositionInfo = m_videoComposition->GetCompositionInfo(this);

	auto windowVisible = [this](bool visible) {
		if (!visible) {
#if !defined(_WIN32) && !defined(__APPLE__)
			display = nullptr;
#endif
			return;
		}

		if (!m_display) {
			CreateDisplay();
		} else {
			QSize size = GetPixelSize(this);
			obs_display_resize(m_display, size.width(),
					   size.height());
		}
	};

	auto screenChanged = [this](QScreen *) {
		CreateDisplay();

		if (!m_display)
			return;

		QSize size = GetPixelSize(this);
		obs_display_resize(m_display, size.width(), size.height());
	};

	connect(windowHandle(), &QWindow::visibleChanged, windowVisible);
	connect(windowHandle(), &QWindow::screenChanged, screenChanged);

#ifdef ENABLE_WAYLAND
	if (obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND)
		windowHandle()->installEventFilter(
			new SurfaceEventFilter(this));
#endif

	CreateDisplay();
}

StreamElementsVideoCompositionViewWidget::
~StreamElementsVideoCompositionViewWidget()
{
	if (m_display) {
		obs_display_remove_draw_callback(
			m_display, obs_display_draw_callback, this);

		obs_display_destroy(m_display);

		m_display = nullptr;
	}

	m_videoCompositionInfo = nullptr;
}

void StreamElementsVideoCompositionViewWidget::CreateDisplay()
{
	if (m_display)
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

	m_display = obs_display_create(&info, 0L);

	obs_display_add_draw_callback(m_display, obs_display_draw_callback, this);
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

void StreamElementsVideoCompositionViewWidget::obs_display_draw_callback(void* data, uint32_t viewportWidth,
	uint32_t viewportHeight)
{
	StreamElementsVideoCompositionViewWidget *window =
		reinterpret_cast<StreamElementsVideoCompositionViewWidget *>(
			data);

	uint32_t sourceWidth;
	uint32_t sourceHeight;
	int viewX, viewY;
	int viewWidth, viewHeight;
	float scale;

	auto video = window->m_videoCompositionInfo->GetVideo();
	auto ovi = video_output_get_info(video);

	sourceWidth = ovi->width;
	sourceHeight = ovi->height;

	GetScaleAndViewPos(sourceWidth, sourceHeight, viewportWidth, viewportHeight, viewX, viewY, scale);

	viewWidth = int(scale * float(sourceWidth));
	viewHeight = int(scale * float(sourceHeight));

	startProjectionRegion(viewX, viewY, viewWidth, viewHeight, 0.0f, float(sourceWidth), 0.0f,
		    float(sourceHeight));

	// Render the view into the region set above
	window->m_videoCompositionInfo->Render();

	endProjectionRegion();
}
