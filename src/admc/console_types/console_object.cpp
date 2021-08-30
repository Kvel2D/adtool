/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2021 BaseALT Ltd.
 * Copyright (C) 2020-2021 Dmitry Degtyarev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "console_types/console_object.h"

#include "adldap.h"
#include "central_widget.h"
#include "console_types/console_policy.h"
#include "console_types/console_query.h"
#include "globals.h"
#include "search_thread.h"
#include "select_object_dialog.h"
#include "settings.h"
#include "status.h"
#include "utils.h"
#include "create_object_dialog.h"
#include "rename_object_dialog.h"
#include "move_object_dialog.h"
#include "change_dc_dialog.h"
#include "find_object_dialog.h"
#include "select_object_dialog.h"
#include "properties_dialog.h"
#include "object_multi_properties_dialog.h"
#include "password_dialog.h"
#include "editors/multi_editor.h"
#include "filter_dialog.h"
#include "central_widget.h"
#include "item_type.h"

#include <QDebug>
#include <QMenu>
#include <QSet>
#include <QStandardItemModel>

enum DropType {
    DropType_Move,
    DropType_AddToGroup,
    DropType_None
};

QStandardItem *object_tree_head = nullptr;

DropType console_object_get_drop_type(const QModelIndex &dropped, const QModelIndex &target);
void disable_drag_if_object_cant_be_moved(const QList<QStandardItem *> &items, const AdObject &object);
bool console_object_create_check(ConsoleWidget *console, const QModelIndex &parent);
bool console_object_search_id_match(QStandardItem *item, SearchThread *thread);
QList<QString> index_list_to_dn_list(const QList<QModelIndex> &index_list);
QList<QString> get_selected_dn_list_object(ConsoleWidget *console);
QString get_selected_dn_object(ConsoleWidget *console);
void console_object_delete(ConsoleWidget *console, const QList<QString> &dn_list, const QModelIndex &tree_head);

void console_object_load(const QList<QStandardItem *> row, const AdObject &object) {
    // Load attribute columns
    for (int i = 0; i < g_adconfig->get_columns().count(); i++) {
        const QString attribute = g_adconfig->get_columns()[i];

        if (!object.contains(attribute)) {
            continue;
        }

        const QString display_value = [attribute, object]() {
            if (attribute == ATTRIBUTE_OBJECT_CLASS) {
                const QString object_class = object.get_string(attribute);

                if (object_class == CLASS_GROUP) {
                    const GroupScope scope = object.get_group_scope();
                    const QString scope_string = group_scope_string(scope);

                    const GroupType type = object.get_group_type();
                    const QString type_string = group_type_string_adjective(type);

                    return QString("%1 - %2").arg(type_string, scope_string);
                } else {
                    return g_adconfig->get_class_display_name(object_class);
                }
            } else {
                const QByteArray value = object.get_value(attribute);
                return attribute_display_value(attribute, value, g_adconfig);
            }
        }();

        row[i]->setText(display_value);
    }

    console_object_item_data_load(row[0], object);

    disable_drag_if_object_cant_be_moved(row, object);
}

void console_object_item_data_load(QStandardItem *item, const AdObject &object) {
    const QIcon icon = get_object_icon(object);
    item->setIcon(icon);

    item->setData(object.get_dn(), ObjectRole_DN);

    const QList<QString> object_classes = object.get_strings(ATTRIBUTE_OBJECT_CLASS);
    item->setData(QVariant(object_classes), ObjectRole_ObjectClasses);

    const bool cannot_move = object.get_system_flag(SystemFlagsBit_CannotMove);
    item->setData(cannot_move, ObjectRole_CannotMove);

    const bool cannot_rename = object.get_system_flag(SystemFlagsBit_CannotRename);
    item->setData(cannot_rename, ObjectRole_CannotRename);

    const bool cannot_delete = object.get_system_flag(SystemFlagsBit_CannotDelete);
    item->setData(cannot_delete, ObjectRole_CannotDelete);

    const bool account_disabled = object.get_account_option(AccountOption_Disabled, g_adconfig);
    item->setData(account_disabled, ObjectRole_AccountDisabled);
}

QList<QString> console_object_header_labels() {
    QList<QString> out;

    for (const QString &attribute : g_adconfig->get_columns()) {
        const QString attribute_display_name = g_adconfig->get_column_display_name(attribute);

        out.append(attribute_display_name);
    }

    return out;
}

QList<int> console_object_default_columns() {
    // By default show first 3 columns: name, class and
    // description
    return {0, 1, 2};
}

