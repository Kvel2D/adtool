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

#include "admc_test_object_menu.h"

#include "adldap.h"
#include "console_impls/object_impl.h"
#include "create_user_dialog.h"
#include "create_group_dialog.h"
#include "create_ou_dialog.h"
#include "create_computer_dialog.h"
#include "filter_widget/filter_widget_advanced_tab.h"
#include "filter_widget/filter_widget_simple_tab.h"
#include "find_object_dialog.h"
#include "find_widget.h"
#include "rename_user_dialog.h"
#include "select_container_dialog.h"
#include "select_object_advanced_dialog.h"
#include "select_object_dialog.h"
#include "utils.h"
#include "ui_create_user_dialog.h"
#include "ui_create_group_dialog.h"
#include "ui_create_ou_dialog.h"
#include "ui_create_computer_dialog.h"
#include "ui_rename_user_dialog.h"
#include "ui_find_object_dialog.h"
#include "ui_find_widget.h"
#include "filter_widget/ui_filter_widget.h"
#include "filter_widget/ui_filter_widget_advanced_tab.h"
#include "filter_widget/ui_filter_widget_simple_tab.h"

#include <QComboBox>
#include <QDebug>
#include <QLineEdit>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTest>
#include <QTreeView>

void ADMCTestObjectMenu::object_menu_new_user() {
    const QString name = TEST_USER;
    const QString logon_name = TEST_USER_LOGON;
    const QString password = TEST_PASSWORD;
    const QString parent = test_arena_dn();
    const QString dn = test_object_dn(name, CLASS_USER);

    // Create user
    auto create_dialog = new CreateUserDialog(parent_widget);
    create_dialog->set_parent_dn(parent);
    create_dialog->open();
    QVERIFY(QTest::qWaitForWindowExposed(create_dialog, 1000));

    create_dialog->ui->name_edit->setText(name);
    create_dialog->ui->sam_name_edit->setText(logon_name);
    create_dialog->ui->password_main_edit->setText(password);
    create_dialog->ui->password_confirm_edit->setText(password);

    create_dialog->accept();

    QVERIFY2(object_exists(dn), "Created user doesn't exist");

    QVERIFY(true);
}

void ADMCTestObjectMenu::object_menu_new_ou() {
    const QString name = TEST_OU;
    const QString parent = test_arena_dn();
    const QString dn = test_object_dn(name, CLASS_OU);

    // Create ou
    auto create_dialog = new CreateOUDialog(parent_widget);
    create_dialog->set_parent_dn(parent);
    create_dialog->open();
    QVERIFY(QTest::qWaitForWindowExposed(create_dialog, 1000));

    create_dialog->ui->name_edit->setText(name);

    create_dialog->accept();

    QVERIFY2(object_exists(dn), "Created OU doesn't exist");

    QVERIFY(true);
}

void ADMCTestObjectMenu::object_menu_new_computer() {
    const QString object_class = CLASS_COMPUTER;
    const QString name = TEST_COMPUTER;
    const QString parent = test_arena_dn();
    const QString dn = test_object_dn(name, object_class);

    // Open create dialog
    auto create_dialog = new CreateComputerDialog(parent_widget);
    create_dialog->set_parent_dn(parent);
    create_dialog->open();
    QVERIFY(QTest::qWaitForWindowExposed(create_dialog, 1000));

    create_dialog->ui->name_edit->setText(name);
    create_dialog->ui->sam_name_edit->setText(name);

    create_dialog->accept();

    QVERIFY2(object_exists(dn), "Created computer doesn't exist");

    QVERIFY(true);
}

void ADMCTestObjectMenu::object_menu_new_group() {
    const QString object_class = CLASS_GROUP;
    const QString name = TEST_GROUP;
    const QString parent = test_arena_dn();
    const QString dn = test_object_dn(name, object_class);

    // Open create dialog
    auto create_dialog = new CreateGroupDialog(parent_widget);
    create_dialog->set_parent_dn(parent);
    create_dialog->open();
    QVERIFY(QTest::qWaitForWindowExposed(create_dialog, 1000));

    create_dialog->ui->name_edit->setText(name);
    create_dialog->ui->sam_name_edit->setText(name);

    create_dialog->accept();

    QVERIFY2(object_exists(dn), "Created group doesn't exist");

    QVERIFY(true);
}

