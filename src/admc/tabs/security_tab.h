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

#ifndef SECURITY_TAB_H
#define SECURITY_TAB_H

#include "tabs/properties_tab.h"

#include "ad_defines.h"

#include <QDialog>

class RightsSortModel;
class QStandardItemModel;
class QStandardItem;
class SecurityTabPrivate;
class SecurityDescriptor;
struct security_descriptor;

enum AceColumn {
    AceColumn_Name,
    AceColumn_Allowed,
    AceColumn_Denied,

    AceColumn_COUNT,
};

namespace Ui {
class SecurityTab;
}

class SecurityTab final : public PropertiesTab {
    Q_OBJECT

public:
    Ui::SecurityTab *ui;

    SecurityTab();
    ~SecurityTab();

    void load(AdInterface &ad, const AdObject &object) override;
    bool verify(AdInterface &ad, const QString &target) const override;
    bool apply(AdInterface &ad, const QString &target) override;

private:
    SecurityTabPrivate *d;
    QStandardItemModel *trustee_model;
    QStandardItemModel *rights_model;
    RightsSortModel *rights_sort_model;
    bool is_policy;
    bool ignore_item_changed_signal;
    bool modified;
    security_descriptor *sd;
    QList<QString> target_class_list;

    void on_item_changed(QStandardItem *item);
    void on_add_trustee_button();
    void on_remove_trustee_button();
    void add_trustees(const QList<QByteArray> &sid_list, AdInterface &ad);
    void on_add_well_known_trustee();
    void load_current_sd(AdInterface &ad);
    void load_rights_model();
    QByteArray get_current_trustee() const;

    void remove_right(const QByteArray &trustee, const uint32_t access_mask, const QByteArray &object_type, const bool allow);
    void add_right(const QByteArray &trustee, const uint32_t access_mask, const QByteArray &object_type, const bool allow);
};

#endif /* SECURITY_TAB_H */