QList<QString> console_object_search_attributes() {
    QList<QString> attributes;

    attributes += g_adconfig->get_columns();

    // NOTE: needed for loading group type/scope into "type"
    // column
    attributes += ATTRIBUTE_GROUP_TYPE;

    // NOTE: system flags are needed to disable
    // delete/move/rename for objects that can't do those
    // actions
    attributes += ATTRIBUTE_SYSTEM_FLAGS;

    return attributes;
}

void disable_drag_if_object_cant_be_moved(const QList<QStandardItem *> &items, const AdObject &object) {
    const bool cannot_move = object.get_system_flag(SystemFlagsBit_CannotMove);

    for (auto item : items) {
        item->setDragEnabled(!cannot_move);
    }
}

void console_object_delete(ConsoleWidget *console, const QList<QString> &dn_list, const QModelIndex &tree_head) {
    for (const QString &dn : dn_list) {
        const QList<QModelIndex> index_list = console->search_items(tree_head, ObjectRole_DN, dn, ItemType_Object);
        const QList<QPersistentModelIndex> persistent_list = persistent_index_list(index_list);

        for (const QPersistentModelIndex &index : persistent_list) {
            console->delete_item(index);
        }
    }
}

void console_object_create(ConsoleWidget *console, AdInterface &ad, const QList<QString> &dn_list, const QModelIndex &parent) {
    if (!console_object_create_check(console, parent)) {
        return;
    }

    const QList<AdObject> object_list = [&]() {
        QList<AdObject> out;

        for (const QString &dn : dn_list) {
            const AdObject object = ad.search_object(dn);
            out.append(object);
        }

        return out;
    }();

    console_object_create(console, object_list, parent);
}

void ConsoleObject::move_and_rename(AdInterface &ad, const QList<QString> &old_dn_list, const QString &new_parent_dn, const QList<QString> &new_dn_list) {
    // NOTE: delete old item AFTER adding new item because:
    // If old item is deleted first, then it's possible for
    // new parent to get selected (if they are next to each
    // other in scope tree). Then what happens is that due
    // to new parent being selected, it gets fetched and
    // loads new object. End result is that new object is
    // duplicated.
    const QModelIndex new_parent_index = [=]() {
        const QList<QModelIndex> results = console->search_items(object_tree_head->index(), ObjectRole_DN, new_parent_dn, ItemType_Object);

        if (results.size() == 1) {
            return results[0];
        } else {
            return QModelIndex();
        }
    }();

    console_object_create(console, ad, new_dn_list, new_parent_index);

    // NOTE: not deleting in query tree because this is a
    // move, objects still exist!
    console_object_delete(console, old_dn_list, object_tree_head->index());
}

// NOTE: this is a helper f-n for move_and_rename() that
// generates the new_dn_list for you, assuming that you just
// want to move objects to new parent without renaming
void ConsoleObject::move(AdInterface &ad, const QList<QString> &old_dn_list, const QString &new_parent_dn) {
    const QList<QString> new_dn_list = [&]() {
        QList<QString> out;

        for (const QString &old_dn : old_dn_list) {
            const QString new_dn = dn_move(old_dn, new_parent_dn);
            out.append(new_dn);
        }

        return out;
    }();

    move_and_rename(ad, old_dn_list, new_parent_dn, new_dn_list);
}

// Check parent index before adding objects to console
bool console_object_create_check(ConsoleWidget *console, const QModelIndex &parent) {
    if (!parent.isValid()) {
        return false;
    }

    // NOTE: don't add if parent wasn't fetched yet. If that
    // is the case then the object will be added naturally
    // when parent is fetched.
    const bool parent_was_fetched = console_item_get_was_fetched(parent);
    if (!parent_was_fetched) {
        return false;
    }

    return true;
}

void console_object_create(ConsoleWidget *console, const QList<AdObject> &object_list, const QModelIndex &parent) {
    if (!console_object_create_check(console, parent)) {
        return;
    }

    for (const AdObject &object : object_list) {
        const bool should_be_in_scope = [&]() {
            // NOTE: "containers" referenced here don't mean
            // objects with "container" object class.
            // Instead it means all the objects that can
            // have children(some of which are not
            // "container" class).
            const bool is_container = [=]() {
                const QList<QString> filter_containers = g_adconfig->get_filter_containers();
                const QString object_class = object.get_string(ATTRIBUTE_OBJECT_CLASS);

                return filter_containers.contains(object_class);
            }();

            const bool show_non_containers_ON = settings_get_bool(SETTING_show_non_containers_in_console_tree);

            return (is_container || show_non_containers_ON);
        }();

        const QList<QStandardItem *> row = [&]() {
            if (should_be_in_scope) {
                return console->add_scope_item(ItemType_Object, parent);
            } else {
                return console->add_results_item(ItemType_Object, parent);
            }
        }();

        console_object_load(row, object);
    }
}