void ADMCTestObjectMenu::object_menu_find_simple() {
    const QString parent = test_arena_dn();

    const QString user_name = TEST_USER;
    const QString user_dn = test_object_dn(user_name, CLASS_USER);

    // Create test user
    const bool create_user_success = ad.object_add(user_dn, CLASS_USER);
    QVERIFY2(create_user_success, "Failed to create user");
    QVERIFY2(object_exists(user_dn), "Created user doesn't exist");

    auto find_dialog = new FindObjectDialog(filter_classes, parent, parent_widget);
    find_dialog->open();
    QVERIFY(QTest::qWaitForWindowExposed(find_dialog, 1000));

    FindWidget *find_widget = find_dialog->ui->find_widget;

    // Enter name in search field
    FilterWidgetSimpleTab *simple_tab = find_widget->ui->filter_widget->ui->simple_tab;
    simple_tab->ui->name_edit->setText(user_name);

    // Press find button
    QPushButton *find_button = find_widget->ui->find_button;
    find_button->click();

    // Confirm that results are not empty
    auto find_results = find_dialog->findChild<QTreeView *>();

    wait_for_find_results_to_load(find_results);

    QVERIFY2(find_results->model()->rowCount(), "No results found");
}

void ADMCTestObjectMenu::object_menu_find_advanced() {
    const QString parent = test_arena_dn();

    const QString user_name = TEST_USER;
    const QString user_dn = test_object_dn(user_name, CLASS_USER);

    // Create test user
    const bool create_user_success = ad.object_add(user_dn, CLASS_USER);
    QVERIFY2(create_user_success, "Failed to create user");
    QVERIFY2(object_exists(user_dn), "Created user doesn't exist");

    auto find_dialog = new FindObjectDialog(filter_classes, parent, parent_widget);
    find_dialog->open();
    QVERIFY(QTest::qWaitForWindowExposed(find_dialog, 1000));

    FilterWidget *filter_widget = find_dialog->ui->find_widget->ui->filter_widget;
    QTabWidget *tab_widget = filter_widget->ui->tab_widget;
    FilterWidgetAdvancedTab *advanced_tab = filter_widget->ui->advanced_tab;
    tab_widget->setCurrentWidget(advanced_tab);

    QPlainTextEdit *ldap_filter_edit = advanced_tab->ui->ldap_filter_edit;
    const QString filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_DN, user_dn);
    ldap_filter_edit->setPlainText(filter);

    QPushButton *find_button = find_dialog->ui->find_widget->ui->find_button;
    find_button->click();

    auto find_results = find_dialog->findChild<QTreeView *>();

    wait_for_find_results_to_load(find_results);

    QVERIFY2(find_results->model()->rowCount(), "No results found");
}

void ADMCTestObjectMenu::object_menu_rename() {
    const QString old_name = TEST_USER;
    const QString new_name = old_name + "2";

    const QString old_dn = test_object_dn(old_name, CLASS_USER);
    const QString new_dn = dn_rename(old_dn, new_name);

    // Create test object
    const bool create_success = ad.object_add(old_dn, CLASS_USER);
    QVERIFY(create_success);
    QVERIFY(object_exists(old_dn));

    // Open rename dialog
    auto rename_dialog = new RenameUserDialog(parent_widget);
    rename_dialog->set_target(old_dn);
    rename_dialog->open();
    QVERIFY(QTest::qWaitForWindowExposed(rename_dialog, 1000));

    rename_dialog->ui->name_edit->setText(new_name);

    rename_dialog->accept();

    QVERIFY(object_exists(new_dn));
}

QTEST_MAIN(ADMCTestObjectMenu)
