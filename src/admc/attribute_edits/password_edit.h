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

#ifndef PASSWORD_EDIT_H
#define PASSWORD_EDIT_H

#include "attribute_edits/attribute_edit.h"

class QLineEdit;

class PasswordEdit final : public AttributeEdit {
    Q_OBJECT
public:
    PasswordEdit(QLineEdit *edit_arg, QLineEdit *confirm_edit_arg, QObject *parent);
    DECL_ATTRIBUTE_EDIT_VIRTUALS();

    bool verify(AdInterface &ad, const QString &dn) const override;

    QLineEdit *get_edit() const;
    QLineEdit *get_confirm_edit() const;

private:
    QLineEdit *edit;
    QLineEdit *confirm_edit;
};

#endif /* PASSWORD_EDIT_H */
