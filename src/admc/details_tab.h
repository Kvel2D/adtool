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

#ifndef DETAILS_TAB_H
#define DETAILS_TAB_H

#include "ad_object.h"

#include <QWidget>
#include <QString>

class DetailsTab : public QWidget {
Q_OBJECT

public:
    DetailsTab();
    
    virtual bool accepts_target(const AdObject &object) const = 0;
    virtual bool changed() const = 0;
    virtual void load(const AdObject &object) = 0;
    virtual bool verify() = 0;
    virtual void apply(const QString &target) = 0;

signals:
    void edited();
    
public slots:
    void on_edit_edited();
};

#define DECL_DETAILS_TAB_VIRTUALS()\
bool accepts_target(const AdObject &object) const;\
bool changed() const;\
void load(const AdObject &object);\
bool verify();\
void apply(const QString &target);

#endif /* DETAILS_TAB_H */
