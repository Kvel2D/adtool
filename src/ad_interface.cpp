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

#include "ad_interface.h"
#include "ad_connection.h"
#include "admc.h"

#include <QSet>

AdInterface::AdInterface(QObject *parent)
: QObject(parent)
{
    connection = new adldap::AdConnection();
}

AdInterface::~AdInterface() {
    delete connection;
}

// "CN=foo,CN=bar,DC=domain,DC=com"
// =>
// "foo"
QString extract_name_from_dn(const QString &dn) {
    int equals_i = dn.indexOf('=') + 1;
    int comma_i = dn.indexOf(',');
    int segment_length = comma_i - equals_i;

    QString name = dn.mid(equals_i, segment_length);

    return name;
}

// "CN=foo,CN=bar,DC=domain,DC=com"
// =>
// "CN=bar,DC=domain,DC=com"
QString extract_parent_dn_from_dn(const QString &dn) {
    int comma_i = dn.indexOf(',');

    QString parent_dn = dn.mid(comma_i + 1);

    return parent_dn;
}

void AdInterface::ad_interface_login(const QString &base, const QString &head) {
    connection->connect(base.toStdString(), head.toStdString());

    if (connection->is_connected()) {
        emit ad_interface_login_complete(base, head);
    } else {
        emit ad_interface_login_failed(base, head);
    }
}

QString AdInterface::get_error_str() {
    return QString(connection->get_errstr());
}

QList<QString> AdInterface::load_children(const QString &dn) {
    const QByteArray dn_array = dn.toLatin1();
    const char *dn_cstr = dn_array.constData();

    char **children_raw = connection->list(dn_cstr);

    if (children_raw != NULL) {
        auto children = QList<QString>();

        for (int i = 0; children_raw[i] != NULL; i++) {
            auto child = QString(children_raw[i]);
            children.push_back(child);
        }

        for (int i = 0; children_raw[i] != NULL; i++) {
            free(children_raw[i]);
        }
        free(children_raw);

        return children;
    } else {
        if (connection->get_errcode() != AD_SUCCESS) {
            emit load_children_failed(dn, get_error_str());
        }

        return QList<QString>();
    }
}

void AdInterface::load_attributes(const QString &dn) {
    const QByteArray dn_array = dn.toLatin1();
    const char *dn_cstr = dn_array.constData();

    char** attributes_raw = connection->get_attribute(dn_cstr, "*");

    if (attributes_raw != NULL) {
        attributes_map[dn] = QMap<QString, QList<QString>>();

        // Load attributes map
        // attributes_raw is in the form of:
        // char** array of {key, value, value, key, value ...}
        // transform it into:
        // map of {key => {value, value ...}, key => {value, value ...} ...}
        for (int i = 0; attributes_raw[i + 2] != NULL; i += 2) {
            auto attribute = QString(attributes_raw[i]);
            auto value = QString(attributes_raw[i + 1]);

            // Make values list if doesn't exist yet
            if (!attributes_map[dn].contains(attribute)) {
                attributes_map[dn][attribute] = QList<QString>();
            }

            attributes_map[dn][attribute].push_back(value);
        }

        // Free attributes_raw
        for (int i = 0; attributes_raw[i] != NULL; i++) {
            free(attributes_raw[i]);
        }
        free(attributes_raw);

        attributes_loaded_set.insert(dn);

        emit load_attributes_complete(dn);
    } else if (connection->get_errcode() != AD_SUCCESS) {
        emit load_attributes_failed(dn, get_error_str());
    }
}

QMap<QString, QList<QString>> AdInterface::get_attributes(const QString &dn) {
    if (!attributes_loaded(dn)) {
        load_attributes(dn);
    }

    if (!attributes_map.contains(dn)) {
        return QMap<QString, QList<QString>>();
    } else {
        return attributes_map[dn];
    }
}

QList<QString> AdInterface::get_attribute_multi(const QString &dn, const QString &attribute) {
    QMap<QString, QList<QString>> attributes = get_attributes(dn);

    if (attributes.contains(attribute)) {
        return attributes[attribute];
    } else {
        return QList<QString>();
    }
}

QString AdInterface::get_attribute(const QString &dn, const QString &attribute) {
    QList<QString> values = get_attribute_multi(dn, attribute);

    if (values.size() > 0) {
        // Return first value only
        return values[0];
    } else {
        return "";
    }
}

