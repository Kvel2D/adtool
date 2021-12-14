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

#include "ad_security.h"

#include "adldap.h"

#include "samba/dom_sid.h"
#include "samba/libsmb_xattr.h"
#include "samba/ndr_security.h"
#include "samba/security_descriptor.h"

#include "ad_filter.h"

#include <QDebug>

#define UNUSED_ARG(x) (void) (x)

QByteArray dom_sid_to_bytes(const dom_sid &sid);
QByteArray dom_sid_string_to_bytes(const dom_sid &sid);
bool check_ace_match(const security_ace &ace, const QByteArray &trustee, const uint32_t access_mask, const QByteArray &object_type, const bool allow, const bool inherited);
QList<security_ace> security_descriptor_get_dacl(const security_descriptor *sd);
void ad_security_replace_dacl(security_descriptor *sd, const QList<security_ace> &new_dacl);
uint32_t ad_security_map_access_mask(const uint32_t access_mask);

const QList<int> ace_types_with_object = {
    SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT,
    SEC_ACE_TYPE_ACCESS_DENIED_OBJECT,
    SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT,
    SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT,
};

const QList<QString> well_known_sid_list = {
    SID_WORLD_DOMAIN,
    SID_WORLD,
    SID_WORLD,
    SID_CREATOR_OWNER_DOMAIN,
    SID_CREATOR_OWNER,
    SID_CREATOR_GROUP,
    SID_OWNER_RIGHTS,
    SID_NT_AUTHORITY,
    SID_NT_DIALUP,
    SID_NT_NETWORK,
    SID_NT_BATCH,
    SID_NT_INTERACTIVE,
    SID_NT_SERVICE,
    SID_NT_ANONYMOUS,
    SID_NT_PROXY,
    SID_NT_ENTERPRISE_DCS,
    SID_NT_SELF,
    SID_NT_AUTHENTICATED_USERS,
    SID_NT_RESTRICTED,
    SID_NT_TERMINAL_SERVER_USERS,
    SID_NT_REMOTE_INTERACTIVE,
    SID_NT_THIS_ORGANISATION,
    SID_NT_IUSR,
    SID_NT_SYSTEM,
    SID_NT_LOCAL_SERVICE,
    SID_NT_NETWORK_SERVICE,
    SID_NT_DIGEST_AUTHENTICATION,
    SID_NT_NTLM_AUTHENTICATION,
    SID_NT_SCHANNEL_AUTHENTICATION,
    SID_NT_OTHER_ORGANISATION,
};

const QHash<QString, QString> trustee_name_map = {
    {SID_WORLD_DOMAIN, "Everyone in Domain"},
    {SID_WORLD, "Everyone"},
    {SID_CREATOR_OWNER_DOMAIN, "CREATOR OWNER DOMAIN"},
    {SID_CREATOR_OWNER, "CREATOR OWNER"},
    {SID_CREATOR_GROUP, "CREATOR GROUP"},
    {SID_OWNER_RIGHTS, "OWNER RIGHTS"},
    {SID_NT_AUTHORITY, "AUTHORITY"},
    {SID_NT_DIALUP, "DIALUP"},
    {SID_NT_NETWORK, "NETWORK"},
    {SID_NT_BATCH, "BATCH"},
    {SID_NT_INTERACTIVE, "INTERACTIVE"},
    {SID_NT_SERVICE, "SERVICE"},
    {SID_NT_ANONYMOUS, "ANONYMOUS LOGON"},
    {SID_NT_PROXY, "PROXY"},
    {SID_NT_ENTERPRISE_DCS, "ENTERPRISE DOMAIN CONTROLLERS"},
    {SID_NT_SELF, "SELF"},
    {SID_NT_AUTHENTICATED_USERS, "Authenticated Users"},
    {SID_NT_RESTRICTED, "RESTRICTED"},
    {SID_NT_TERMINAL_SERVER_USERS, "TERMINAL SERVER USERS"},
    {SID_NT_REMOTE_INTERACTIVE, "REMOTE INTERACTIVE LOGON"},
    {SID_NT_THIS_ORGANISATION, "This Organization"},
    {SID_NT_IUSR, "IUSR"},
    {SID_NT_SYSTEM, "SYSTEM"},
    {SID_NT_LOCAL_SERVICE, "LOCAL SERVICE"},
    {SID_NT_NETWORK_SERVICE, "NETWORK SERVICE"},
    {SID_NT_DIGEST_AUTHENTICATION, "Digest Authentication"},
    {SID_NT_NTLM_AUTHENTICATION, "NTLM Authentication"},
    {SID_NT_SCHANNEL_AUTHENTICATION, "SChannel Authentication"},
    {SID_NT_OTHER_ORGANISATION, "Other Organization"},
};

