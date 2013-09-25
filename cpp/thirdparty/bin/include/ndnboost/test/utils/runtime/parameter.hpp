//  (C) Copyright Gennadiy Rozental 2005-2008.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision: 49312 $
//
//  Description : abstract interface for the formal parameter
// ***************************************************************************

#ifndef BOOST_RT_PARAMETER_HPP_062604GER
#define BOOST_RT_PARAMETER_HPP_062604GER

// Boost.Runtime.Parameter
#include <ndnboost/test/utils/runtime/config.hpp>

namespace ndnboost {

namespace BOOST_RT_PARAM_NAMESPACE {

// ************************************************************************** //
// **************              runtime::parameter              ************** //
// ************************************************************************** //

class parameter {
public:
    virtual ~parameter() {}
};

} // namespace BOOST_RT_PARAM_NAMESPACE

} // namespace ndnboost

#endif // BOOST_RT_PARAMETER_HPP_062604GER
