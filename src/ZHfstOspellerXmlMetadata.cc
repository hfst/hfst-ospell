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
#if HAVE_LIBXML
#  include <libxml++/libxml++.h>
#elif HAVE_TINYXML2
#  include <tinyxml2.h>
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

ZHfstOspellerXmlMetadata::ZHfstOspellerXmlMetadata()
  {
    info_.locale_ = "und";
  }

static
bool
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

static
char*
get_automaton_descr_from_id(const char* id)
  {
    const char* p = strchr(id, '.');
    const char* q = strchr(p + 1, '.');
    return hfst_strndup(p + 1, q - p);
  }


#if HAVE_LIBXML
void
ZHfstOspellerXmlMetadata::verify_hfstspeller(xmlpp::Node* rootNode)
  {
    xmlpp::Element* rootElement = dynamic_cast<xmlpp::Element*>(rootNode);
    if (NULL == rootElement)
      {
        throw ZHfstMetaDataParsingError("Root node is not an element");
      }
    const Glib::ustring rootName = rootElement->get_name();
    if (rootName != "hfstspeller")
      {
        throw ZHfstMetaDataParsingError("could not find <hfstspeller> "
                                        "root from XML file");
      }
    // check versions
    const xmlpp::Attribute* hfstversion =
        rootElement->get_attribute("hfstversion");
    if (NULL == hfstversion)
      {
        throw ZHfstMetaDataParsingError("No hfstversion attribute in root");
      }
    const Glib::ustring hfstversionValue = hfstversion->get_value();
    if (hfstversionValue != "3")
      {
        throw ZHfstMetaDataParsingError("Unrecognised HFST version...");
      }
    const xmlpp::Attribute* dtdversion = rootElement->get_attribute("dtdversion");
    if (NULL == dtdversion)
      {
        throw ZHfstMetaDataParsingError("No dtdversion attribute in root");
      }
    const Glib::ustring dtdversionValue = dtdversion->get_value();
    if (dtdversionValue != "1.0")
      {
        throw ZHfstMetaDataParsingError("Unrecognised DTD version...");
      }
  }

void
ZHfstOspellerXmlMetadata::parse_info(xmlpp::Node* infoNode)
  {
    xmlpp::Node::NodeList infos = infoNode->get_children();
    for (auto& info : infos)
      {
        const Glib::ustring infoName = info->get_name();
        if (infoName == "locale")
          {
            parse_locale(info);
          }
        else if (infoName == "title")
          {
            parse_title(info);
          }
        else if (infoName == "description")
          {
            parse_description(info);
          }
        else if (infoName == "version")
          {
            parse_version(info);
          }
        else if (infoName == "date")
          {
            parse_date(info);
          }
        else if (infoName == "producer")
          {
            parse_producer(info);
          }
        else if (infoName == "contact")
          {
            parse_contact(info);
          }
        else
          {
            const xmlpp::TextNode* text = dynamic_cast<xmlpp::TextNode*>(info);
            if ((text == NULL) || (!text->is_white_space()))
              {
                fprintf(stderr, "DEBUG: unknown info child %s\n",
                        infoName.c_str());
              }
          }
      } // while info childs
  }

void
ZHfstOspellerXmlMetadata::parse_locale(xmlpp::Node* localeNode)
  {
    xmlpp::Element* localeElement = dynamic_cast<xmlpp::Element*>(localeNode);
    if (NULL == localeElement->get_child_text())
      {
        throw ZHfstXmlParsingError("<locale> must be non-empty");
      }
    const Glib::ustring localeContent = localeElement->get_child_text()->get_content();
    if ((info_.locale_ != "und") && (info_.locale_ != localeContent))
      {
        // locale in XML mismatches previous definition
        // warnable, but overridden as per spec.
        fprintf(stderr, "Warning: mismatched languages in "
                "file data (%s) and XML (%s)\n",
                info_.locale_.c_str(), localeContent.c_str());
      }
    info_.locale_ = localeContent;
  }