const QList<QString> cant_change_pass_trustee_cn_list = {
    SID_NT_SELF,
    SID_WORLD,
};

const QList<uint32_t> protect_deletion_mask_list = {
    SEC_STD_DELETE,
    SEC_ADS_DELETE_TREE,
};

const QSet<security_ace_type> ace_type_allow_set = {
    SEC_ACE_TYPE_ACCESS_ALLOWED,
    SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT,
};
const QSet<security_ace_type> ace_type_deny_set = {
    SEC_ACE_TYPE_ACCESS_DENIED,
    SEC_ACE_TYPE_ACCESS_DENIED_OBJECT,
};

SecurityRightState::SecurityRightState(const bool data_arg[SecurityRightStateInherited_COUNT][SecurityRightStateType_COUNT]) {
    for (int inherited = 0; inherited < SecurityRightStateInherited_COUNT; inherited++) {
        for (int type = 0; type < SecurityRightStateType_COUNT; type++) {
            data[inherited][type] = data_arg[inherited][type];
        }
    }
}

bool SecurityRightState::get(const SecurityRightStateInherited inherited, const SecurityRightStateType type) const {
    return data[inherited][type];
}

security_descriptor *security_descriptor_make_from_bytes(TALLOC_CTX *mem_ctx, const QByteArray &sd_bytes) {
    DATA_BLOB blob = data_blob_const(sd_bytes.data(), sd_bytes.size());

    security_descriptor *out = talloc(mem_ctx, struct security_descriptor);

    ndr_pull_struct_blob(&blob, out, out, (ndr_pull_flags_fn_t) ndr_pull_security_descriptor);

    return out;
}

security_descriptor *security_descriptor_make_from_bytes(const QByteArray &sd_bytes) {
    security_descriptor *out = security_descriptor_make_from_bytes(NULL, sd_bytes);

    return out;
}

void security_descriptor_free(security_descriptor *sd) {
    talloc_free(sd);
}

security_descriptor *security_descriptor_copy(security_descriptor *sd) {
    security_descriptor *out = talloc(NULL, struct security_descriptor);

    out = security_descriptor_copy(out, sd);

    return out;
}

QString ad_security_get_well_known_trustee_name(const QByteArray &trustee) {
    const QString trustee_string = object_sid_display_value(trustee);
    return trustee_name_map.value(trustee_string, QString());
}

QString ad_security_get_trustee_name(AdInterface &ad, const QByteArray &trustee) {
    const QString trustee_string = object_sid_display_value(trustee);

    if (trustee_name_map.contains(trustee_string)) {
        return trustee_name_map[trustee_string];
    } else {
        // Try to get name of trustee by finding it's DN
        const QString filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_SID, trustee_string);
        const QList<QString> attributes = {
            ATTRIBUTE_DISPLAY_NAME,
            ATTRIBUTE_SAM_ACCOUNT_NAME,
        };
        const auto trustee_search = ad.search(ad.adconfig()->domain_dn(), SearchScope_All, filter, QList<QString>());
        if (!trustee_search.isEmpty()) {
            // NOTE: this is some weird name selection logic
            // but that's how microsoft does it. Maybe need
            // to use this somewhere else as well?
            const QString name = [&]() {
                const AdObject object = trustee_search.values()[0];

                if (object.contains(ATTRIBUTE_DISPLAY_NAME)) {
                    return object.get_string(ATTRIBUTE_DISPLAY_NAME);
                } else if (object.contains(ATTRIBUTE_SAM_ACCOUNT_NAME)) {
                    return object.get_string(ATTRIBUTE_SAM_ACCOUNT_NAME);
                } else {
                    return dn_get_name(object.get_dn());
                }
            }();

            return name;
        } else {
            // Return raw sid as last option
            return trustee_string;
        }
    }
}