bool AdInterface::attribute_value_exists(const QString &dn, const QString &attribute, const QString &value) {
    QList<QString> values = get_attribute_multi(dn, attribute);

    if (values.contains(value)) {
        return true;
    } else {
        return false;
    }
}

bool AdInterface::set_attribute(const QString &dn, const QString &attribute, const QString &value) {
    int result = AD_INVALID_DN;

    const QString old_value = get_attribute(dn, attribute);
    
    const QByteArray dn_array = dn.toLatin1();
    const char *dn_cstr = dn_array.constData();

    const QByteArray attribute_array = attribute.toLatin1();
    const char *attribute_cstr = attribute_array.constData();

    const QByteArray value_array = value.toLatin1();
    const char *value_cstr = value_array.constData();

    result = connection->mod_replace(dn_cstr, attribute_cstr, value_cstr);

    if (result == AD_SUCCESS) {
        // Reload attributes to get new value
        load_attributes(dn);

        emit attributes_changed(dn);
        emit set_attribute_complete(dn, attribute, old_value, value);

        return true;
    } else {
        emit set_attribute_failed(dn, attribute, old_value, value, get_error_str());

        return false;
    }
}

void AdInterface::create_entry(const QString &name, const QString &dn, NewEntryType type) {
    int result = AD_INVALID_DN;
    
    const QByteArray name_array = name.toLatin1();
    const char *name_cstr = name_array.constData();

    const QByteArray dn_array = dn.toLatin1();
    const char *dn_cstr = dn_array.constData();

    switch (type) {
        case User: {
            result = connection->create_user(name_cstr, dn_cstr);
            break;
        }
        case Computer: {
            result = connection->create_computer(name_cstr, dn_cstr);
            break;
        }
        case OU: {
            result = connection->ou_create(name_cstr, dn_cstr);
            break;
        }
        case Group: {
            result = connection->group_create(name_cstr, dn_cstr);
            break;
        }
        case COUNT: break;
    }

    if (result == AD_SUCCESS) {
        emit create_entry_complete(dn, type);
    } else {
        emit create_entry_failed(dn, type, get_error_str());
    }
}

void AdInterface::delete_entry(const QString &dn) {
    // Load attributes so they are available in signal connections
    if (!attributes_loaded(dn)) {
        load_attributes(dn);
    }

    int result = AD_INVALID_DN;

    const QByteArray dn_array = dn.toLatin1();
    const char *dn_cstr = dn_array.constData();

    result = connection->object_delete(dn_cstr);

    if (result == AD_SUCCESS) {
        update_related_entries(dn, "");    
        unload_internal_attributes(dn);

        emit delete_entry_complete(dn);
    } else {
        emit delete_entry_failed(dn, get_error_str());
    }
}

void AdInterface::move(const QString &dn, const QString &new_container) {
    // Load attributes so they are available in signal connections
    if (!attributes_loaded(dn)) {
        load_attributes(dn);
    }

    int result = AD_INVALID_DN;

    QList<QString> dn_split = dn.split(',');
    QString new_dn = dn_split[0] + "," + new_container;

    const QByteArray dn_array = dn.toLatin1();
    const char *dn_cstr = dn_array.constData();

    const QByteArray new_container_array = new_container.toLatin1();
    const char *new_container_cstr = new_container_array.constData();

    const bool entry_is_group = is_group(dn);
    const bool entry_is_user = is_user(dn);

    if (!entry_is_user && !entry_is_group) {
        emit move_failed(dn, new_container, new_dn, "AdInterface::move() only supports moving users and groups at the moment");

        return;
    }

    if (entry_is_user) {
        result = connection->move_user(dn_cstr, new_container_cstr);
    } else {
        result = connection->move(dn_cstr, new_container_cstr);
    }
    
    if (result == AD_SUCCESS) {
        load_attributes(new_dn);

        update_related_entries(dn, new_dn);
        unload_internal_attributes(dn);

        emit dn_changed(dn, new_dn);
        emit move_complete(dn, new_container, new_dn);
    } else {
        emit move_failed(dn, new_container, new_dn, get_error_str());
    }
}

