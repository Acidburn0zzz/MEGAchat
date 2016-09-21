#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QInputDialog>
#include <QDrag>
#include <QMimeData>
#include <mstrophepp.h>
#include <IRtcModule.h>
#include <mstrophepp.h>
#include <../strophe.disco.h>
#include <ui_mainwindow.h>
#include <ui_clistitem.h>
#include <ui_loginDialog.h>
#include <ui_settings.h>
#include <IJingleSession.h>
#include <chatClient.h>
#include "chatWindow.h"

namespace Ui {
class MainWindow;
class SettingsDialog;
}
namespace karere {
class Client;
}
QString prettyInterval(int64_t secs);
class CListItem;

class MainWindow : public QMainWindow, public karere::IApp,
        public karere::IApp::IContactListHandler,
        public karere::IApp::IChatListHandler
{
    Q_OBJECT
    karere::Client* mClient;
public:
    explicit MainWindow(karere::Client* aClient=nullptr);
    void setClient(karere::Client& client) { mClient = &client; }
    karere::Client& client() const { return *mClient; }
    ~MainWindow();
    Ui::MainWindow ui;
    void removeItem(IListItem& item);
//IContactList
    virtual IContactListItem* addContactItem(karere::Contact& contact);
    virtual void removeContactItem(IContactListItem& item);
//IChatListItem
    virtual IGroupChatListItem* addGroupChatItem(karere::GroupChatRoom& room);
    virtual void removeGroupChatItem(IGroupChatListItem& item);
    virtual IPeerChatListItem* addPeerChatItem(karere::PeerChatRoom& room);
    virtual void removePeerChatItem(IPeerChatListItem& item);
//IApp
    virtual karere::IApp::IContactListHandler* contactListHandler() { return this; }
    virtual karere::IApp::IChatListHandler* chatListHandler() { return this; }
    IChatHandler* createChatHandler(karere::ChatRoom& room);
    virtual rtcModule::IEventHandler* onIncomingCall(const std::shared_ptr<rtcModule::ICallAnswer> &ans)
    {
        return new CallAnswerGui(*this, ans);
    }
    virtual karere::IApp::ILoginDialog* createLoginDialog();
    virtual void onOwnPresence(karere::Presence pres);
    virtual void onIncomingContactRequest(const mega::MegaContactRequest &req);
protected:
    karere::IApp::IContactListItem* addItem(bool front, karere::Contact* contact,
                karere::GroupChatRoom* room);
    void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu menu(this);
        auto addAction = menu.addAction(tr("Add user to contacts"));
        connect(addAction, SIGNAL(triggered()), this, SLOT(onAddContact()));
        menu.setStyleSheet("background-color: lightgray");
        menu.exec(event->globalPos());
    }
protected slots:
    void onAddContact();
    void onSettingsBtn(bool);
    void onOnlineStatusBtn(bool);
    void setOnlineStatus();
};

class SettingsDialog: public QDialog
{
    Q_OBJECT
protected:
    Ui::SettingsDialog ui;
    int mAudioInIdx;
    int mVideoInIdx;
    MainWindow& mMainWindow;
    void selectVideoInput();
    void selectAudioInput();
protected slots:
public:
    SettingsDialog(MainWindow &parent);
    void applySettings();
};

