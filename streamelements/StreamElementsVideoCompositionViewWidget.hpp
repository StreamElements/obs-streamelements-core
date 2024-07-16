#pragma once

#include "StreamElementsVideoCompositionManager.hpp"
#include <QWidget>
#include <QMouseEvent>
#include <QEnterEvent>

class StreamElementsVideoCompositionViewWidget : public QWidget, public StreamElementsCompositionEventListener
{
private:
	std::shared_ptr<StreamElementsVideoCompositionBase> m_videoComposition;
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo> m_videoCompositionInfo;

	obs_display_t *m_display = nullptr;

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
	uint32_t m_currMouseWorldX = 0;
	uint32_t m_currMouseWorldY = 0;
	bool m_currUnderMouse = false;

	void viewportToWorldCoords(uint32_t viewportX, uint32_t viewportY,
				   uint32_t *worldX, uint32_t *worldY);

	inline void viewportToWorldCoords(QPoint pos, uint32_t *worldX,
					  uint32_t *worldY)
	{
		viewportToWorldCoords(pos.x(), pos.y(), worldX, worldY);
	}

	inline void viewportToWorldCoords(QPointF pos, uint32_t *worldX,
					  uint32_t *worldY)
	{
		viewportToWorldCoords(pos.x(), pos.y(), worldX, worldY);
	}

	inline void viewportToWorldCoords(QMouseEvent *event, uint32_t *worldX,
					  uint32_t *worldY)
	{
		viewportToWorldCoords(event->localPos(), worldX, worldY);
	}

	virtual void mouseMoveEvent(QMouseEvent *event) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;

	virtual void enterEvent(QEnterEvent *event) override {
		viewportToWorldCoords(event->localPos(), &m_currMouseWorldX, &m_currMouseWorldY);

		m_currUnderMouse = true;
	}
	virtual void leaveEvent(QEvent *event) override { m_currUnderMouse = false; }

private:
	static void obs_display_draw_callback(void *data, uint32_t cx,
					      uint32_t cy);
};