void AdInterface::add_user_to_group(const QString &group_dn, const QString &user_dn) {
    int result = AD_INVALID_DN;

    const QByteArray group_dn_array = group_dn.toLatin1();
    const char *group_dn_cstr = group_dn_array.constData();

    const QByteArray user_dn_array = user_dn.toLatin1();
    const char *user_dn_cstr = user_dn_array.constData();

    result = connection->group_add_user(group_dn_cstr, user_dn_cstr);

    if (result == AD_SUCCESS) {
        // Update attributes of user and group
        if (attributes_loaded(group_dn)) {
            add_attribute_internal(group_dn, "member", user_dn);
        }
        if (attributes_loaded(group_dn)) {
            add_attribute_internal(user_dn, "memberOf", group_dn);
        }

        emit attributes_changed(user_dn);
        emit attributes_changed(group_dn);

        emit add_user_to_group_complete(group_dn, user_dn);
    } else {
        emit add_user_to_group_failed(group_dn, user_dn, get_error_str());
    }
}

void AdInterface::rename(const QString &dn, const QString &new_name) {
    // Load attributes so they are available for objects connecting to signals
    // NOTE: have to do this before operation
    if (!attributes_loaded(dn)) {
        load_attributes(dn);
    }

    // Compose new_rdn and new_dn
    const QStringList exploded_dn = dn.split(',');
    const QString old_rdn = exploded_dn[0];
    const int prefix_i = old_rdn.indexOf('=') + 1;
    const QString prefix = old_rdn.left(prefix_i);
    const QString new_rdn = prefix + new_name;
    QStringList new_exploded_dn(exploded_dn);
    new_exploded_dn.replace(0, new_rdn);
    const QString new_dn = new_exploded_dn.join(',');

    const QByteArray dn_array = dn.toLatin1();
    const char *dn_cstr = dn_array.constData();
    const QByteArray new_name_array = new_name.toLatin1();
    const char *new_name_cstr = new_name_array.constData();
    const QByteArray new_rdn_array = new_rdn.toLatin1();
    const char *new_rdn_cstr = new_rdn_array.constData();

    int result = AD_INVALID_DN;
    if (is_user(dn)) {
        result = connection->rename_user(dn_cstr, new_name_cstr);
    } else if (is_group(dn)) {
        result = connection->rename_group(dn_cstr, new_name_cstr);
    } else {
        result = connection->rename(dn_cstr, new_rdn_cstr);
    }

    if (result == AD_SUCCESS) {
        load_attributes(new_dn);

        update_related_entries(dn, new_dn);
        unload_internal_attributes(dn);

        emit dn_changed(dn, new_dn);
        emit attributes_changed(new_dn);
        emit rename_complete(dn, new_name, new_dn);
    } else {
        emit rename_failed(dn, new_name, new_dn, get_error_str());
    }
}

bool AdInterface::is_user(const QString &dn) {
    return attribute_value_exists(dn, "objectClass", "user");
}

bool AdInterface::is_group(const QString &dn) {
    return attribute_value_exists(dn, "objectClass", "group");
}

bool AdInterface::is_container(const QString &dn) {
    return attribute_value_exists(dn, "objectClass", "container");
}

bool AdInterface::is_ou(const QString &dn) {
    return attribute_value_exists(dn, "objectClass", "organizationalUnit");
}

bool AdInterface::is_policy(const QString &dn) {
    return attribute_value_exists(dn, "objectClass", "groupPolicyContainer");
}

bool AdInterface::is_container_like(const QString &dn) {
    // TODO: check that this includes all fitting objectClasses
    const QList<QString> containerlike_objectClasses = {"organizationalUnit", "builtinDomain", "domain"};
    for (auto c : containerlike_objectClasses) {
        if (AD()->attribute_value_exists(dn, "objectClass", c)) {
            return true;
        }
    }

    return false;
}

enum DropType {
    DropType_Move,
    DropType_AddToGroup,
    DropType_None
};