bool ad_security_replace_security_descriptor(AdInterface &ad, const QString &dn, security_descriptor *new_sd) {
    const QByteArray new_descriptor_bytes = [&]() {
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);

        DATA_BLOB blob;
        ndr_push_struct_blob(&blob, tmp_ctx, new_sd, (ndr_push_flags_fn_t) ndr_push_security_descriptor);

        const QByteArray out = QByteArray((char *) blob.data, blob.length);

        talloc_free(tmp_ctx);

        return out;
    }();

    const bool apply_success = ad.attribute_replace_value(dn, ATTRIBUTE_SECURITY_DESCRIPTOR, new_descriptor_bytes);

    return apply_success;
}

QByteArray dom_sid_to_bytes(const dom_sid &sid) {
    const QByteArray bytes = QByteArray((char *) &sid, sizeof(struct dom_sid));

    return bytes;
}

QByteArray dom_sid_string_to_bytes(const QString &string) {
    dom_sid sid;
    dom_sid_parse(cstr(string), &sid);
    const QByteArray bytes = dom_sid_to_bytes(sid);

    return bytes;
}

void security_descriptor_sort_dacl(security_descriptor *sd) {
    qsort(sd->dacl->aces, sd->dacl->num_aces, sizeof(security_ace), ace_compare);
}

bool ad_security_get_protected_against_deletion(const AdObject &object, AdConfig *adconfig) {
    security_descriptor *sd = object.get_security_descriptor();

    const QByteArray trustee_everyone = sid_string_to_bytes(SID_WORLD);

    const bool is_enabled_for_trustee = [&]() {
        const QByteArray protect_against = adconfig->get_right_guid("User-Change-Password");

        bool all_enabled = false;

        for (const uint32_t &mask : protect_deletion_mask_list) {
            const SecurityRightState state = security_descriptor_get_right(sd, trustee_everyone, mask, QByteArray());

            const bool deny = state.get(SecurityRightStateInherited_No, SecurityRightStateType_Deny);

            if (!deny) {
                all_enabled = false;
            }
        }

        return all_enabled;
    }();

    security_descriptor_free(sd);

    return is_enabled_for_trustee;
}

bool ad_security_get_user_cant_change_pass(const AdObject *object, AdConfig *adconfig) {
    security_descriptor *sd = object->get_security_descriptor();

    const bool enabled = [&]() {
        bool out = false;

        for (const QString &trustee_cn : cant_change_pass_trustee_cn_list) {
            const bool is_denied = [&]() {
                const QByteArray trustee = sid_string_to_bytes(trustee_cn);
                const QByteArray change_pass_right = adconfig->get_right_guid("User-Change-Password");
                const SecurityRightState state = security_descriptor_get_right(sd, trustee, SEC_ADS_CONTROL_ACCESS, change_pass_right);
                const bool out_denied = state.get(SecurityRightStateInherited_No, SecurityRightStateType_Deny);

                return out_denied;
            }();

            // Enabled if enabled for either of the
            // trustee's. Both don't have to be
            // enabled
            if (is_denied) {
                out = true;

                break;
            }
        }

        return out;
    }();

    security_descriptor_free(sd);

    return enabled;
}

