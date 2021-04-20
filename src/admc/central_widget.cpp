/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020 BaseALT Ltd.
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

#include "central_widget.h"

#include "console_types/object.h"
#include "console_types/policy.h"
#include "console_types/query.h"
#include "utils.h"
#include "adldap.h"
#include "properties_dialog.h"
#include "globals.h"
#include "settings.h"
#include "filter_dialog.h"
#include "console_actions.h"
#include "status.h"
#include "rename_dialog.h"
#include "create_dialog.h"
#include "create_policy_dialog.h"
#include "create_query_dialog.h"
#include "move_query_dialog.h"
#include "create_query_folder_dialog.h"
#include "move_dialog.h"
#include "find_dialog.h"
#include "password_dialog.h"
#include "rename_policy_dialog.h"
#include "select_dialog.h"
#include "console_widget/console_widget.h"
#include "console_widget/results_view.h"
#include "editors/multi_editor.h"
#include "gplink.h"
#include "policy_results_widget.h"
#include "edit_query_folder_dialog.h"

#include <QDebug>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDebug>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QApplication>
#include <QTreeWidget>
#include <QStack>
#include <QMenu>
#include <QLabel>
#include <QSortFilterProxyModel>

CentralWidget::CentralWidget()
: QWidget()
{
    console_actions = new ConsoleActions(this);

    open_filter_action = new QAction(tr("&Filter objects"), this);
    dev_mode_action = new QAction(tr("Dev mode"), this);
    show_noncontainers_action = new QAction(tr("&Show non-container objects in Console tree"), this);

    filter_dialog = nullptr;
    open_filter_action->setEnabled(false);

    console = new ConsoleWidget();

    auto create_query_dialog = new CreateQueryDialog(console);
    auto create_query_folder_dialog = new CreateQueryFolderDialog(console);
    auto edit_query_folder_dialog = new EditQueryFolderDialog(console);
    auto create_policy_dialog = new CreatePolicyDialog(console);
    auto rename_policy_dialog = new RenamePolicyDialog(console);
    auto move_query_dialog = new MoveQueryDialog(console);

    auto policy_container_results = new ResultsView(this);
    policy_container_results->detail_view()->header()->setDefaultSectionSize(200);
    policy_container_results_id = console->register_results(policy_container_results, policy_model_header_labels(), policy_model_default_columns());
    
    policy_results_widget = new PolicyResultsWidget();
    policy_results_id = console->register_results(policy_results_widget);

    auto query_results = new ResultsView(this);
    query_results->detail_view()->header()->setDefaultSectionSize(200);
    query_folder_results_id = console->register_results(query_results, query_folder_header_labels(), query_folder_default_columns());
    
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
    layout->addWidget(console);

    // Refresh head when settings affecting the filter
    // change. This reloads the model with an updated filter
    const BoolSettingSignal *advanced_features = g_settings->get_bool_signal(BoolSetting_AdvancedFeatures);
    connect(
        advanced_features, &BoolSettingSignal::changed,
        this, &CentralWidget::refresh_head);

    const BoolSettingSignal *show_non_containers = g_settings->get_bool_signal(BoolSetting_ShowNonContainersInConsoleTree);
    connect(
        show_non_containers, &BoolSettingSignal::changed,
        this, &CentralWidget::refresh_head);

    const BoolSettingSignal *dev_mode_signal = g_settings->get_bool_signal(BoolSetting_DevMode);
    connect(
        dev_mode_signal, &BoolSettingSignal::changed,
        this, &CentralWidget::refresh_head);

    g_settings->connect_toggle_widget(console->get_scope_view(), BoolSetting_ShowConsoleTree);
    g_settings->connect_toggle_widget(console->get_description_bar(), BoolSetting_ShowResultsHeader);

    g_settings->connect_action_to_bool_setting(dev_mode_action, BoolSetting_DevMode);
    g_settings->connect_action_to_bool_setting(show_noncontainers_action, BoolSetting_ShowNonContainersInConsoleTree);

    connect(
        open_filter_action, &QAction::triggered,
        this, &CentralWidget::open_filter);

    connect(
        console_actions->get(ConsoleAction_NewUser), &QAction::triggered,
        this, &CentralWidget::create_user);
    connect(
        console_actions->get(ConsoleAction_NewComputer), &QAction::triggered,
        this, &CentralWidget::create_computer);
    connect(
        console_actions->get(ConsoleAction_NewOU), &QAction::triggered,
        this, &CentralWidget::create_ou);
    connect(
        console_actions->get(ConsoleAction_NewGroup), &QAction::triggered,
        this, &CentralWidget::create_group);
    connect(
        console_actions->get(ConsoleAction_Delete), &QAction::triggered,
        this, &CentralWidget::delete_objects);
    connect(
        console_actions->get(ConsoleAction_Rename), &QAction::triggered,
        this, &CentralWidget::rename);
    connect(
        console_actions->get(ConsoleAction_Move), &QAction::triggered,
        this, &CentralWidget::move);
    connect(
        console_actions->get(ConsoleAction_AddToGroup), &QAction::triggered,
        this, &CentralWidget::add_to_group);
    connect(
        console_actions->get(ConsoleAction_Enable), &QAction::triggered,
        this, &CentralWidget::enable);
    connect(
        console_actions->get(ConsoleAction_Disable), &QAction::triggered,
        this, &CentralWidget::disable);
    connect(
        console_actions->get(ConsoleAction_ResetPassword), &QAction::triggered,
        this, &CentralWidget::reset_password);
    connect(
        console_actions->get(ConsoleAction_Find), &QAction::triggered,
        this, &CentralWidget::find);
    connect(
        console_actions->get(ConsoleAction_EditUpnSuffixes), &QAction::triggered,
        this, &CentralWidget::edit_upn_suffixes);

    connect(
        console_actions->get(ConsoleAction_PolicyCreate), &QAction::triggered,
        create_policy_dialog, &QDialog::open);
    connect(
        console_actions->get(ConsoleAction_PolicyAddLink), &QAction::triggered,
        this, &CentralWidget::add_link);
    connect(
        console_actions->get(ConsoleAction_PolicyRename), &QAction::triggered,
        rename_policy_dialog, &QDialog::open);
    connect(
        console_actions->get(ConsoleAction_PolicyDelete), &QAction::triggered,
        this, &CentralWidget::delete_policy);

    connect(
        console_actions->get(ConsoleAction_QueryCreateFolder), &QAction::triggered,
        create_query_folder_dialog, &QDialog::open);
    connect(
        console_actions->get(ConsoleAction_QueryCreateItem), &QAction::triggered,
        create_query_dialog, &QDialog::open);
    connect(
        console_actions->get(ConsoleAction_QueryEditFolder), &QAction::triggered,
        edit_query_folder_dialog, &QDialog::open);
    connect(
        console_actions->get(ConsoleAction_QueryMoveItemOrFolder), &QAction::triggered,
        move_query_dialog, &QDialog::open);
    connect(
        console_actions->get(ConsoleAction_QueryDeleteItemOrFolder), &QAction::triggered,
        this, &CentralWidget::delete_query_item_or_folder);

    connect(
        console, &ConsoleWidget::current_scope_item_changed,
        this, &CentralWidget::on_current_scope_changed);
    connect(
        console, &ConsoleWidget::results_count_changed,
        this, &CentralWidget::update_description_bar);
    connect(
        console, &ConsoleWidget::item_fetched,
        this, &CentralWidget::fetch_scope_node);
    connect(
        console, &ConsoleWidget::items_can_drop,
        this, &CentralWidget::on_items_can_drop);
    connect(
        console, &ConsoleWidget::items_dropped,
        this, &CentralWidget::on_items_dropped);
    connect(
        console, &ConsoleWidget::properties_requested,
        this, &CentralWidget::on_properties_requested);
    connect(
        console, &ConsoleWidget::selection_changed,
        this, &CentralWidget::update_actions_visibility);
    connect(
        console, &ConsoleWidget::context_menu,
        this, &CentralWidget::context_menu);

    update_actions_visibility();
}

