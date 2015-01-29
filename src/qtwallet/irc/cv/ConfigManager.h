// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// ConfigManager manages all the configuration settings used by the client.
// It allows creating a config file with default options or, if one already
// exists, then reading existing options into memory. It owns a map of ConfigOptions,
// which hold the value and type of an individual option. Config options are
// stored in text files as JSON.
//
// ConfigOption stores both the value and the type of a config property. It
// doesn't need to store the name because they are stored in a map by name.

#pragma once

#include <QHash>
#include <QString>
#include <QMap>
#include <QList>
#include <QRegExp>
#include <QVariant>
#include <QColor>
#include "json.h"
#include "cv/EventManager.h"

#define GET_OPT(x)      g_pCfgManager->getOptionValue(x)
#define GET_STRING(x)   g_pCfgManager->getOptionValue(x).toString()
#define GET_INT(x)      g_pCfgManager->getOptionValue(x).toInt()
#define GET_BOOL(x)     g_pCfgManager->getOptionValue(x).toBool()
#define GET_COLOR(x)    g_pCfgManager->getOptionValue(x).value<QColor>()
#define GET_LIST(x)     g_pCfgManager->getOptionValue(x).toList()
#define GET_MAP(x)      g_pCfgManager->getOptionValue(x).toMap()

namespace cv {

enum ConfigType {
    CONFIG_TYPE_UNKNOWN = 0,

    CONFIG_TYPE_INTEGER,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_BOOLEAN,
    CONFIG_TYPE_COLOR,
    CONFIG_TYPE_LIST,
    CONFIG_TYPE_MAP
};

//-----------------------------------//

struct ConfigOption
{
    QVariant    value;
    ConfigType  type;

    ConfigOption(const QVariant &v, ConfigType t = CONFIG_TYPE_STRING)
        : value(v),
          type(t)
    { }

    // Converts the underlying value to the correct type, in case
    // it's currently being stored as a string; this can happen
    // if the value gets changed directly in the config file, or
    // by the user using the input box.
    void ensureTypeIsCorrect()
    {
        switch(type)
        {
            case CONFIG_TYPE_INTEGER:
            {
                value.convert(QVariant::Int);
                break;
            }
            case CONFIG_TYPE_BOOLEAN:
            {
                value.convert(QVariant::Bool);
                break;
            }
            default:
                break;
        }
    }
};

//-----------------------------------//

class ConfigEvent : public Event
{
    QString         m_filename;
    QString         m_name;
    QVariant        m_value;
    ConfigType      m_type;

public:
    ConfigEvent(const QString &filename, const QString &name, const QVariant &value, ConfigType type)
        : m_filename(filename),
          m_name(name),
          m_value(value),
          m_type(type)
    { }

    QString getFilename() const { return m_filename; }
    QString getName() const { return m_name; }
    QVariant getValue() const { return m_value; }
    QString getString() const { return m_value.toString(); }
    bool getBool() const { return m_value.toBool(); }
    int getInt() const { return m_value.toInt(); }
    QColor getColor() const { return m_value.value<QColor>(); }
    QVariantList getList() const { return m_value.toList(); }
    QVariantMap getMap() const { return m_value.toMap(); }
    ConfigType getType() const { return m_type; }
};

//-----------------------------------//

class ConfigManager
{
    QHash<QString, QMap<QString, ConfigOption> > m_files;
    QString m_defaultFilename;

    // Comments start the line with '#'.
    QRegExp m_commentRegex;
    QRegExp m_newlineRegex;

public:
    ConfigManager(const QString &defaultFilename);

    bool setupConfigFile(const QString &filename, QMap<QString, ConfigOption> &options);
    bool writeToFile(const QString &filename);
    QVariant getOptionValue(const QString &filename, const QString &optName);
    ConfigType getOptionType(const QString &filename, const QString &optName);
    bool setOptionValue(const QString &filename, const QString &optName, const QVariant &optValue, bool fireEvent);
    bool isValueValid(const QVariant &value, ConfigType type);

    // Calls setupConfigFile() with the default filename.
    bool setupDefaultConfigFile(QMap<QString, ConfigOption> &options)
    {
        return setupConfigFile(m_defaultFilename, options);
    }

    // Calls writeToFile() with the default filename.
    bool writeToDefaultFile()
    {
        return writeToFile(m_defaultFilename);
    }

    // Returns the value of the key in the default file.
    inline QVariant getOptionValue(const QString &optName)
    {
        return getOptionValue(m_defaultFilename, optName);
    }

    // Returns the type of the key in the default file.
    inline ConfigType getOptionType(const QString &optName)
    {
        return getOptionType(m_defaultFilename, optName);
    }

    // Sets the option's value to [optValue] in the default file.
    inline bool setOptionValue(const QString &optName, const QVariant &optValue, bool fireEvent)
    {
        return setOptionValue(m_defaultFilename, optName, optValue, fireEvent);
    }
};

extern ConfigManager *g_pCfgManager;

} // End namespace
