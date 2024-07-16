#pragma once

#include "StreamElementsVideoCompositionManager.hpp"
#include <QWidget>

class StreamElementsVideoCompositionViewWidget : public QWidget, public StreamElementsCompositionEventListener
{
private:
	std::shared_ptr<StreamElementsVideoCompositionBase> m_videoComposition;
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo> m_videoCompositionInfo;

	obs_display_t *m_display;

public:
	StreamElementsVideoCompositionViewWidget(
		QWidget *parent,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition);
	~StreamElementsVideoCompositionViewWidget();

private:
	void CreateDisplay();

	void OnMove();
	void OnDisplayChange();

protected:
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void moveEvent(QMoveEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual QPaintEngine *paintEngine() const override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	virtual bool nativeEvent(const QByteArray &, void *message,
				 qintptr *) override;
#else
	virtual bool nativeEvent(const QByteArray &, void *message,
				 long *) override;
#endif

protected:
	virtual void mouseMoveEvent(QMouseEvent *event) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;

private:
	static void obs_display_draw_callback(void *data, uint32_t cx,
					      uint32_t cy);
};
