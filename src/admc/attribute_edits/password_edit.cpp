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

#include "attribute_edits/password_edit.h"

#include "adldap.h"
#include "globals.h"
#include "utils.h"

#include <QLineEdit>
#include <QTextCodec>

PasswordEdit::PasswordEdit(QLineEdit *edit_arg, QLineEdit *confirm_edit_arg, QObject *parent)
: AttributeEdit(parent) {
    edit = edit_arg;
    confirm_edit = confirm_edit_arg;

    limit_edit(edit, ATTRIBUTE_PASSWORD);
    limit_edit(confirm_edit, ATTRIBUTE_PASSWORD);

    connect(
        edit, &QLineEdit::textChanged,
        this, &AttributeEdit::edited);
}

void PasswordEdit::load(AdInterface &ad, const AdObject &object) {
    UNUSED_ARG(ad);
    UNUSED_ARG(object);

    edit->clear();
    confirm_edit->clear();
}

void PasswordEdit::set_read_only(const bool read_only) {
    edit->setDisabled(read_only);
    confirm_edit->setDisabled(read_only);
}

bool PasswordEdit::verify(AdInterface &ad, const QString &) const {
    UNUSED_ARG(ad);

    const QString pass = edit->text();
    const QString confirm_pass = confirm_edit->text();
    if (pass != confirm_pass) {
        const QString error_text = QString(tr("Passwords don't match!"));
        message_box_warning(edit, tr("Error"), error_text);

        return false;
    }

    const auto codec = QTextCodec::codecForName("UTF-16LE");
    const bool can_encode = codec->canEncode(pass);
    if (!can_encode) {
        const QString error_text = QString(tr("Password contains invalid characters"));
        message_box_warning(edit, tr("Error"), error_text);

        return false;
    }

    return true;
}

bool PasswordEdit::apply(AdInterface &ad, const QString &dn) {
    const QString new_value = edit->text();

    const bool success = ad.user_set_pass(dn, new_value);

    return success;
}

QLineEdit *PasswordEdit::get_edit() const {
    return edit;
}

QLineEdit *PasswordEdit::get_confirm_edit() const {
    return confirm_edit;
}
