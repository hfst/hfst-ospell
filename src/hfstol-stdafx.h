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

#ifndef HFST_OSPELL_STDAFX_H_
#define HFST_OSPELL_STDAFX_H_

#ifdef _MSC_VER
    // warning C4512: assignment operator could not be generated
    #pragma warning (disable: 4512)
    // warning C4456: declaration hides previous local declaration
    #pragma warning (disable: 4456)
    // warning C4458: declaration hides class member
    #pragma warning (disable: 4458)
    // warning C4996: POSIX names deprecated
    #pragma warning (disable: 4996)
#endif

#ifdef _WIN32
    #ifdef LIBHFSTOSPELL_EXPORTS
        #define OSPELL_API __declspec(dllexport)
    #else
        #define OSPELL_API __declspec(dllimport)
    #endif
#else
    #ifdef LIBHFSTOSPELL_EXPORTS
        #define OSPELL_API __attribute__ ((visibility ("default")))
    #else
        #define OSPELL_API
    #endif
#endif

#endif
