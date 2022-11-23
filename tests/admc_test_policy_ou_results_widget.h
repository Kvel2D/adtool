/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2022 BaseALT Ltd.
 * Copyright (C) 2020-2022 Dmitry Degtyarev
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

#ifndef ADMC_TEST_POLICY_OU_RESULTS_WIDGET_H
#define ADMC_TEST_POLICY_OU_RESULTS_WIDGET_H

#include "admc_test.h"

class PolicyOUResultsWidget;
class QTreeView;
class QStandardItemModel;
class QSortFilterProxyModel;

class ADMCTestPolicyOUResultsWidget : public ADMCTest {
    Q_OBJECT

private slots:
    void initTestCase() override;
    void cleanupTestCase() override;

    void init() override;

    void load_empty();
    void load();
    void remove_link();
    void move_up();
    void move_down();

private:
    PolicyOUResultsWidget *widget;
    QTreeView *view;
    QStandardItemModel *model;
    QSortFilterProxyModel *proxy_model;
    QString ou_dn;
};

#endif /* ADMC_TEST_POLICY_OU_RESULTS_WIDGET_H */
