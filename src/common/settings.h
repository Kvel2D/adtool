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

#ifndef SETTINGS_H
#define SETTINGS_H

/**
 * Provides access to settings via enums rather than plain strings.
 * Settings are saved to file automatically when this object is
 * destructed.
 * Settings of boolean type have BoolSettingSignal objects which emit
 * changed() signal when setting is changed
 *
 * NOTE: admc and gpgui share settings keys BUT
 * the settings files are separate
 *
 * NOTE: MUST BE used AFTER app's organization and applications names are set (in main)
 */

#include <QObject>

class QAction;
class QSettings;
class QVariant;
class QWidget;

enum VariantSetting {
    // ADMC
    VariantSetting_Principal,    
    VariantSetting_Locale,

    // GPGUI

    // Shared
    VariantSetting_MainWindowGeometry,

    VariantSetting_COUNT,    
};

enum BoolSetting {
    // ADMC
    BoolSetting_AdvancedView,
    BoolSetting_ConfirmActions,
    BoolSetting_ShowStatusLog,
    BoolSetting_AutoLogin,
    BoolSetting_DevMode,
    BoolSetting_DetailsIsDocked,
    BoolSetting_ShowNonContainersInContainersTree,
    BoolSetting_LastNameBeforeFirstName,

    // GPGUI

    BoolSetting_COUNT,
};

class BoolSettingSignal final : public QObject {
Q_OBJECT
signals:
    void changed();
};

class Settings final : public QObject {
Q_OBJECT

public:
    static Settings *instance();

    QVariant get_variant(VariantSetting type) const;
    void set_variant(VariantSetting type, const QVariant &value);

    const BoolSettingSignal *get_bool_signal(BoolSetting type) const;
    bool get_bool(BoolSetting type) const;
    void set_bool(const BoolSetting type, const bool value);

    /** 
     * Connect action and bool setting so that toggling
     * the action updates the setting value
     * Action becomes checkable
     */ 
    void connect_action_to_bool_setting(QAction *action, const BoolSetting type);

    void restore_geometry(QWidget *widget, const VariantSetting geometry_setting);
    void save_geometry(QWidget *widget, const VariantSetting geometry_setting);

private:
    QSettings *qsettings = nullptr;
    BoolSettingSignal bools[BoolSetting_COUNT];

    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings(Settings&&) = delete;
    Settings& operator=(Settings&&) = delete;
};

Settings *SETTINGS();

#endif /* SETTINGS_H */
