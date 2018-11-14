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

#ifndef HFST_OSPELL_ZHFSTOSPELLERXMLMETADATA_H_
#define HFST_OSPELL_ZHFSTOSPELLERXMLMETADATA_H_ 1

#include "hfstol-stdafx.h"

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <map>

using std::map;

#if HAVE_PUGIXML
#  include <pugixml.hpp>
#endif

#include "ospell.h"
#include "hfst-ol.h"

namespace hfst_ospell 
{
    //! @brief data type for associating set of translations to languages.
    typedef std::map<std::string,std::string> LanguageVersions;

    //! @brief ZHfstOspellerInfo represents one info block of an zhfst file.
    //! @see https://victorio.uit.no/langtech/trunk/plan/proof/doc/lexfile-spec.xml
    struct ZHfstOspellerInfoMetadata
    {
        //! @brief active locale of speller in BCP format
        std::string locale_;
        //! @brief transalation of titles to all languages
        LanguageVersions title_;
        //! @brief translation of descriptions to all languages
        LanguageVersions description_;
        //! @brief version defintition as free form string
        std::string version_;
        //! @brief vcs revision as string
        std::string vcsrev_;
        //! @brief date for age of speller as string
        std::string date_;
        //! @brief producer of the speller
        std::string producer_;
        //! @brief email address of the speller
        std::string email_;
        //! @brief web address of the speller
        std::string website_;
    };

    //! @brief Represents one acceptor block in XML metadata
    struct ZHfstOspellerAcceptorMetadata
    {
        //! @brief unique id of acceptor
        std::string id_;
        //! @brief descr part of acceptor
        std::string descr_;
        //! @brief type of dictionary
        std::string type_;
        //! @brief type of transducer
        std::string transtype_;
        //! @brief titles of dictionary in languages
        LanguageVersions title_;
        //! @brief descriptions of dictionary in languages
        LanguageVersions description_;
    };

    //! @brief Represents one errmodel block in XML metadata
    struct ZHfstOspellerErrModelMetadata
    {
        //! @brief id of each error model in set
        std::string id_;
        //! @brief descr part of each id
        std::string descr_;
        //! @brief title of error models in languages
        LanguageVersions title_;
        //! @brief description of error models in languages
        LanguageVersions description_;
        //! @brief types of error models
        std::vector<std::string> type_; 
        //! @brief models
        std::vector<std::string> model_;
    };

    //! @brief holds one index.xml metadata for whole ospeller
    class ZHfstOspellerXmlMetadata
    {
        public:
        //! @brief construct metadata for undefined language and other default
        //!        values
        ZHfstOspellerXmlMetadata();
        //! @brief read metadata from XML file by @a filename.
        void read_xml(const std::string& filename);
        //! @brief read XML from in memory @a data pointer with given @a length
        //!
        //! Depending on the XML library compiled in, the data length may
        //! be omitted and the buffer may be overflown.
        void read_xml(const char* data, size_t data_length);
        //! @brief create a programmer readable dump of XML metadata.
        //!
        //! shows linear serialisation of all header data in random order.
        std::string debug_dump() const;

        ZHfstOspellerInfoMetadata info_; //!< The info node data
        //! @brief data for acceptor nodes
        std::map<std::string,ZHfstOspellerAcceptorMetadata> acceptor_; 
        //! @brief data for errmodel nodes
        std::vector<ZHfstOspellerErrModelMetadata> errmodel_;

        private:
#if HAVE_PUGIXML
        void deserialize(pugi::xml_document& doc);
#endif
    };
}

#endif // inclusion GUARD
// vim: set ft=cpp.doxygen:
