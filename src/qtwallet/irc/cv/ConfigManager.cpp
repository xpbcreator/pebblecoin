// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QFile>
#include <QTemporaryFile>
#include <QVariant>
#include <QColor>
#include <QDebug>
#include "cv/ConfigManager.h"
#include "json.h"

namespace cv {

ConfigManager::ConfigManager(const QString &defaultFilename)
    : m_commentRegex("^\\s*#"),
      m_newlineRegex("[\r\n]*"),
      m_defaultFilename(defaultFilename)
{
    g_pEvtManager->createEvent("configChanged", STRING_EVENT);
}

// This function puts options (and their values) into memory under
// a specific filename.
//
// If the file with the given name exists, it overwrites any of the
// provided default options with the values from the file. Options
// within the file that are not found in the list of default options
// are ignored; this allows options to be easily deprecated without
// replacing the file.
//
// If the file does not exist, it creates a new file with the given
// map of default ConfigOptions.
bool ConfigManager::setupConfigFile(const QString &filename, QMap<QString, ConfigOption> &options)
{
    QFile file(filename);
    if(file.exists() && !file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug("[CM::setupConfigFile] File %s could not be opened for read", filename.toLatin1().constData());
        return false;
    }

    if(file.exists())
    {
        // Read in the entire file, parse the JSON.
        QString jsonStr = file.readAll();
        bool success;
        QVariantMap cfgMap = QtJson::Json::parse(jsonStr, success).toMap();
        if(success)
        {
            // For each default option...
            for(QMap<QString, ConfigOption>::iterator i = options.begin(); i != options.end(); ++i)
            {
                // ...find the option with the same key in the cfg from the file;
                // if it exists AND its value is valid, replace it.
                QVariantMap::iterator optionFromFile = cfgMap.find(i.key());
                ConfigOption &opt = i.value();
                if(optionFromFile != cfgMap.end() && isValueValid(optionFromFile.value(), opt.type))
                {
                    opt.value = optionFromFile.value();
                    opt.ensureTypeIsCorrect();
                }
            }
        }
    }
    else
    {
        if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qDebug("[CM::setupConfigFile] File %s could not be opened for write", filename.toLatin1().constData());
            return false;
        }

        // Write all the default options to the file.
        QVariantMap cfgMap;
        for(QMap<QString, ConfigOption>::iterator i = options.begin(); i != options.end(); ++i)
            cfgMap.insert(i.key(), i->value);

        file.write(QtJson::Json::serialize(cfgMap));
    }

    file.close();
    m_files.insert(filename, options);
    return true;
}

//-----------------------------------//

// Writes all the data in memory to the provided file.
bool ConfigManager::writeToFile(const QString &filename)
{
    QHash<QString, QMap<QString, ConfigOption> >::iterator i = m_files.find(filename);

    if(i != m_files.end())
    {
        QFile file(filename);
        if(file.exists() && !file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        {
            qDebug("[CM::writeToFile] File %s could not be opened for write", filename.toLatin1().constData());
            return false;
        }

        // Write all the current options in memory to the file.
        QMap<QString, ConfigOption> options = i.value();
        QVariantMap cfgMap;
        for(QMap<QString, ConfigOption>::iterator j = options.begin(); j != options.end(); ++j)
            cfgMap.insert(j.key(), j->value);

        file.write(QtJson::Json::serialize(cfgMap));
        return true;
    }
    else
        qDebug("[CM::writeToFile] Config file %s not found in memory", filename.toLatin1().constData());

    return false;
}

//-----------------------------------//

// Returns the value of the provided option inside the provided file,
// or an empty string upon error.
QVariant ConfigManager::getOptionValue(const QString &filename, const QString &optName)
{
    QHash<QString, QMap<QString, ConfigOption> >::iterator i = m_files.find(filename);
    if(i != m_files.end())
    {
        QMap<QString, ConfigOption>::iterator j = i.value().find(optName);
        if(j != i.value().end())
            return j->value;
        else
            qDebug("[CM::getOptionValue] Config option %s (in file %s) does not exist",
                   optName.toLatin1().constData(),
                   filename.toLatin1().constData());
    }
    else
        qDebug("[CM::getOptionValue] Config file %s not found", filename.toLatin1().constData());

    return "";
}

//-----------------------------------//

// Returns the config type for the provided key.
ConfigType ConfigManager::getOptionType(const QString &filename, const QString &optName)
{
    QHash<QString, QMap<QString, ConfigOption> >::iterator i = m_files.find(filename);
    if(i != m_files.end())
    {
        QMap<QString, ConfigOption>::iterator j = i.value().find(optName);
        if(j != i.value().end())
            return j->type;
        else
            qDebug("[CM::getOptionType] Config option %s (in file %s) does not exist",
                   optName.toLatin1().constData(),
                   filename.toLatin1().constData());
    }
    else
        qDebug("[CM::getOptionType] Config file %s not found", filename.toLatin1().constData());

    return CONFIG_TYPE_UNKNOWN;
}

//-----------------------------------//

// Sets the provided option's value to [optValue].
bool ConfigManager::setOptionValue(
        const QString &filename,
        const QString &optName,
        const QVariant &optValue,
        bool fireEvent)
{
    QHash<QString, QMap<QString, ConfigOption> >::iterator i = m_files.find(filename);
    if(i != m_files.end())
    {
        QMap<QString, ConfigOption>::iterator j = i.value().find(optName);
        if(j != i.value().end())
        {
            // If the new value is valid...
            ConfigOption &opt = j.value();
            if(isValueValid(optValue, opt.type))
            {
                // ...then set the new value and fire the "configChanged" event.
                opt.value = optValue;
                opt.ensureTypeIsCorrect();
                if(fireEvent)
                {
                    ConfigEvent *pEvent = new ConfigEvent(filename, optName, opt.value, opt.type);
                    g_pEvtManager->fireEvent("configChanged", optName, pEvent);
                    delete pEvent;
                }
                return true;
            }
            else
                qDebug("[CM::setOptionValue] Value is not valid for config option %s",
                       optName.toLatin1().constData());
        }
        else
            qDebug("[CM::setOptionValue] Config option %s (in file %s) does not exist",
                   optName.toLatin1().constData(),
                   filename.toLatin1().constData());
    }
    else
        qDebug("[CM::setOptionValue] Config file %s not found", filename.toLatin1().constData());

    return false;
}

//-----------------------------------//

// Returns true if the [value] can be converted to the given [type], false otherwise.
bool ConfigManager::isValueValid(const QVariant &value, ConfigType type)
{
    switch(type)
    {
        case CONFIG_TYPE_INTEGER:
        {
            bool ok;
            value.toInt(&ok);
            return ok;
        }
        case CONFIG_TYPE_BOOLEAN:
        {
            value.toBool();
            return true;
        }
        case CONFIG_TYPE_COLOR:
            return value.value<QColor>().isValid();
        case CONFIG_TYPE_LIST:
            return value.canConvert(QVariant::List);
        case CONFIG_TYPE_MAP:
            return value.canConvert(QVariant::Map);
        default:
            break;
    }

    return true;
}

} // End namespace
