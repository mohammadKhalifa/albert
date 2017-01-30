// albert - a simple application launcher for linux
// Copyright (C) 2014-2016 Manuel Schneider
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QAbstractItemModel>
#include <QGraphicsDropShadowEffect>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDebug>
#include <QDesktopWidget>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QStandardPaths>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>
#include "mainwindow.h"

namespace  {

const char*   CFG_WND_POS  = "windowPosition";
const char*   CFG_CENTERED = "showCentered";
const bool    DEF_CENTERED = true;
const char*   CFG_THEME = "theme";
const char*   DEF_THEME = "Bright";
const char*   CFG_HIDE_ON_FOCUS_LOSS = "hideOnFocusLoss";
const bool    DEF_HIDE_ON_FOCUS_LOSS = true;
const char*   CFG_HIDE_ON_CLOSE = "hideOnClose";
const bool    DEF_HIDE_ON_CLOSE = false;
const char*   CFG_CLEAR_ON_HIDE = "clearOnHide";
const bool    DEF_CLEAR_ON_HIDE = false;
const char*   CFG_ALWAYS_ON_TOP = "alwaysOnTop";
const bool    DEF_ALWAYS_ON_TOP = true;
const char*   CFG_MAX_PROPOSALS = "itemCount";
const uint8_t DEF_MAX_PROPOSALS = 5;
const char*   CFG_DISPLAY_SCROLLBAR = "displayScrollbar";
const bool    DEF_DISPLAY_SCROLLBAR = false;
const char*   CFG_DISPLAY_ICONS = "displayIcons";
const bool    DEF_DISPLAY_ICONS = true;
const char*   CFG_DISPLAY_SHADOW = "displayShadow";
const bool    DEF_DISPLAY_SHADOW = true;

}