void CentralWidget::go_online(AdInterface &ad) {
    // NOTE: filter dialog requires a connection to load
    // display strings from adconfig so create it here
    filter_dialog = new FilterDialog(this);
    connect(
        filter_dialog, &QDialog::accepted,
        this, &CentralWidget::refresh_head);
    open_filter_action->setEnabled(true);

    // NOTE: need to create object results here because it
    // requires header labels which come from ADCONFIG, so
    // need to be online
    auto object_results = new ResultsView(this);
    object_results_id = console->register_results(object_results, object_header_labels(), object_default_columns());

    object_tree_head = object_tree_init(console, ad);
    policy_tree_init(console, ad);
    query_tree_init(console);

    console->sort_scope();
    console->set_current_scope(object_tree_head);
}

void CentralWidget::open_filter() {
    if (filter_dialog != nullptr) {
        filter_dialog->open();
    }
}

void CentralWidget::delete_objects() {
    const QHash<QString, QPersistentModelIndex> selected = get_selected_dns_and_indexes();
    const QList<QString> deleted_objects = object_delete(selected.keys(), this);

    object_delete(console, deleted_objects);
}

void CentralWidget::on_properties_requested() {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();
    if (targets.size() != 1) {
        return;
    }

    const QString target = targets.keys()[0];

    PropertiesDialog *dialog = PropertiesDialog::open_for_target(target);

    connect(
        dialog, &PropertiesDialog::applied,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const AdObject object = ad.search_object(target);

            const QList<QModelIndex> scope_indexes = console->search_scope_by_role(ObjectRole_DN, target, ItemType_Object);
            for (const QModelIndex &index : scope_indexes) {
                QStandardItem *scope_item = console->get_scope_item(index);
                object_scope_load(scope_item, object);
            }

            const QList<QModelIndex> results_indexes = console->search_results_by_role(ObjectRole_DN, target, ItemType_Object);
            for (const QModelIndex &index : results_indexes) {
                const QList<QStandardItem *> results_row = console->get_results_row(index);
                object_results_load(results_row, object);
            }
            
            update_actions_visibility();
        });
}

