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

#include "console.h"

#include "object_model.h"
#include "object_menu.h"
#include "utils.h"
#include "ad/ad_utils.h"
#include "properties_dialog.h"
#include "ad/ad_config.h"
#include "ad/ad_interface.h"
#include "ad/ad_object.h"
#include "ad/ad_filter.h"
#include "settings.h"
#include "filter_dialog.h"
#include "filter_widget/filter_widget.h"
#include "object_menu.h"
#include "status.h"
#include "rename_dialog.h"
#include "select_container_dialog.h"
#include "create_dialog.h"
#include "console_widget/console_widget.h"
#include "console_widget/results_view.h"

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

enum DropType {
    DropType_Move,
    DropType_AddToGroup,
    DropType_None
};

DropType get_object_drop_type(const QModelIndex &dropped, const QModelIndex &target);
bool object_should_be_in_scope(const AdObject &object);

Console::Console()
: QWidget()
{
    delete_action = new QAction(tr("&Delete"));
    rename_action = new QAction(tr("&Rename"));
    move_action = new QAction(tr("&Move"));
    open_filter_action = new QAction(tr("&Filter objects"));
    dev_mode_action = new QAction(tr("Dev mode"));
    show_noncontainers_action = new QAction(tr("&Show non-container objects in Console tree"));

    filter_dialog = nullptr;

    console_widget = new ConsoleWidget();

    object_results = new ResultsView(this);
    object_results->detail_view()->header()->setDefaultSectionSize(200);
    // TODO: not sure how to do this. View headers dont even
    // have sections until their models are loaded.
    // SETTINGS()->setup_header_state(object_results->header(),
    // VariantSetting_ResultsHeader);
    
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
    layout->addWidget(console_widget);

    // Refresh head when settings affecting the filter
    // change. This reloads the model with an updated filter
    const BoolSettingSignal *advanced_features = SETTINGS()->get_bool_signal(BoolSetting_AdvancedFeatures);
    connect(
        advanced_features, &BoolSettingSignal::changed,
        this, &Console::refresh_head);

    const BoolSettingSignal *show_non_containers = SETTINGS()->get_bool_signal(BoolSetting_ShowNonContainersInConsoleTree);
    connect(
        show_non_containers, &BoolSettingSignal::changed,
        this, &Console::refresh_head);

    const BoolSettingSignal *dev_mode_signal = SETTINGS()->get_bool_signal(BoolSetting_DevMode);
    connect(
        dev_mode_signal, &BoolSettingSignal::changed,
        this, &Console::refresh_head);

    SETTINGS()->connect_toggle_widget(console_widget->get_scope_view(), BoolSetting_ShowConsoleTree);
    SETTINGS()->connect_toggle_widget(console_widget->get_description_bar(), BoolSetting_ShowResultsHeader);

    SETTINGS()->connect_action_to_bool_setting(dev_mode_action, BoolSetting_DevMode);
    SETTINGS()->connect_action_to_bool_setting(show_noncontainers_action, BoolSetting_ShowNonContainersInConsoleTree);

    connect(
        open_filter_action, &QAction::triggered,
        this, &Console::open_filter);
    connect(
        delete_action, &QAction::triggered,
        this, &Console::delete_objects);
    connect(
        rename_action, &QAction::triggered,
        this, &Console::rename);
    connect(
        move_action, &QAction::triggered,
        this, &Console::move);
    connect(
        console_widget, &ConsoleWidget::current_scope_item_changed,
        this, &Console::update_description_bar);
    connect(
        console_widget, &ConsoleWidget::results_count_changed,
        this, &Console::update_description_bar);
    connect(
        console_widget, &ConsoleWidget::action_menu,
        this, &Console::on_action_menu);
    connect(
        console_widget, &ConsoleWidget::view_menu,
        this, &Console::on_view_menu);
    connect(
        console_widget, &ConsoleWidget::item_fetched,
        this, &Console::fetch_scope_node);
    connect(
        console_widget, &ConsoleWidget::items_can_drop,
        this, &Console::on_items_can_drop);
    connect(
        console_widget, &ConsoleWidget::items_dropped,
        this, &Console::on_items_dropped);
    connect(
        console_widget, &ConsoleWidget::properties_requested,
        this, &Console::on_properties_requested);
}

