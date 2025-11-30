#include "include/mainwindow.h"
#include "./ui_mainwindow.h"
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
{
  ui->setupUi(this);
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

  // Configure the view that displays the animation preview (ui->graphicsViewResult).
  ui->graphicsViewResult->setRenderHint(QPainter::Antialiasing, true);
  ui->graphicsViewResult->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
  ui->graphicsViewResult->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  ui->graphicsViewResult->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  ui->graphicsViewResult->fitInView(ui->graphicsViewResult->sceneRect(), Qt::KeepAspectRatio);

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