/** ***************************************************************************/
MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent),
      actionsShown_(false),
      historyMoveMod_(Qt::ControlModifier) {

	// INITIALIZE UI
    ui.setupUi(this);
    setWindowTitle(qAppName());
    setWindowFlags(Qt::Tool
                   | Qt::WindowCloseButtonHint // No close event w/o this
                   | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(20);
    effect->setColor(QColor(0, 0, 0 , 192 ))  ;
    effect->setXOffset(0.0);
    effect->setYOffset(3.0);
    setGraphicsEffect(effect);

     // Disable tabbing completely
    ui.actionList->setFocusPolicy(Qt::NoFocus);
    ui.proposalList->setFocusPolicy(Qt::NoFocus);

    // Set initial event filter pipeline: window -> proposallist -> lineedit
    ui.inputLine->installEventFilter(ui.proposalList);
    ui.inputLine->installEventFilter(this);

    // Set stringlistmodel for actions view
    actionsListModel_ = new QStringListModel(this);
    ui.actionList->setModel(actionsListModel_);

    // Hide lists
    ui.actionList->hide();
    ui.proposalList->hide();

    // Settings button
    settingsButton_ = new SettingsButton(this);
    settingsButton_->setObjectName("settingsButton");
    settingsButton_->setFocusPolicy(Qt::NoFocus);
    settingsButton_->setContextMenuPolicy(Qt::ActionsContextMenu);

    // Context menu of settingsbutton
    QAction *action = new QAction("Settings", settingsButton_);
    action->setShortcuts({QKeySequence("Ctrl+,"), QKeySequence("Alt+,")});
    connect(action, &QAction::triggered, this, &MainWindow::hide);
    connect(action, &QAction::triggered, this, &MainWindow::settingsWidgetRequested);
    connect(settingsButton_, &QPushButton::clicked, action, &QAction::trigger);
    settingsButton_->addAction(action);

    action = new QAction("Hide", settingsButton_);
    action->setShortcut(QKeySequence("Esc"));
    connect(action, &QAction::triggered, this, &MainWindow::hide);
    settingsButton_->addAction(action);

    action = new QAction("Separator", settingsButton_);
    action->setSeparator(true);
    settingsButton_->addAction(action);

    action = new QAction("Quit", settingsButton_);
    action->setShortcut(QKeySequence("Alt+F4"));
    connect(action, &QAction::triggered, qApp, &QApplication::quit);
    settingsButton_->addAction(action);

    // History
    history_ = new History(this);

    /*
     * Settings
     */

    QSettings s(qApp->applicationName());
    setShowCentered(s.value(CFG_CENTERED, DEF_CENTERED).toBool());
    if (!showCentered() && s.contains(CFG_WND_POS) && s.value(CFG_WND_POS).canConvert(QMetaType::QPoint))
        move(s.value(CFG_WND_POS).toPoint());
    setHideOnFocusLoss(s.value(CFG_HIDE_ON_FOCUS_LOSS, DEF_HIDE_ON_FOCUS_LOSS).toBool());
    setHideOnClose(s.value(CFG_HIDE_ON_CLOSE, DEF_HIDE_ON_CLOSE).toBool());
    setClearOnHide(s.value(CFG_CLEAR_ON_HIDE, DEF_CLEAR_ON_HIDE).toBool());
    setAlwaysOnTop(s.value(CFG_ALWAYS_ON_TOP, DEF_ALWAYS_ON_TOP).toBool());
    setMaxProposals(s.value(CFG_MAX_PROPOSALS, DEF_MAX_PROPOSALS).toInt());
    setDisplayScrollbar(s.value(CFG_DISPLAY_SCROLLBAR, DEF_DISPLAY_SCROLLBAR).toBool());
    setDisplayIcons(s.value(CFG_DISPLAY_ICONS, DEF_DISPLAY_ICONS).toBool());
    setDisplayShadow(s.value(CFG_DISPLAY_SHADOW, DEF_DISPLAY_SHADOW).toBool());
    theme_ = s.value(CFG_THEME, DEF_THEME).toString();
    if (!setTheme(theme_)) {
        qFatal("FATAL: Stylefile not found: %s", theme_.toStdString().c_str());
        qApp->quit();
    }


    /*
     * Signals
     */

    // Trigger query, if text changed
    connect(ui.inputLine, &QLineEdit::textChanged, this, &MainWindow::inputChanged);

    // Hide the actionview, if text was changed
    connect(ui.inputLine, &QLineEdit::textChanged, this, &MainWindow::hideActions);

    // Reset history, if text was manually changed
    connect(ui.inputLine, &QLineEdit::textEdited, history_, &History::resetIterator);

    // Hide the actionview, if another item gets clicked
    connect(ui.proposalList, &ProposalList::pressed, this, &MainWindow::hideActions);

    // Trigger default action, if item in proposallist was activated
    QObject::connect(ui.proposalList, &ProposalList::activated, [this](const QModelIndex &index){

        switch (qApp->queryKeyboardModifiers()) {
        case Qt::AltModifier: // AltAction
            ui.proposalList->model()->setData(index, -1, Qt::UserRole+101);
            break;
        case Qt::MetaModifier: // MetaAction
            ui.proposalList->model()->setData(index, -1, Qt::UserRole+102);
            break;
        case Qt::ControlModifier: // ControlAction
            ui.proposalList->model()->setData(index, -1, Qt::UserRole+103);
            break;
        case Qt::ShiftModifier: // ShiftAction
            ui.proposalList->model()->setData(index, -1, Qt::UserRole+104);
            break;
        default: // DefaultAction
            ui.proposalList->model()->setData(index, -1, Qt::UserRole+100);
            break;
        }

        // Do not move this up! (Invalidates index)
        history_->add(ui.inputLine->text());
        this->setVisible(false);
        ui.inputLine->clear();
    });

    // Trigger alternative action, if item in actionList was activated
    QObject::connect(ui.actionList, &ActionList::activated, [this](const QModelIndex &index){
        history_->add(ui.inputLine->text());
        ui.proposalList->model()->setData(ui.proposalList->currentIndex(), index.row(), Qt::UserRole);
        this->setVisible(false);
    });
}



/** ***************************************************************************/
void MainWindow::setVisible(bool visible) {

    // Skip if nothing to do
    if ( (isVisible() && visible) || !(isVisible() || visible) )
        return;

    QWidget::setVisible(visible);

    if (visible) {
        // Move widget after showing it since QWidget::move works only on widgets
        // that have been shown once. Well as long as this does not introduce ugly
        // flicker this may be okay.
        if (showCentered_){
            QDesktopWidget *dw = QApplication::desktop();
            this->move(dw->availableGeometry(dw->screenNumber(QCursor::pos())).center()
                       -QPoint(rect().right()/2,192 ));
        }
        this->raise();
        this->activateWindow();
        ui.inputLine->setFocus();
        emit widgetShown();
    } else {
        setShowActions(false);
        history_->resetIterator();
        ( clearOnHide_ ) ? ui.inputLine->clear() : ui.inputLine->selectAll();
        emit widgetHidden();
    }
}


/** ***************************************************************************/
void MainWindow::toggleVisibility() {
   setVisible(!isVisible());
}


/** ***************************************************************************/
void MainWindow::setInput(const QString &input) {
    ui.inputLine->setText(input);
}



/** ***************************************************************************/
void MainWindow::setModel(QAbstractItemModel *m) {
    ui.proposalList->setModel(m);
}



/** ***************************************************************************/
void MainWindow::setShowCentered(bool b) {
    QSettings(qApp->applicationName()).setValue(CFG_CENTERED, b);
    showCentered_ = b;
}



/** ***************************************************************************/
bool MainWindow::showCentered() const {
    return showCentered_;
}



/** ***************************************************************************/
const QString &MainWindow::theme() const {
    return theme_;
}



/** ***************************************************************************/
bool MainWindow::setTheme(const QString &theme) {
    theme_ = theme;
    QFileInfoList themes;
    QStringList themeDirs = QStandardPaths::locateAll(
        QStandardPaths::DataLocation, "themes", QStandardPaths::LocateDirectory);
    for (const QDir &d : themeDirs)
        themes << d.entryInfoList(QStringList("*.qss"), QDir::Files | QDir::NoSymLinks);
    // Find and apply the theme
    bool success = false;
    for (const QFileInfo &fi : themes) {
        if (fi.baseName() == theme_) {
            QFile f(fi.canonicalFilePath());
            if (f.open(QFile::ReadOnly)) {
                QSettings(qApp->applicationName()).setValue(CFG_THEME, theme_);
                setStyleSheet(f.readAll());
                f.close();
                success = true;
                break;
            }
        }
    }
    return success;
}



/** ***************************************************************************/
bool MainWindow::hideOnFocusLoss() const {
    return hideOnFocusLoss_;
}



/** ***************************************************************************/
void MainWindow::setHideOnFocusLoss(bool b) {
    QSettings(qApp->applicationName()).setValue(CFG_HIDE_ON_FOCUS_LOSS, b);
    hideOnFocusLoss_ = b;
}



/** ***************************************************************************/
bool MainWindow::hideOnClose() const {
    return hideOnClose_;
}



/** ***************************************************************************/
void MainWindow::setHideOnClose(bool b) {
    QSettings(qApp->applicationName()).setValue(CFG_HIDE_ON_CLOSE, b);
    hideOnClose_ = b;
}



/** ***************************************************************************/
bool MainWindow::clearOnHide() const {
    return clearOnHide_;
}



/** ***************************************************************************/
void MainWindow::setClearOnHide(bool b) {
    QSettings(qApp->applicationName()).setValue(CFG_CLEAR_ON_HIDE, b);
    clearOnHide_ = b;
}



/** ***************************************************************************/
bool MainWindow::alwaysOnTop() const {
    return windowFlags().testFlag(Qt::WindowStaysOnTopHint);
}



/** ***************************************************************************/
void MainWindow::setAlwaysOnTop(bool alwaysOnTop) {
    QSettings(qApp->applicationName()).setValue(CFG_ALWAYS_ON_TOP, alwaysOnTop);
    // TODO: QT_MINREL 5.7 setFlag
    alwaysOnTop ? setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint)
                : setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
}



