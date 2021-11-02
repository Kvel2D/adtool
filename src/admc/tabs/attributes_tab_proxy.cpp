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

#include "tabs/attributes_tab_proxy.h"

#include "adldap.h"
#include "editors/bool_editor.h"
#include "editors/datetime_editor.h"
#include "editors/multi_editor.h"
#include "editors/octet_editor.h"
#include "editors/string_editor.h"
#include "globals.h"
#include "settings.h"
#include "tabs/attributes_tab.h"
#include "tabs/attributes_tab_filter_menu.h"
#include "utils.h"

#include <QAction>
#include <QDebug>
#include <QHeaderView>
#include <QMenu>
#include <QStandardItemModel>

QString attribute_type_display_string(const AttributeType type);

AttributesTabProxy::AttributesTabProxy(AttributesTabFilterMenu *filter_menu_arg, QObject *parent)
: QSortFilterProxyModel(parent) {
    filter_menu = filter_menu_arg;
}

void AttributesTabProxy::load(const AdObject &object) {
    const QList<QString> object_classes = object.get_strings(ATTRIBUTE_OBJECT_CLASS);
    mandatory_attributes = g_adconfig->get_mandatory_attributes(object_classes).toSet();
    optional_attributes = g_adconfig->get_optional_attributes(object_classes).toSet();

    set_attributes = object.attributes().toSet();
}

bool AttributesTabProxy::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    auto source = sourceModel();
    const QString attribute = source->index(source_row, AttributesColumn_Name, source_parent).data().toString();

    const bool system_only = g_adconfig->get_attribute_is_system_only(attribute);
    const bool unset = !set_attributes.contains(attribute);
    const bool mandatory = mandatory_attributes.contains(attribute);
    const bool optional = optional_attributes.contains(attribute);

    if (!filter_menu->filter_is_enabled(AttributeFilter_Unset) && unset) {
        return false;
    }

    if (!filter_menu->filter_is_enabled(AttributeFilter_Mandatory) && mandatory) {
        return false;
    }

    if (!filter_menu->filter_is_enabled(AttributeFilter_Optional) && optional) {
        return false;
    }

    if (filter_menu->filter_is_enabled(AttributeFilter_ReadOnly) && system_only) {
        const bool constructed = g_adconfig->get_attribute_is_constructed(attribute);
        const bool backlink = g_adconfig->get_attribute_is_backlink(attribute);

        if (!filter_menu->filter_is_enabled(AttributeFilter_SystemOnly) && !constructed && !backlink) {
            return false;
        }

        if (!filter_menu->filter_is_enabled(AttributeFilter_Constructed) && constructed) {
            return false;
        }

        if (!filter_menu->filter_is_enabled(AttributeFilter_Backlink) && backlink) {
            return false;
        }
    }

    if (!filter_menu->filter_is_enabled(AttributeFilter_ReadOnly) && system_only) {
        return false;
    }

    return true;
}