bool ad_security_set_user_cant_change_pass(AdInterface *ad, const QString &dn, const bool enabled) {
    security_descriptor *sd = [&]() {
        const AdObject object = ad->search_object(dn, {ATTRIBUTE_SECURITY_DESCRIPTOR});
        security_descriptor *out = object.get_security_descriptor();

        return out;
    }();

    for (const QString &trustee_cn : cant_change_pass_trustee_cn_list) {
        const QByteArray trustee = sid_string_to_bytes(trustee_cn);
        const QByteArray change_pass_right = ad->adconfig()->get_right_guid("User-Change-Password");

        // NOTE: the logic is a bit confusing here with
        // all the layers of negation but: "enabled"
        // means "denied", so we remove the opposite of
        // what we want, and add the type of right that
        // we want
        const bool allow = !enabled;
        security_descriptor_remove_right(sd, trustee, SEC_ADS_CONTROL_ACCESS, change_pass_right, !allow);
        security_descriptor_add_right(sd, trustee, SEC_ADS_CONTROL_ACCESS, change_pass_right, allow);
    }

    const bool success = ad_security_replace_security_descriptor(*ad, dn, sd);

    security_descriptor_free(sd);

    return success;
}

bool ad_security_set_protected_against_deletion(AdInterface &ad, const QString dn, const bool enabled) {
    const AdObject object = ad.search_object(dn);

    const bool is_enabled = ad_security_get_protected_against_deletion(object, ad.adconfig());

    const bool dont_need_to_change = (is_enabled == enabled);
    if (dont_need_to_change) {
        return true;
    }

    security_descriptor *new_sd = [&]() {
        security_descriptor *out = object.get_security_descriptor();

        const QByteArray trustee_everyone = sid_string_to_bytes(SID_WORLD);

        // NOTE: we only add/remove deny entries. If
        // there are any allow entries, they are
        // untouched.
        for (const uint32_t &mask : protect_deletion_mask_list) {
            if (enabled) {
                security_descriptor_add_right(out, trustee_everyone, mask, QByteArray(), false);
            } else {
                security_descriptor_remove_right(out, trustee_everyone, mask, QByteArray(), false);
            }
        }

        return out;
    }();

    const bool apply_success = ad_security_replace_security_descriptor(ad, dn, new_sd);

    security_descriptor_free(new_sd);

    return apply_success;
}

QList<QByteArray> security_descriptor_get_trustee_list(security_descriptor *sd) {
    const QSet<QByteArray> trustee_set = [&]() {
        QSet<QByteArray> out;

        const QList<security_ace> dacl = security_descriptor_get_dacl(sd);

        for (const security_ace &ace : dacl) {
            const QByteArray trustee = dom_sid_to_bytes(ace.trustee);

            out.insert(trustee);
        }

        return out;
    }();

    const QList<QByteArray> trustee_list = trustee_set.toList();

    return trustee_list;
}

QList<security_ace> security_descriptor_get_dacl(const security_descriptor *sd) {
    QList<security_ace> out;

    security_acl *dacl = sd->dacl;

    for (size_t i = 0; i < dacl->num_aces; i++) {
        security_ace ace = dacl->aces[i];

        out.append(ace);
    }

    return out;
}

