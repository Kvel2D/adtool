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

#ifndef ENTRY_WIDGET_H
#define ENTRY_WIDGET_H

#include <QWidget>
#include <QList>
#include <QSet>

class QTreeView;
class QLabel;

// Shows names of AdModel as a tree
class EntryWidget : public QWidget {
Q_OBJECT

public:
    EntryWidget(int column_count, int dn_column);
    ~EntryWidget();

    static QString get_selected_dn();

signals:
    void clicked_dn(const QString &dn);

private slots:
    void on_action_toggle_dn(bool checked);
    void on_context_menu_requested(const QPoint &pos);
    void on_view_clicked(const QModelIndex &index);

protected:
    QTreeView *view = nullptr;
    QList<bool> column_hidden;
    
    void update_column_visibility();

private:
    int dn_column = -1;
    static QSet<EntryWidget *> instances;

    QString get_dn_from_index(const QModelIndex &index) const;
};

#endif /* ENTRY_WIDGET_H */
