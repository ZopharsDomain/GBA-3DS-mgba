#include "Window.h"

#include <QFileDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QStackedLayout>

#include "GameController.h"
#include "GDBWindow.h"
#include "GDBController.h"
#include "LoadSaveState.h"
#include "LogView.h"

using namespace QGBA;

Window::Window(QWidget* parent)
	: QMainWindow(parent)
	, m_logView(new LogView())
	, m_stateWindow(nullptr)
	, m_screenWidget(new QWidget())
#ifdef USE_GDB_STUB
	, m_gdbController(nullptr)
#endif
{
	m_controller = new GameController(this);

	QGLFormat format(QGLFormat(QGL::Rgba | QGL::DoubleBuffer));
	format.setSwapInterval(1);
	m_screenWidget->setLayout(new QStackedLayout());
	m_screenWidget->layout()->setContentsMargins(0, 0, 0, 0);
	setCentralWidget(m_screenWidget);
	m_display = new Display(format);
	attachWidget(m_display);
	connect(m_controller, SIGNAL(gameStarted(GBAThread*)), this, SLOT(gameStarted(GBAThread*)));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_display, SLOT(stopDrawing()));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(gameStopped()));
	connect(m_controller, SIGNAL(stateLoaded(GBAThread*)), m_display, SLOT(forceDraw()));
	connect(m_controller, SIGNAL(postLog(int, const QString&)), m_logView, SLOT(postLog(int, const QString&)));
	connect(this, SIGNAL(startDrawing(const uint32_t*, GBAThread*)), m_display, SLOT(startDrawing(const uint32_t*, GBAThread*)), Qt::QueuedConnection);
	connect(this, SIGNAL(shutdown()), m_display, SLOT(stopDrawing()));
	connect(this, SIGNAL(shutdown()), m_controller, SLOT(closeGame()));
	connect(this, SIGNAL(shutdown()), m_logView, SLOT(hide()));
	connect(this, SIGNAL(audioBufferSamplesChanged(int)), m_controller, SLOT(setAudioBufferSamples(int)));
	connect(this, SIGNAL(fpsTargetChanged(float)), m_controller, SLOT(setFPSTarget(float)));

	setupMenu(menuBar());
}

Window::~Window() {
	delete m_logView;
}

GBAKey Window::mapKey(int qtKey) {
	switch (qtKey) {
	case Qt::Key_Z:
		return GBA_KEY_A;
		break;
	case Qt::Key_X:
		return GBA_KEY_B;
		break;
	case Qt::Key_A:
		return GBA_KEY_L;
		break;
	case Qt::Key_S:
		return GBA_KEY_R;
		break;
	case Qt::Key_Return:
		return GBA_KEY_START;
		break;
	case Qt::Key_Backspace:
		return GBA_KEY_SELECT;
		break;
	case Qt::Key_Up:
		return GBA_KEY_UP;
		break;
	case Qt::Key_Down:
		return GBA_KEY_DOWN;
		break;
	case Qt::Key_Left:
		return GBA_KEY_LEFT;
		break;
	case Qt::Key_Right:
		return GBA_KEY_RIGHT;
		break;
	default:
		return GBA_KEY_NONE;
	}
}

void Window::selectROM() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select ROM"));
	if (!filename.isEmpty()) {
		m_controller->loadGame(filename);
	}
}

#ifdef USE_GDB_STUB
void Window::gdbOpen() {
	if (!m_gdbController) {
		m_gdbController = new GDBController(m_controller, this);
	}
	GDBWindow* window = new GDBWindow(m_gdbController);
	window->show();
}
#endif

void Window::keyPressEvent(QKeyEvent* event) {
	if (event->isAutoRepeat()) {
		QWidget::keyPressEvent(event);
		return;
	}
	GBAKey key = mapKey(event->key());
	if (key == GBA_KEY_NONE) {
		QWidget::keyPressEvent(event);
		return;
	}
	m_controller->keyPressed(key);
	event->accept();
}

void Window::keyReleaseEvent(QKeyEvent* event) {
	if (event->isAutoRepeat()) {
		QWidget::keyReleaseEvent(event);
		return;
	}
	GBAKey key = mapKey(event->key());
	if (key == GBA_KEY_NONE) {
		QWidget::keyPressEvent(event);
		return;
	}
	m_controller->keyReleased(key);
	event->accept();
}

void Window::closeEvent(QCloseEvent* event) {
	emit shutdown();
	QMainWindow::closeEvent(event);
}

void Window::toggleFullScreen() {
	if (isFullScreen()) {
		showNormal();
		setCursor(Qt::ArrowCursor);
	} else {
		showFullScreen();
		setCursor(Qt::BlankCursor);
	}
}

