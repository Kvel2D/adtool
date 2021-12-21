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

// TODO: this tab is kind of useless, policy results
// widget does more and allows editing links

#include "tabs/gpo_links_tab.h"
#include "tabs/ui_gpo_links_tab.h"

#include "adldap.h"
#include "globals.h"
#include "properties_dialog.h"
#include "settings.h"
#include "utils.h"

#include <algorithm>

#include <QStandardItemModel>

enum GpoLinksColumn {
    GpoLinksColumn_Name,
    GpoLinksColumn_COUNT,
};

enum GpoLinksRole {
    GpoLinksRole_DN = Qt::UserRole + 1,
};

GpoLinksTab::GpoLinksTab(QList<AttributeEdit *> *edit_list, QWidget *parent) 
: QWidget(parent) {
    ui = new Ui::GpoLinksTab();
    ui->setupUi(this);

    new GpoLinksTabEdit(edit_list, ui, this);
}

GpoLinksTabEdit::GpoLinksTabEdit(Ui::GpoLinksTab *ui_arg, QObject *parent)
: AttributeEdit(parent) {
    ui = ui_arg;

    model = new QStandardItemModel(0, GpoLinksColumn_COUNT, this);
    set_horizontal_header_labels_from_map(model,
        {
            {GpoLinksColumn_Name, tr("Name")},
        });

    ui->view->setModel(model);

    PropertiesDialog::open_when_view_item_activated(ui->view, GpoLinksRole_DN);

    settings_restore_header_state(SETTING_gpo_links_tab_header_state, ui->view->header());
}

GpoLinksTab::~GpoLinksTab() {
    settings_save_header_state(SETTING_gpo_links_tab_header_state, ui->view->header());

    delete ui;
}

void GpoLinksTabEdit::load(AdInterface &ad, const AdObject &object) {
    const QString base = g_adconfig->domain_dn();
    const SearchScope scope = SearchScope_All;
    const QList<QString> attributes = {ATTRIBUTE_NAME};
    const QString filter = filter_CONDITION(Condition_Contains, ATTRIBUTE_GPLINK, object.get_dn());
    const QHash<QString, AdObject> results = ad.search(base, scope, filter, attributes);

    // Sort objects by dn(identical to sorting by name)
    QList<QString> dns = results.keys();
    std::sort(dns.begin(), dns.end());

    for (auto dn : dns) {
        const AdObject linked_object = results[dn];
        const QString name = linked_object.get_string(ATTRIBUTE_NAME);

        const QList<QStandardItem *> row = make_item_row(GpoLinksColumn_COUNT);
        row[GpoLinksColumn_Name]->setText(name);

        set_data_for_row(row, dn, GpoLinksRole_DN);

        model->appendRow(row);
    }

    model->sort(GpoLinksColumn_Name);
}

void GpoLinksTabEdit::set_read_only(const bool read_only) {
    UNUSED_ARG(read_only);
}

bool GpoLinksTabEdit::apply(AdInterface &ad, const QString &dn) {
    UNUSED_ARG(ad);
    UNUSED_ARG(dn);

    return true;
}