extern bool inCall;
extern QColor gAvatarColors[];
extern QString gOnlineIndColors[karere::Presence::kLast+1];
class CListItem: public QWidget, public virtual karere::IApp::IListItem
{
protected:
    Ui::CListItemGui ui;
    int mLastOverlayCount = 0;
public:
//karere::ITitleDisplay interface
    virtual void onUnreadCountChanged(int count)
    {
        if (count < 0)
            ui.mUnreadIndicator->setText(QString::number(-count)+"+");
        else
            ui.mUnreadIndicator->setText(QString::number(count));
        ui.mUnreadIndicator->adjustSize();
        if (count)
        {
            if (!mLastOverlayCount)
                ui.mUnreadIndicator->show();
        }
        else
        {
            if (mLastOverlayCount)
                ui.mUnreadIndicator->hide();
        }
        mLastOverlayCount = count;
    }
    virtual void onPresenceChanged(karere::Presence state)
    {
        ui.mOnlineIndicator->setStyleSheet(
            QString("background-color: ")+gOnlineIndColors[state]+
            ";border-radius: 4px");
    }
//===
    CListItem(QWidget* parent)
    : QWidget(parent)
    {
        ui.setupUi(this);
        ui.mUnreadIndicator->hide();
    }
    virtual void showChatWindow() = 0;
    void showAsHidden()
    {
        ui.mName->setStyleSheet("color: rgba(0,0,0,128)\n");
    }
    void unshowAsHidden()
    {
        ui.mName->setStyleSheet("color: rgba(255,255,255,255)\n");
    }
};

class CListChatItem: public CListItem, public virtual karere::IApp::IChatListItem
{
    Q_OBJECT
public:
    void showChatWindow()
    {
        ChatWindow* window;
        auto& thisRoom = room();
        if (!thisRoom.appChatHandler())
        {
            window = new ChatWindow(this, thisRoom);
            thisRoom.setAppChatHandler(window);
        }
        else
        {
            window = static_cast<ChatWindow*>(thisRoom.appChatHandler()->userp);
        }
        window->show();
    }
    CListChatItem(QWidget* parent): CListItem(parent){}
//ITitleHandler intefrace
    virtual void onTitleChanged(const std::string& title)
    {
        QString text = QString::fromUtf8(title.c_str(), title.size());
        ui.mName->setText(text);
    }
    virtual void onVisibilityChanged(int newVisibility) {}
//==
    virtual void mouseDoubleClickEvent(QMouseEvent* event)
    {
        showChatWindow();
    }
    virtual karere::ChatRoom& room() const = 0;
protected slots:
    void truncateChat();
};

