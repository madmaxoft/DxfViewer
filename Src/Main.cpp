// Main.cpp

// Implements the main entrypoint to the app

#include <memory>

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include <QMessageBox>

#include "Ui/DxfWindow.hpp"





// fwd:
namespace Dxf
{
	class Drawing;
}

Q_DECLARE_METATYPE(std::shared_ptr<Dxf::Drawing>);





/** Loads translation for the specified locale from all reasonable locations.
Returns true if successful, false on failure.
Tries: resources, <curdir>/translations, <exepath>/translations. */
static bool tryLoadTranslation(QTranslator & aTranslator, const QLocale & aLocale)
{
    static const QString exePath = QCoreApplication::applicationDirPath();

    if (aTranslator.load("DxfViewer_" + aLocale.name(), ":/translations"))
    {
        qDebug() << "Loaded translation " << aLocale.name() << " from resources";
        return true;
    }
    if (aTranslator.load("DxfViewer_" + aLocale.name(), "translations"))
    {
        qDebug() << "Loaded translation " << aLocale.name() << " from current folder";
        return true;
    }
    if (aTranslator.load("DxfViewer_" + aLocale.name(), exePath + "/translations"))
    {
        qDebug() << "Loaded translation " << aLocale.name() << " from exe folder";
        return true;
    }
    return false;
}





/** Initializes the translations, by loading the appropriate translation. */
void initTranslations(QApplication & aApp)
{
    auto translator = std::make_unique<QTranslator>();
    auto locale = QLocale::system();
    if (!tryLoadTranslation(*translator, locale))
    {
        qWarning() << "Could not load translations for locale " << locale.name() << ", trying all UI languages " << locale.uiLanguages();
        if (!translator->load(locale, "DxfViewer", "_", "translations"))
        {
            qWarning() << "Could not load translations for " << locale;
            return;
        }
    }
    qDebug() << "Translator isEmpty: " << translator->isEmpty();
    aApp.installTranslator(translator.release());
}





int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	qRegisterMetaType<std::shared_ptr<Dxf::Drawing>>();

	try
	{
		// Initialize translations:
		initTranslations(app);

		// Show the UI:
		DxfWindow mainWindow;
		if (argc > 1)
		{
			mainWindow.openFile(argv[1]);
		}
		mainWindow.showMaximized();

		// Run the app:
		auto res = app.exec();

		return res;
	}
	catch (const std::exception & exc)
	{
		QMessageBox::warning(
			nullptr,
			QApplication::tr("DxfViewer: Fatal error"),
			QApplication::tr("DxfViewer has detected a fatal error:\n\n%1").arg(exc.what())
		);
		return -1;
	}
	catch (...)
	{
		QMessageBox::warning(
			nullptr,
			QApplication::tr("DxfViewer: Fatal error"),
			QApplication::tr("DxfViewer has detected an unknown fatal error. Use a debugger to view detailed runtime log.")
		);
		return -1;
	}
}