// NOTE: it is possible for a search to start while a
// previous one hasn't finished. For that reason, this f-n
// contains multiple workarounds for issues caused by that
// case.
void console_object_search(ConsoleWidget *console, const QModelIndex &index, const QString &base, const SearchScope scope, const QString &filter, const QList<QString> &attributes) {
    QStandardItem *item = console->get_item(index);

    // Save original icon

    // NOTE: only save original icon if there isn't one
    // saved already. If this search overlaps a previous
    // one, then previous search would've already saved it.
    const QString icon_before_search_current = item->data(MyConsoleRole_IconBeforeSearch).toString();
    if (icon_before_search_current.isEmpty()) {
        const QIcon original_icon = item->icon();
        const QString original_icon_name = original_icon.name();
        item->setData(original_icon_name, MyConsoleRole_IconBeforeSearch);
    }

    // Set icon to indicate that item is in "search" state
    item->setIcon(QIcon::fromTheme("system-search"));

    // NOTE: need to set this role to disable actions during
    // fetch
    item->setData(true, ObjectRole_Fetching);
    item->setDragEnabled(false);

    auto search_thread = new SearchThread(base, scope, filter, attributes);

    // NOTE: change item's search thread, this will be used
    // later to handle situations where a thread is started
    // while another is running
    item->setData(search_thread->get_id(), MyConsoleRole_SearchThreadId);

    const QPersistentModelIndex persistent_index = index;

    // NOTE: need to pass console as receiver object to
    // connect() even though we're using lambda as a slot.
    // This is to be able to define queuedconnection type,
    // because there's no connect() version with no receiver
    // which has a connection type argument.
    QObject::connect(
        search_thread, &SearchThread::results_ready,
        console,
        [console, search_thread, persistent_index](const QHash<QString, AdObject> &results) {
            // NOTE: fetched index might become invalid for
            // many reasons, parent getting moved, deleted,
            // item at the index itself might get modified.
            // Since this slot runs in the main thread, it's
            // not possible for any catastrophic conflict to
            // happen, so it's enough to just stop the
            // search.
            if (!persistent_index.isValid()) {
                search_thread->stop();

                return;
            }

            QStandardItem *item_now = console->get_item(persistent_index);

            // NOTE: if another thread was started for this
            // item, abort this thread
            const bool thread_id_match = console_object_search_id_match(item_now, search_thread);
            if (!thread_id_match) {
                search_thread->stop();

                return;
            }

            console_object_create(console, results.values(), persistent_index);
        },
        Qt::QueuedConnection);
    QObject::connect(
        search_thread, &SearchThread::finished,
        console,
        [console, persistent_index, search_thread]() {
            if (!persistent_index.isValid()) {
                return;
            }

            QStandardItem *item_now = console->get_item(persistent_index);

            // NOTE: if another thread was started for this
            // item, don't change item data. It will be
            // changed by that other thread.
            const bool thread_id_match = console_object_search_id_match(item_now, search_thread);
            if (!thread_id_match) {
                return;
            }

            const QString original_icon_name = item_now->data(MyConsoleRole_IconBeforeSearch).toString();
            item_now->setIcon(QIcon::fromTheme(original_icon_name));

            // NOTE: empty IconBeforeSearch so next search
            // can use this as clean state
            item_now->setData(QString(), MyConsoleRole_IconBeforeSearch);

            item_now->setData(false, ObjectRole_Fetching);
            item_now->setDragEnabled(true);
        },
        Qt::QueuedConnection);

    search_thread->start();
}

// Load children of this item in scope tree
// and load results linked to this scope item
void ConsoleObject::fetch(const QModelIndex &index) {
    const QString base = index.data(ObjectRole_DN).toString();

    const SearchScope scope = SearchScope_Children;

    //
    // Search object's children
    //
    const QString filter = [=]() {
        QString out;

        out = is_container_filter();

        // NOTE: OR user filter with containers filter so
        // that container objects are always shown, even if
        // they are filtered out by user filter
        const QString current_filter = filter_dialog->get_filter();
        out = filter_OR({current_filter, out});

        advanced_features_filter(out);
        dev_mode_filter(out);

        return out;
    }();

    const QList<QString> attributes = console_object_search_attributes();

    // NOTE: do an extra search before real search for
    // objects that should be visible in dev mode
    const bool dev_mode = settings_get_bool(SETTING_dev_mode);
    if (dev_mode) {
        AdInterface ad;
        if (ad_connected(ad)) {
            QHash<QString, AdObject> results;
            dev_mode_search_results(results, ad, base);

            console_object_create(console, results.values(), index);
        }
    }

    console_object_search(console, index, base, scope, filter, attributes);
}