void CentralWidget::rename() {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();

    auto dialog = new RenameDialog(targets.keys(), this);
    dialog->open();

    connect(
        dialog, &RenameDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QString old_dn = targets.keys()[0];
            const QString new_dn = dialog->get_new_dn();
            const QString parent_dn = dn_get_parent(old_dn);
            object_move(console, ad, {old_dn}, {new_dn}, parent_dn);
        });
}

void CentralWidget::create_helper(const QString &object_class) {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();

    auto dialog = new CreateDialog(targets.keys(), object_class, this);
    dialog->open();

    // NOTE: can't just add new object to console by adding
    // to selected index, because you can create an object
    // by using action menu of an object in a query tree.
    // Therefore need to search for parent in domain tree.
    connect(
        dialog, &CreateDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            show_busy_indicator();

            const QString parent_dn = targets.keys()[0];
            const QList<QModelIndex> search_parent = console->search_scope_by_role(ObjectRole_DN, parent_dn, ItemType_Object);

            if (search_parent.isEmpty()) {
                hide_busy_indicator();
                return;
            }

            const QModelIndex scope_parent_index = search_parent[0];
            const QString created_dn = dialog->get_created_dn();
            object_create(console, ad, {created_dn}, scope_parent_index);

            console->sort_scope();

            hide_busy_indicator();
        });
}

void CentralWidget::move() {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();

    auto dialog = new MoveDialog(targets.keys(), this);
    dialog->open();

    connect(
        dialog, &QDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QList<QString> old_dn_list = dialog->get_moved_objects();
            const QString new_parent_dn = dialog->get_selected();
            object_move(console, ad, old_dn_list, new_parent_dn);

            console->sort_scope();
        });
}

void CentralWidget::add_to_group() {
    const QList<QString> targets = get_selected_dns();
    object_add_to_group(targets, this);
}

void CentralWidget::enable() {
    enable_disable_helper(false);
}

void CentralWidget::disable() {
    enable_disable_helper(true);
}

void CentralWidget::find() {
    const QList<QString> targets = get_selected_dns();

    if (targets.size() != 1) {
        return;
    }

    const QString target = targets[0];

    auto find_dialog = new FindDialog(filter_classes, target, this);
    find_dialog->open();
}

void CentralWidget::reset_password() {
    const QList<QString> targets = get_selected_dns();
    const auto password_dialog = new PasswordDialog(targets, this);
    password_dialog->open();
}

void CentralWidget::create_user() {
    create_helper(CLASS_USER);
}

void CentralWidget::create_computer() {
    create_helper(CLASS_COMPUTER);
}

void CentralWidget::create_ou() {
    create_helper(CLASS_OU);
}

void CentralWidget::create_group() {
    create_helper(CLASS_GROUP);
}

void CentralWidget::edit_upn_suffixes() {
    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    // Open editor for upn suffixes attribute of partitions
    // object
    const QString partitions_dn = g_adconfig->partitions_dn();
    const AdObject partitions_object = ad.search_object(partitions_dn);
    const QList<QByteArray> current_values = partitions_object.get_values(ATTRIBUTE_UPN_SUFFIXES);

    auto editor = new MultiEditor(ATTRIBUTE_UPN_SUFFIXES, current_values, this);
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
            g_status()->display_ad_messages(ad2, this);
        });
}

