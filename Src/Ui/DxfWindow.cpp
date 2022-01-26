#include "DxfWindow.hpp"

#include <algorithm>
#include <cmath>

#include <QFileDialog>
#include <QMessageBox>
#include <QThreadPool>
#include <QLabel>
#include <QDebug>
#include <QGraphicsPathItem>

#include "ui_DxfWindow.h"
#include "DxfDrawing.hpp"
#include "DxfParser.hpp"





/** Converts angular degrees to radians. */
static double constexpr degToRad(double aDegrees)
{
	return (90 - aDegrees) * 3.141592 / 180;
}





/** Translates the DXF color constant into a QColor value.
Does NOT handle the BY_LAYER and BY_BLOCK colors. Use colorFromObj() instead for such cases. */
static QColor colorFromDxfColor(Dxf::Color aColor)
{
	if ((aColor < 0) || (aColor >= static_cast<int>(Dxf::gNumColors)))
	{
		return QColor::fromRgb(255, 0, 0);
	}
	auto bgr = Dxf::gColors[aColor];
	if (bgr == 0xffffff)  // Invert white into black (we've got white background, unlike CAD)
	{
		bgr = 0;
	}
	return QColor::fromRgb(
		((bgr & 0xff) << 16) |    // Red
		((bgr & 0xff00)) |        // Green
		((bgr & 0xff0000) >> 16)  // Blue
	);
}





/** Returns the color with which the specified primitive is to be drawn. */
static QColor colorFromObj(const Dxf::Primitive & aObj, const Dxf::Layer & aParentLayer, const Dxf::Block * aBlock = nullptr)
{
	switch (aObj.mColor)
	{
		case Dxf::COLOR_BYLAYER: return colorFromDxfColor(aParentLayer.defaultColor());
		case Dxf::COLOR_BYBLOCK:
		{
			if (aBlock == nullptr)
			{
				return QColor::fromRgb(255, 0, 0);
			}
			return colorFromDxfColor(aBlock->mColor);
		}
		default: return colorFromDxfColor(aObj.mColor);
	}
}





static QRectF rectFromExtent(const Dxf::Extent & aExtent)
{
	if (aExtent.isEmpty())
	{
		return QRectF();
	}
	QPointF topLeft(aExtent.minCoord().mX, aExtent.minCoord().mY);
	QPointF bottomRight(aExtent.maxCoord().mX, aExtent.maxCoord().mY);
	return QRectF(topLeft, bottomRight);
}





/** Draws an arc in a QGraphicsView. */
class ArcItem: public QGraphicsPathItem
{

	using Super = QGraphicsPathItem;


public:

	/** Constructs a QPainterPath containing the arc of the specified parameters. */
	static QPainterPath arcToPath(
		qreal aCenterX,
		qreal aCenterY,
		qreal aRadius,
		qreal aStartAngleDeg,
		qreal aEndAngleDeg
	)
	{
		if (aStartAngleDeg > aEndAngleDeg)
		{
			aEndAngleDeg += 360;
		}

		QPainterPath res;
		QRectF rect(aCenterX - aRadius, aCenterY - aRadius, 2 * aRadius, 2 * aRadius);
		res.arcMoveTo(rect, aStartAngleDeg);
		res.arcTo(rect, aStartAngleDeg, aEndAngleDeg - aStartAngleDeg);

		// DEBUG:
		auto x1 = aCenterX + std::sin(degToRad(aStartAngleDeg)) * aRadius;
		auto y1 = aCenterY - std::cos(degToRad(aStartAngleDeg)) * aRadius;
		auto x2 = aCenterX + std::sin(degToRad(aEndAngleDeg))   * aRadius;
		auto y2 = aCenterY - std::cos(degToRad(aEndAngleDeg))   * aRadius;
		res.moveTo(x1, y1);
		res.lineTo(x2, y2);

		return res;
	}


	ArcItem(
		qreal aCenterX,
		qreal aCenterY,
		qreal aRadius,
		qreal aStartAngleDeg,
		qreal aEndAngleDeg,
		QPen && aPen
	):
		Super(arcToPath(aCenterX, aCenterY, aRadius, aStartAngleDeg, aEndAngleDeg))
	{
		setPen(std::move(aPen));
		setBrush(QBrush(Qt::NoBrush));
	}
};





////////////////////////////////////////////////////////////////////////////////
// DxfWindow:

DxfWindow::DxfWindow(QWidget * aParent):
	Super(aParent),
	mUI(new Ui::DxfWindow),
	mCurrentDrawing(new Dxf::Drawing)
{
	mUI->setupUi(this);
	mUI->cvDrawing->setScene(&mScene);
	mLblPosX = new QLabel(mUI->statusBar);
	mLblPosY = new QLabel(mUI->statusBar);
	mUI->statusBar->addWidget(mLblPosX);
	mUI->statusBar->addWidget(mLblPosY);

	// Connect the signals / slots:
	connect(mUI->mActExit,     &QAction::triggered,  this, &DxfWindow::close);
	connect(mUI->mActFileOpen, &QAction::triggered,  this, &DxfWindow::selectAndOpenFile);
	connect(mUI->mActZoomAll,  &QAction::triggered,  this, &DxfWindow::zoomAll);
	connect(mUI->cvDrawing,    &CadView::mouseMoved, this, &DxfWindow::updatePosLabels);
}





