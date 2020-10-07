// -------------------------------------------------------------
// file: gridpack.cpp
// -------------------------------------------------------------
// -------------------------------------------------------------
/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
// -------------------------------------------------------------
// -------------------------------------------------------------
// Created January 24, 2020 by Perkins
// Last Change: 2020-08-25 08:02:15 d3g096
// -------------------------------------------------------------

#include <pybind11/pybind11.h>
namespace py = pybind11;
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <gridpack/environment/environment.hpp>
#include <gridpack/environment/no_print.hpp>
#include <gridpack/parallel/communicator.hpp>
#include <gridpack/parallel/task_manager.hpp>
#include <gridpack/applications/modules/hadrec/hadrec_app_module.hpp>

namespace gp = gridpack;
namespace gpp = gridpack::parallel;
namespace gph = gridpack::hadrec;
namespace gpds = gridpack::dynamic_simulation;

// Some temporary hacks

// #define RHEL_OPENMPI_HACK 1
#ifdef RHEL_OPENMPI_HACK

// This stupidity is needed on RHEL7 with stock OpenMPI packages
// installed.

#include <dlfcn.h>

// -------------------------------------------------------------
// stupid_openmpi_hack
// from https://github.com/baidu-research/tensorflow-allreduce/issues/4
// -------------------------------------------------------------
static void 
stupid_openmpi_hack(void)
{
  void *handle = NULL;
  int mode = RTLD_NOW | RTLD_GLOBAL;

  // GNU/Linux and others 
#ifdef RTLD_NOLOAD
      mode |= RTLD_NOLOAD;
#endif
  if (!handle) handle = dlopen("libmpi.so.20", mode);
  if (!handle) handle = dlopen("libmpi.so.12", mode);
  if (!handle) handle = dlopen("libmpi.so.1", mode);
  if (!handle) handle = dlopen("libmpi.so.0", mode);
  if (!handle) handle = dlopen("libmpi.so", mode);
}

#endif

/// A functor to keep smart pointers from deleting their pointer
struct null_deleter
{
  void operator()(void const *) const { }
};


// GridPACK uses Boost smart pointers, so let's use those here
PYBIND11_DECLARE_HOLDER_TYPE(T, boost::shared_ptr<T>, false);

// Some pybind11 magic for a vector of Event
PYBIND11_MAKE_OPAQUE( std::vector< gpds::Event > )


// Hack to return a value from nextTask functions
class TaskCounter
{
public:
  TaskCounter(){};
  ~TaskCounter(){};
  int task_id;
};

// Wrapper class to deal with method pointer arguments (nextTask)
class TaskManagerWrapper
{
public:
  TaskManagerWrapper(gpp::Communicator &comm)
  {
    p_tskmgr.reset(new gpp::TaskManager(comm));
  }
  ~TaskManagerWrapper()
  { }
  void set(int ntask)
  {
    p_tskmgr->set(ntask);
  }
  bool nextTask(TaskCounter &next)
  {
    return p_tskmgr->nextTask(&next.task_id);
  }
  bool nextTask(gpp::Communicator &comm, TaskCounter &next)
  {
    return p_tskmgr->nextTask(comm,&next.task_id);
  }
  void cancel()
  {
    p_tskmgr->cancel();
  }
  void printStats()
  {
    p_tskmgr->printStats();
  }
private:
  boost::shared_ptr<gpp::TaskManager> p_tskmgr;
};