void Console::go_online(AdInterface &ad) {
    // NOTE: filter dialog requires a connection to load
    // display strings from adconfig so create it here
    filter_dialog = new FilterDialog(this);
    connect(
        filter_dialog, &QDialog::accepted,
        this, &Console::refresh_head);

    // NOTE: Header labels are from ADCONFIG, so have to get them
    // after going online
    object_results_id = console_widget->register_results(object_results, object_model_header_labels(), object_model_default_columns());

    const QString head_dn = ADCONFIG()->domain_head();
    const AdObject head_object = ad.search_object(head_dn);

    QStandardItem *item = console_widget->add_scope_item(object_results_id, ScopeNodeType_Dynamic, QModelIndex());

    scope_head_index = QPersistentModelIndex(item->index());

    setup_scope_item(item, head_object);
}

void Console::open_filter() {
    if (filter_dialog != nullptr) {
        filter_dialog->open();
    }
}

void Console::delete_objects() {
    const QList<QModelIndex> indexes = console_widget->get_selected_items();

    const QString text = QString(QCoreApplication::translate("object_menu", "Are you sure you want to delete %1?")).arg("targets_display_string");
    // const QString text = QString(QCoreApplication::translate("object_menu", "Are you sure you want to delete %1?")).arg(targets_display_string(indexes));
    const bool confirmed = confirmation_dialog(text, this);

    if (confirmed) {
        AdInterface ad;
        if (ad_failed(ad)) {
            return;
        }

        for (const QModelIndex &index : indexes) {
            const QString dn = index.data(ObjectRole_DN).toString();
            const bool delete_success = ad.object_delete(dn);

            if (delete_success) {
                console_widget->delete_item(index);
            }
        }

        STATUS()->display_ad_messages(ad, this);
    }
}

void Console::on_properties_requested() {
    const QList<QModelIndex> indexes = console_widget->get_selected_items();
    if (indexes.size() != 1) {
        return;
    }
    const QModelIndex index = indexes[0];

    const QString dn = index.data(ObjectRole_DN).toString();

    PropertiesDialog *dialog = PropertiesDialog::open_for_target(dn);

    connect(
        dialog, &PropertiesDialog::applied,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const AdObject updated_object = ad.search_object(dn);
            update_console_item(index, updated_object);
        });
}

void Console::rename() {
    const QList<QModelIndex> indexes = console_widget->get_selected_items();
    if (indexes.size() != 1) {
        return;
    }
    const QModelIndex index = indexes[0];

    const QString dn = index.data(ObjectRole_DN).toString();

    auto dialog = new RenameDialog(dn, this);

    connect(
        dialog, &RenameDialog::accepted,
        [this, dialog, index]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QString new_dn = dialog->get_new_dn();
            const AdObject updated_object = ad.search_object(new_dn);
            update_console_item(index, updated_object);
        });

    dialog->open();
}

void Console::create(const QString &object_class) {
    const QList<QModelIndex> selected_indexes = console_widget->get_selected_items();
    if (selected_indexes.size() == 0) {
        return;
    }

    const QModelIndex selected_index = selected_indexes[0];
    const QString selected_dn = selected_index.data(ObjectRole_DN).toString();

    const auto dialog = new CreateDialog(selected_dn, object_class, this);

    connect(
        dialog, &CreateDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QString new_dn = dialog->get_created_dn();
            const AdObject new_object = ad.search_object(new_dn);

            add_object_to_console(new_object, selected_index);
        });

    dialog->open();
}

