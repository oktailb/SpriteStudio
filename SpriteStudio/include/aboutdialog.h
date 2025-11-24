#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPixmap>
#include <QIcon>

class AboutDialog : public QDialog
{
  Q_OBJECT

public:
  explicit AboutDialog(QWidget *parent = nullptr);

private:
  void setupUI();
  void loadApplicationInfo();
  void loadCredits();
  void loadLicense();
  QString readTextFile(const QString &filePath);

  QTabWidget *tabWidget;
  QTextEdit *aboutText;
  QTextEdit *creditsText;
  QTextEdit *licenseText;
  QLabel *iconLabel;
  QLabel *titleLabel;
  QLabel *versionLabel;
  QPushButton *closeButton;
};

#endif // ABOUTDIALOG_H
