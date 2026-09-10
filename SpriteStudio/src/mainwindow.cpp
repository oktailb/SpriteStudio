#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QShortcut>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , frameModel(new ArrangementModel(this))
      , timerId(0)
      , ready(false)
      , extractor(nullptr)
      , animationTimer(new QTimer(this))
      , currentAnimationFrameIndex(0)
      , m_document(new SpriteDocument(this))
      , m_undoStack(new QUndoStack(this))
      , m_player(new AnimationPlayer(this))
{
  ui->setupUi(this);

  // Initialize extractor registry
  ExtractorRegistry::instance();

  // Create Edit Menu for Undo/Redo
  QMenu *editMenu = new QMenu(tr("&Edit"), this);
  menuBar()->insertMenu(ui->menuHelp->menuAction(), editMenu);
  QAction *undoAction = m_undoStack->createUndoAction(this, tr("&Undo"));
  undoAction->setShortcut(QKeySequence::Undo);
  editMenu->addAction(undoAction);

  QAction *redoAction = m_undoStack->createRedoAction(this, tr("&Redo"));
  redoAction->setShortcut(QKeySequence::Redo);
  editMenu->addAction(redoAction);

  // Standard File Shortcuts
  ui->actionOpen->setShortcut(QKeySequence::Open);
  ui->actionExport->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
  ui->actionExit->setShortcut(QKeySequence::Quit);

  // Animation and Navigation Shortcuts
  QShortcut *spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
  connect(spaceShortcut, &QShortcut::activated, this, [this]() {
      if (m_player->isPlaying()) {
          on_Pause_clicked();
      } else {
          on_Play_clicked();
      }
  });

  QShortcut *stepLeftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
  connect(stepLeftShortcut, &QShortcut::activated, m_player, &AnimationPlayer::stepBackward);

  QShortcut *stepRightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
  connect(stepRightShortcut, &QShortcut::activated, m_player, &AnimationPlayer::stepForward);

  QShortcut *framesListDeleteShortcut = new QShortcut(QKeySequence::Delete, ui->framesList);
  connect(framesListDeleteShortcut, &QShortcut::activated, this, &MainWindow::deleteSelectedFrame);

  // Connect Player
  connect(m_player, &AnimationPlayer::frameChanged, this, [this](int seqIdx, int /*globalIdx*/) {
      currentAnimationFrameIndex = seqIdx;
      updateAnimation();
  });
  connect(m_player, &AnimationPlayer::playbackStateChanged, this, [this](bool playing) {
      ui->Play->setVisible(!playing);
      ui->Pause->setVisible(playing);
  });

  // Connect Document signals to UI synchronization
  connect(m_document, &SpriteDocument::framesChanged, this, &MainWindow::syncFromDocument);
  connect(m_document, &SpriteDocument::animationsChanged, this, &MainWindow::syncAnimationListWidget);
  connect(m_document, &SpriteDocument::atlasChanged, this, [this]() {
      setupGraphicsView(m_document->atlas());
  });

  // Enable drag and drop events for the main window (to handle file drops).
  setAcceptDrops(true);
  // Start a low-frequency system timer (100ms interval) for general background checks/updates.
  timerId = startTimer(100);

  // Assign the custom model to the frames list view.
  ui->framesList->setModel(frameModel);
  ui->framesList->setViewMode(QListView::IconMode);
  ui->framesList->setSelectionMode(QAbstractItemView::ExtendedSelection);

  ui->timingLabel->setText(" -> " + tr("_timing") + ": " + QString::number(1000.0  / (double)ui->fps->value(), 'g', 4) + "ms");

  // Connect the click signal to the slot that handles highlighting the frame in the atlas view.
  QObject::connect(ui->framesList, &QListView::clicked,
                    this, &MainWindow::on_framesList_clicked);
  QObject::connect(frameModel, &ArrangementModel::mergeRequested,
                    this, &MainWindow::onMergeFrames);
  listDelegate = new FrameDelegate(this);
  ui->framesList->setItemDelegate(listDelegate);

  // Install the main window as an event filter on the list view's viewport.
  // This allows the main window to intercept mouse/drag events for the custom drag-and-drop delegate logic.
  ui->framesList->viewport()->installEventFilter(this);
  // Another event filter for Atlas view selection features
  ui->graphicsViewLayers->viewport()->installEventFilter(this);
  ui->framesList->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->framesList, &QListView::customContextMenuRequested,
                    this, &MainWindow::on_framesList_customContextMenuRequested);

  // --- Animation Setup ---
  QObject::connect(animationTimer, &QTimer::timeout,
                    this, &MainWindow::updateAnimation);
  QObject::connect(ui->fps, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &MainWindow::startAnimation);

  // --- Graphics View Setup ---
  ui->graphicsViewResult->setScene(new QGraphicsScene(this));
  ui->Play->setVisible(false);
  ui->Pause->setVisible(true);
  ui->graphicsViewLayers->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->graphicsViewLayers, &QWidget::customContextMenuRequested,
           this, &MainWindow::onAtlasContextMenuRequested);

  // Configure the view that displays the sprite atlas (ui->graphicsViewLayers).
  // QGraphicsView::FitInView ensures the entire scene fits inside the view, scaling if necessary.
  ui->graphicsViewLayers->setRenderHint(QPainter::Antialiasing, true);
  ui->graphicsViewLayers->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
  ui->graphicsViewLayers->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  ui->graphicsViewLayers->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  ui->graphicsViewLayers->fitInView(ui->graphicsViewLayers->sceneRect(), Qt::KeepAspectRatio);
  adjustZoomSliderToWindow();

  // Start the animation immediately (it will likely run with a single frame until a file is loaded).
  startAnimation();

  ui->animationList->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(ui->animationList, &QTreeWidget::customContextMenuRequested,
          this, &MainWindow::on_animationList_customContextMenuRequested);

  QShortcut *deleteShortcut = new QShortcut(QKeySequence::Delete, ui->animationList);
  QShortcut *backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), ui->animationList);

  connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::removeSelectedAnimation);
  connect(backspaceShortcut, &QShortcut::activated, this, &MainWindow::removeSelectedAnimation);

  connect(ui->animationList, &QTreeWidget::itemClicked,
          this, &MainWindow::on_animationList_itemClicked);
  connect(ui->animationList, &QTreeWidget::itemSelectionChanged,
          this, &MainWindow::on_animationList_itemSelectionChanged);

  connect(ui->fps, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this](int fps) {
              // Mettre à jour le label immédiatement
              ui->timingLabel->setText(" -> " + tr("_timing") + ": " +
                                       QString::number(1000.0 / (double)fps, 'g', 4) + "ms");

              // Mettre à jour les animations seulement si nécessaire
              if (extractor && !ui->animationList->selectedItems().isEmpty()) {
                  updateAnimationsList();
              }
          });
  for (int c = 0 ; c < ui->animationList->columnCount() ; c++)
      ui->animationList->resizeColumnToContents(c);

  statusLabel = new QLabel(this);
  statusLabel->setText(tr("_ready_to_start"));
  ui->statusBar->addPermanentWidget(statusLabel, 1);

  zoomSlider = new QSlider(Qt::Horizontal, this);
  zoomSlider->setRange(10, 1000);
  zoomSlider->setValue(100);
  zoomSlider->setMinimumWidth(200);
  zoomSlider->setTickInterval(10);
  ui->statusBar->addPermanentWidget(zoomSlider);

  zoomLabel = new QLabel(this);
  zoomLabel->setText(QString::number(zoomSlider->value()) + "%");
  ui->statusBar->addPermanentWidget(zoomLabel);

  progressBar = new QProgressBar(this);
  progressBar->setRange(0, 100); // Set the minimum and maximum values
  progressBar->setValue(0);      // Set the initial value
  progressBar->setTextVisible(true); // Show percentage or custom text
  progressBar->setFormat(tr("_progress") + " %p%"); // Custom format string
  progressBar->setMinimumWidth(300);

  ui->statusBar->addPermanentWidget(progressBar);

  connect(zoomSlider, &QSlider::valueChanged,
          this, &MainWindow::zoomSliderChanged);

  // Set the ready flag to true now that basic initialization is complete.
  ready = true;
}

MainWindow::~MainWindow()
{
  killTimer(timerId);
  stopAnimation();
  clearBoundingBoxHighlighters();
  if (selectionRectItem) {
      if (selectionRectItem->scene()) {
          selectionRectItem->scene()->removeItem(selectionRectItem);
        }
      delete selectionRectItem;
    }

  delete ui;
}