class CListContactItem: public CListItem, public virtual karere::IApp::IContactListItem
{
    Q_OBJECT
protected:
    karere::Contact& mContact;
    QPoint mDragStartPos;
public:
    CListContactItem(QWidget* parent, karere::Contact& contact)
        :CListItem(parent), mContact(contact)
    {
        if (contact.visibility() == ::mega::MegaUser::VISIBILITY_HIDDEN)
        {
            showAsHidden();
        }
        karere::setTimeout([this]() { updateToolTip(); }, 100);
    }
    void updateToolTip() //WARNING: Must be called after app init, as the xmpp jid is not initialized during creation
    {
        QChar lf('\n');
        QString text;
        if (mContact.visibility() == ::mega::MegaUser::VISIBILITY_HIDDEN)
            text = "INVISIBLE\n";
        text.append(tr("Email: "));
        text.append(QString::fromStdString(mContact.email())).append(lf);
        text.append(tr("User handle: ")).append(QString::fromStdString(karere::Id(mContact.userId()).toString())).append(lf);
        text.append(tr("XMPP jid: ")).append(QString::fromStdString(mContact.jid())).append(lf);
        if (mContact.chatRoom())
            text.append(tr("Chat handle: ")).append(QString::fromStdString(karere::Id(mContact.chatRoom()->chatid()).toString()));
        else
            text.append(tr("You have never chatted with this person"));
//        auto now = time(NULL);
//        text.append(tr("\nFriends since: ")).append(prettyInterval(now-contact.since())).append(lf);
        setToolTip(text);
    }
    virtual void showChatWindow()
    {
        auto room = mContact.chatRoom();
        if (room)
        {
            auto chatItem = static_cast<CListChatItem*>(room->roomGui()->userp);
            if (chatItem) //may be null if app returned null from IChatListHandler::addXXXChatItem()
                chatItem->showChatWindow();
            return;
        }
        mContact.createChatRoom()
        .then([this](karere::ChatRoom* room)
        {
            updateToolTip();
            auto window = new ChatWindow(this, *room);
            room->setAppChatHandler(window);
            window->show();
        })
        .fail([this](const promise::Error& err)
        {
            QMessageBox::critical(nullptr, "rtctestapp",
                    "Error creating chatroom:\n"+QString::fromStdString(err.what()));
        });
    }
    virtual void onTitleChanged(const std::string &title)
    {
        QString text = QString::fromUtf8(title.c_str(), title.size());
        ui.mName->setText(text);
        ui.mAvatar->setText(QString(text[0].toUpper()));
        auto& col = gAvatarColors[mContact.userId() & 0x0f];

        QString style = "border-radius: 4px;"
            "border: 2px solid rgba(0,0,0,0);"
            "color: white;"
            "font: 24px;"
            "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0,"
            "stop:0 rgba(%1,%2,%3,180), stop:1 rgba(%1,%2,%3,255))";
        style = style.arg(col.red()).arg(col.green()).arg(col.blue());
        ui.mAvatar->setStyleSheet(style);
    }
    void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu menu(this);
        auto chatInviteAction = menu.addAction(tr("Invite to group chat"));
        auto removeAction = menu.addAction(tr("Remove contact"));
        connect(chatInviteAction, SIGNAL(triggered()), this, SLOT(onCreateGroupChat()));
        connect(removeAction, SIGNAL(triggered()), this, SLOT(onContactRemove()));
        menu.setStyleSheet("background-color: lightgray");
        menu.exec(event->globalPos());
    }
    void mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton)
        {
            mDragStartPos = event->pos();
        }
        QWidget::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event)
    {
        if (!(event->buttons() & Qt::LeftButton))
            return;
        if ((event->pos() - mDragStartPos).manhattanLength() < QApplication::startDragDistance())
            return;
        startDrag();
    }
    void startDrag()
    {
        QDrag drag(this);
        QMimeData *mimeData = new QMimeData;
        auto userid = mContact.userId();
        mimeData->setData("application/mega-user-handle", QByteArray((const char*)(&userid), sizeof(userid)));
        drag.setMimeData(mimeData);
        drag.exec();
    }
    virtual void onVisibilityChanged(int newVisibility)
    {
        GUI_LOG_DEBUG("onVisibilityChanged for contact %s: new visibility is %d",
               karere::Id(mContact.userId()).toString().c_str(), newVisibility);
        auto chat = mContact.chatRoom()
            ? static_cast<CListChatItem*>(mContact.chatRoom()->roomGui()->userp)
            : nullptr;

        if (newVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN)
        {
            showAsHidden();
            if (chat)
                chat->showAsHidden();
        }
        else
        {
            unshowAsHidden();
            if (chat)
                chat->unshowAsHidden();
        }
        updateToolTip();
    }
    virtual void mouseDoubleClickEvent(QMouseEvent* event)
    {
        showChatWindow();
    }

