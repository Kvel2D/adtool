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

#include "tabs/telephones_tab.h"
#include "tabs/ui_telephones_tab.h"

#include "adldap.h"
#include "attribute_edits/string_large_edit.h"
#include "attribute_edits/string_other_edit.h"

TelephonesTab::TelephonesTab(QList<AttributeEdit *> *edit_list, QWidget *parent)
: QWidget(parent) {
    ui = new Ui::TelephonesTab();
    ui->setupUi(this);

    new StringOtherEdit(ui->home_phone_edit, ui->home_phone_button, ATTRIBUTE_HOME_PHONE, ATTRIBUTE_OTHER_HOME_PHONE, this);
    new StringOtherEdit(ui->pager_edit, ui->pager_button, ATTRIBUTE_PAGER, ATTRIBUTE_OTHER_PAGER, this);
    new StringOtherEdit(ui->mobile_edit, ui->mobile_button, ATTRIBUTE_MOBILE, ATTRIBUTE_OTHER_MOBILE, this);
    new StringOtherEdit(ui->fax_edit, ui->fax_button, ATTRIBUTE_FAX_NUMBER, ATTRIBUTE_OTHER_FAX_NUMBER, this);
    new StringOtherEdit(ui->ip_phone_edit, ui->ip_phone_button, ATTRIBUTE_IP_PHONE, ATTRIBUTE_OTHER_IP_PHONE, this);

    new StringLargeEdit(ui->notes_edit, ATTRIBUTE_INFO, this);
}

TelephonesTab::~TelephonesTab() {
    delete ui;
}