QStandardItem *console_object_tree_init(ConsoleWidget *console, AdInterface &ad) {
    // Create tree head
    const QList<QStandardItem *> head_row = console->add_scope_item(ItemType_Object, QModelIndex());
    object_tree_head = head_row[0];

    const QString top_dn = g_adconfig->domain_head();
    const AdObject top_object = ad.search_object(top_dn);
    console_object_item_data_load(object_tree_head, top_object);
    console_object_load_domain_head_text(object_tree_head);

    return object_tree_head;
}

bool ConsoleObject::can_drop(const QList<QPersistentModelIndex> &dropped_list, const QSet<int> &dropped_type_list, const QPersistentModelIndex &target, const int target_type) {
    const bool dropped_are_all_objects = (dropped_type_list.size() == 1 && dropped_type_list.contains(ItemType_Object));
    const bool dropped_are_policies = (dropped_type_list == QSet<int>({ItemType_Policy}));

    if (dropped_are_all_objects) {
        // NOTE: always allow dropping when dragging multiple
        // objects. This way, whatever objects can drop will be
        // dropped and if others fail to drop it's not a big
        // deal.
        if (dropped_list.size() != 1) {
            return true;
        } else {
            const QPersistentModelIndex dropped = dropped_list[0];

            const DropType drop_type = console_object_get_drop_type(dropped, target);
            const bool can_drop = (drop_type != DropType_None);

            return can_drop;
        }
    } else if (dropped_are_policies) {
        const bool target_is_ou = console_object_is_ou(target);

        return target_is_ou;
    } else {
        return false;
    }
}

void ConsoleObject::drop(const QList<QPersistentModelIndex> &dropped_list, const QSet<int> &dropped_type_list, const QPersistentModelIndex &target, const int target_type) {
    const bool dropped_are_all_objects = (dropped_type_list.size() == 1 && dropped_type_list.contains(ItemType_Object));
    const bool dropped_are_policies = (dropped_type_list == QSet<int>({ItemType_Policy}));

    if (dropped_are_all_objects) {
        drop_objects(dropped_list, target);
    } else if (dropped_are_policies) {
        drop_policies(dropped_list, target);
    }
}

// Determine what kind of drop type is dropping this object
// onto target. If drop type is none, then can't drop this
// object on this target.
DropType console_object_get_drop_type(const QModelIndex &dropped, const QModelIndex &target) {
    const bool dropped_is_target = [&]() {
        const QString dropped_dn = dropped.data(ObjectRole_DN).toString();
        const QString target_dn = target.data(ObjectRole_DN).toString();

        return (dropped_dn == target_dn);
    }();

    const QList<QString> dropped_classes = dropped.data(ObjectRole_ObjectClasses).toStringList();
    const QList<QString> target_classes = target.data(ObjectRole_ObjectClasses).toStringList();

    const bool dropped_is_user = dropped_classes.contains(CLASS_USER);
    const bool dropped_is_group = dropped_classes.contains(CLASS_GROUP);
    const bool target_is_group = target_classes.contains(CLASS_GROUP);
    const bool target_is_fetching = target.data(ObjectRole_Fetching).toBool();

    if (dropped_is_target || target_is_fetching) {
        return DropType_None;
    } else if (dropped_is_user && target_is_group) {
        return DropType_AddToGroup;
    } else if (dropped_is_group && target_is_group) {
        return DropType_AddToGroup;
    } else {
        const QList<QString> dropped_superiors = g_adconfig->get_possible_superiors(dropped_classes);

        const bool target_is_valid_superior = [&]() {
            for (const auto &object_class : dropped_superiors) {
                if (target_classes.contains(object_class)) {
                    return true;
                }
            }

            return false;
        }();

        if (target_is_valid_superior) {
            return DropType_Move;
        } else {
            return DropType_None;
        }
    }
}

QList<QString> object_operation_delete(const QList<QString> &targets, QWidget *parent) {
    if (targets.size() == 0) {
        return QList<QString>();
    }

    const bool confirmed = confirmation_dialog(QCoreApplication::translate("ConsoleActions", "Are you sure you want to delete this object?"), parent);
    if (!confirmed) {
        return QList<QString>();
    }

    QList<QString> deleted_objects;

    AdInterface ad;
    if (ad_failed(ad)) {
        return QList<QString>();
    }

    show_busy_indicator();

    for (const QString &dn : targets) {
        const bool success = ad.object_delete(dn);

        if (success) {
            deleted_objects.append(dn);
        }
    }

    hide_busy_indicator();

    g_status()->display_ad_messages(ad, parent);

    return deleted_objects;
}

