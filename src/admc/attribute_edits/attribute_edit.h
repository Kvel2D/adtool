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

#ifndef ATTRIBUTE_EDIT_H
#define ATTRIBUTE_EDIT_H

#include <QObject>

/**
 * AttributeEdit's wrap regular Qt widgets so that they can
 * be used to edit attributes of an AD object. Depending on
 * what kind of attribute is being edited, different widgets
 * are used to represent different data types. 
 */

class AdInterface;
class AdObject;

class AttributeEdit : public QObject {
    Q_OBJECT
public:

    // Verify edit. Verify process will stop on first
    // failure. This is so that only one failure message is
    // shown at a time.
    static bool verify(const QList<AttributeEdit *> &edit_list, AdInterface &ad, const QString &dn);

    // Applies edits. If one of the edits fails to apply
    // midway, the apply process still continues. This is
    // so that if more errors occur, they are all gathered
    // together and presented to the user together. If
    // process stopped on first error, the user would have
    // to apply multiple times while fixing errors to see
    // all of them.
    static bool apply(const QList<AttributeEdit *> &edit_list, AdInterface &ad, const QString &dn);

    static void load(const QList<AttributeEdit *> &edit_list, AdInterface &ad, const AdObject &object);

    // NOTE: not all edits might support read-only mode, see
    // specific edit headers to verify that they implement
    // set_read_only()
    static void set_read_only(const QList<AttributeEdit *> &edit_list, const bool read_only);

    using QObject::QObject;

    // Load state from object, used to initialize or
    // reset edit.
    virtual void load(AdInterface &ad, const AdObject &object) = 0;

    virtual void set_read_only(const bool read_only) = 0;

    // Verify current input. This is for the kinds of errors
    // that the server doesn't or can't check for. For
    // example password confirmation matching password.
    // Should be called before apply().
    virtual bool verify(AdInterface &ad, const QString &dn) const;

    // Apply current input by making a modification to the
    // AD server
    virtual bool apply(AdInterface &ad, const QString &dn) = 0;

signals:
    // Emitted when edit was edited by user
    void edited();
};

#define DECL_ATTRIBUTE_EDIT_VIRTUALS()                                    \
    void set_read_only(const bool read_only) override;                    \
    bool apply(AdInterface &ad, const QString &dn) override;        \
    void load(AdInterface &ad, const AdObject &object) override; \


#endif /* ATTRIBUTE_EDIT_H */
