#include "CadView.hpp"

#include <cmath>

#include <QWheelEvent>
#include <QDebug>





CadView::CadView(QWidget * aParentWidget):
	Super(aParentWidget),
	mZoomSpeed(DEFAULT_ZOOM_SPEED)
{
	// Disable scrollbars:
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	// Disable zoom anchor (we're providing our own):
	setTransformationAnchor(QGraphicsView::NoAnchor);

	auto min = std::numeric_limits<qint32>::min();
	auto span = std::numeric_limits<quint32>::max();
	setSceneRect(QRectF(min, min, span, span));
}





void CadView::zoomTo(QRectF aRect)
{
	if (!aRect.isValid())
	{
		qDebug() << "Cannot zoomTo() an invalid rect";
		return;
	}
	QTransform transform;
	auto factor = viewport()->width() / aRect.width();
	auto factor2 = viewport()->height() / aRect.height();
	factor = std::min(factor, factor2);
	transform.scale(factor, factor);
	auto center = -aRect.topLeft();  // TODO: Center the image
	transform.translate(center.x(), center.y());
	setTransform(transform);
}





void CadView::wheelEvent(QWheelEvent * aEvent)
{
	auto angle = aEvent->angleDelta().y() / 120.0;
	auto factor = std::pow(mZoomSpeed, angle);
	auto scale = transform().m22() * factor;
	auto dx = transform().dx() * factor;
	auto dy = transform().dy() * factor;
	auto mousePos = aEvent->position();
	// Adjust dx and dy by the current mouse position:
	dx += mousePos.x() * (1 - factor);
	dy += mousePos.y() * (1 - factor);
	setTransform(QTransform(scale, 0, 0, scale, dx, dy));
	aEvent->setAccepted(true);
	Q_EMIT mouseMoved(mapToScene(mousePos.toPoint()));
}





QSize CadView::sizeHint() const
{
	// Disable size-by-content implemented in QGraphicsView
	return QAbstractScrollArea::sizeHint();
}





void CadView::mouseMoveEvent(QMouseEvent * aEvent)
{
	if (aEvent->buttons() & Qt::MiddleButton)
	{
		// Pan the view:
		auto scale = transform().m22();
		auto dx = (aEvent->pos().x() - mMousePanLastPos.x()) / scale;
		auto dy = (aEvent->pos().y() - mMousePanLastPos.y()) / scale;
		setTransform(transform().translate(dx, dy));
		mMousePanLastPos = aEvent->pos();
	}
	Q_EMIT mouseMoved(mapToScene(aEvent->pos()));
}





void CadView::mousePressEvent(QMouseEvent * aEvent)
{
	if (aEvent->button() != Qt::MiddleButton)
	{
		return;
	}
	// Prepare for panning the view:
	mMousePanLastPos = aEvent->pos();
}





void CadView::mouseReleaseEvent(QMouseEvent * aEvent)
{
	// Finish any pending drag-operation by processing the final mousemove:
	mouseMoveEvent(aEvent);
}