QList<QString> object_operation_set_disabled(const QList<QString> &targets, const bool disabled, QWidget *parent) {
    AdInterface ad;
    if (ad_failed(ad)) {
        return QList<QString>();
    }

    show_busy_indicator();

    QList<QString> changed_objects;

    for (const QString &dn : targets) {
        const bool success = ad.user_set_account_option(dn, AccountOption_Disabled, disabled);

        if (success) {
            changed_objects.append(dn);
        }
    }

    hide_busy_indicator();

    g_status()->display_ad_messages(ad, parent);

    return changed_objects;
}

void object_operation_add_to_group(const QList<QString> &targets, QWidget *parent) {
    auto dialog = new SelectObjectDialog({CLASS_GROUP}, SelectObjectDialogMultiSelection_Yes, parent);
    dialog->setWindowTitle(QObject::tr("Add to Group"));

    QObject::connect(
        dialog, &SelectObjectDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            show_busy_indicator();

            const QList<QString> groups = dialog->get_selected();

            for (const QString &target : targets) {
                for (auto group : groups) {
                    ad.group_add_member(group, target);
                }
            }

            hide_busy_indicator();

            g_status()->display_ad_messages(ad, parent);
        });

    dialog->open();
}

void ConsoleObject::on_reset_account() {
    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    const QList<QString> target_list = get_selected_dn_list_object(console);

    for (const QString &target : target_list) {
        ad.computer_reset_account(target);
    }
}

bool console_object_is_ou(const QModelIndex &index) {
    const QList<QString> classes = index.data(ObjectRole_ObjectClasses).toStringList();
    const bool is_ou = classes.contains(CLASS_OU);

    return is_ou;
}

void ConsoleObject::drop_objects(const QList<QPersistentModelIndex> &dropped_list, const QPersistentModelIndex &target) {
    const QString target_dn = target.data(ObjectRole_DN).toString();

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    show_busy_indicator();

    for (const QPersistentModelIndex &dropped : dropped_list) {
        const QString dropped_dn = dropped.data(ObjectRole_DN).toString();
        const DropType drop_type = console_object_get_drop_type(dropped, target);

        switch (drop_type) {
            case DropType_Move: {
                const bool move_success = ad.object_move(dropped_dn,
                    target_dn);

                if (move_success) {
                    move(ad, QList<QString>({dropped_dn}), target_dn);
                }

                break;
            }
            case DropType_AddToGroup: {
                ad.group_add_member(target_dn, dropped_dn);

                break;
            }
            case DropType_None: {
                break;
            }
        }
    }

    hide_busy_indicator();

    g_status()->display_ad_messages(ad, console);
}

void ConsoleObject::drop_policies(const QList<QPersistentModelIndex> &dropped_list, const QPersistentModelIndex &target) {
    const QList<QString> policy_list = [&]() {
        QList<QString> out;

        for (const QPersistentModelIndex &index : dropped_list) {
            const QString dn = index.data(PolicyRole_DN).toString();
            out.append(dn);
        }

        return out;
    }();

    const QString target_dn = target.data(ObjectRole_DN).toString();
    const QList<QString> ou_list = {target_dn};

    console_policy_add_link(console, policy_list, ou_list, policy_results_widget);
}

void console_object_load_domain_head_text(QStandardItem *item) {
    const QString domain_head = g_adconfig->domain().toLower();
    const QString dc = AdInterface::get_dc();
    const QString domain_text = QString("%1 [%2]").arg(domain_head, dc);

    item->setText(domain_text);
}

QStandardItem *console_object_head() {
    return object_tree_head;
}

void ConsoleObject::delete_action(const QList<QModelIndex> &index_list) {
    const QList<QString> selected_list = index_list_to_dn_list(index_list);
    const QList<QString> deleted_list = object_operation_delete(selected_list, console);

    // NOTE: also delete in query tree
    console_object_delete(console, deleted_list, object_tree_head->index());
    console_object_delete(console, deleted_list, console_query_head()->index());
}

void ConsoleObject::new_object(const QString &object_class) {
    const QString parent_dn = get_selected_dn_object(console);

    auto dialog = new CreateObjectDialog(parent_dn, object_class, console);
    dialog->open();

    // NOTE: can't just add new object to this by adding
    // to selected index, because you can create an object
    // by using action menu of an object in a query tree.
    // Therefore need to search for parent in domain tree.
    QObject::connect(
        dialog, &CreateObjectDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            show_busy_indicator();

            const QList<QModelIndex> search_parent = console->search_items(console_object_head()->index(), ObjectRole_DN, parent_dn, ItemType_Object);

            if (search_parent.isEmpty()) {
                hide_busy_indicator();
                return;
            }

            const QModelIndex scope_parent_index = search_parent[0];
            const QString created_dn = dialog->get_created_dn();
            console_object_create(console, ad, {created_dn}, scope_parent_index);

            hide_busy_indicator();
        });
}

