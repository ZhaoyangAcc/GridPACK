/**
 * @file   nonlinear_solver_test.cpp
 * @author William A. Perkins
 * @date   2013-08-13 11:37:08 d3g096
 * 
 * @brief  Unit tests for NonlinearSolver
 * 
 * 
 */

#include <mpi.h>
#include <iostream>
#include <boost/format.hpp>

#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

#include "gridpack/parallel/parallel.hpp"
#include "gridpack/utilities/exception.hpp"
#include "math.hpp"
#include "nonlinear_solver.hpp"



BOOST_AUTO_TEST_SUITE(NonlinearSolver)


// -------------------------------------------------------------
// In this test, a very small nonlinear system is solved, too small to
// be parallel. So, each process solves it separately.
// -------------------------------------------------------------

struct build_tiny_jacobian_1
{
  void operator() (const gridpack::math::Vector& X, gridpack::math::Matrix& J) const
  {
    gridpack::ComplexType x, y;
    X.get_element(0, x);
    X.get_element(1, y);
    J.set_element(0, 0, 2.0*x-2.0);
    J.set_element(0, 1, -1);
    J.set_element(1, 0, 2.0*x);
    J.set_element(1, 1, 8.0*y);
    J.ready();
    // J.print();
  }
};

struct build_tiny_function_1
{
  void operator() (const gridpack::math::Vector& X, gridpack::math::Vector& F) const
  {
    gridpack::ComplexType x, y;
    X.get_element(0, x);
    X.get_element(1, y);
    F.set_element(0, x*x - 2.0*x - y + 0.5);
    F.set_element(1, x*x + 4.0*y*y - 4.0);
    F.ready();
    // F.print();
  }
};

BOOST_AUTO_TEST_CASE( tiny_serial_1 )
{
  boost::mpi::communicator world;
  boost::mpi::communicator self = world.split(world.rank());

  gridpack::math::JacobianBuilder j = build_tiny_jacobian_1();
  gridpack::math::FunctionBuilder f = build_tiny_function_1();

  gridpack::math::NonlinearSolver solver(self, 2, j, f);
  gridpack::math::Vector X(self, 2);
  X.set_element(0, 2.00);
  X.set_element(1, 0.25);
  X.ready();
  solver.solve(X);
  BOOST_TEST_MESSAGE("tiny_serial_1 results:");
  X.print();

  gridpack::ComplexType x, y;
  X.get_element(0, x);
  X.get_element(1, y);

  BOOST_CHECK_CLOSE(real(x), 1.900677, 1.0e-04);
  BOOST_CHECK_CLOSE(real(y), 0.3112186, 1.0e-04);
}

// -------------------------------------------------------------
// Another tiny test. This one is example 1 (not hard) from the PETSc
// SNES examples.
// -------------------------------------------------------------

void 
build_tiny_jacobian_2(const gridpack::math::Vector& X, gridpack::math::Matrix& J)
{
  gridpack::ComplexType x, y;
  X.get_element(0, x);
  X.get_element(1, y);
  J.set_element(0, 0, 2.0*x + y);
  J.set_element(0, 1, x);
  J.set_element(1, 0, y);
  J.set_element(1, 1, x + 2.0*y);
  J.ready();
  // J.print();
}

void
build_tiny_function_2(const gridpack::math::Vector& X, gridpack::math::Vector& F)
{
  gridpack::ComplexType x, y;
  X.get_element(0, x);
  X.get_element(1, y);
  F.set_element(0, x*x + x*y - 3.0);
  F.set_element(1, x*y + y*y - 6.0);
  F.ready();
  // F.print();
}

BOOST_AUTO_TEST_CASE( tiny_serial_2 )
{
  boost::mpi::communicator world;
  boost::mpi::communicator self = world.split(world.rank());

  gridpack::math::JacobianBuilder j = &build_tiny_jacobian_2;
  gridpack::math::FunctionBuilder f = &build_tiny_function_2;

  gridpack::math::NonlinearSolver solver(self, 2, j, f);
  gridpack::math::Vector X(self, 2);
  X.set_element(0, 2.00);
  X.set_element(1, 3.00);
  X.ready();
  solver.solve(X);

  BOOST_TEST_MESSAGE("tiny_serial_2 results:");
  X.print();

  gridpack::ComplexType x, y;
  X.get_element(0, x);
  X.get_element(1, y);

  BOOST_CHECK_CLOSE(real(x), 1.0, 1.0e-04);
  BOOST_CHECK_CLOSE(real(y), 2.0, 1.0e-04);
}

struct build_jacobian_2
{
  void operator() (const gridpack::math::Vector& X, gridpack::math::Matrix& J) const
  {
    int n(X.size());
    gridpack::ComplexType d(static_cast<double>(n - 1));
    gridpack::ComplexType h(1.0/d);
    d *= d;

    int lo, hi;
    X.local_index_range(lo, hi);

    for (int row = lo; row < hi; ++row) {
      if (row == 0 || row == n - 1) {
        J.set_element(row, row, 1.0);
      } else {
        int i[3] = { row,     row, row };
        int j[3] = { row - 1, row, row + 1 };
        gridpack::ComplexType x;
        X.get_element(row, x);
        gridpack::ComplexType A[3] = 
          { d, -2.0*d + 2.0*x, d};
        J.set_elements(3, i, j, A);
      }
    }
    J.ready();
    // J.print();
  }
};

struct build_function_2
{
  void operator() (const gridpack::math::Vector& X, gridpack::math::Vector& F) const
  {
    int n(X.size());
    gridpack::ComplexType d(static_cast<double>(n - 1));
    gridpack::ComplexType h(1.0/d);

    d *= d;

    int lo, hi;
    X.local_index_range(lo, hi);
    
    int imin(std::max(lo-1, 0));
    int imax(std::min(hi+1, n));
    std::vector<gridpack::ComplexType> x(n);
    X.get_all_elements(&x[0]);

    for (int row = lo; row < hi; ++row) {
      gridpack::ComplexType f;
      int i(row);
      if (row == 0) {
        f = x[i];
      } else if (row == n - 1) {
        f = x[i] - 1.0;
      } else {
        gridpack::ComplexType xp, g;
        xp = static_cast<double>(row)*h;
        g = 6.0*xp + pow(xp+1.0e-12, 6);
        f = d*(x[i-1] - 2.0*x[i] + x[i+1]) + x[i]*x[i] - g;
      }
      F.set_element(row, f);
    }
    F.ready();
    // F.print();
  }
};

BOOST_AUTO_TEST_CASE( example2 )
{
  boost::mpi::communicator world;
  static const int local_size(4);

  gridpack::math::JacobianBuilder j = build_jacobian_2();
  gridpack::math::FunctionBuilder f = build_function_2();

  gridpack::math::NonlinearSolver solver(world, local_size, j, f);
  gridpack::math::Vector X(world, local_size);
  X.fill(0.5);
  X.ready();
  solver.solve(X);
  X.print();
}


BOOST_AUTO_TEST_SUITE_END()


// -------------------------------------------------------------
// A larger test.  This is example 2 from the PETSc SNES examples
// -------------------------------------------------------------




// -------------------------------------------------------------
// init_function
// -------------------------------------------------------------
bool init_function()
{
  return true;
}

// -------------------------------------------------------------
//  Main Program
// -------------------------------------------------------------
int
main(int argc, char **argv)
{
  gridpack::parallel::Environment env(argc, argv);
  gridpack::math::Initialize();
  int result = ::boost::unit_test::unit_test_main( &init_function, argc, argv );
  gridpack::math::Finalize();
}
