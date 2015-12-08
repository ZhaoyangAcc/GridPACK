/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
// -------------------------------------------------------------
/**
 * @file   ds_factory.hpp
 * @author Shuangshuang Jin 
 * @date   September 19, 2013
 * 
 * @brief  
 * 
 * 
 */
// -------------------------------------------------------------

#ifndef _ds_factory_h_
#define _ds_factory_h_

#include "boost/smart_ptr/shared_ptr.hpp"
#include "gridpack/include/gridpack.hpp"
#include "gridpack/applications/components/ds_matrix/ds_components.hpp"

namespace gridpack {
namespace dynamic_simulation {

class DSFactory
  : public gridpack::factory::BaseFactory<DSNetwork> {
  public:
    /**
     * Basic constructor
     * @param network: network associated with factory
     */
    DSFactory(NetworkPtr network);

    /**
     * Basic destructor
     */
    ~DSFactory();

    /**
     * Create the admittance (Y-Bus) matrix
     */
    void setYBus(void);

    /**
     * Apply an event to all branches in the system
     * @param event a struct describing a fault
     */
    void setEvent(const DSBranch::Event &event);

    /**
     * Check network to see if there is a process with no generators
     * @return true if all processors have at least on generator
     */
    bool checkGen(void);

    /**
     * Initialize dynamic simulation data structures
     */
    void setDSParams();

    /**
     * Evaluate first part of a dynamic simulation step
     * @param flag false if step is not initial step
     */
    void initDSStep(bool flag);

    /**
     * Evaluate predictor part of dynamic simulation step
     * @param t_inc time increment
     */
    void predDSStep(double t_inc);

    /**
     * Evaluate corrector part of dynamic simulation step
     * @param t_inc time increment
     */
    void corrDSStep(double t_inc);

  private:

    NetworkPtr p_network;
};

} // dynamic_simulation
} // gridpack
#endif
