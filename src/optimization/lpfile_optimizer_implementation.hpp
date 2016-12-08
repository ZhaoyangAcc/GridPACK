// Emacs Mode Line: -*- Mode:c++;-*-
// -------------------------------------------------------------
/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
// -------------------------------------------------------------
// -------------------------------------------------------------
/**
 * @file   lpfile_optimizer_implementation.hpp
 * @author William A. Perkins
 * @date   2016-12-07 15:23:49 d3g096
 * 
 * @brief  
 * 
 * 
 */
// -------------------------------------------------------------


#ifndef _lpfile_optimizer_implementation_hpp_
#define _lpfile_optimizer_implementation_hpp_

#include <iosfwd>
#include <string>
#include "file_optimizer_implementation.hpp"

namespace gridpack {
namespace optimization {



// -------------------------------------------------------------
//  class LPFileOptimizerImplementation
// -------------------------------------------------------------
class LPFileOptimizerImplementation 
  : public FileOptimizerImplementation
{
public:

  /// Default constructor.
  LPFileOptimizerImplementation(const parallel::Communicator& comm)
    : FileOptimizerImplementation(comm)
  {}

  /// Destructor
  ~LPFileOptimizerImplementation(void)
  {}

protected:

  /// Open a stream to a new temporary file
  std::string p_temporaryFileName(void);

  /// Write an LP file to the specified stream
  virtual void p_write(const p_optimizeMethod& m, std::ostream& out);

};


} // namespace optimization
} // namespace gridpack


#endif