SecurityRightState security_descriptor_get_right(const security_descriptor *sd, const QByteArray &trustee, const uint32_t access_mask_arg, const QByteArray &object_type) {
    bool out_data[SecurityRightStateInherited_COUNT][SecurityRightStateType_COUNT];

    const uint32_t access_mask = ad_security_map_access_mask(access_mask_arg);

    for (int x = 0; x < SecurityRightStateInherited_COUNT; x++) {
        for (int y = 0; y < SecurityRightStateType_COUNT; y++) {
            out_data[x][y] = false;
        }
    }

    const QList<security_ace> dacl = security_descriptor_get_dacl(sd);

    for (const security_ace &ace : dacl) {
        const bool match = [&]() {
            const bool trustee_match = [&]() {
                const QByteArray ace_trustee = dom_sid_to_bytes(ace.trustee);
                const bool out = (ace_trustee == trustee);

                return out;
            }();

            const bool access_mask_match = bit_is_set(ace.access_mask, access_mask);

            
            const bool object_match = [&]() {
                const bool object_present = ace_types_with_object.contains(ace.type);

                if (object_present) {
                    const GUID out_guid = ace.object.object.type.type;
                    const QByteArray ace_object_type = QByteArray((char *) &out_guid, sizeof(GUID));
                    const bool types_are_equal = (ace_object_type == object_type);

                    return types_are_equal;
                } else {
                    // NOTE: if ace doesn't have an
                    // object it can still match.
                    // Example: ace that allows
                    // "generic read" will also allow
                    // reading of all properties.
                    return true;
                }
            }();

            const bool out = (trustee_match && access_mask_match && object_match);

            return out;
        }();

        if (match) {
            bool state_list[2];
            state_list[SecurityRightStateType_Allow] = ace_type_allow_set.contains(ace.type);
            state_list[SecurityRightStateType_Deny] = ace_type_deny_set.contains(ace.type);

            const int inherit_i = [&]() {
                const bool ace_is_inherited = bit_is_set(ace.flags, SEC_ACE_FLAG_INHERITED_ACE);

                if (ace_is_inherited) {
                    return SecurityRightStateInherited_Yes;
                } else {
                    return SecurityRightStateInherited_No;
                }
            }();

            for (int type_i = 0; type_i < SecurityRightStateType_COUNT; type_i++) {
                const bool right_state = state_list[type_i];

                if (right_state) {
                    out_data[inherit_i][type_i] = true;
                }
            }
        }
    }

    const SecurityRightState out = SecurityRightState(out_data);

    return out;
}

void security_descriptor_add_right(security_descriptor *sd, const QByteArray &trustee, const uint32_t access_mask_arg, const QByteArray &object_type, const bool allow) {
    const uint32_t access_mask = ad_security_map_access_mask(access_mask_arg);

    const QList<security_ace> dacl = security_descriptor_get_dacl(sd);

    const int matching_index = [&]() {
        for (int i = 0; i < dacl.size(); i++) {
            const security_ace ace = dacl[i];

            const bool match = check_ace_match(ace, trustee, access_mask, object_type, allow, false);

            if (match) {
                return -1;
            }
        }

        return -1;
    }();

    if (matching_index != -1) {
        const bool right_already_set = [&]() {
            const security_ace matching_ace = dacl[matching_index];
            const bool out = bit_is_set(matching_ace.access_mask, access_mask);

            return out;
        }();

        // Matching ace exists, so reuse it by adding
        // given mask to this ace, but only if it's not set already
        if (!right_already_set) {
            security_ace new_ace = dacl[matching_index];
            new_ace.access_mask = bit_set(new_ace.access_mask, access_mask, true);
            sd->dacl->aces[matching_index] = new_ace;
        }
    } else {
        // No matching ace, so make a new ace for this
        // right
        const security_ace ace = [&]() {
            security_ace out;

            const bool object_present = !object_type.isEmpty();

            out.type = [&]() {
                if (allow) {
                    if (object_present) {
                        return SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT;
                    } else {
                        return SEC_ACE_TYPE_ACCESS_ALLOWED;
                    }
                } else {
                    if (object_present) {
                        return SEC_ACE_TYPE_ACCESS_DENIED_OBJECT;
                    } else {
                        return SEC_ACE_TYPE_ACCESS_DENIED;
                    }
                }

                return SEC_ACE_TYPE_ACCESS_ALLOWED;
            }();

            out.flags = 0x00;
            out.access_mask = access_mask;
            out.object.object.flags = [&]() {
                if (object_present) {
                    return SEC_ACE_OBJECT_TYPE_PRESENT;
                } else {
                    return 0;
                }
            }();

            if (object_present) {
                out.object.object.type.type = [&]() {
                    struct GUID type_guid;
                    memcpy(&type_guid, object_type.data(), sizeof(GUID));

                    return type_guid;
                }();
            }
            
            memcpy(&out.trustee, trustee.data(), sizeof(dom_sid));

            return out;
        }();
        
        security_descriptor_dacl_add(sd, &ace);
    }
}

