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

#include "attribute_edits/expiry_edit.h"

#include "adldap.h"
#include "attribute_edits/expiry_widget.h"
#include "globals.h"
#include "utils.h"

ExpiryEdit::ExpiryEdit(ExpiryWidget *edit_widget_arg, QObject *parent)
: AttributeEdit(parent) {
    edit_widget = edit_widget_arg;

    connect(
        edit_widget, &ExpiryWidget::edited,
        this, &AttributeEdit::edited);
}

void ExpiryEdit::load(AdInterface &ad, const AdObject &object) {
    UNUSED_ARG(ad);

    edit_widget->load(object);
}

void ExpiryEdit::set_read_only(const bool read_only) {
    edit_widget->set_read_only(read_only);
}

bool ExpiryEdit::apply(AdInterface &ad, const QString &dn) {
    return edit_widget->apply(ad, dn);
}
