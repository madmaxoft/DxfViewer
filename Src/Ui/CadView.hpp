#pragma once

#include <QGraphicsView>





/** Wrapper for QGraphicsView that adjusts the behavior of the control to be more CAD-like:
	- mouse wheel zooming
	- middle-mouse panning
	- no scrollbars
	- no limits on the panning
	- no size adjustments based on the contents size.
The wrapper also provides a mouseMoved() signal for tracking the mouse position.
To use this, create an instance, set a QGraphicsScene into it and finally call zoomTo() for the scene's extents
to properly initialize the zoom. */
class CadView:
	public QGraphicsView
{
	Q_OBJECT

	using Super = QGraphicsView;


public:

	/** Creates a new instance of the class. */
	CadView(QWidget * aParentWidget = nullptr);

	/** Zooms the view so that the specified rectangle is just about visible (touches the edges either horizontally, or vertically, depending on the aspect ratio). */
	void zoomTo(QRectF aRect);


signals:

	/** Emitted when a mouse is moved within the space of this widget (and mouse-tracking is on). */
	void mouseMoved(QPointF aSceneMousePos);


protected:

	/** The value that mZoomSpeed is initialized to. */
	static constexpr qreal DEFAULT_ZOOM_SPEED = 1.2;

	/** The speed at which the zoom-by-wheel is performed.
	Should be a number slightly larger than 1, such as DEFAULT_ZOOM_SPEED (the default).
	Zooming multiplies or divides the current zoom by this value. */
	qreal mZoomSpeed;

	/** The last processed mouse position (in screen coords) while middle-mouse panning. */
	QPointF mMousePanLastPos;


	// QGraphicsView overrides:
	virtual void wheelEvent(QWheelEvent * aEvent) override;
	virtual QSize sizeHint() const override;
	virtual void mouseMoveEvent(QMouseEvent * aEvent) override;
	virtual void mousePressEvent(QMouseEvent * aEvent) override;
	virtual void mouseReleaseEvent(QMouseEvent * aEvent) override;
};