void ConsoleObject::on_new_user() {
    new_object(CLASS_USER);
}

void ConsoleObject::on_new_computer() {
    new_object(CLASS_COMPUTER);
}

void ConsoleObject::on_new_ou() {
    new_object(CLASS_OU);
}

void ConsoleObject::on_new_group() {
    new_object(CLASS_GROUP);
}

void ConsoleObject::rename(const QList<QModelIndex> &index_list) {
    const QString dn = get_selected_dn_object(console);

    auto dialog = new RenameObjectDialog(dn, console);
    dialog->open();

    QObject::connect(
        dialog, &RenameObjectDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QString old_dn = dn;
            const QString new_dn = dialog->get_new_dn();
            const QString parent_dn = dn_get_parent(old_dn);
            move_and_rename(ad, {old_dn}, parent_dn, {new_dn});
        });
}

void ConsoleObject::on_move() {
    const QList<QString> dn_list = get_selected_dn_list_object(console);

    auto dialog = new MoveObjectDialog(dn_list, console);
    dialog->open();

    QObject::connect(
        dialog, &QDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QList<QString> old_dn_list = dialog->get_moved_objects();
            const QString new_parent_dn = dialog->get_selected();
            move(ad, old_dn_list, new_parent_dn);
        });
}

void ConsoleObject::on_add_to_group() {
    const QList<QString> dn_list = get_selected_dn_list_object(console);
    object_operation_add_to_group(dn_list, console);
}

void ConsoleObject::set_disabled(const bool disabled) {
    const QList<QString> dn_list = get_selected_dn_list_object(console);

    show_busy_indicator();

    const QList<QString> changed_objects = object_operation_set_disabled(dn_list, disabled, console);

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    for (const QString &dn : changed_objects) {
        const QList<QModelIndex> index_list = console->search_items(QModelIndex(), ObjectRole_DN, dn, ItemType_Object);
        for (const QModelIndex &index : index_list) {
            QStandardItem *item = console->get_item(index);
            item->setData(disabled, ObjectRole_AccountDisabled);
        }
    }

    hide_busy_indicator();
}

void ConsoleObject::on_enable() {
    set_disabled(false);
}

void ConsoleObject::on_disable() {
    set_disabled(true);
}

void ConsoleObject::on_find() {
    const QList<QString> dn_list = get_selected_dn_list_object(console);

    if (dn_list.size() != 1) {
        return;
    }

    const QString dn = dn_list[0];

    auto find_dialog = new FindObjectDialog(filter_classes, dn, console);
    find_dialog->open();
}

void ConsoleObject::on_reset_password() {
    const QString dn = get_selected_dn_object(console);
    const auto password_dialog = new PasswordDialog(dn, console);
    password_dialog->open();
}

void ConsoleObject::on_edit_upn_suffixes() {
    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    // Open editor for upn suffixes attribute of partitions
    // object
    const QString partitions_dn = g_adconfig->partitions_dn();
    const AdObject partitions_object = ad.search_object(partitions_dn);
    const QList<QByteArray> current_values = partitions_object.get_values(ATTRIBUTE_UPN_SUFFIXES);

    g_status()->display_ad_messages(ad, console);

    auto editor = new MultiEditor(ATTRIBUTE_UPN_SUFFIXES, console);
    editor->load(current_values);
    editor->open();

    // When editor is accepted, update values of upn
    // suffixes
    connect(editor, &QDialog::accepted,
        [this, editor, partitions_dn]() {
            AdInterface ad2;
            if (ad_failed(ad2)) {
                return;
            }

            const QList<QByteArray> new_values = editor->get_new_values();

            ad2.attribute_replace_values(partitions_dn, ATTRIBUTE_UPN_SUFFIXES, new_values);
            g_status()->display_ad_messages(ad2, console);
        });
}

void ConsoleObject::on_change_dc() {
    auto change_dc_dialog = new ChangeDCDialog(console_object_head(), console);
    change_dc_dialog->open();
}