public slots:
    void onCreateGroupChat()
    {
        std::string name;
        auto qname = QInputDialog::getText(this, tr("Invite to group chat"), tr("Enter group chat name"));
        if (!qname.isNull())
            name = qname.toStdString();

        mContact.contactList().client.createGroupChat({std::make_pair(mContact.userId(), chatd::PRIV_FULL)}, name)
        .fail([this](const promise::Error& err)
        {
            QMessageBox::critical(this, tr("Create group chat"), tr("Error creating group chat:\n")+QString::fromStdString(err.msg()));
        });
    }
    void onContactRemove()
    {
        QString msg = tr("Are you sure you want to remove ");
        msg.append(mContact.titleString().c_str());
        if (mContact.titleString() != mContact.email())
        {
            msg.append(" (").append(mContact.email().c_str());
        }
        msg.append(tr(" from your contacts?"));

        auto ret = QMessageBox::question(this, tr("Remove contact"), msg);
        if (ret != QMessageBox::Yes)
            return;
        mContact.contactList().removeContactFromServer(mContact.userId())
        .fail([](const promise::Error& err)
        {
            QMessageBox::critical(nullptr, tr("Remove contact"), tr("Error removing contact: ").append(err.what()));
        });
    }
};
class CListGroupChatItem: public CListChatItem, public virtual karere::IApp::IGroupChatListItem
{
    Q_OBJECT
public:
    CListGroupChatItem(QWidget* parent, karere::GroupChatRoom& room)
        :CListChatItem(parent), mRoom(room)
    {
        ui.mAvatar->setText("G");
        updateToolTip();
    }
    void updateToolTip()
    {
        QString text(tr("Group chat room: "));
        text.append(QString::fromStdString(karere::Id(mRoom.chatid()).toString())).append(QChar('\n'))
            .append(tr("Own privilege: ").append(QString::number(mRoom.ownPriv())).append(QChar('\n')))
            .append(tr("Other participants:\n"));
        for (const auto& item: mRoom.peers())
        {
            auto& peer = *item.second;
            const std::string* email = mRoom.parent.client.contactList->getUserEmail(item.first);
            auto line = QString(" %1 (%2, %3): priv %4\n").arg(QString::fromStdString(peer.name()))
                .arg(email?QString::fromStdString(*email):tr("(email unknown)"))
                .arg(QString::fromStdString(karere::Id(item.first).toString()))
                .arg((int)item.second->priv());
            text.append(line);
        }
        text.truncate(text.size()-1);
        setToolTip(text);
    }
    virtual void onMembersUpdated() { updateToolTip(); }
protected:
    karere::GroupChatRoom& mRoom;
    void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu menu(this);
        auto actLeave = menu.addAction(tr("Leave group chat"));
        connect(actLeave, SIGNAL(triggered()), this, SLOT(leaveGroupChat()));
        auto actTopic = menu.addAction(tr("Set chat topic"));
        connect(actTopic, SIGNAL(triggered()), this, SLOT(setTitle()));
        auto actTruncate = menu.addAction(tr("Truncate chat"));
        connect(actTruncate, SIGNAL(triggered()), this, SLOT(truncateChat()));
        menu.setStyleSheet("background-color: lightgray");
        menu.exec(event->globalPos());
    }
    virtual void mouseDoubleClickEvent(QMouseEvent* event) { showChatWindow(); }
    virtual karere::ChatRoom& room() const { return mRoom; }
protected slots:
    void leaveGroupChat() { karere::marshallCall([this]() { mRoom.leave(); }); } //deletes this
    void setTitle();
};

class CListPeerChatItem: public CListChatItem, public virtual karere::IApp::IPeerChatListItem
{
protected:
    karere::PeerChatRoom& mRoom;
    CListContactItem* mContactItem;
public:
    CListPeerChatItem(QWidget* parent, karere::PeerChatRoom& room)
        : CListChatItem(parent), mRoom(room),
          mContactItem(dynamic_cast<CListContactItem*>(room.contact().appItem()))
    {
        if(mRoom.contact().visibility() == ::mega::MegaUser::VISIBILITY_HIDDEN)
            showAsHidden();
        ui.mAvatar->setText("1");
        updateToolTip();
    }
    void updateToolTip() //WARNING: Must be called after app init, as the xmpp jid is not initialized during creation
    {
        QChar lf('\n');
        QString text(tr("1on1 Chat room: "));
        text.append(QString::fromStdString(karere::Id(mRoom.chatid()).toString())).append(lf);
        text.append(tr("Email: "));
        text.append(QString::fromStdString(mRoom.contact().email())).append(lf);
        text.append(tr("User handle: ")).append(QString::fromStdString(karere::Id(mRoom.contact().userId()).toString()));
        setToolTip(text);
    }
    void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu menu(this);
        auto actTruncate = menu.addAction(tr("Truncate chat"));
        connect(actTruncate, SIGNAL(triggered()), this, SLOT(truncateChat()));
        menu.setStyleSheet("background-color: lightgray");
        menu.exec(event->globalPos());
    }
    virtual karere::ChatRoom& room() const { return mRoom; }
};

#endif // MAINWINDOW_H