// Checks if ace matches given members. Note that
// access mask matches if they are equal or if ace mask
// contains given mask.
bool check_ace_match(const security_ace &ace, const QByteArray &trustee, const uint32_t access_mask, const QByteArray &object_type, const bool allow, const bool inherited) {
    const bool type_match = [&]() {
        const security_ace_type ace_type = ace.type;

        const bool ace_allow = ace_type_allow_set.contains(ace_type);
        const bool ace_deny = ace_type_deny_set.contains(ace_type);

        if (allow && ace_allow) {
            return true;
        } else if (!allow && ace_deny) {
            return true;
        } else {
            return false;
        }
    }();

    const bool flags_match = [&]() {
        const bool ace_is_inherited = bit_is_set(ace.flags, SEC_ACE_FLAG_INHERITED_ACE);
        const bool out = (ace_is_inherited == inherited);

        return out;
    }();

    // NOTE: matches both on equality and
    // contains because of cases where a
    // generic right is removed that is
    // part of a full control ace for
    // example
    const bool access_mask_match = [&]() {
        const bool ace_mask_contains_mask = bit_is_set(ace.access_mask, access_mask);

        return ace_mask_contains_mask;
    }();

    const bool trustee_match = [&]() {
        const QByteArray ace_trustee = dom_sid_to_bytes(ace.trustee);
        const bool trustees_are_equal = (ace_trustee == trustee);

        return trustees_are_equal;
    }();

    const bool object_match = [&]() {
        const bool object_present = ace_types_with_object.contains(ace.type);

        if (object_present) {
            const GUID ace_object_type_guid = ace.object.object.type.type;
            const QByteArray ace_object_type = QByteArray((char *) &ace_object_type_guid, sizeof(GUID));
            const bool types_are_equal = (ace_object_type == object_type);

            return types_are_equal;
        } else {
            return object_type.isEmpty();
        }
    }();

    const bool out_match = (type_match && flags_match && access_mask_match && trustee_match && object_match);

    return out_match;
}

void security_descriptor_remove_right(security_descriptor *sd, const QByteArray &trustee, const uint32_t access_mask_arg, const QByteArray &object_type, const bool allow) {
    const uint32_t access_mask = ad_security_map_access_mask(access_mask_arg);

    const QList<security_ace> new_dacl = [&]() {
        QList<security_ace> out;

        const QList<security_ace> old_dacl = security_descriptor_get_dacl(sd);

        for (const security_ace &ace : old_dacl) {
            const bool match = check_ace_match(ace, trustee, access_mask, object_type, allow, false);

            if (match) {
                const security_ace edited_ace = [&]() {
                    security_ace out_ace = ace;

                    // NOTE: need to handle a special
                    // case due to read and write
                    // rights sharing the "read
                    // control" bit. When setting
                    // either read/write, don't change
                    // that shared bit if the other of
                    // these rights is set
                    const uint32_t mask_to_unset = [&]() {
                        const QHash<uint32_t, uint32_t> opposite_map = {
                            {SEC_ADS_GENERIC_READ, SEC_ADS_GENERIC_WRITE},
                            {SEC_ADS_GENERIC_WRITE, SEC_ADS_GENERIC_READ},
                        };

                        for (const uint32_t &mask : opposite_map.keys()) {
                            const uint32_t opposite = opposite_map[mask];
                            const bool opposite_is_set = bit_is_set(ace.access_mask, opposite);

                            if (access_mask == mask && opposite_is_set) {
                                const uint32_t out_mask = (access_mask & ~SEC_STD_READ_CONTROL);

                                return out_mask;
                            }
                        }

                        return access_mask;
                    }();

                    out_ace.access_mask = bit_set(ace.access_mask, mask_to_unset, false);

                    return out_ace;
                }();

                const bool edited_ace_became_empty = (edited_ace.access_mask == 0);

                if (!edited_ace_became_empty) {
                    out.append(edited_ace);
                }
            } else {
                out.append(ace);
            }
        }

        return out;
    }();

    ad_security_replace_dacl(sd, new_dacl);
}