void ConsoleObject::properties(const QList<QModelIndex> &index_list) {
    const QList<QString> dn_list = index_list_to_dn_list(index_list);

    auto on_object_properties_applied = [=]() {
        AdInterface ad;
        if (ad_failed(ad)) {
            return;
        }

        for (const QString &dn : dn_list) {
            const AdObject object = ad.search_object(dn);

            // NOTE: search for indexes instead of using the
            // list given to f-n because we want to update
            // objects in both object and query tree
            const QList<QModelIndex> indexes_for_this_object = console->search_items(QModelIndex(), ObjectRole_DN, dn, ItemType_Object);
            for (const QModelIndex &index : indexes_for_this_object) {
                const QList<QStandardItem *> row = console->get_row(index);
                console_object_load(row, object);
            }
        }

        g_status()->display_ad_messages(ad, console);
    };

    if (dn_list.size() == 1) {
        const QString dn = dn_list[0];

        PropertiesDialog *dialog = PropertiesDialog::open_for_target(dn);

        connect(
            dialog, &PropertiesDialog::applied,
            on_object_properties_applied);
    } else if (dn_list.size() > 1) {
        const QList<QString> class_list = [&]() {
            QSet<QString> out;

            for (const QPersistentModelIndex &index : index_list) {
                const QList<QString> this_class_list = index.data(ObjectRole_ObjectClasses).toStringList();
                const QString main_class = this_class_list.last();
                out.insert(main_class);
            }

            return out.toList();
        }();

        auto dialog = new ObjectMultiPropertiesDialog(dn_list, class_list);
        dialog->open();

        connect(
            dialog, &ObjectMultiPropertiesDialog::applied,
            on_object_properties_applied);
    }
}

QString console_object_count_string(ConsoleWidget *console, const QModelIndex &index) {
    const int count = console->get_child_count(index);
    const QString out = QCoreApplication::translate("console_object", "%n object(s)", "", count);

    return out;
}

ConsoleObject::ConsoleObject(PolicyResultsWidget *policy_results_widget_arg, FilterDialog *filter_dialog_arg, ConsoleWidget *console_arg)
: ConsoleImpl(console_arg) {
    policy_results_widget = policy_results_widget_arg;
    filter_dialog = filter_dialog_arg;

    auto new_user_action = new QAction(tr("User"), this);
    auto new_computer_action = new QAction(tr("Computer"), this);
    auto new_ou_action = new QAction(tr("OU"), this);
    auto new_group_action = new QAction(tr("Group"), this);
    find_action = new QAction(tr("Find..."), this);
    move_action = new QAction(tr("Move..."), this);
    add_to_group_action = new QAction(tr("Add to group..."), this);
    enable_action = new QAction(tr("Enable"), this);
    disable_action = new QAction(tr("Disable"), this);
    reset_password_action = new QAction(tr("Reset password"), this);
    reset_account_action = new QAction(tr("Reset account"), this);
    edit_upn_suffixes_action = new QAction(tr("Edit UPN suffixes"), this);
    change_dc_action = new QAction(tr("Change domain controller"), this);

    auto new_menu = new QMenu(tr("New"), console_arg);
    new_action = new_menu->menuAction();

    new_menu->addAction(new_user_action);
    new_menu->addAction(new_computer_action);
    new_menu->addAction(new_ou_action);
    new_menu->addAction(new_group_action);

    // TODO: probably should just be members
    connect(
        new_user_action, &QAction::triggered,
        this, &ConsoleObject::on_new_user);
    connect(
        new_computer_action, &QAction::triggered,
        this, &ConsoleObject::on_new_computer);
    connect(
        new_ou_action, &QAction::triggered,
        this, &ConsoleObject::on_new_ou);
    connect(
        new_group_action, &QAction::triggered,
        this, &ConsoleObject::on_new_group);
    connect(
        move_action, &QAction::triggered,
        this, &ConsoleObject::on_move);
    connect(
        add_to_group_action, &QAction::triggered,
        this, &ConsoleObject::on_add_to_group);
    connect(
        enable_action, &QAction::triggered,
        this, &ConsoleObject::on_enable);
    connect(
        disable_action, &QAction::triggered,
        this, &ConsoleObject::on_disable);
    connect(
        reset_password_action, &QAction::triggered,
        this, &ConsoleObject::on_reset_password);
    connect(
        reset_account_action, &QAction::triggered,
        this, &ConsoleObject::on_reset_account);
    connect(
        find_action, &QAction::triggered,
        this, &ConsoleObject::on_find);
    connect(
        edit_upn_suffixes_action, &QAction::triggered,
        this, &ConsoleObject::on_edit_upn_suffixes);
    connect(
        change_dc_action, &QAction::triggered,
        this, &ConsoleObject::on_change_dc);
}

QString ConsoleObject::get_description(const QModelIndex &index) const {
    QString out;

    const QString object_count_text = console_object_count_string(console, index);

    out += object_count_text;

    const bool filtering_ON = filter_dialog->filtering_ON();
    if (filtering_ON) {
        out += tr(" [Filtering enabled]");
    }

    return out;
}