void Console::move() {
    const QList<QModelIndex> selected_indexes = console_widget->get_selected_items();
    if (selected_indexes.size() == 0) {
        return;
    }

    auto dialog = new SelectContainerDialog(this);

    connect(
        dialog, &SelectContainerDialog::accepted,
        [=]() {
            const QString selected_dn = dialog->get_selected();

            const QModelIndex target =
            [=]() {
                const QList<QModelIndex> selected_in_scope = console_widget->search_scope_by_role(ObjectRole_DN, selected_dn);

                // NOTE: there's a crazy bug possible here
                // where a query item has some role with
                // same index as ObjectRole_DN and it's
                // value is also set to the same dn as target.
                if (selected_in_scope.size() == 1) {
                    return selected_in_scope[0];
                } else {
                    return QModelIndex();
                }
            }();

            AdInterface ad;
            if (ad_connected(ad)) {
                for (const QModelIndex &index : selected_indexes) {
                    move_object_in_console(ad, index, selected_dn, target);
                }

                STATUS()->display_ad_messages(ad, this);
            }

            console_widget->sort_scope();
        });

    dialog->open();
}

// NOTE: only check if object can be dropped if dropping a
// single object, because when dropping multiple objects it
// is ok for some objects to successfully drop and some to
// fail. For example, if you drop users together with OU's
// onto a group, users will be added to that group while OU
// will fail to drop.
void Console::on_items_can_drop(const QList<QModelIndex> &dropped_list, const QModelIndex &target, bool *ok) {
    if (dropped_list.size() != 1) {
        *ok = true;
        return;
    } else {
        const QModelIndex dropped = dropped_list[0];

        const DropType drop_type = get_object_drop_type(dropped, target);
        const bool can_drop = (drop_type != DropType_None);

        *ok = can_drop;
    }
}

