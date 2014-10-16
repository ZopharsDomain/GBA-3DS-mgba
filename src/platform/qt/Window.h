#ifndef QGBA_WINDOW
#define QGBA_WINDOW

#include <QAudioOutput>
#include <QMainWindow>

extern "C" {
#include "gba.h"
}

#include "Display.h"
#include "LoadSaveState.h"

namespace QGBA {

class GameController;
class GDBController;
class LogView;

class Window : public QMainWindow {
Q_OBJECT

public:
	Window(QWidget* parent = nullptr);
	virtual ~Window();

	static GBAKey mapKey(int qtKey);

signals:
	void startDrawing(const uint32_t*, GBAThread*);
	void shutdown();

public slots:
	void selectROM();

#ifdef USE_GDB_STUB
	void gdbOpen();
#endif

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;
	virtual void keyReleaseEvent(QKeyEvent* event) override;
	virtual void closeEvent(QCloseEvent*) override;

signals:
	void audioBufferSamplesChanged(int samples);
	void fpsTargetChanged(float target);

public slots:
	void toggleFullScreen();

private slots:
	void gameStarted(GBAThread*);
	void gameStopped();

private:
	void setupMenu(QMenuBar*);
	void openStateWindow(LoadSave);

	void attachWidget(QWidget* widget);

	GameController* m_controller;
	Display* m_display;
	QList<QAction*> m_gameActions;
	LogView* m_logView;
	LoadSaveState* m_stateWindow;
	QWidget* m_screenWidget;

#ifdef USE_GDB_STUB
	GDBController* m_gdbController;
#endif
};

}

#endif