DxfWindow::~DxfWindow()
{
	// Nothing explicit needed
}





void DxfWindow::selectAndOpenFile()
{
	QString filter("DXF files (*.dxf);;All files (*.*)");
	auto fileName = QFileDialog::getOpenFileName(this, tr("Open a drawing"), QString(), filter);
	if (fileName.isEmpty())
	{
		return;
	}
	openFile(fileName);
}





void DxfWindow::openFile(const QString & aFileName)
{
	QThreadPool::globalInstance()->start([aFileName, this]()
	{
		try
		{
			QFile f(aFileName);
			if (!f.open(QFile::ReadOnly | QFile::Text))
			{
				throw std::runtime_error(tr("Cannot read file %1.").arg(aFileName).toStdString());
				return;
			}
			auto dataSource = [&f](char * aDest, size_t aSize)
			{
				auto res = f.read(aDest, aSize);
				if (res < 0)
				{
					throw std::runtime_error("Reading the file failed");
				}
				return res;
			};
			auto drawing = Dxf::Parser::parse(std::move(dataSource));
			if (drawing == nullptr)
			{
				throw std::runtime_error(tr("Cannot parse file %1.").arg(aFileName).toStdString());
			}
			for (auto & layer: drawing->layers())
			{
				layer->updateExtent();
			}
			QMetaObject::invokeMethod(this, "setCurrentDrawing", Q_ARG(std::shared_ptr<Dxf::Drawing>, drawing));
		}
		catch (const Dxf::Parser::Error & exc)
		{
			QString message(tr("Error on line %1: %2").arg(exc.lineNumber()).arg(QString::fromStdString(exc.message())));
			QMetaObject::invokeMethod(this, "openFileFailed", Q_ARG(QString, aFileName), Q_ARG(QString, message));
		}
		catch (const std::exception & exc)
		{
			QMetaObject::invokeMethod(this, "openFileFailed", Q_ARG(QString, aFileName), Q_ARG(QString, exc.what()));
		}
		catch (...)
		{
			QMetaObject::invokeMethod(this, "openFileFailed", Q_ARG(QString, aFileName), Q_ARG(QString, "Unknown error"));
		}
	}
	);
}





void DxfWindow::updatePosLabels(QPointF aSceneMousePos)
{
	mLblPosX->setText(QString::number(aSceneMousePos.x(), 'f', 3));
	mLblPosY->setText(QString::number(aSceneMousePos.y(), 'f', 3));
}





void DxfWindow::zoomAll()
{
	Dxf::Extent extent;
	for (const auto & lay: mCurrentDrawing->layers())
	{
		extent.expandTo(lay->extent());
	}
	mUI->cvDrawing->zoomTo(rectFromExtent(extent));
}





void DxfWindow::setCurrentDrawing(std::shared_ptr<Dxf::Drawing> aNewDrawing)
{
	std::swap(mCurrentDrawing, aNewDrawing);
	mScene.clear();
	Dxf::Extent extent;
	for (const auto & lay: mCurrentDrawing->layers())
	{
		extent.expandTo(lay->extent());
		for (const auto & obj: lay->objects())
		{
			auto color = colorFromObj(*obj, *lay, nullptr);
			switch (obj->mObjectType)
			{
				case Dxf::otLine:
				{
					auto line = reinterpret_cast<Dxf::Line *>(obj.get());
					QPen pen(color);
					pen.setWidth(0);
					mScene.addLine(line->mPos.mX, -line->mPos.mY, line->mPos2.mX, -line->mPos2.mY, pen);
					break;
				}
				case Dxf::otPolyline:
				{
					auto polyline = reinterpret_cast<Dxf::Polyline *>(obj.get());
					auto num = polyline->mVertices.size();
					auto & vertex = polyline->mVertices;
					QPen pen(color);
					pen.setWidth(0);
					for (size_t i = 1; i < num; ++i)
					{
						mScene.addLine(vertex[i - 1].mPos.mX, -vertex[i - 1].mPos.mY, vertex[i].mPos.mX, -vertex[i].mPos.mY, pen);
					}
					break;
				}
				case Dxf::otCircle:
				{
					auto circle = reinterpret_cast<Dxf::Circle *>(obj.get());
					auto r = circle->mRadius;
					QPen pen(color);
					pen.setWidth(0);
					mScene.addEllipse(circle->mPos.mX - r, -circle->mPos.mY + r, 2 * r, 2 * r, pen);
					break;
				}
				case Dxf::otArc:
				{
					auto arc = reinterpret_cast<Dxf::Arc *>(obj.get());
					QPen pen("Red");
					pen.setWidth(0);
					mScene.addItem(new ArcItem(arc->mPos.mX, -arc->mPos.mY, arc->mRadius, arc->mStartAngle, arc->mEndAngle, std::move(pen)));
					break;
				}
				default:
				{
					qDebug() << "Unhandled DXF ObjectType: " << obj->mObjectType;
					break;
				}
				// TODO: More object types
			}
		}
	}
	mUI->cvDrawing->zoomTo(rectFromExtent(extent));
}





void DxfWindow::openFileFailed(const QString & aFileName, const QString & aMessage)
{
	QMessageBox::warning(this, tr("Failed to open file"), tr("Failed to open file %1\n%2").arg(aFileName, aMessage));
}