void Console::on_items_dropped(const QList<QModelIndex> &dropped_list, const QModelIndex &target) {
    const QString target_dn = target.data(ObjectRole_DN).toString();

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    for (const QModelIndex &dropped : dropped_list) {
        const QString dropped_dn = dropped.data(ObjectRole_DN).toString();
        const DropType drop_type = get_object_drop_type(dropped, target);

        switch (drop_type) {
            case DropType_Move: {
                move_object_in_console(ad, dropped, target_dn, target);

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

    STATUS()->display_ad_messages(ad, nullptr);

    console_widget->sort_scope();
}

void Console::refresh_head() {
    console_widget->refresh_scope(scope_head_index);
}

// TODO: currently calling this when current scope changes,
// but also need to call this when items are added/deleted
void Console::update_description_bar() {
    const QString text =
    [this]() {
        const int results_count = console_widget->get_current_results_count();
        const QString out = tr("%n object(s)", "", results_count);

        return out;
    }();

    console_widget->set_description_bar_text(text);
}

void Console::on_action_menu(QMenu *menu) {
    QMenu *submenu_new = new QMenu(tr("New"));
    static const QList<QString> create_classes = {
        CLASS_USER,
        CLASS_COMPUTER,
        CLASS_OU,
        CLASS_GROUP,
    };
    for (const auto &object_class : create_classes) {
        const QString action_text = ADCONFIG()->get_class_display_name(object_class);

        submenu_new->addAction(action_text,
            [=]() {
                create(object_class);
            });
    }

    ObjectMenuData menu_data;
    menu_data.rename = rename_action;
    menu_data.move = move_action;
    menu_data.delete_object = delete_action;
    menu_data.new_menu = submenu_new;
    menu_data.properties_already_added = true;

    const QList<QModelIndex> selected = console_widget->get_selected_items();

    add_object_actions_to_menu(menu, selected, this, true, menu_data);
}

void Console::on_view_menu(QMenu *menu) {
    menu->addAction(open_filter_action);

    // NOTE: insert separator between non-checkbox actions
    // and checkbox actions
    menu->addSeparator();

    menu->addAction(show_noncontainers_action);

    #ifdef QT_DEBUG
    menu->addAction(dev_mode_action);
    #endif
}

// Load children of this item in scope tree
// and load results linked to this scope item
void Console::fetch_scope_node(const QModelIndex &index) {
    show_busy_indicator();

    const bool dev_mode = SETTINGS()->get_bool(BoolSetting_DevMode);

    //
    // Search object's children
    //
    const QString filter =
    [=]() {
        const QString user_filter = filter_dialog->filter_widget->get_filter();

        const QString is_container = is_container_filter();

        // NOTE: OR user filter with containers filter so that container objects are always shown, even if they are filtered out by user filter
        QString out = filter_OR({user_filter, is_container});

        // Hide advanced view only" objects if advanced view
        // setting is off
        const bool advanced_features_OFF = !SETTINGS()->get_bool(BoolSetting_AdvancedFeatures);
        if (advanced_features_OFF) {
            const QString advanced_features = filter_CONDITION(Condition_NotEquals, ATTRIBUTE_SHOW_IN_ADVANCED_VIEW_ONLY, "true");
            out = filter_OR({out, advanced_features});
        }

        // OR filter with some dev mode object classes, so that they show up no matter what when dev mode is on
        if (dev_mode) {
            const QList<QString> schema_classes = {
                "classSchema",
                "attributeSchema",
                "displaySpecifier",
            };

            QList<QString> class_filters;
            for (const QString &object_class : schema_classes) {
                const QString class_filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_CLASS, object_class);
                class_filters.append(class_filter);
            }

            out = filter_OR({out, filter_OR(class_filters)});
        }

        return out;
    }();

    const QList<QString> search_attributes =
    []() {
        QList<QString> out;

        out += ADCONFIG()->get_columns();

        // NOTE: load_object_row() needs this for loading group type/scope
        out += ATTRIBUTE_GROUP_TYPE;

        // NOTE: system flags are needed for drag and drop logic
        out += ATTRIBUTE_SYSTEM_FLAGS;

        return out;
    }();

    const QString dn = index.data(ObjectRole_DN).toString();

    // TODO: handle connect/search failure
    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    QHash<QString, AdObject> search_results = ad.search(filter, search_attributes, SearchScope_Children, dn);

    // Dev mode
    // NOTE: configuration and schema objects are hidden so that they don't show up in regular searches. Have to use search_object() and manually add them to search results.
    if (dev_mode) {
        const QString search_base = ADCONFIG()->domain_head();
        const QString configuration_dn = ADCONFIG()->configuration_dn();
        const QString schema_dn = ADCONFIG()->schema_dn();

        if (dn == search_base) {
            search_results[configuration_dn] = ad.search_object(configuration_dn);
        } else if (dn == configuration_dn) {
            search_results[schema_dn] = ad.search_object(schema_dn);
        }
    }

    //
    // Add items to scope and results
    //

    for (const AdObject &object : search_results.values()) {
        add_object_to_console(object, index);
    }

    // TODO: make sure to sort scope everywhere it should be
    // sorted
    console_widget->sort_scope();

    hide_busy_indicator();
}

void Console::setup_scope_item(QStandardItem *item, const AdObject &object) {
    const QString name =
    [&]() {
        const QString dn = object.get_dn();
        return dn_get_name(dn);
    }();

    item->setText(name);

    console_widget->set_has_properties(item->index(), true);

    load_object_item_data(item, object);
    disable_drag_if_object_cant_be_moved({item}, object);
}

void Console::setup_results_row(const QList<QStandardItem *> row, const AdObject &object) {
    console_widget->set_has_properties(row[0]->index(), true);
    load_object_row(row, object);
    disable_drag_if_object_cant_be_moved(row, object);
}

// NOTE: "containers" referenced here don't mean objects
// with "container" object class. Instead it means all the
// objects that can have children(some of which are not
// "container" class).
bool object_should_be_in_scope(const AdObject &object) {
    const bool is_container =
    [=]() {
        const QList<QString> filter_containers = ADCONFIG()->get_filter_containers();
        const QString object_class = object.get_string(ATTRIBUTE_OBJECT_CLASS);

        return filter_containers.contains(object_class);
    }();

    const bool show_non_containers_ON = SETTINGS()->get_bool(BoolSetting_ShowNonContainersInConsoleTree);

    return (is_container || show_non_containers_ON);
}

void Console::add_object_to_console(const AdObject &object, const QModelIndex &parent) {
    const bool should_be_in_scope = object_should_be_in_scope(object);

    if (should_be_in_scope) {
        QStandardItem *scope_item;
        QList<QStandardItem *> results_row;
        console_widget->add_buddy_scope_and_results(object_results_id, ScopeNodeType_Dynamic, parent, &scope_item, &results_row);

        setup_scope_item(scope_item, object);
        setup_results_row(results_row, object);
    } else {
        const QList<QStandardItem *> results_row = console_widget->add_results_row(parent);
        setup_results_row(results_row, object);
    }
}

// Moves object on AD server and if that succeeds, also
// moves it in console. Note that both new parent dn and
// index are passed because new_parent_index may be invalid.
// For example if it's not loaded into console but was
// selected through SelectContainDialog. In that case we
// only delete object from it's old location and do nothing
// for new location. The object will be loaded at new
// location when it's parent is loaded.
void Console::move_object_in_console(AdInterface &ad, const QModelIndex &old_index, const QString &new_parent_dn, const QModelIndex &new_parent_index) {
    const QString old_dn = old_index.data(ObjectRole_DN).toString();
    const QString new_dn = dn_move(old_dn, new_parent_dn);

    const bool move_success = ad.object_move(old_dn, new_parent_dn);

    if (move_success) {
        console_widget->delete_item(old_index);

        if (new_parent_index.isValid()) {
            const AdObject updated_object = ad.search_object(new_dn);
            add_object_to_console(updated_object, new_parent_index);
        }
    }
}

void Console::update_console_item(const QModelIndex &index, const AdObject &object) {
    auto update_helper =
    [this, object](const QModelIndex &the_index) {
        const bool is_scope = console_widget->is_scope_item(the_index);

        if (is_scope) {
            QStandardItem *scope_item = console_widget->get_scope_item(the_index);

            const QString old_dn = scope_item->data(ObjectRole_DN).toString();
            const bool dn_changed = (old_dn != object.get_dn());

            setup_scope_item(scope_item, object);

            // NOTE: if dn changed, then this change affects
            // this item's children, so have to refresh to
            // reload children.
            if (dn_changed) {
                console_widget->refresh_scope(the_index);
            }
        } else {
            QList<QStandardItem *> results_row = console_widget->get_results_row(the_index);
            load_object_row(results_row, object);
        }
    };

    update_helper(index);

    const QModelIndex buddy = console_widget->get_buddy(index);
    if (buddy.isValid()) {
        update_helper(buddy);
    }
}

void Console::disable_drag_if_object_cant_be_moved(const QList<QStandardItem *> &items, const AdObject &object) {
    const bool cannot_move = object.get_system_flag(SystemFlagsBit_CannotMove);

    for (auto item : items) {
        item->setDragEnabled(!cannot_move);
    }
}

// Determine what kind of drop type is dropping this object
// onto target. If drop type is none, then can't drop this
// object on this target.
DropType get_object_drop_type(const QModelIndex &dropped, const QModelIndex &target) {
    const bool dropped_is_target =
    [&]() {
        const QString dropped_dn = dropped.data(ObjectRole_DN).toString();
        const QString target_dn = target.data(ObjectRole_DN).toString();

        return (dropped_dn == target_dn);
    }();

    const QList<QString> dropped_classes = dropped.data(ObjectRole_ObjectClasses).toStringList();
    const QList<QString> target_classes = target.data(ObjectRole_ObjectClasses).toStringList();

    const bool dropped_is_user = dropped_classes.contains(CLASS_USER);
    const bool dropped_is_group = dropped_classes.contains(CLASS_GROUP);
    const bool target_is_group = target_classes.contains(CLASS_GROUP);

    if (dropped_is_target) {
        return DropType_None;
    } else if (dropped_is_user && target_is_group) {
        return DropType_AddToGroup;
    } else if (dropped_is_group && target_is_group) {
        return DropType_AddToGroup;
    } else {
        const QList<QString> dropped_superiors = ADCONFIG()->get_possible_superiors(dropped_classes);

        const bool target_is_valid_superior =
        [&]() {
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
