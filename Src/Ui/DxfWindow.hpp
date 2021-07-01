#pragma once

#include <QMainWindow>
#include <QGraphicsScene>





// fwd: DxfDrawing.h
namespace Dxf
{
	class Drawing;
};





// fwd:
class QLabel;
namespace Ui
{
	class DxfWindow;
}





/** The main app window. */
class DxfWindow:
	public QMainWindow
{
	using Super = QMainWindow;

	Q_OBJECT


public:

	explicit DxfWindow(QWidget * aParent = nullptr);
	~DxfWindow();


public slots:

	/** Lets the user choose a file, then attempts to open it. */
	void selectAndOpenFile();

	/** Opens the specified file, replacing the current contents with the newly loaded drawing. */
	void openFile(const QString & aFileName);

	/** Updates the position labels in the statusbar, based on the mouse pos reported in the scene coords. */
	void updatePosLabels(QPointF aSceneMousePos);

	/** Resets the zoom on the drawing to view everything. */
	void zoomAll();


protected slots:

	/** Replaces the currently displayed drawing with the specified one.
	Refreshes the drawing, the layer list and clears the selection / property view.
	Used when loading the data from a file. Receives the data from a background thread. */
	void setCurrentDrawing(std::shared_ptr<Dxf::Drawing> aNewDrawing);

	/** Displays an error message about being unable to load the data from the specified file.
	Called from the background parsing thread. */
	void openFileFailed(const QString & aFileName, const QString & aMessage);


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DxfWindow> mUI;

	/** The statusbar labels for the current mouse position. */
	QLabel * mLblPosX;
	QLabel * mLblPosY;

	/** The currently displayed drawing. */
	std::shared_ptr<Dxf::Drawing> mCurrentDrawing;

	/** The scene containing all the Dxf primitives being displayed. */
	QGraphicsScene mScene;
};