void
ZHfstOspellerXmlMetadata::parse_title(xmlpp::Node* titleNode)
  {
    xmlpp::Element* titleElement = dynamic_cast<xmlpp::Element*>(titleNode);
    const xmlpp::Attribute* lang = titleElement->get_attribute("lang");
    if (NULL == titleElement->get_child_text())
      {
        throw ZHfstXmlParsingError("<title> must be non-empty");
      }
    if (lang != NULL)
      {
        info_.title_[lang->get_value()] = titleElement->get_child_text()->get_content();
      }
    else
      {
        info_.title_[info_.locale_] = titleElement->get_child_text()->get_content();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_description(xmlpp::Node* descriptionNode)
  {
    xmlpp::Element* descriptionElement =
        dynamic_cast<xmlpp::Element*>(descriptionNode);
    const xmlpp::Attribute* lang = descriptionElement->get_attribute("lang");
    if (NULL == descriptionElement->get_child_text())
      {
        throw ZHfstXmlParsingError("<description> must be non-empty");
      }
    if (lang != NULL)
      {
        info_.description_[lang->get_value()] =
          descriptionElement->get_child_text()->get_content();
      }
    else
      {
        info_.description_[info_.locale_] =
          descriptionElement->get_child_text()->get_content();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_version(xmlpp::Node* versionNode)
  {
    xmlpp::Element* versionElement = dynamic_cast<xmlpp::Element*>(versionNode);
    const xmlpp::Attribute* revision = versionElement->get_attribute("vcsrev");
    if (revision != NULL)
      {
        info_.vcsrev_ = revision->get_value();
      }
    info_.version_ = versionElement->get_child_text()->get_content();
  }

void
ZHfstOspellerXmlMetadata::parse_date(xmlpp::Node* dateNode)
  {
    xmlpp::Element* dateElement =
        dynamic_cast<xmlpp::Element*>(dateNode);
    info_.date_ = dateElement->get_child_text()->get_content();
  }

void
ZHfstOspellerXmlMetadata::parse_producer(xmlpp::Node* producerNode)
  {
    xmlpp::Element* producerElement =
        dynamic_cast<xmlpp::Element*>(producerNode);
    info_.producer_ = producerElement->get_child_text()->get_content();
  }

void
ZHfstOspellerXmlMetadata::parse_contact(xmlpp::Node* contactNode)
  {
    xmlpp::Element* contactElement = dynamic_cast<xmlpp::Element*>(contactNode);
    const xmlpp::Attribute* email = contactElement->get_attribute("email");
    const xmlpp::Attribute* website = contactElement->get_attribute("website");
    if (email != NULL)
      {
        info_.email_ = email->get_value();
      }
    if (website != NULL)
      {
        info_.website_ = website->get_value();
      }
  }


void
ZHfstOspellerXmlMetadata::parse_acceptor(xmlpp::Node* acceptorNode)
  {
    xmlpp::Element* acceptorElement =
        dynamic_cast<xmlpp::Element*>(acceptorNode);
    xmlpp::Attribute* xid = acceptorElement->get_attribute("id");
    if (xid == NULL)
      {
        throw ZHfstMetaDataParsingError("id missing in acceptor");
      }
    const Glib::ustring xidValue = xid->get_value();
    if (validate_automaton_id(xidValue.c_str()) == false)
      {
        throw ZHfstMetaDataParsingError("Invalid id in acceptor");
      }
    char* descr = get_automaton_descr_from_id(xidValue.c_str());
    acceptor_[descr].descr_ = descr;
    acceptor_[descr].id_ = xidValue;
    const xmlpp::Attribute* trtype =
        acceptorElement->get_attribute("transtype");
    if (trtype != NULL)
      {
        acceptor_[descr].transtype_ = trtype->get_value();
      }
    const xmlpp::Attribute* xtype = acceptorElement->get_attribute("type");
    if (xtype != NULL)
      {
        acceptor_[descr].type_ = xtype->get_value();
      }
    xmlpp::Node::NodeList accs = acceptorNode->get_children();
    for (auto& acc : accs)
      {
        const Glib::ustring accName = acc->get_name();
        if (accName == "title")
          {
            parse_title(acc, descr);
          }
        else if (accName == "description")
          {
            parse_description(acc, descr);
          }
        else
          {
            const xmlpp::TextNode* text = dynamic_cast<xmlpp::TextNode*>(acc);
            if ((text == NULL) || (!text->is_white_space()))
              {
                fprintf(stderr, "DEBUG: unknown acceptor node %s\n",
                        accName.c_str());
              }
          }
      }
    free(descr);
  }

void
ZHfstOspellerXmlMetadata::parse_title(xmlpp::Node* titleNode,
                                      const string& descr)
  {
    xmlpp::Element* titleElement = dynamic_cast<xmlpp::Element*>(titleNode);
    const xmlpp::Attribute* lang = titleElement->get_attribute("lang");
    if (lang != NULL)
      {
        acceptor_[descr].title_[lang->get_value()] = titleElement->get_child_text()->get_content();
      }
    else
      {
        acceptor_[descr].title_[info_.locale_] = titleElement->get_child_text()->get_content();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_description(xmlpp::Node* descriptionNode,
                                            const string& descr)
  {
    xmlpp::Element* descriptionElement =
        dynamic_cast<xmlpp::Element*>(descriptionNode);
    const xmlpp::Attribute* lang = descriptionElement->get_attribute("lang");
    if (lang != NULL)
      {
        acceptor_[descr].description_[lang->get_value()] = descriptionElement->get_child_text()->get_content();
      }
    else
      {
        acceptor_[descr].description_[info_.locale_] = descriptionElement->get_child_text()->get_content();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_errmodel(xmlpp::Node* errmodelNode)
  {
    xmlpp::Element* errmodelElement =
        dynamic_cast<xmlpp::Element*>(errmodelNode);
    xmlpp::Attribute* xid = errmodelElement->get_attribute("id");
    if (xid == NULL)
      {
        throw ZHfstMetaDataParsingError("id missing in errmodel");
      }
    const Glib::ustring xidValue = xid->get_value();
    if (validate_automaton_id(xidValue.c_str()) == false)
      {
        throw ZHfstMetaDataParsingError("Invalid id in errmodel");
      }
    char* descr = get_automaton_descr_from_id(xidValue.c_str());
    errmodel_.push_back(ZHfstOspellerErrModelMetadata());
    size_t errm_count = errmodel_.size() - 1;
    if (descr != NULL)
      {
        errmodel_[errm_count].descr_ = descr;
      }
    free(descr);
    errmodel_[errm_count].id_ = xidValue;
    xmlpp::Node::NodeList errms = errmodelNode->get_children();
    for (auto& errm : errms)
      {
        const Glib::ustring errmName = errm->get_name();
        if (errmName == "title")
          {
            parse_title(errm, errm_count);
          }
        else if (errmName == "description")
          {
            parse_description(errm, errm_count);
          }
        else if (errmName == "type")
          {
            parse_type(errm, errm_count);
          }
        else if (errmName == "model")
          {
            parse_model(errm, errm_count);
          }
        else
          {
            const xmlpp::TextNode* text = dynamic_cast<xmlpp::TextNode*>(errm);
            if ((text == NULL) || (!text->is_white_space()))
              {
                fprintf(stderr, "DEBUG: unknown errmodel node %s\n",
                        errmName.c_str());
              }
          }
      }
  }

void
ZHfstOspellerXmlMetadata::parse_title(xmlpp::Node* titleNode,
                                      size_t errm_count)
  {
    xmlpp::Element* titleElement = dynamic_cast<xmlpp::Element*>(titleNode);
    const xmlpp::Attribute* lang = titleElement->get_attribute("lang");
    if (lang != NULL)
      {
        errmodel_[errm_count].title_[lang->get_value()] = titleElement->get_child_text()->get_content();
      }
    else
      {
        errmodel_[errm_count].title_[info_.locale_] = titleElement->get_child_text()->get_content();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_description(xmlpp::Node* descriptionNode,
                                            size_t errm_count)
  {
    xmlpp::Element* descriptionElement =
        dynamic_cast<xmlpp::Element*>(descriptionNode);
    const xmlpp::Attribute* lang = descriptionElement->get_attribute("lang");
    if (lang != NULL)
      {
        errmodel_[errm_count].description_[lang->get_value()] = descriptionElement->get_child_text()->get_content();
      }
    else
      {
        errmodel_[errm_count].description_[info_.locale_] = descriptionElement->get_child_text()->get_content();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_type(xmlpp::Node* typeNode, size_t errm_count)
  {
    xmlpp::Element* typeElement = dynamic_cast<xmlpp::Element*>(typeNode);
    const xmlpp::Attribute* xtype = typeElement->get_attribute("type");
    if (xtype != NULL)
      {
        errmodel_[errm_count].type_.push_back(xtype->get_value());

      }
    else
      {
        throw ZHfstMetaDataParsingError("No type in type");
      }
  }

void
ZHfstOspellerXmlMetadata::parse_model(xmlpp::Node* modelNode, size_t errm_count)
  {
    xmlpp::Element* modelElement = dynamic_cast<xmlpp::Element*>(modelNode);
    errmodel_[errm_count].model_.push_back(modelElement->get_child_text()->get_content());
  }

void
ZHfstOspellerXmlMetadata::parse_xml(const xmlpp::Document* doc)
  {
    if (NULL == doc)
      {
        throw ZHfstMetaDataParsingError("Cannot parse XML data");
      }
    xmlpp::Node* rootNode = doc->get_root_node();
    // check validity
    if (NULL == rootNode)
      {
        throw ZHfstMetaDataParsingError("No root node in index XML");
      }
    verify_hfstspeller(rootNode);
    // parse
    xmlpp::Node::NodeList nodes = rootNode->get_children();
    for (auto& node : nodes)
      {
        const Glib::ustring nodename = node->get_name();
        if (nodename == "info")
          {
            parse_info(node);
          } // if info node
        else if (nodename == "acceptor")
          {
            parse_acceptor(node);
          } // acceptor node
        else if (nodename == "errmodel")
          {
            parse_errmodel(node);
          } // errmodel node
        else
          {
            const xmlpp::TextNode* text = dynamic_cast<xmlpp::TextNode*>(node);
            if ((text == NULL) || (!text->is_white_space()))
              {
                fprintf(stderr, "DEBUG: unknown root child %s\n",
                        nodename.c_str());
              }
          } // unknown root child node
      }
  }

void
ZHfstOspellerXmlMetadata::read_xml(const char* xml_data, size_t xml_len)
  {
    xmlpp::DomParser parser;
    parser.set_substitute_entities();
    parser.parse_memory_raw(reinterpret_cast<const unsigned char*>(xml_data),
                            xml_len);
    this->parse_xml(parser.get_document());
  }

void
ZHfstOspellerXmlMetadata::read_xml(const string& filename)
  {
    xmlpp::DomParser parser;
    parser.set_substitute_entities();
    parser.parse_file(filename);
    this->parse_xml(parser.get_document());
  }
#elif HAVE_TINYXML2

void
ZHfstOspellerXmlMetadata::parse_xml(const tinyxml2::XMLDocument& doc)
  {
    const tinyxml2::XMLElement* rootNode = doc.RootElement();
    if (NULL == rootNode)
      {
        throw ZHfstMetaDataParsingError("No root node in index XML");
      }
    // check validity
    if (strcmp(rootNode->Name(), "hfstspeller") != 0)
      {
        throw ZHfstMetaDataParsingError("could not find <hfstspeller> "
                                        "root from XML file");
      }
    verify_hfstspeller(*rootNode);
    // parse
    const tinyxml2::XMLElement* child = rootNode->FirstChildElement();
    while (child != NULL)
      {
        if (strcmp(child->Name(), "info") == 0)
          {
            parse_info(*child);
          }
        else if (strcmp(child->Name(), "acceptor") == 0)
          {
            parse_acceptor(*child);
          }
        else if (strcmp(child->Name(), "errmodel") == 0)
          {
            parse_errmodel(*child);
          }
        else
          {
            fprintf(stderr, "DEBUG: Unknown root child %s\n",
                    child->Name());
          }
        child = child->NextSiblingElement();
      }
  }

void
ZHfstOspellerXmlMetadata::verify_hfstspeller(const tinyxml2::XMLElement& hfstspellerNode)
  {
    if (!hfstspellerNode.Attribute("hfstversion"))
      {
        throw ZHfstMetaDataParsingError("No hfstversion attribute in root");
      }
    if (!hfstspellerNode.Attribute("hfstversion", "3"))
      {
        throw ZHfstMetaDataParsingError("Unrecognised HFST version...");
      }
    if (!hfstspellerNode.Attribute("dtdversion"))
      {
        throw ZHfstMetaDataParsingError("No dtdversion attribute in root");
      }
    if (!hfstspellerNode.Attribute("dtdversion", "1.0"))
      {
        throw ZHfstMetaDataParsingError("Unrecognised DTD version...");
      }
  }

void
ZHfstOspellerXmlMetadata::parse_info(const tinyxml2::XMLElement& infoNode)
  {
    const tinyxml2::XMLElement* info = infoNode.FirstChildElement();
    while (info != NULL)
      {
        if (strcmp(info->Name(), "locale") == 0)
          {
            parse_locale(*info);
          }
        else if (strcmp(info->Name(), "title") == 0)
          {
            parse_title(*info);
          }
        else if (strcmp(info->Name(), "description") == 0)
          {
            parse_description(*info);
          }
        else if (strcmp(info->Name(), "version") == 0)
          {
            parse_version(*info);
          }
        else if (strcmp(info->Name(), "date") == 0)
          {
            parse_date(*info);
          }
        else if (strcmp(info->Name(), "producer") == 0)
          {
            parse_producer(*info);
          }
        else if (strcmp(info->Name(), "contact") == 0)
          {
            parse_contact(*info);
          }
        else
          {
            fprintf(stderr, "DEBUG: unknown info child %s\n",
                    info->Name());
          }
        info = info->NextSiblingElement();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_locale(const tinyxml2::XMLElement& localeNode)
  {
    const char* localeContent = localeNode.GetText();
    if (NULL == localeNode.GetText())
      {
        throw ZHfstXmlParsingError("<locale> must be non-empty");
      }
    if ((info_.locale_ != "und") && (info_.locale_ != localeContent))
      {
        // locale in XML mismatches previous definition
        // warnable, but overridden as per spec.
        fprintf(stderr, "Warning: mismatched languages in "
                "file data (%s) and XML (%s)\n",
                info_.locale_.c_str(), localeContent);
      }
    info_.locale_ = localeContent;
  }

void
ZHfstOspellerXmlMetadata::parse_title(const tinyxml2::XMLElement& titleNode)
  {
    if (NULL == titleNode.GetText())
      {
        throw ZHfstXmlParsingError("<title> must be non-empty");
      }
    if (titleNode.Attribute("lang"))
      {
        info_.title_[titleNode.Attribute("lang")] = titleNode.GetText();
      }
    else
      {
        info_.title_[info_.locale_] = titleNode.GetText();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_description(const tinyxml2::XMLElement& descriptionNode)
  {
    if (NULL == descriptionNode.GetText())
      {
        throw ZHfstXmlParsingError("<description> must be non-empty");
      }
    if (descriptionNode.Attribute("lang"))
      {
        info_.description_[descriptionNode.Attribute("lang")] =
          descriptionNode.GetText();
      }
    else
      {
        info_.description_[info_.locale_] = descriptionNode.GetText();
      }

  }

void
ZHfstOspellerXmlMetadata::parse_version(const tinyxml2::XMLElement& versionNode)
  {
    if (versionNode.Attribute("vcsrev"))
      {
        info_.vcsrev_ = versionNode.Attribute("vcsrev");
      }
    info_.version_ = versionNode.GetText();
  }

void
ZHfstOspellerXmlMetadata::parse_date(const tinyxml2::XMLElement& dateNode)
  {
    info_.date_ = dateNode.GetText();
  }

void
ZHfstOspellerXmlMetadata::parse_producer(const tinyxml2::XMLElement& producerNode)
  {
    info_.producer_ = producerNode.GetText();
  }

void
ZHfstOspellerXmlMetadata::parse_contact(const tinyxml2::XMLElement& contactNode)
  {
    if (contactNode.Attribute("email"))
      {
        info_.email_ = contactNode.Attribute("email");
      }
    if (contactNode.Attribute("website"))
      {
        info_.website_ = contactNode.Attribute("website");
      }
  }

void
ZHfstOspellerXmlMetadata::parse_acceptor(const tinyxml2::XMLElement& acceptorNode)
  {
    const char* xid = acceptorNode.Attribute("id");
    if (xid == NULL)
      {
        throw ZHfstMetaDataParsingError("id missing in acceptor");
      }
    if (validate_automaton_id(xid) == false)
      {
        throw ZHfstMetaDataParsingError("Invalid id in accpetor");
      }
    char* descr = get_automaton_descr_from_id(xid);
    acceptor_[descr].descr_ = descr;
    acceptor_[descr].id_ = xid;
    if (acceptorNode.Attribute("trtype"))
      {
        acceptor_[descr].transtype_ = acceptorNode.Attribute("trtype");
      }
    if (acceptorNode.Attribute("type"))
      {
        acceptor_[descr].type_ = acceptorNode.Attribute("type");
      }
    const tinyxml2::XMLElement* acc = acceptorNode.FirstChildElement();
    while (acc != NULL)
      {
        if (strcmp(acc->Name(), "title") == 0)
          {
            parse_title(*acc, descr);
          }
        else if (strcmp(acc->Name(), "description") == 0)
          {
            parse_description(*acc, descr);
          }
        else
          {
            fprintf(stderr, "DEBUG: unknown acceptor child %s\n",
                    acc->Name());
          }
        acc = acc->NextSiblingElement();
      }
    free(descr);
  }

void
ZHfstOspellerXmlMetadata::parse_title(const tinyxml2::XMLElement& titleNode,
                                      const std::string& accName)
  {
    if (titleNode.Attribute("lang"))
      {
        acceptor_[accName].title_[titleNode.Attribute("lang")] =
          titleNode.GetText();
      }
    else
      {
        acceptor_[accName].title_[info_.locale_] =
          titleNode.GetText();
      }
  }


void
ZHfstOspellerXmlMetadata::parse_description(const tinyxml2::XMLElement& descriptionNode,
                       const std::string& accName)
  {
    if (descriptionNode.Attribute("lang"))
      {
        acceptor_[accName].description_[descriptionNode.Attribute("lang")] =
          descriptionNode.GetText();
      }
    else
      {
        acceptor_[accName].description_[info_.locale_] =
          descriptionNode.GetText();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_errmodel(const tinyxml2::XMLElement& errmodelNode)
  {
    const char* xid = errmodelNode.Attribute("id");
    if (xid == NULL)
      {
        throw ZHfstMetaDataParsingError("id missing in errmodel");
      }
    if (validate_automaton_id(xid) == false)
      {
        throw ZHfstMetaDataParsingError("Invalid id in errmodel");
      }
    char* descr = get_automaton_descr_from_id(xid);
    errmodel_.push_back(ZHfstOspellerErrModelMetadata());
    size_t errm_count = errmodel_.size() - 1;
    if (descr != NULL)
      {
        errmodel_[errm_count].descr_ = descr;
      }
    free(descr);
    errmodel_[errm_count].id_ = xid;
    const tinyxml2::XMLElement* errm = errmodelNode.FirstChildElement();
    while (errm != NULL)
      {
        if (strcmp(errm->Name(), "title") == 0)
          {
            parse_title(*errm, errm_count);
          }
        else if (strcmp(errm->Name(), "description") == 0)
          {
            parse_description(*errm, errm_count);
          }
        else if (strcmp(errm->Name(), "type") == 0)
          {
            parse_type(*errm, errm_count);
          }
        else if (strcmp(errm->Name(), "model") == 0)
          {
            parse_model(*errm, errm_count);
          }
        else
          {
            fprintf(stderr, "DEBUG: unknown errmodel child %s\n",
                    errm->Name());
          }
        errm = errm->NextSiblingElement();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_title(const tinyxml2::XMLElement& titleNode, size_t errm_count)
  {
    if (titleNode.Attribute("lang"))
      {
        errmodel_[errm_count].title_[titleNode.Attribute("lang")] =
          titleNode.GetText();
      }
    else
      {
        errmodel_[errm_count].title_[info_.locale_] = titleNode.GetText();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_description(const tinyxml2::XMLElement& descriptionNode, size_t errm_count)
  {
    if (descriptionNode.Attribute("lang"))
      {
        errmodel_[errm_count].description_[descriptionNode.Attribute("lang")] =
          descriptionNode.GetText();
      }
    else
      {
        errmodel_[errm_count].description_[info_.locale_] =
          descriptionNode.GetText();
      }
  }

void
ZHfstOspellerXmlMetadata::parse_type(const tinyxml2::XMLElement& typeNode, size_t errm_count)
  {
    if (typeNode.Attribute("type"))
      {
        errmodel_[errm_count].type_.push_back(typeNode.Attribute("type"));
      }
    else
      {
        throw ZHfstMetaDataParsingError("No type in type");
      }
  }

void
ZHfstOspellerXmlMetadata::parse_model(const tinyxml2::XMLElement& modelNode, size_t errm_count)
  {
    errmodel_[errm_count].model_.push_back(modelNode.GetText());
  }

void
ZHfstOspellerXmlMetadata::read_xml(const char* xml_data, size_t xml_len)
  {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml_data, xml_len) != tinyxml2::XML_NO_ERROR)
      {
        throw ZHfstMetaDataParsingError("Reading XML from memory");
      }
    this->parse_xml(doc);
  }

void
ZHfstOspellerXmlMetadata::read_xml(const string& filename)
  {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_NO_ERROR)
      {
        throw ZHfstMetaDataParsingError("Reading XML from file");
      }
    this->parse_xml(doc);
  }
#else

void
ZHfstOspellerXmlMetadata::read_xml(const char* xml_data, size_t xml_len)
  {
    // Parse locale
    auto b = strstr(xml_data, "<locale>");
    auto e = strstr(b, "</locale>");

    if (b == nullptr || e == nullptr || b >= e) {
        throw ZHfstMetaDataParsingError("Could not find <locale>...</locale> in the XML");
    }

    info_.locale_.assign(b + 8, e);

    // Parse title
    b = strstr(xml_data, "<title>");
    e = strstr(b, "</title>");

    if (b == nullptr || e == nullptr || b >= e) {
        throw ZHfstMetaDataParsingError("Could not find <title>...</title> in the XML");
    }

    info_.title_[info_.locale_].assign(b + 7, e);

    // Parse description
    b = strstr(xml_data, "<description>");
    e = strstr(b, "</description>");

    if (b == nullptr || e == nullptr || b >= e) {
        throw ZHfstMetaDataParsingError("Could not find <description>...</description> in the XML");
    }

    info_.description_[info_.locale_].assign(b + 13, e);
  }

void
ZHfstOspellerXmlMetadata::read_xml(const std::string& filename)
  {
    std::ifstream file(filename, std::ios::binary);
    struct stat st;
    stat(filename.c_str(), &st);

    std::string xml(static_cast<size_t>(st.st_size), 0);
    file.read(&xml[0], static_cast<size_t>(st.st_size));

    read_xml(xml.c_str(), xml.size());
  }

#endif // HAVE_LIBXML


string
ZHfstOspellerXmlMetadata::debug_dump() const
  {
    string retval = "locale: " + info_.locale_ + "\n"
        "version: " + info_.version_ + " [vcsrev: " + info_.vcsrev_ + "]\n"
        "date: " + info_.date_ + "\n"
        "producer: " + info_.producer_ + "[email: <" + info_.email_ + ">, "
        "website: <" + info_.website_ + ">]\n";
    for (auto& title : info_.title_)
      {
        retval.append("title [" + title.first + "]: " + title.second + "\n");
      }
    for (auto& description : info_.description_)
      {
        retval.append("description [" + description.first + "]: " +
            description.second + "\n");
      }
    for (auto& acc : acceptor_)
      {
        retval.append("acceptor[" + acc.second.descr_ + "] [id: " + acc.second.id_ +
                      ", type: " + acc.second.type_ + "trtype: " + acc.second.transtype_ +
                      "]\n");

        for (auto& title : acc.second.title_)
          {
            retval.append("title [" + title.first + "]: " + title.second +
                          "\n");
          }
        for (auto& description : acc.second.description_)
          {
            retval.append("description[" + description.first + "]: "
                          + description.second + "\n");
          }
      }
    for (auto& errm : errmodel_)
      {
        retval.append("errmodel[" + errm.descr_ + "] [id: " + errm.id_ +
                      "]\n");

        for (auto& title : errm.title_)
          {
            retval.append("title [" + title.first + "]: " + title.second +
                          "\n");
          }
        for (auto& description : errm.description_)
          {
            retval.append("description[" + description.first + "]: "
                          + description.second + "\n");
          }
        for (auto& type : errm.type_)
          {
            retval.append("type: " + type + "\n");
          }
        for (auto& model : errm.model_)
          {
            retval.append("model: " + model + "\n");
          }
      }

    return retval;
  }

  } // namespace hfst_ospell


