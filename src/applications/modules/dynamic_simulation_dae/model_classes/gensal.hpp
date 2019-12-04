/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
// -------------------------------------------------------------
/**
 * @file   gensal.hpp
 * @author Zakaria El Mrabet 
 * @Modified:   10/10/19
 * @Last Modified: Shri 12/04/19
 * 
 * @brief: Header file for GENSAL model  
 * 
 * 
 */

#ifndef _gensal_h_
#define _gensal_h_

#include <base_gen_model.hpp>
#include <gridpack/include/gridpack.hpp>
#include "boost/smart_ptr/shared_ptr.hpp"

class GensalGen: public BaseGenModel
{
   public:
  /**
     * Basic constructor
     */
    GensalGen();

    /**
     * Basic destructor
     */
    ~GensalGen();

    /**
     * Load parameters from DataCollection object into generator model
     * @param data collection of generator parameters from input files
     * @param index of generator on bus
     * TODO: might want to move this functionality to BaseGeneratorModel
     */
    void load(const boost::shared_ptr<gridpack::component::DataCollection>
        data, int idx);

    /**
     * Saturation function
     * @ param x
     */
    double Sat(double x); 

    /**
     * Initialize generator model before calculation
     * @param [output] values - array where initialized generator variables should be set
     */
  void init(gridpack::ComplexType *values);

    /**
     * Write output from generators to a string.
     * @param string (output) string with information to be printed out
     * @param bufsize size of string buffer in bytes
     * @param signal an optional character string to signal to this
     * routine what about kind of information to write
     * @return true if bus is contributing string to output, false otherwise
     */
    bool serialWrite(char *string, const int bufsize,
        const char *signal);

    double getAngle();

    /**
     * Write out generator state
     * @param signal character string used to determine behavior
     * @param string buffer that contains output
     */
    void write(const char* signal, char* string);

    /**
     *  Set the number of variables for this generator model
     *  @param [output] number of variables for this model
     */
    bool vectorSize(int *nvar) const;

    /**
     * Set the internal values of the voltage magnitude and phase angle. Need this
     * function to push values from vectors back onto generators
     * @param values array containing generator state variables
     */
     void setValues(gridpack::ComplexType*);

    /**
     * Return the values of the generator vector block
     * @param values: pointer to vector values
     * @return: false if generator does not contribute
     *        vector element
     */
    bool vectorValues(gridpack::ComplexType *values);

    /**
     * Calculate current Norton injections
     */
    void currentNortonInjection();

    /**
     * Return the generator current injection (in rectangular form) 
     * @param [output] IGD - real part of the generator current
     * @param [output] IGQ - imaginary part of the generator current
     */
    void getCurrent(double *IGD, double *IGQ);

    /**
     * Return the matrix entries
     * @param [output] nval - number of values set
     * @param [output] row - row indices for matrix entries
     * @param [output] col - col indices for matrix entries
     * @param [output] values - matrix entries
     * return true when matrix entries set
     */
    bool matrixDiagEntries(int *nval,int *row, int *col, gridpack::ComplexType *values);

  private:
    // Machine parameters
    double H, D, Ra, Xd, Xq, Xdp, Xdpp, Xl;
    double Tdop, Tdopp, Tqopp, S10, S12, Tqop;

    // Internal constants
    double Efd; // Field voltage
    double LadIfd; // Field current
    double Pmech; // Mechanical power
  
    // Generator variables and their derivatives
    double Vterm, Theta; // Terminal voltage magnitude and angle
    double Ir, Ii; // Terminal current
    double Id, Iq; // Generator current on d and q axis

    double B, G;
    double Vd, Vq;
    
    double Psiqpp, Xqp, Xqpp;

    // Variables and their derivatives
    double x1d, x2w, x3Eqp, x4Psidp, x5Psiqp; 
    double dx1d, dx2w, dx3Eqp, dx4Psidp, dx5Psiqp;
    // previous step values of the variables
    double x1dprev, x2wprev, x3Eqpprev, x4Psidpprev, x5Psiqpprev; 

};

#endif