/** ***************************************************************************/
void MainWindow::setMaxProposals(uint8_t maxItems) {
    QSettings(qApp->applicationName()).setValue(CFG_MAX_PROPOSALS, maxItems);
    ui.proposalList->setMaxItems(maxItems);
}



/** ***************************************************************************/
bool MainWindow::displayIcons() const {
    return ui.proposalList->displayIcons();
}



/** ***************************************************************************/
void MainWindow::setDisplayIcons(bool value) {
    QSettings(qApp->applicationName()).setValue(CFG_DISPLAY_ICONS, value);
    ui.proposalList->setDisplayIcons(value);
}



/** ***************************************************************************/
bool MainWindow::displayScrollbar() const {
    return ui.proposalList->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff;
}



/** ***************************************************************************/
void MainWindow::setDisplayScrollbar(bool value) {
    QSettings(qApp->applicationName()).setValue(CFG_DISPLAY_SCROLLBAR, value);
    ui.proposalList->setVerticalScrollBarPolicy(
                value ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}



/** ***************************************************************************/
bool MainWindow::displayShadow() const {
    return displayShadow_;
}



/** ***************************************************************************/
void MainWindow::setDisplayShadow(bool value) {
    QSettings(qApp->applicationName()).setValue(CFG_DISPLAY_SHADOW, value);
    displayShadow_ = value;
    graphicsEffect()->setEnabled(value);
    value ? setContentsMargins(20,20,20,20) : setContentsMargins(0,0,0,0);
}



/** ***************************************************************************/
uint8_t MainWindow::maxProposals() const {
    return ui.proposalList->maxItems();
}



/** ***************************************************************************/
bool MainWindow::actionsAreShown() const {
    return actionsShown_;
}



/** ***************************************************************************/
void MainWindow::setShowActions(bool showActions) {

    // Show actions
    if ( showActions && !actionsShown_ ) {

        // Skip if nothing selected
        if ( !ui.proposalList->currentIndex().isValid())
            return;

        // Get actions
        actionsListModel_->setStringList(ui.proposalList->model()->data(
                                             ui.proposalList->currentIndex(),
                                             Qt::UserRole).toStringList());

        // Skip if actions are empty
        if (actionsListModel_->rowCount() < 1)
            return;

        ui.actionList->setCurrentIndex(actionsListModel_->index(0, 0, ui.actionList->rootIndex()));
        ui.actionList->show();

        // Change event filter pipeline: window -> _action_list -> lineedit
        ui.inputLine->removeEventFilter(this);
        ui.inputLine->removeEventFilter(ui.proposalList);
        ui.inputLine->installEventFilter(ui.actionList);
        ui.inputLine->installEventFilter(this);

        // Finally set the state
        actionsShown_ = true;
    }

    // Hide actions
    if ( !showActions && actionsShown_ ) {

        ui.actionList->hide();

        // Change event filter pipeline: window -> _proposal_list -> lineedit
        ui.inputLine->removeEventFilter(this);
        ui.inputLine->removeEventFilter(ui.actionList);
        ui.inputLine->installEventFilter(ui.proposalList);
        ui.inputLine->installEventFilter(this);

        // Finally set the state
        actionsShown_ = false;
    }
}



/** ***************************************************************************/
void MainWindow::closeEvent(QCloseEvent *event) {
    event->accept();
    if (!hideOnClose_)
        qApp->quit();
}



/** ***************************************************************************/
void MainWindow::resizeEvent(QResizeEvent *event) {
    // Let settingsbutton be in top right corner of frame
    settingsButton_->move(ui.frame->geometry().topRight() - QPoint(settingsButton_->width()-1,0));
    QWidget::resizeEvent(event);
}



/** ***************************************************************************/
void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    // Move the widget with the mouse
    move(event->globalPos() - clickOffset_);
    QWidget::mouseMoveEvent(event);
}



