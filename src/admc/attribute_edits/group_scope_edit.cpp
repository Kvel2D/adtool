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

#include "attribute_edits/group_scope_edit.h"

#include "adldap.h"
#include "utils.h"

#include <QComboBox>
#include <QFormLayout>

GroupScopeEdit::GroupScopeEdit(QComboBox *combo_arg, QObject *parent)
: AttributeEdit(parent) {
    combo = combo_arg;

    for (int i = 0; i < GroupScope_COUNT; i++) {
        const GroupScope type = (GroupScope) i;
        const QString type_string = group_scope_string(type);

        combo->addItem(type_string, (int) type);
    }

    connect(
        combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AttributeEdit::edited);
}

void GroupScopeEdit::load(AdInterface &ad, const AdObject &object) {
    UNUSED_ARG(ad);

    const GroupScope scope = object.get_group_scope();

    combo->setCurrentIndex((int) scope);
}

void GroupScopeEdit::set_read_only(const bool read_only) {
    combo->setDisabled(read_only);
}

bool GroupScopeEdit::apply(AdInterface &ad, const QString &dn) {
    const GroupScope new_value = (GroupScope) combo->currentData().toInt();
    const bool success = ad.group_set_scope(dn, new_value);

    return success;
}