void ConsoleObject::activate(const QModelIndex &index) {
    properties({index});
}

bool console_object_search_id_match(QStandardItem *item, SearchThread *thread) {
    const int id_from_item = item->data(MyConsoleRole_SearchThreadId).toInt();
    const int thread_id = thread->get_id();

    const bool match = (id_from_item == thread_id);

    return match;
}

QList<QAction *> ConsoleObject::get_all_custom_actions() const {
    const QList<QAction *> out = {
        new_action,
        find_action,
        add_to_group_action,
        enable_action,
        disable_action,
        reset_password_action,
        reset_account_action,
        edit_upn_suffixes_action,
        change_dc_action,
        move_action,
    };

    return out;
}

QSet<QAction *> ConsoleObject::get_custom_actions(const QModelIndex &index, const bool single_selection) const {
    QSet<QAction *> out;

    const QString object_class = index.data(ObjectRole_ObjectClasses).toStringList().last();

    const bool is_container = [=]() {
        const QList<QString> container_classes = g_adconfig->get_filter_containers();

        return container_classes.contains(object_class);
    }();

    const bool is_user = (object_class == CLASS_USER);
    const bool is_group = (object_class == CLASS_GROUP);
    const bool is_domain = (object_class == CLASS_DOMAIN);
    const bool is_computer = (object_class == CLASS_COMPUTER);

    const bool account_disabled = index.data(ObjectRole_AccountDisabled).toBool();

    if (single_selection) {
        // Single selection only
        if (is_container) {
            out.insert(new_action);
            out.insert(find_action);
        }

        if (is_user) {
            out.insert(reset_password_action);
        }

        if (is_user || is_computer) {
            if (account_disabled) {
                out.insert(enable_action);
            } else {
                out.insert(disable_action);
            }
        }

        if (is_computer) {
            out.insert(reset_account_action);
        }

        if (is_domain) {
            out.insert(edit_upn_suffixes_action);
            out.insert(change_dc_action);
        }

    } else {
        // Multi selection only
        if (is_user) {
            out.insert(enable_action);
            out.insert(disable_action);
        }

        if (is_computer) {
            out.insert(reset_account_action);
        }
    }

    // Single OR multi selection
    if (is_user || is_group) {
        out.insert(add_to_group_action);
    }

    out.insert(move_action);

    return out;
}

QSet<QAction *> ConsoleObject::get_disabled_custom_actions(const QModelIndex &index, const bool single_selection) const {
    QSet<QAction *> out;

    const bool cannot_move = index.data(ObjectRole_CannotMove).toBool();

    if (cannot_move) {
        out.insert(move_action);
    }

    return out;
}

QSet<StandardAction> ConsoleObject::get_standard_actions(const QModelIndex &index, const bool single_selection) const {
    QSet<StandardAction> out;

    out.insert(StandardAction_Properties);

    // NOTE: only add refresh action if item was fetched,
    // this filters out all the objects like users that
    // should never get refresh action
    const bool can_refresh = console_item_get_was_fetched(index);
    if (can_refresh && single_selection) {
        out.insert(StandardAction_Refresh);
    }

    if (single_selection) {
        out.insert(StandardAction_Rename);
    }

    out.insert(StandardAction_Delete);

    return out;
}

QSet<StandardAction> ConsoleObject::get_disabled_standard_actions(const QModelIndex &index, const bool single_selection) const {
    QSet<StandardAction> out;

    const bool cannot_rename = index.data(ObjectRole_CannotRename).toBool();
    const bool cannot_delete = index.data(ObjectRole_CannotDelete).toBool();

    if (cannot_rename) {
        out.insert(StandardAction_Rename);
    }

    if (cannot_delete) {
        out.insert(StandardAction_Delete);
    }

    return out;
}

void ConsoleObject::refresh(const QList<QModelIndex> &index_list) {
    if (index_list.size() != 1) {
        return;
    }

    const QModelIndex index = index_list[0];

    console->delete_children(index);
    fetch(index);
}

QList<QString> index_list_to_dn_list(const QList<QModelIndex> &index_list) {
    QList<QString> out;

    for (const QModelIndex &index : index_list) {
        const QString dn = index.data(ObjectRole_DN).toString();
        out.append(dn);
    }

    return out;
}

QList<QString> get_selected_dn_list_object(ConsoleWidget *console) {
    return get_selected_dn_list(console, ItemType_Object, ObjectRole_DN);
}

QString get_selected_dn_object(ConsoleWidget *console) {
    return get_selected_dn(console, ItemType_Object, ObjectRole_DN);
}