/** ***************************************************************************/
void MainWindow::mousePressEvent(QMouseEvent *event) {
    // Save the offset on press for movement calculations
    clickOffset_ = event->pos();
    QWidget::mousePressEvent(event);
}



/** ***************************************************************************/
void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    // Save the window position ()
    QSettings(qApp->applicationName()).setValue(CFG_WND_POS, pos());
    QWidget::mousePressEvent(event);
}



/** ***************************************************************************/
bool MainWindow::eventFilter(QObject *, QEvent *event) {

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key()) {

        // Toggle actionsview
        case Qt::Key_Tab:
            setShowActions(!actionsAreShown());
            return true;

        case Qt::Key_Up:{
            // Move up in the history
            if ( !ui.proposalList->currentIndex().isValid() // Empty list
                 || keyEvent->modifiers() == historyMoveMod_ // MoveMod (Ctrl) hold
                 || ( !actionsAreShown() // Not in actions state...
                      && ui.proposalList->currentIndex().row()==0 && !keyEvent->isAutoRepeat() ) ){ // ... and first row (non repeat)
                QString next = history_->next();
                if (!next.isEmpty())
                    ui.inputLine->setText(next);
                return true;
            }
        }

        // Move down in the history
        case Qt::Key_Down:{
            if ( !actionsAreShown() && keyEvent->modifiers() == Qt::ControlModifier ) {
                QString prev = history_->prev();
                if (!prev.isEmpty())
                    ui.inputLine->setText(prev);
                return true;
            }
        }
        }
    }

    if (event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if ( wheelEvent->angleDelta().y() > 0 ) {
            QString next = history_->next();
            if (!next.isEmpty())
                ui.inputLine->setText(next);
        } else {
            QString prev = history_->prev();
            if (!prev.isEmpty())
                ui.inputLine->setText(prev);
        }
    }

    return false;
}