// -------------------------------------------------------------
// gridpack module
// -------------------------------------------------------------
PYBIND11_MODULE(gridpack, gpm) {
  gpm.doc() = "GridPACK module";

#ifdef RHEL_OPENMPI_HACK
  stupid_openmpi_hack();
#endif

  // -------------------------------------------------------------
  // gridpack.Envronment
  // -------------------------------------------------------------
  py::class_<gp::Environment, boost::shared_ptr<gp::Environment> >(gpm, "Environment")
    .def(py::init<>([]()
                    { return boost::shared_ptr<gp::Environment>
                        (new gp::Environment(0, NULL)); }))
    ;

  // -------------------------------------------------------------
  // gridpack.NoPrint
  // -------------------------------------------------------------
  py::class_<gp::NoPrint, std::unique_ptr<gp::NoPrint, py::nodelete> >(gpm, "NoPrint")
    .def(py::init([](){
                    return std::unique_ptr<gp::NoPrint, py::nodelete>
                      (gp::NoPrint::instance());
                  }))
    .def("status", &gp::NoPrint::status)
    .def("setStatus", &gp::NoPrint::setStatus)
    ;

  // -------------------------------------------------------------
  // gridpack.Communicator
  // -------------------------------------------------------------
  py::class_<gpp::Communicator>(gpm, "Communicator")
    .def(py::init<>())
    .def("size", &gpp::Communicator::size)
    .def("rank", &gpp::Communicator::rank)
    .def("worldRank", &gpp::Communicator::worldRank)
    .def("barrier", &gpp::Communicator::barrier)
    .def("sync", &gpp::Communicator::sync)
    .def("divide", &gpp::Communicator::divide)
    .def("split", &gpp::Communicator::split)
    ;
    
  // -------------------------------------------------------------
  // gridpack.TaskCounter
  // -------------------------------------------------------------
  py::class_<TaskCounter>(gpm, "TaskCounter")
    .def(py::init<>())
    .def_readwrite("task_id", &TaskCounter::task_id)
    ;

  // -------------------------------------------------------------
  // gridpack.TaskManager
  // -------------------------------------------------------------
  py::class_<TaskManagerWrapper> (gpm, "TaskManager")
    .def(py::init<gpp::Communicator&>())
    .def("set", &TaskManagerWrapper::set)
    .def("nextTask",
         (bool (TaskManagerWrapper::*)(TaskCounter&))
         &TaskManagerWrapper::nextTask)
    .def("nextTask",
         (bool (TaskManagerWrapper::*)(gpp::Communicator&, TaskCounter&))
         &TaskManagerWrapper::nextTask)
    .def("cancel", &TaskManagerWrapper::cancel)
    .def("printStats", &TaskManagerWrapper::printStats)
    ;

  // -------------------------------------------------------------
  // gridpack.dynamic_simulation module
  // -------------------------------------------------------------
  py::module dsm =
    gpm.def_submodule("dynamic_simulation",
                      "GridPACK Dynamic Simulation Application module");

  // -------------------------------------------------------------
  // gridpack.dynamic_simulation.EventVector
  // -------------------------------------------------------------
  py::bind_vector< std::vector< gpds::Event > >(dsm, "EventVector");

  // -------------------------------------------------------------
  // gridpack.dynamic_simulation.Event
  // -------------------------------------------------------------
  py::class_<gpds::Event>(dsm, "Event")
    .def(py::init<>())
    .def_readwrite("start", &gpds::Event::start)
    .def_readwrite("end", &gpds::Event::end)
    .def_readwrite("step", &gpds::Event::step)
    .def_readwrite("tag", &gpds::Event::tag)
    .def_readwrite("isGenerator", &gpds::Event::isGenerator)
    .def_readwrite("isBus", &gpds::Event::isBus)
    .def_readwrite("bus_idx", &gpds::Event::bus_idx)
    .def_readwrite("isLine", &gpds::Event::isLine)
    .def_readwrite("from_idx", &gpds::Event::from_idx)
    .def_readwrite("to_idx", &gpds::Event::to_idx)
    ;

  
  // -------------------------------------------------------------
  // gridpack.hadrec module
  // -------------------------------------------------------------

  py::module hadm =
    gpm.def_submodule("hadrec", "GridPACK HADREC Application module");
  
  // -------------------------------------------------------------
  // gridpack.hadrec.Action
  // -------------------------------------------------------------
  py::class_<gph::HADRECAction>(hadm, "Action")
    .def(py::init<>())
    .def_readwrite("actiontype", &gph::HADRECAction::actiontype)
    .def_readwrite("bus_number", &gph::HADRECAction::bus_number)
    .def_readwrite("componentID", &gph::HADRECAction::componentID)
    .def_readwrite("percentage", &gph::HADRECAction::percentage)
    .def_readwrite("brch_from_bus_number", &gph::HADRECAction::brch_from_bus_number)
    .def_readwrite("brch_to_bus_number", &gph::HADRECAction::brch_to_bus_number)
    .def_readwrite("branch_ckt", &gph::HADRECAction::branch_ckt)
    ;

  // -------------------------------------------------------------
  // gridpack.hadrec.Module
  // -------------------------------------------------------------
  py::class_<gph::HADRECAppModule> hadapp(hadm, "Module");
  hadapp
    .def(py::init<>())
    .def("transferPFtoDS", &gph::HADRECAppModule::transferPFtoDS)
    .def("executeDynSimuOneStep", &gph::HADRECAppModule::executeDynSimuOneStep)
    .def("isDynSimuDone",  &gph::HADRECAppModule::isDynSimuDone)
    .def("applyAction", &gph::HADRECAppModule::applyAction)
    .def("getObservations", &gph::HADRECAppModule::getObservations,
         py::return_value_policy::copy)
    ;

  // These methods need to be reworked char * and/or optional args
  hadapp
    .def("initializeDynSimu",
         [](gph::HADRECAppModule& self, std::vector< gpds::Event > faults, int dscase_idx) {
           self.initializeDynSimu(faults, dscase_idx);
         },
         py::arg("faults") = std::vector< gpds::Event >(), py::arg("dscase_idx") = -1
         )
    .def("solvePowerFlowBeforeDynSimu",
         [](gph::HADRECAppModule& self, const std::string& s, int pfcase_idx) {
           self.solvePowerFlowBeforeDynSimu(s.c_str(), pfcase_idx);
         },
         py::arg("s") = "", py::arg("pfcase_idx") = -1
         )
    .def("fullInitializationBeforeDynSimuSteps",
         [](gph::HADRECAppModule& self, const std::string& s,
            const std::vector<gpds::Event>& BusFaults, int pfcase_idx, int dscase_idx) {
           self.fullInitializationBeforeDynSimuSteps(s.c_str(), BusFaults,
                                                     pfcase_idx, dscase_idx);
         },
         py::arg("s") = "",
         py::arg("BusFaults") = std::vector<gpds::Event>(),
         py::arg("pfcase_idx") = -1,
         py::arg("dscase_idx")
         )
    ;

  // This method returns a tuple containing 3 lists (int, string, int)

  hadapp
    .def("getObservationLists",
         [](gph::HADRECAppModule& self) {
             std::vector<int> obs_genBus;
             std::vector<std::string> obs_genIDs;
             std::vector<int> obs_loadBuses;
             std::vector<std::string> obs_loadIDs;
             std::vector<int> obs_busIDs;
             self.getObservationLists(obs_genBus, obs_genIDs,
                                      obs_loadBuses, obs_loadIDs, obs_busIDs);
             return py::make_tuple(obs_genBus, obs_genIDs,
                                   obs_loadBuses, obs_loadIDs, obs_busIDs);
         })
    ;
             
  // These methods return a tuple on success or False on failure
  hadapp
    .def("getBusTotalLoadPower",
         [](gph::HADRECAppModule& self, const int& busid) -> py::object {
           double pg, qg;
           bool flag;
           flag = self.getBusTotalLoadPower(busid, pg, qg);
           if (flag) {
             return py::make_tuple(pg, qg);
           } else {
             return py::cast<py::none>(Py_None);
           }
         })
    .def("getGeneratorPower",
         [](gph::HADRECAppModule& self, const int& busid, const std::string& genid) -> py::object {
           double pg, qg;
           bool flag;
           flag = self.getGeneratorPower(busid, genid, pg, qg);
           if (flag) {
             return py::make_tuple(pg, qg);
           } else {
             return py::cast<py::none>(Py_None);
           }
         })
    .def("getZoneLoads",
         [](gph::HADRECAppModule& self) -> py::object {
           std::vector<double> load_p, load_q;
           std::vector<int> zone_id;
           bool flag = self.getZoneLoads(load_p, load_q, zone_id);
           if (flag) {
             return py::make_tuple(load_p, load_q, zone_id);
           } else {
             return py::cast<py::none>(Py_None);
           }
         })
    .def("getZoneGeneratorPower",
         [](gph::HADRECAppModule& self) -> py::object {
           std::vector<double> generator_p, generator_q;
           std::vector<int> zone_id;
           bool flag = self.getZoneGeneratorPower(generator_p, generator_q, zone_id);
           if (flag) {
             return py::make_tuple(generator_p, generator_q, zone_id);
           } else {
             return py::cast<py::none>(Py_None);
           }
         })
    ;
  

}