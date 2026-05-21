/****************************************************************************
** Meta object code from reading C++ file 'chatdialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.9)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../llfcchat/chatdialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'chatdialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.9. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ChatDialog_t {
    QByteArrayData data[39];
    char stringdata0[664];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ChatDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ChatDialog_t qt_meta_stringdata_ChatDialog = {
    {
QT_MOC_LITERAL(0, 0, 10), // "ChatDialog"
QT_MOC_LITERAL(1, 11, 22), // "slot_loading_chat_user"
QT_MOC_LITERAL(2, 34, 0), // ""
QT_MOC_LITERAL(3, 35, 14), // "slot_side_chat"
QT_MOC_LITERAL(4, 50, 17), // "slot_side_contact"
QT_MOC_LITERAL(5, 68, 17), // "slot_side_setting"
QT_MOC_LITERAL(6, 86, 17), // "slot_text_changed"
QT_MOC_LITERAL(7, 104, 3), // "str"
QT_MOC_LITERAL(8, 108, 14), // "slot_focus_out"
QT_MOC_LITERAL(9, 123, 25), // "slot_loading_contact_user"
QT_MOC_LITERAL(10, 149, 29), // "slot_switch_apply_friend_page"
QT_MOC_LITERAL(11, 179, 21), // "slot_friend_info_page"
QT_MOC_LITERAL(12, 201, 25), // "std::shared_ptr<UserInfo>"
QT_MOC_LITERAL(13, 227, 9), // "user_info"
QT_MOC_LITERAL(14, 237, 16), // "slot_show_search"
QT_MOC_LITERAL(15, 254, 4), // "show"
QT_MOC_LITERAL(16, 259, 17), // "slot_apply_friend"
QT_MOC_LITERAL(17, 277, 31), // "std::shared_ptr<AddFriendApply>"
QT_MOC_LITERAL(18, 309, 5), // "apply"
QT_MOC_LITERAL(19, 315, 20), // "slot_add_auth_friend"
QT_MOC_LITERAL(20, 336, 25), // "std::shared_ptr<AuthInfo>"
QT_MOC_LITERAL(21, 362, 9), // "auth_info"
QT_MOC_LITERAL(22, 372, 13), // "slot_auth_rsp"
QT_MOC_LITERAL(23, 386, 24), // "std::shared_ptr<AuthRsp>"
QT_MOC_LITERAL(24, 411, 8), // "auth_rsp"
QT_MOC_LITERAL(25, 420, 19), // "slot_jump_chat_item"
QT_MOC_LITERAL(26, 440, 27), // "std::shared_ptr<SearchInfo>"
QT_MOC_LITERAL(27, 468, 2), // "si"
QT_MOC_LITERAL(28, 471, 33), // "slot_jump_chat_item_from_info..."
QT_MOC_LITERAL(29, 505, 2), // "ui"
QT_MOC_LITERAL(30, 508, 17), // "slot_item_clicked"
QT_MOC_LITERAL(31, 526, 16), // "QListWidgetItem*"
QT_MOC_LITERAL(32, 543, 4), // "item"
QT_MOC_LITERAL(33, 548, 18), // "slot_text_chat_msg"
QT_MOC_LITERAL(34, 567, 28), // "std::shared_ptr<TextChatMsg>"
QT_MOC_LITERAL(35, 596, 3), // "msg"
QT_MOC_LITERAL(36, 600, 25), // "slot_append_send_chat_msg"
QT_MOC_LITERAL(37, 626, 29), // "std::shared_ptr<TextChatData>"
QT_MOC_LITERAL(38, 656, 7) // "msgdata"

    },
    "ChatDialog\0slot_loading_chat_user\0\0"
    "slot_side_chat\0slot_side_contact\0"
    "slot_side_setting\0slot_text_changed\0"
    "str\0slot_focus_out\0slot_loading_contact_user\0"
    "slot_switch_apply_friend_page\0"
    "slot_friend_info_page\0std::shared_ptr<UserInfo>\0"
    "user_info\0slot_show_search\0show\0"
    "slot_apply_friend\0std::shared_ptr<AddFriendApply>\0"
    "apply\0slot_add_auth_friend\0"
    "std::shared_ptr<AuthInfo>\0auth_info\0"
    "slot_auth_rsp\0std::shared_ptr<AuthRsp>\0"
    "auth_rsp\0slot_jump_chat_item\0"
    "std::shared_ptr<SearchInfo>\0si\0"
    "slot_jump_chat_item_from_infopage\0ui\0"
    "slot_item_clicked\0QListWidgetItem*\0"
    "item\0slot_text_chat_msg\0"
    "std::shared_ptr<TextChatMsg>\0msg\0"
    "slot_append_send_chat_msg\0"
    "std::shared_ptr<TextChatData>\0msgdata"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ChatDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      18,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,  104,    2, 0x0a /* Public */,
       3,    0,  105,    2, 0x0a /* Public */,
       4,    0,  106,    2, 0x0a /* Public */,
       5,    0,  107,    2, 0x0a /* Public */,
       6,    1,  108,    2, 0x0a /* Public */,
       8,    0,  111,    2, 0x0a /* Public */,
       9,    0,  112,    2, 0x0a /* Public */,
      10,    0,  113,    2, 0x0a /* Public */,
      11,    1,  114,    2, 0x0a /* Public */,
      14,    1,  117,    2, 0x0a /* Public */,
      16,    1,  120,    2, 0x0a /* Public */,
      19,    1,  123,    2, 0x0a /* Public */,
      22,    1,  126,    2, 0x0a /* Public */,
      25,    1,  129,    2, 0x0a /* Public */,
      28,    1,  132,    2, 0x0a /* Public */,
      30,    1,  135,    2, 0x0a /* Public */,
      33,    1,  138,    2, 0x0a /* Public */,
      36,    1,  141,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 12,   13,
    QMetaType::Void, QMetaType::Bool,   15,
    QMetaType::Void, 0x80000000 | 17,   18,
    QMetaType::Void, 0x80000000 | 20,   21,
    QMetaType::Void, 0x80000000 | 23,   24,
    QMetaType::Void, 0x80000000 | 26,   27,
    QMetaType::Void, 0x80000000 | 12,   29,
    QMetaType::Void, 0x80000000 | 31,   32,
    QMetaType::Void, 0x80000000 | 34,   35,
    QMetaType::Void, 0x80000000 | 37,   38,

       0        // eod
};

void ChatDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ChatDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->slot_loading_chat_user(); break;
        case 1: _t->slot_side_chat(); break;
        case 2: _t->slot_side_contact(); break;
        case 3: _t->slot_side_setting(); break;
        case 4: _t->slot_text_changed((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 5: _t->slot_focus_out(); break;
        case 6: _t->slot_loading_contact_user(); break;
        case 7: _t->slot_switch_apply_friend_page(); break;
        case 8: _t->slot_friend_info_page((*reinterpret_cast< std::shared_ptr<UserInfo>(*)>(_a[1]))); break;
        case 9: _t->slot_show_search((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 10: _t->slot_apply_friend((*reinterpret_cast< std::shared_ptr<AddFriendApply>(*)>(_a[1]))); break;
        case 11: _t->slot_add_auth_friend((*reinterpret_cast< std::shared_ptr<AuthInfo>(*)>(_a[1]))); break;
        case 12: _t->slot_auth_rsp((*reinterpret_cast< std::shared_ptr<AuthRsp>(*)>(_a[1]))); break;
        case 13: _t->slot_jump_chat_item((*reinterpret_cast< std::shared_ptr<SearchInfo>(*)>(_a[1]))); break;
        case 14: _t->slot_jump_chat_item_from_infopage((*reinterpret_cast< std::shared_ptr<UserInfo>(*)>(_a[1]))); break;
        case 15: _t->slot_item_clicked((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 16: _t->slot_text_chat_msg((*reinterpret_cast< std::shared_ptr<TextChatMsg>(*)>(_a[1]))); break;
        case 17: _t->slot_append_send_chat_msg((*reinterpret_cast< std::shared_ptr<TextChatData>(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ChatDialog::staticMetaObject = { {
    &QDialog::staticMetaObject,
    qt_meta_stringdata_ChatDialog.data,
    qt_meta_data_ChatDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ChatDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ChatDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ChatDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int ChatDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 18;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
