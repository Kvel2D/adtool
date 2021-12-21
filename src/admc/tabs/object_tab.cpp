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

#include "tabs/object_tab.h"
#include "tabs/ui_object_tab.h"

#include "adldap.h"
#include "attribute_edits/datetime_edit.h"
#include "attribute_edits/protect_deletion_edit.h"
#include "attribute_edits/string_edit.h"

#include <QFormLayout>

ObjectTab::ObjectTab(QList<AttributeEdit *> *edit_list, QWidget *parent)
: QWidget(parent) {
    ui = new Ui::ObjectTab();
    ui->setupUi(this);

    QList<AttributeEdit *> my_edit_list;

    new StringEdit(ui->dn_edit, ATTRIBUTE_DN, &my_this);
    new StringEdit(ui->class_edit, ATTRIBUTE_OBJECT_CLASS, &my_this);

    new DateTimeEdit(ui->created_edit, ATTRIBUTE_WHEN_CREATED, &my_this);
    new DateTimeEdit(ui->changed_edit, ATTRIBUTE_WHEN_CHANGED, &my_this);

    new StringEdit(ui->usn_created_edit, ATTRIBUTE_USN_CREATED, &my_this);
    new StringEdit(ui->usn_changed_edit, ATTRIBUTE_USN_CHANGED, &my_this);

    new ProtectDeletionEdit(ui->deletion_check, &my_this);

    AttributeEdit::set_read_only(my_edit_list, true);

    edit_list->append(my_edit_list);
}

ObjectTab::~ObjectTab() {
    delete ui;
}