/** ***************************************************************************/
bool MainWindow::event(QEvent *event) {
    if (event->type() == QEvent::WindowDeactivate) {
        /* This is a horribly hackish but working solution.

         A triggered key grab on X11 steals the focus of the window for short
         period of time. This may result in the following annoying behaviour:
         When the hotkey is pressed and X11 steals the focus there arises a
         race condition between the hotkey event and the focus out event.
         When the app is visible and the focus out event is delivered the app
         gets hidden. Finally when the hotkey is received the app gets shown
         again although the user intended to hide the app with the hotkey.

         Solutions:
         Although X11 differs between the two focus out events, qt does not.
         One might install a native event filter and use the XCB structs to
         decide which type of event is delivered, but this approach is not
         platform independent (unless designed so explicitely, but its a
         hassle). The behaviour was expected when the app hides on:

         (mode==XCB_NOTIFY_MODE_GRAB && detail==XCB_NOTIFY_DETAIL_NONLINEAR)||
          (mode==XCB_NOTIFY_MODE_NORMAL && detail==XCB_NOTIFY_DETAIL_NONLINEAR)
         (Check Xlib Programming Manual)

         The current, much simpler but less elegant solution is to delay the
         hiding a few milliseconds, so that the hotkey event will always be
         handled first. */
        if (hideOnFocusLoss_){
            // Note fix if least LTS goes beyond Qt5.4
            // QTimer::singleShot(50, this, &MainWindow::hide);
            QTimer::singleShot(50, this, SLOT(hide()));
        }
    }
    return QWidget::event(event);
}