void CentralWidget::add_link() {
    const QList<QModelIndex> selected = console->get_selected_items();
    if (selected.size() == 0) {
        return;
    }

    auto dialog = new SelectDialog({CLASS_OU}, SelectDialogMultiSelection_Yes, this);

    QObject::connect(
        dialog, &SelectDialog::accepted,
        [=]() {           
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            show_busy_indicator();

            const QList<QString> gpos =
            [selected]() {
                QList<QString> out;

                for (const QModelIndex &index : selected) {
                    const QString gpo = index.data(PolicyRole_DN).toString();
                    out.append(gpo);
                }

                return out;
            }();

            const QList<QString> ou_list = dialog->get_selected();

            for (const QString &ou_dn : ou_list) {
                const QHash<QString, AdObject> results = ad.search(QString(), {ATTRIBUTE_GPLINK}, SearchScope_Object, ou_dn);
                const AdObject ou_object = results[ou_dn];
                const QString gplink_string = ou_object.get_string(ATTRIBUTE_GPLINK);
                Gplink gplink = Gplink(gplink_string);

                for (const QString &gpo : gpos) {
                    gplink.add(gpo);
                }

                ad.attribute_replace_string(ou_dn, ATTRIBUTE_GPLINK, gplink.to_string());
            }

            const QModelIndex current_scope = console->get_current_scope_item();
            policy_results_widget->update(current_scope);

            hide_busy_indicator();

            g_status()->display_ad_messages(ad, this);
        });

    dialog->open();
}

void CentralWidget::delete_policy() {
    const QHash<QString, QPersistentModelIndex> selected = get_selected_dns_and_indexes();

    if (selected.size() == 0) {
        return;
    }

    const bool confirmed = confirmation_dialog(tr("Are you sure you want to delete this policy and all of it's links?"), this);
    if (!confirmed) {
        return;
    }

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    show_busy_indicator();

    for (const QModelIndex &index : selected) {
        const QString dn = index.data(PolicyRole_DN).toString();
        const bool success = ad.object_delete(dn);

        if (success) {
            // Remove deleted policy from console
            console->delete_item(index);

            // Remove links to delete policy
            const QString filter = filter_CONDITION(Condition_Contains, ATTRIBUTE_GPLINK, dn);
            const QList<QString> search_attributes = {
                ATTRIBUTE_GPLINK,
            };
            const QHash<QString, AdObject> search_results = ad.search(filter, search_attributes, SearchScope_All);
            for (const AdObject &object : search_results.values()) {
                const QString gplink_string = object.get_string(ATTRIBUTE_GPLINK);
                Gplink gplink = Gplink(gplink_string);
                gplink.remove(dn);

                const QString updated_gplink_string = gplink.to_string();
                ad.attribute_replace_string(object.get_dn(), ATTRIBUTE_GPLINK, updated_gplink_string);
            }
        }
    }

    hide_busy_indicator();

    g_status()->display_ad_messages(ad, this);
}

void CentralWidget::delete_query_item_or_folder() {
    const QList<QModelIndex> selected_indexes = console->get_selected_items();
    
    if (selected_indexes.size() != 1) {
        return;
    }

    const QModelIndex index = selected_indexes[0];
    console->delete_item(index);

    query_tree_save(console);
}

void CentralWidget::on_items_can_drop(const QList<QModelIndex> &dropped_list, const QModelIndex &target, bool *ok) {
    const ItemType target_type = (ItemType) target.data(ConsoleRole_Type).toInt();
    const QSet<ItemType> dropped_types =
    [&]() {
        QSet<ItemType> out;

        for (const QModelIndex &index : dropped_list) {
            const ItemType type = (ItemType) index.data(ConsoleRole_Type).toInt();
            out.insert(type);
        }

        return out;
    }();

    switch (target_type) {
        case ItemType_Unassigned: break;
        case ItemType_Object: {
            object_can_drop(dropped_list, target, dropped_types,ok);
            break;
        }
        case ItemType_PolicyRoot: break;
        case ItemType_Policy: break;
        case ItemType_QueryRoot: break;
        case ItemType_QueryFolder: break;
        case ItemType_QueryItem: break;
        case ItemType_LAST: break;
    }
}

