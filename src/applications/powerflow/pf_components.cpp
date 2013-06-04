// -------------------------------------------------------------
/**
 * @file   pf_components.cpp
 * @author Bruce Palmer
 * @date   June 4, 2013
 * 
 * @brief  
 * 
 * 
 */
// -------------------------------------------------------------

#include "gridpack/utilities/complex_type.hpp"
#include "gridpack/component/base_component.hpp"
#include "gridpack/component/data_collection.hpp"

/**
 *  Simple constructor
 */
gridpack::powerflow::PFBus::PFBus(void)
{
  p_shunt_gs = 0.0;
  p_shunt_bs = 0.0;
}

/**
 *  Simple destructor
 */
gridpack::powerflow::PFBus::~PFBus(void)
{
}

/**
 *  Return size of matrix block contributed by the component
 *  @param isize, jsize: number of rows and columns of matrix block
 *  @return: false if network component does not contribute matrix element
 */
bool gridpack::powerflow::PFBus::matrixSize(int *isize, int *jsize) const
{
}

/**
 * Return the values of the matrix block. The values are
 * returned in row-major order.
 * @param values: pointer to matrix block values
 * @return: false if network component does not contribute matrix element
 */
bool gridpack::powerflow::PFBus::matrixValues(void *values)
{
  gridpack::ComplexType ret(0.0,0.0);
  std::vector<boost::shared_ptr<PFBranch> > branches = getBranchNeighbors();
  int size = branches.size();
  int i;
  boost::shared_ptr<PFBus> This(this);
  for (i=0; i<size; i++) {
    ret += branches[i]->getAdmittance();
    ret += branches[i]->getTransformer(This);
    ret += branches[i]->getShunt(This);
  }
  *values = ret;
  return true;
}

/**
 * Load values stored in DataCollection object into PFBus object. The
 * DataCollection object will have been filled when the network was created
 * from an external configuration file
 * @param data: DataCollection object contain parameters relevant to this
 *       bus that were read in when network was initialized
 */
void gridpack::powerflow::PFBus::load(shared_ptr<gridpack::component::DataCollection> data)
{
  bool ok = true;
  ok = ok && data->getValue(BUS_SHUNT_GS, &p_shunt_gs);
  ok = ok && data->getValue(BUS_SHUNT_BS, &p_shunt_bs);
}

/**
 *  Simple constructor
 */
gripack::powerflow::PFBranch::PFBranch(void)
{
  p_reactance = 0.0;
  p_resistance = 0.0;
  p_tap_ratio = 1.0;
  p_phase_shift = 0.0;
  p_charging = 0.0;
  p_shunt_admt_g1 = 0.0;
  p_shunt_admt_b1 = 0.0;
  p_shunt_admt_g2 = 0.0;
  p_shunt_admt_b2 = 0.0;
}

/**
 *  Simple destructor
 */
gripack::powerflow::PFBranch::~PFBranch(void)
{
}

/**
 *  Return size of matrix block contributed by the component
 *  @param isize, jsize: number of rows and columns of matrix block
 *  @return: false if network component does not contribute matrix element
 */
bool gripack::powerflow::PFBranch::matrixSize(int *isize, int *jsize) const
{
  *isize = 1;
  *jsize = 1;
  return true;
}

/**
 * Return the values of the matrix block. The values are
 * returned in row-major order.
 * @param values: pointer to matrix block values
 * @return: false if network component does not contribute matrix element
 */
bool gripack::powerflow::PFBranch::matrixValues(void *values)
{
  gripack::ComplexType ret(p_resistance,p_reactance);
  ret = -1.0/ret;
  gridpack::ComplexType a(cos(p_phase_shift),sin(p_phase_shift));
  a = p_tap_ratio*a;
  ret = ret - ret/a.conjg();
  *values = ret;
  return true;
}

/**
 * Load values stored in DataCollection object into PFBranch object. The
 * DataCollection object will have been filled when the network was created
 * from an external configuration file
 * @param data: DataCollection object contain parameters relevant to this
 *       branch that were read in when network was initialized
 */
void gripack::powerflow::PFBranch::load(shared_ptr<gridpack::component::DataCollection> data)
{
  bool ok = true;
  ok = ok && data->getValue(BRANCH_REACTANCE, &p_reactance);
  ok = ok && data->getValue(BRANCH_RESISTANCE, &p_resistance);
  ok = ok && data->getValue(BRANCH_TAP_RATIO, &p_tap_ratio);
  ok = ok && data->getValue(BRANCH_PHASE_SHIFT, &p_phase_shift);
  ok = ok && data->getValue(BRANCH_CHARGING, &p_charging);
  ok = ok && data->getValue(BRANCH_SHUNT_ADMTTNC_G1, &p_shunt_admt_g1);
  ok = ok && data->getValue(BRANCH_SHUNT_ADMTTNC_B1, &p_shunt_admt_b1);
  ok = ok && data->getValue(BRANCH_SHUNT_ADMTTNC_G2, &p_shunt_admt_g2);
  ok = ok && data->getValue(BRANCH_SHUNT_ADMTTNC_B2, &p_shunt_admt_b2);
}

/**
 * Return the complex admittance of the branch
 * @return: complex addmittance of branch
 */
gridpack::ComplexType getAdmittance(void)
{
  gripack::ComplexType ret(p_resistance, p_reactance);
  return -1.0/ret;
}

/**
 * Return transformer contribution from the branch to the calling
 * bus
 * @param bus: pointer to the bus making the call
 * @return: contribution to Y matrix from branch
 */
gridpack::ComplexType getTransformer(boost::shared_ptr<PFBus> bus)
{
  gridpack::ComplexType ret(p_resistance,p_reactance);
  ret = -1.0/ret;
  if (bus == getBus1()) {
    ret = ret/(p_tap_ratio*p_tap_ratio);
  } else if (bus == getBus2()) {
    // No further action required
  } else {
    // TODO: Some kind of error
  }
  return ret;
}

/**
 * Return the contribution to a bus from shunts
 * @param bus: pointer to the bus making the call
 * @return: contribution to Y matrix from shunts associated with branches
 */
gridpack::ComplexType getShunt(boost::shared_ptr<PFBus> bus)
{
  double retr, reti
  retr = 0.5*p_charging;
  reti = 0.0;
  if (bus == getBus1()) {
    retr += p_shunt_admt_g1;
    reti += p_shunt_admt_b1;
  } else if (bus == getBus2()) {
    retr += p_shunt_admt_g2;
    reti += p_shunt_admt_b2;
  } else {
    // TODO: Some kind of error
  }
  return gridpack::ComplexType(retr,reti);
}