void security_descriptor_remove_trustee(security_descriptor *sd, const QList<QByteArray> &trustee_list) {
    const QList<security_ace> new_dacl = [&]() {
        QList<security_ace> out;

        const QList<security_ace> old_dacl = security_descriptor_get_dacl(sd);

        for (const security_ace &ace : old_dacl) {
            const bool match = [&]() {
                const bool trustee_match = [&]() {
                    const QByteArray ace_trustee = dom_sid_to_bytes(ace.trustee);
                    const bool out_trustee_match = trustee_list.contains(ace_trustee);

                    return out_trustee_match;
                }();

                const bool inherited = bit_is_set(ace.flags, SEC_ACE_FLAG_INHERITED_ACE);

                const bool out_match = trustee_match && !inherited;

                return out_match;
            }();

            if (!match) {
                out.append(ace);
            }
        }

        return out;
    }();

    ad_security_replace_dacl(sd, new_dacl);
}

QString ad_security_get_right_name(AdConfig *adconfig, const uint32_t access_mask, const QByteArray &object_type) {
    // TODO: translate object type name. How to: object
    // type guid -> extended right CN -> map of cn's to
    // translations (both english and russian!)
    // 
    // TODO: Add "translated" to decl comment when
    // done.
    // 
    // TODO: also add this note explaning *why* we need
    // to translate these ourselves. 
    // Predefined schema classes use the localizationDisplayId attribute of a controlAccessRight object to specify a message identifier used to retrieve a localized display name from Dssec.dll.
    // we don't have dssec.dll!

    const QString object_type_name = adconfig->get_right_name(object_type);

    if (access_mask == SEC_ADS_CONTROL_ACCESS) {
        return object_type_name;
    } else if (access_mask == SEC_ADS_READ_PROP) {
        return QString(QCoreApplication::translate("ad_security.cpp", "Read %1")).arg(object_type_name);
    } else if (access_mask == SEC_ADS_WRITE_PROP) {
        return QString(QCoreApplication::translate("ad_security.cpp", "Write %1")).arg(object_type_name);
    } else {
        const QHash<uint32_t, QString> common_right_name_map = {
            {SEC_ADS_GENERIC_ALL, "Full control"},
            {SEC_ADS_GENERIC_READ, "Read"},
            {SEC_ADS_GENERIC_WRITE, "Write"},
            {SEC_STD_DELETE, "Delete"},
            {SEC_ADS_CREATE_CHILD, "Create all child objects"},
            {SEC_ADS_DELETE_CHILD, "Delete all child objects"},
        };

        return common_right_name_map.value(access_mask, QCoreApplication::translate("ad_security.cpp", "<unknown right>"));
    }
}

void ad_security_replace_dacl(security_descriptor *sd, const QList<security_ace> &new_dacl) {
    
    // Free old dacl
    talloc_free(sd->dacl);
    sd->dacl = NULL;

    // Fill new dacl
    // NOTE: dacl_add() allocates new dacl
    for (const security_ace &ace : new_dacl) {
        security_descriptor_dacl_add(sd, &ace);
    }

    security_descriptor_sort_dacl(sd);
}

// This f-n is only necessary to band-aid one problem
// with generic read. For some reason, security editing
// in RSAT has a different value for generic read,
// without the "list object" right. Need to remove that
// bit both when setting generic read and when reading
// it.
uint32_t ad_security_map_access_mask(const uint32_t access_mask) {
    const bool is_generic_read = (access_mask == SEC_ADS_GENERIC_READ);

    if (is_generic_read) {
        const uint32_t out = (access_mask & ~SEC_ADS_LIST_OBJECT);

        return out;
    } else {
        return access_mask;
    }
}