void CentralWidget::on_items_dropped(const QList<QModelIndex> &dropped_list, const QModelIndex &target) {
    const ItemType target_type = (ItemType) target.data(ConsoleRole_Type).toInt();

    switch (target_type) {
        case ItemType_Unassigned: break;
        case ItemType_Object: {
            object_drop(console, dropped_list, target);
            break;
        }
        case ItemType_PolicyRoot: break;
        case ItemType_Policy: break;
        case ItemType_QueryRoot: break;
        case ItemType_QueryFolder: break;
        case ItemType_QueryItem: break;
        case ItemType_LAST: break;
    }
}

void CentralWidget::on_current_scope_changed() {
    const QModelIndex current_scope = console->get_current_scope_item();
    policy_results_widget->update(current_scope);

    update_description_bar();
}

void CentralWidget::refresh_head() {
    show_busy_indicator();

    console->refresh_scope(object_tree_head);

    hide_busy_indicator();
}

// TODO: currently calling this when current scope changes,
// but also need to call this when items are added/deleted
void CentralWidget::update_description_bar() {
    const QString text =
    [this]() {
        const QModelIndex current_scope = console->get_current_scope_item();
        const ItemType type = (ItemType) current_scope.data(ConsoleRole_Type).toInt();

        if (type == ItemType_Object) {
            const int results_count = console->get_current_results_count();
            const QString out = tr("%n object(s)", "", results_count);

            return out;
        } else {
            return QString();
        }
    }();

    console->set_description_bar_text(text);
}

void CentralWidget::add_actions_to_action_menu(QMenu *menu) {
    console_actions->add_to_menu(menu);

    for (const QList<QAction *> actions : item_actions.values()) {
        for (QAction *action : actions) {
            menu->addAction(action);
        }
    }

    menu->addSeparator();

    console->add_actions_to_action_menu(menu);
}

void CentralWidget::add_actions_to_navigation_menu(QMenu *menu) {
    console->add_actions_to_navigation_menu(menu);
}

void CentralWidget::add_actions_to_view_menu(QMenu *menu) {
    console->add_actions_to_view_menu(menu);

    menu->addSeparator();

    menu->addAction(open_filter_action);

    menu->addAction(show_noncontainers_action);

    #ifdef QT_DEBUG
    menu->addAction(dev_mode_action);
    #endif
}

void CentralWidget::fetch_scope_node(const QModelIndex &index) {
    const ItemType type = (ItemType) index.data(ConsoleRole_Type).toInt();

    if (type == ItemType_Object) {
        object_fetch(console, filter_dialog, index);
    } else if (type == ItemType_QueryItem) {
        query_item_fetch(console, index);
    }
}

void CentralWidget::enable_disable_helper(const bool disabled) {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();

    show_busy_indicator();

    const QList<QString> changed_objects = object_enable_disable(targets.keys(), disabled, this);

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    for (const QString &dn : changed_objects) {
        const QPersistentModelIndex index = targets[dn];

        auto update_helper =
        [=](const QPersistentModelIndex &the_index) {
            const bool is_scope = console->is_scope_item(the_index);

            if (is_scope) {
                QStandardItem *scope_item = console->get_scope_item(the_index);

                scope_item->setData(disabled, ObjectRole_AccountDisabled);
            } else {
                QList<QStandardItem *> results_row = console->get_results_row(the_index);
                results_row[0]->setData(disabled, ObjectRole_AccountDisabled);
            }
        };

        update_helper(index);

        const QPersistentModelIndex buddy = console->get_buddy(index);
        if (buddy.isValid()) {
            update_helper(buddy);
        }
    }

    update_actions_visibility();

    hide_busy_indicator();
}

// First, hide all actions, then show whichever actions are
// appropriate for current console selection
void CentralWidget::update_actions_visibility() {
    // Figure out what kind of types of items are selected
    const QList<QModelIndex> selected_indexes = console->get_selected_items();
    if (selected_indexes.isEmpty()) {
        return;
    }

    console_actions->update_actions_visibility(selected_indexes);
}

// Get selected indexes mapped to their DN's
QHash<QString, QPersistentModelIndex> CentralWidget::get_selected_dns_and_indexes() {
    QHash<QString, QPersistentModelIndex> out;

    const QList<QModelIndex> indexes = console->get_selected_items();
    for (const QModelIndex &index : indexes) {
        const QString dn = index.data(ObjectRole_DN).toString();
        out[dn] = QPersistentModelIndex(index);
    }

    return out;
}

QList<QString> CentralWidget::get_selected_dns() {
    const QHash<QString, QPersistentModelIndex> selected = get_selected_dns_and_indexes();

    return selected.keys();
}
