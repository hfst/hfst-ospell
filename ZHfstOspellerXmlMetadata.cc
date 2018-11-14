/* -*- Mode: C++ -*- */
// Copyright 2010 University of Helsinki
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

//! XXX: Valgrind note: all errors about invalid reads in wcscmp are because of:
//! https://bugzilla.redhat.com/show_bug.cgi?id=755242

#if HAVE_CONFIG_H
#  include <config.h>
#endif

// C++
#if HAVE_PUGIXML
#  include <pugixml.hpp>
#endif

#include <fstream>
#include <string>
#include <map>
#include <cstring>
#include <sys/stat.h>

using std::string;
using std::map;

// local
#include "ospell.h"
#include "hfst-ol.h"
#include "ZHfstOspeller.h"
#include "ZHfstOspellerXmlMetadata.h"

namespace hfst_ospell
{

static bool
validate_automaton_id(const char* id)
{
    const char* p = strchr(id, '.');
    if (p == NULL)
    {
        return false;
    }

    const char* q = strchr(p + 1, '.');
    if (q == NULL)
    {
        return false;
    }

    return true;
}

static std::string
get_automaton_descr_from_id(const char* id)
{
    const char* p = strchr(id, '.');
    const char* q = strchr(p + 1, '.');
    return std::string(p + 1, q - p - 1);
}

ZHfstOspellerXmlMetadata::ZHfstOspellerXmlMetadata()
{
    info_.locale_ = "und";
}

void
ZHfstOspellerXmlMetadata::read_xml(const std::string& filename)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.c_str());

    deserialize(doc);
}

void
ZHfstOspellerXmlMetadata::read_xml(const char* data, size_t data_length)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(data, data_length);

    deserialize(doc);
}

LanguageVersions make_language_map(pugi::xml_node& doc, const char* tag)
{
    LanguageVersions map;

    for (pugi::xml_node node: doc.children(tag)) {
        std::string lang = node.attribute("lang").value();
        std::string value = node.text().as_string();
        map.insert(std::pair(lang, value));
    }

    return map;
}

std::pair<std::string, ZHfstOspellerAcceptorMetadata>
parse_acceptor(pugi::xml_node& node) {
    std::string id = node.attribute("id").value();
    if (validate_automaton_id(id.c_str()) == false) {
        throw ZHfstMetaDataParsingError("Invalid ID in acceptor");
    }

    std::string descr = get_automaton_descr_from_id(id.c_str());

    ZHfstOspellerAcceptorMetadata acceptor;

    acceptor.descr_ = descr;
    acceptor.id_ = id;

    if (node.attribute("type")) {
        acceptor.type_ = node.attribute("type").value();
    }

    if (node.attribute("transtype")) {
        acceptor.transtype_ = node.attribute("transtype").value();
    }

    acceptor.title_ = make_language_map(node, "title");
    acceptor.description_ = make_language_map(node, "description");
    
    return std::pair(descr, acceptor);
}

ZHfstOspellerErrModelMetadata
parse_errmodel(pugi::xml_node& node) {
    std::string id = node.attribute("id").value();
    if (validate_automaton_id(id.c_str()) == false) {
        throw ZHfstMetaDataParsingError("Invalid ID in errmodel");
    }

    std::string descr = get_automaton_descr_from_id(id.c_str());

    ZHfstOspellerErrModelMetadata errmodel;

    errmodel.descr_ = descr;
    errmodel.id_ = id;

    for (pugi::xml_node type_node: node.children("type")) {
        if (type_node.attribute("type") != NULL) {
            errmodel.type_.push_back(type_node.attribute("type").value());
            continue;
        }
        
        std::string value = std::string(type_node.text().as_string());
        if (!value.empty()) {
            errmodel.type_.push_back(value);
        }
    }

    for (pugi::xml_node type_node: node.children("model")) {
        errmodel.model_.push_back(type_node.text().as_string());
    }

    errmodel.title_ = make_language_map(node, "title"); 
    errmodel.description_ = make_language_map(node, "description");
    
    return errmodel;
}

void
ZHfstOspellerXmlMetadata::deserialize(pugi::xml_document& doc)
{
    // Validate root attrs
    pugi::xml_node root = doc.child("hfstspeller");

    if (root.name() != std::string("hfstspeller")) {
        throw ZHfstMetaDataParsingError("Root node must be 'hfstspeller', got: " + std::string(doc.name()));
    }

    if (root.attribute("dtdversion").value() != std::string("1.0")) {
        throw ZHfstMetaDataParsingError("DTD version not supported");
    }

    if (root.attribute("hfstversion").value() != std::string("3")) {
        throw ZHfstMetaDataParsingError("HFST version not supported");
    }
    
    ZHfstOspellerInfoMetadata info;
    pugi::xml_node info_node = root.child("info");

    pugi::xml_node version_node = info_node.child("version");
    pugi::xml_node contact_node = info_node.child("contact");

    info.locale_ = info_node.child("locale").text().as_string();
    info.title_ = make_language_map(info_node, "title");
    info.description_ = make_language_map(info_node, "description");
    info.version_ = version_node.text().as_string();
    info.vcsrev_ = version_node.attribute("vcsrev").value();
    info.date_ = info_node.child("date").text().as_string();
    info.producer_ = info_node.child("producer").value();
    info.email_ = contact_node.attribute("email").value();
    info.website_ = contact_node.attribute("website").value();
    
    std::map<std::string, ZHfstOspellerAcceptorMetadata> acceptors;
    for (pugi::xml_node node: root.children("acceptor")) {
        acceptors.insert(parse_acceptor(node));
    }

    std::vector<ZHfstOspellerErrModelMetadata> errmodels;
    for (pugi::xml_node node: root.children("errmodel")) {
        errmodels.push_back(parse_errmodel(node));
    }

    info_ = info;
    acceptor_ = acceptors;
    errmodel_ = errmodels;
}

std::string
ZHfstOspellerXmlMetadata::debug_dump() const
{
    std::string retval = "locale: " + info_.locale_ + "\n"
        "version: " + info_.version_ + " [vcsrev: " + info_.vcsrev_ + "]\n"
        "date: " + info_.date_ + "\n"
        "producer: " + info_.producer_ + "[email: <" + info_.email_ + ">, "
        "website: <" + info_.website_ + ">]\n";
    for (auto& title : info_.title_) {
        retval.append("title [" + title.first + "]: " + title.second + "\n");
    }

    for (auto& description : info_.description_) {
        retval.append("description [" + description.first + "]: " +
            description.second + "\n");
    }

    for (auto& acc : acceptor_) {
        retval.append("acceptor[" + acc.second.descr_ + "] [id: " + acc.second.id_ +
            ", type: " + acc.second.type_ + " trtype: " + acc.second.transtype_ +
            "]\n");

        for (auto& title : acc.second.title_) {
            retval.append("title [" + title.first + "]: " + title.second + "\n");
        }
        for (auto& description : acc.second.description_) {
            retval.append("description[" + description.first + "]: " + description.second + "\n");
        }
    }

    for (auto& errm : errmodel_) {
        retval.append("errmodel[" + errm.descr_ + "] [id: " + errm.id_ + "]\n");

        for (auto& title : errm.title_) {
            retval.append("title [" + title.first + "]: " + title.second + "\n");
        }

        for (auto& description : errm.description_) {
            retval.append("description[" + description.first + "]: "
                + description.second + "\n");
        }

        for (auto& type : errm.type_) {
            retval.append("type: " + type + "\n");
        }

        for (auto& model : errm.model_) {
            retval.append("model: " + model + "\n");
        }
    }

    return retval;
}

} // namespace hfst_ospell


