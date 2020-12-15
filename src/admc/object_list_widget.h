/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020 BaseALT Ltd.
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

#ifndef OBJECTS_LIST_WIDGET_H
#define OBJECTS_LIST_WIDGET_H

/**
 * Displays a detailed list of objects with many optional
 * toggleable columns. Can load children of a parent or load
 * all objects matching a filter. Does NOT update on AD
   modifications. Intended to be used as a sub-widget
 * in some other widget.
 */

#include <QWidget>
#include <QHash>
#include <QList>
#include <QPoint>

class QTreeView;
class QLabel;
class QLineEdit;
class AdObject;
class QStandardItemModel;

enum ObjectListWidgetType {
    ObjectListWidgetType_Contents,
    ObjectListWidgetType_Find,
};

class ObjectListWidget final : public QWidget {
Q_OBJECT

public:
    ObjectListWidget(const ObjectListWidgetType list_type_arg);

    void load_children(const QString &new_parent_dn, const QString &filter = QString());
    void load_filter(const QString &filter, const QString &search_base);
    void reset_name_filter();

private slots:
    void on_context_menu(const QPoint pos);
    void on_header_toggled();

private:
    ObjectListWidgetType list_type;
    QString parent_dn;
    QLabel *label;
    QStandardItemModel *model;
    QTreeView *view;
    QLineEdit *filter_name_edit;
    QWidget *header;

    void load(const QHash<QString, AdObject> &objects);
    void showEvent(QShowEvent *event);
};

#endif /* OBJECTS_LIST_WIDGET_H */
