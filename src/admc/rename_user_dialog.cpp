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

#include "rename_user_dialog.h"
#include "ui_rename_user_dialog.h"

#include "adldap.h"
#include "attribute_edits/sam_name_edit.h"
#include "attribute_edits/string_edit.h"
#include "attribute_edits/upn_edit.h"
#include "utils.h"
#include "settings.h"

RenameUserDialog::RenameUserDialog(AdInterface &ad, const QString &target_arg, QWidget *parent)
: RenameObjectDialog(parent) {
    ui = new Ui::RenameUserDialog();
    ui->setupUi(this);

    auto first_name_edit = new StringEdit(ui->first_name_edit, ATTRIBUTE_FIRST_NAME, this);
    auto last_name_edit = new StringEdit(ui->last_name_edit, ATTRIBUTE_LAST_NAME, this);
    auto display_name_edit = new StringEdit(ui->full_name_edit, ATTRIBUTE_DISPLAY_NAME, this);

    auto upn_edit = new UpnEdit(ui->upn_prefix_edit, ui->upn_suffix_edit, this);
    upn_edit->init_suffixes(ad);

    auto same_name_edit = new SamNameEdit(ui->sam_name_edit, ui->sam_name_domain_edit, this);

    init(ad, target_arg, ui->name_edit, edit_list);

    settings_setup_dialog_geometry(SETTING_rename_user_dialog_geometry, this);
}

RenameUserDialog::~RenameUserDialog() {
    delete ui;
}
