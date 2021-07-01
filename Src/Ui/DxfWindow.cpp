#include "DxfWindow.hpp"
#include <algorithm>
#include <QFileDialog>
#include <QMessageBox>
#include <QThreadPool>
#include <QLabel>
#include <QDebug>
#include "ui_DxfWindow.h"
#include "DxfDrawing.hpp"
#include "DxfParser.hpp"





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
					mScene.addLine(line->mPos.mX, line->mPos.mY, line->mPos2.mX, line->mPos2.mY, QPen(color));
					break;
				}
				case Dxf::otPolyline:
				{
					auto polyline = reinterpret_cast<Dxf::Polyline *>(obj.get());
					auto num = polyline->mVertices.size();
					auto & vertex = polyline->mVertices;
					for (size_t i = 1; i < num; ++i)
					{
						mScene.addLine(vertex[i - 1].mPos.mX, vertex[i - 1].mPos.mY, vertex[i].mPos.mX, vertex[i].mPos.mY, QPen(color));
					}
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
	QMessageBox::warning(this, tr("Failed to open file"), tr("Failed to open file %1\n%2").arg(aFileName).arg(aMessage));
}