void Window::gameStarted(GBAThread* context) {
	emit startDrawing(m_controller->drawContext(), context);
	foreach (QAction* action, m_gameActions) {
		action->setDisabled(false);
	}
}

void Window::gameStopped() {
	foreach (QAction* action, m_gameActions) {
		action->setDisabled(true);
	}
}

void Window::openStateWindow(LoadSave ls) {
	if (m_stateWindow) {
		return;
	}
	bool wasPaused = m_controller->isPaused();
	m_stateWindow = new LoadSaveState(m_controller);
	connect(this, SIGNAL(shutdown()), m_stateWindow, SLOT(hide()));
	connect(m_stateWindow, &LoadSaveState::closed, [this]() {
		m_screenWidget->layout()->removeWidget(m_stateWindow);
		m_stateWindow = nullptr;
		setFocus();
	});
	if (!wasPaused) {
		m_controller->setPaused(true);
		connect(m_stateWindow, &LoadSaveState::closed, [this]() { m_controller->setPaused(false); });
	}
	m_stateWindow->setAttribute(Qt::WA_DeleteOnClose);
	m_stateWindow->setMode(ls);
	if (isFullScreen()) {
		attachWidget(m_stateWindow);
	} else {
		m_stateWindow->show();
	}
}

void Window::setupMenu(QMenuBar* menubar) {
	menubar->clear();
	QMenu* fileMenu = menubar->addMenu(tr("&File"));
	fileMenu->addAction(tr("Load &ROM..."), this, SLOT(selectROM()), QKeySequence::Open);

	QMenu* emulationMenu = menubar->addMenu(tr("&Emulation"));
	QAction* reset = new QAction(tr("&Reset"), emulationMenu);
	reset->setShortcut(tr("Ctrl+R"));
	connect(reset, SIGNAL(triggered()), m_controller, SLOT(reset()));
	m_gameActions.append(reset);
	emulationMenu->addAction(reset);

	QAction* shutdown = new QAction(tr("Sh&utdown"), emulationMenu);
	connect(shutdown, SIGNAL(triggered()), m_controller, SLOT(closeGame()));
	m_gameActions.append(shutdown);
	emulationMenu->addAction(shutdown);
	emulationMenu->addSeparator();

	QAction* loadState = new QAction(tr("&Load state"), emulationMenu);
	loadState->setShortcut(tr("Ctrl+L"));
	connect(loadState, &QAction::triggered, [this]() { this->openStateWindow(LoadSave::LOAD); });
	m_gameActions.append(loadState);
	emulationMenu->addAction(loadState);

	QAction* saveState = new QAction(tr("&Save state"), emulationMenu);
	saveState->setShortcut(tr("Ctrl+S"));
	connect(saveState, &QAction::triggered, [this]() { this->openStateWindow(LoadSave::SAVE); });
	m_gameActions.append(saveState);
	emulationMenu->addAction(saveState);

	QMenu* quickLoadMenu = emulationMenu->addMenu(tr("Quick load"));
	QMenu* quickSaveMenu = emulationMenu->addMenu(tr("Quick save"));
	int i;
	for (i = 1; i < 10; ++i) {
		QAction* quickLoad = new QAction(tr("State &%1").arg(i), quickLoadMenu);
		quickLoad->setShortcut(tr("F%1").arg(i));
		connect(quickLoad, &QAction::triggered, [this, i]() { m_controller->loadState(i); });
		m_gameActions.append(quickLoad);
		quickLoadMenu->addAction(quickLoad);

		QAction* quickSave = new QAction(tr("State &%1").arg(i), quickSaveMenu);
		quickSave->setShortcut(tr("Shift+F%1").arg(i));
		connect(quickSave, &QAction::triggered, [this, i]() { m_controller->saveState(i); });
		m_gameActions.append(quickSave);
		quickSaveMenu->addAction(quickSave);
	}

	emulationMenu->addSeparator();

	QAction* pause = new QAction(tr("&Pause"), emulationMenu);
	pause->setChecked(false);
	pause->setCheckable(true);
	pause->setShortcut(tr("Ctrl+P"));
	connect(pause, SIGNAL(triggered(bool)), m_controller, SLOT(setPaused(bool)));
	connect(m_controller, &GameController::gamePaused, [pause]() { pause->setChecked(true); });
	connect(m_controller, &GameController::gameUnpaused, [pause]() { pause->setChecked(false); });
	m_gameActions.append(pause);
	emulationMenu->addAction(pause);

	QAction* frameAdvance = new QAction(tr("&Next frame"), emulationMenu);
	frameAdvance->setShortcut(tr("Ctrl+N"));
	connect(frameAdvance, SIGNAL(triggered()), m_controller, SLOT(frameAdvance()));
	m_gameActions.append(frameAdvance);
	emulationMenu->addAction(frameAdvance);

	QMenu* target = emulationMenu->addMenu("FPS target");
	QAction* setTarget = new QAction(tr("15"), emulationMenu);
	connect(setTarget, &QAction::triggered, [this]() { emit fpsTargetChanged(15); });
	target->addAction(setTarget);
	setTarget = new QAction(tr("30"), emulationMenu);
	connect(setTarget, &QAction::triggered, [this]() { emit fpsTargetChanged(30); });
	target->addAction(setTarget);
	setTarget = new QAction(tr("45"), emulationMenu);
	connect(setTarget, &QAction::triggered, [this]() { emit fpsTargetChanged(45); });
	target->addAction(setTarget);
	setTarget = new QAction(tr("60"), emulationMenu);
	connect(setTarget, &QAction::triggered, [this]() { emit fpsTargetChanged(60); });
	target->addAction(setTarget);
	setTarget = new QAction(tr("90"), emulationMenu);
	connect(setTarget, &QAction::triggered, [this]() { emit fpsTargetChanged(90); });
	target->addAction(setTarget);
	setTarget = new QAction(tr("120"), emulationMenu);
	connect(setTarget, &QAction::triggered, [this]() { emit fpsTargetChanged(120); });
	target->addAction(setTarget);
	setTarget = new QAction(tr("240"), emulationMenu);
	connect(setTarget, &QAction::triggered, [this]() { emit fpsTargetChanged(240); });
	target->addAction(setTarget);

	QMenu* videoMenu = menubar->addMenu(tr("&Video"));
	QMenu* frameMenu = videoMenu->addMenu(tr("Frame &size"));
	QAction* setSize = new QAction(tr("1x"), videoMenu);
	connect(setSize, &QAction::triggered, [this]() {
		showNormal();
		resize(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	});
	frameMenu->addAction(setSize);
	setSize = new QAction(tr("2x"), videoMenu);
	connect(setSize, &QAction::triggered, [this]() {
		showNormal();
		resize(VIDEO_HORIZONTAL_PIXELS * 2, VIDEO_VERTICAL_PIXELS * 2);
	});
	frameMenu->addAction(setSize);
	setSize = new QAction(tr("3x"), videoMenu);
	connect(setSize, &QAction::triggered, [this]() {
		showNormal();
		resize(VIDEO_HORIZONTAL_PIXELS * 3, VIDEO_VERTICAL_PIXELS * 3);
	});
	frameMenu->addAction(setSize);
	setSize = new QAction(tr("4x"), videoMenu);
	connect(setSize, &QAction::triggered, [this]() {
		showNormal();
		resize(VIDEO_HORIZONTAL_PIXELS * 4, VIDEO_VERTICAL_PIXELS * 4);
	});
	frameMenu->addAction(setSize);
	frameMenu->addAction(tr("Fullscreen"), this, SLOT(toggleFullScreen()), QKeySequence("Ctrl+F"));

	QMenu* soundMenu = menubar->addMenu(tr("&Sound"));
	QMenu* buffersMenu = soundMenu->addMenu(tr("Buffer &size"));
	QAction* setBuffer = new QAction(tr("512"), buffersMenu);
	connect(setBuffer, &QAction::triggered, [this]() { emit audioBufferSamplesChanged(512); });
	buffersMenu->addAction(setBuffer);
	setBuffer = new QAction(tr("1024"), buffersMenu);
	connect(setBuffer, &QAction::triggered, [this]() { emit audioBufferSamplesChanged(1024); });
	buffersMenu->addAction(setBuffer);
	setBuffer = new QAction(tr("2048"), buffersMenu);
	connect(setBuffer, &QAction::triggered, [this]() { emit audioBufferSamplesChanged(2048); });
	buffersMenu->addAction(setBuffer);

	QMenu* debuggingMenu = menubar->addMenu(tr("&Debugging"));
	QAction* viewLogs = new QAction(tr("View &logs..."), debuggingMenu);
	connect(viewLogs, SIGNAL(triggered()), m_logView, SLOT(show()));
	debuggingMenu->addAction(viewLogs);
#ifdef USE_GDB_STUB
	QAction* gdbWindow = new QAction(tr("Start &GDB server..."), debuggingMenu);
	connect(gdbWindow, SIGNAL(triggered()), this, SLOT(gdbOpen()));
	debuggingMenu->addAction(gdbWindow);
#endif

	foreach (QAction* action, m_gameActions) {
		action->setDisabled(true);
	}
}

void Window::attachWidget(QWidget* widget) {
	m_screenWidget->layout()->addWidget(widget);
	static_cast<QStackedLayout*>(m_screenWidget->layout())->setCurrentWidget(widget);
}