// Determine what kind of drop type is dropping this entry onto target
// If drop type is none, then can't drop this entry on this target
DropType get_drop_type(const QString &dn, const QString &target_dn) {
    const bool dropped_is_user = AD()->is_user(dn);
    const bool dropped_is_group = AD()->is_group(dn);

    const bool target_is_group = AD()->is_group(target_dn);
    const bool target_is_ou = AD()->is_ou(target_dn);
    const bool target_is_container = AD()->is_container(target_dn);
    const bool target_is_container_like = AD()->is_container_like(target_dn);

    if (dropped_is_user) {
        if (target_is_ou || target_is_container) {
            return DropType_Move;
        } else if (target_is_group) {
            return DropType_AddToGroup;
        }
    } else if (dropped_is_group) {
        if (target_is_ou || target_is_container || target_is_container_like) {
            return DropType_Move;
        }
    }

    return DropType_None;
}

bool AdInterface::can_drop_entry(const QString &dn, const QString &target_dn) {
    DropType drop_type = get_drop_type(dn, target_dn);

    if (drop_type == DropType_None) {
        return false;
    } else {
        return true;
    }
}

// General "drop" operation that can either move, link or change membership depending on which types of entries are involved
void AdInterface::drop_entry(const QString &dn, const QString &target_dn) {
    DropType drop_type = get_drop_type(dn, target_dn);

    switch (drop_type) {
        case DropType_Move: {
            AD()->move(dn, target_dn);
            break;
        }
        case DropType_AddToGroup: {
            AD()->add_user_to_group(target_dn, dn);
            break;
        }
        case DropType_None: {
            break;
        }
    }
}

bool AdInterface::attributes_loaded(const QString &dn) {
    const bool loaded = attributes_loaded_set.contains(dn);
    return loaded;
}

void AdInterface::unload_internal_attributes(const QString &dn) {
    attributes_map.remove(dn);
    attributes_loaded_set.remove(dn);
}

void AdInterface::add_attribute_internal(const QString &dn, const QString &attribute, const QString &value) {
    attributes_map[dn][attribute].push_back(value);
}

void AdInterface::remove_attribute_internal(const QString &dn, const QString &attribute, const QString &value) {
    const int value_i = attributes_map[dn][attribute].indexOf(value);
    attributes_map[dn][attribute].removeAt(value_i);
}

// NOTE: keeps order of attributes
// This must be used if possible instead of combining add()+remove()
void AdInterface::replace_attribute_internal(const QString &dn, const QString &attribute, const QString &old_value, const QString &new_value) {
    const int old_value_i = attributes_map[dn][attribute].indexOf(old_value);
    attributes_map[dn][attribute].replace(old_value_i, new_value);
}

// Update DN and/or attributes of all entries that are related
// to this one through membership
// LDAP database does all this on it's own so need to replicate it
// NOTE: if entry was deleted, new_dn should be ""
// NOTE: attributes_map should contain both new_dn and old_dn when
// this is called, so that signals/connections can access them
void AdInterface::update_related_entries(const QString &old_dn, const QString &new_dn) {
    const bool deleted = (old_dn != "" && new_dn == "");
    const bool changed = (old_dn != "" && new_dn != "" && old_dn != new_dn);

    QList<QString> updated_entries;

    // TODO: update state connected through policy linkage

    // TODO: update state connected through tree/children relationship
    // for changed container update dn of all of it's children
    // for deleted container delete all it's children

    // Update "memberOf" attribute of all groups that this entry
    // is member of
    QList<QString> groups = get_attribute_multi(old_dn, "memberOf");
    for (auto group : groups) {
        // Only update if loaded
        if (attributes_loaded(group)) {
            if (deleted) {
                remove_attribute_internal(group, "member", old_dn);
                updated_entries.append(group);
            } else if (changed) {
                replace_attribute_internal(group, "member", old_dn, new_dn);
                updated_entries.append(group);
            }
        }
    }

    // Update "member" attribute of all entries that are members of this entry
    QList<QString> members = get_attribute_multi(old_dn, "member");
    for (auto member : members) {
        // Only update if loaded
        if (attributes_loaded(member)) {
            if (deleted) {
                remove_attribute_internal(member, "memberOf", old_dn);
                updated_entries.append(member);
            } else if (changed) {
                replace_attribute_internal(member, "memberOf", old_dn, new_dn);
                updated_entries.append(member);
            }
        }
    }

    for (auto dn : updated_entries) {
        emit attributes_changed(dn);
    }
}

AdInterface *AD() {
    ADMC *app = qobject_cast<ADMC *>(qApp);
    AdInterface *ad = app->ad_interface();
    return ad;
}
